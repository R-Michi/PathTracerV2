#include "../application.h"

void pt::PathTracer::load_model(const std::string& path)
{
    this->model_paths.push_back(path);
}

void pt::PathTracer::create_render_models(void)
{
    this->models.clear();
    this->materials.clear();

    // create buffers for every mesh of every model
    this->models.reserve(this->model_paths.size()); // allocate enough mesh vectors for every model
    uint32_t gID = 0;
    for(const std::string& path : this->model_paths)
    {
        vka::Model model;
        model.load(path, vka::VKA_MODEL_LOAD_OPTION_FORCE_PER_MESH_MATERIAL);   // if loading was not successful, model.load() will throw an exception
        this->models.resize(this->models.size() + 1);           // create a new vector of render meshes
        this->models.back().reserve(model.meshes().size());     // allocate enough meshes inside the current mesh vector

        for(const vka::Mesh& mesh : model.meshes())
        {   
            this->models.back().resize(this->models.back().size() + 1); // create a new mesh
            RenderMesh& rmesh = this->models.back().back();

            // the current mesh is the last mesh allocated, and the current model is the last mesh vector allocated
            this->load_render_mesh(mesh, rmesh);

            // the geometry IDs are a sequential number and are unique per mesh
            rmesh.properties.record.geometryID = gID++;

            // However, multiple materials may have the same material ID.
            // Those ID's are continued counting upwards across models, but inside a
            // model the ID's start at 0. To get a unique range of material ID's
            // per model, the ID's of the current model are offset by the sum of all materials of the
            // models before. Moreover the material ID's can safely offset by this amount because
            // there are as many unique material ID's are materials in the material vector of the model.
            // However, this does only work if all meshes are loaded!!!
            rmesh.properties.record.materialID = this->materials.size() + mesh.materials().at(0);
        }

        // load material properties
        for(const tinyobj::material_t& mtl : model.materials())
        {
            this->materials.resize(this->materials.size() + 1);
            RenderMaterial& rmtl = this->materials.back();

            // set all properties that are requiered to load the materials
            rmtl.properties.single_emission = glm2::vec3(mtl.emission[0], mtl.emission[1], mtl.emission[2]);
            rmtl.properties.map_albedo      = mtl.diffuse_texname;
            rmtl.properties.map_emission    = mtl.emissive_texname;
            rmtl.properties.map_roughness   = mtl.roughness_texname;
            rmtl.properties.map_metallic    = mtl.metallic_texname;
            rmtl.properties.map_alpha       = mtl.alpha_texname;
            rmtl.properties.map_normal      = mtl.normal_texname;

            // the non-texture materials can also be set at this stage
            rmtl.uniform.ior                = mtl.ior;
        }

        // call callback for model loading
        if(this->model_load_callback != nullptr)
            this->model_load_callback(path);
    }

    // the model paths are not needed anymore
    this->model_paths.clear();
}

void pt::PathTracer::load_render_mesh(const vka::Mesh& mesh, RenderMesh& rmesh)
{
    static const std::vector<vka::VertexAttributeType> merge_attribs = {
        vka::VKA_VERTEX_ATTRIBUTE_TYPE_NORMAL,
        vka::VKA_VERTEX_ATTRIBUTE_TYPE_TEXTURE_COORDINATE  
    };

    std::vector<float> vertex_attributes;
    mesh.merge(vertex_attributes, merge_attribs);
    const size_t attrib_size = vertex_attributes.size() * sizeof(float);

    // init vertex buffer
    rmesh.vectices.set_create_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    rmesh.vectices.set_memory_properties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // init attribute buffer
    rmesh.attributes.set_create_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    rmesh.attributes.set_memory_properties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // init index buffer
    rmesh.indices.set_create_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    rmesh.indices.set_memory_properties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // partly initialize the mesh propertis, the other part is initialized inside create_render_buffers
    rmesh.properties.vertex_count       = mesh.vertex_count();
    rmesh.properties.vertex_format      = VK_FORMAT_R32G32B32_SFLOAT;
    rmesh.properties.attribute_stride   = (mesh.normal_component_count() + mesh.texcoord_component_count()) * sizeof(float);
    rmesh.properties.index_count        =  mesh.index_count();
    // the vertex attribute consists of a normal vector (XYZ) and a texture coordinate (UV) and the size of one component = sizeof(float)
    // -> (size of attribute) = (number of components) * (size of one component)

    // create staging buffers
    // 0: staging buffer for vertices
    // 1: staging buffer for vertex attributes
    // 2: staging buffer for indices
    vka::Buffer staging[3];
    for(uint32_t i = 0; i < 3; i++)
    {
        staging[i].set_create_flags(0);
        staging[i].set_create_usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        staging[i].set_create_sharing_mode(VK_SHARING_MODE_EXCLUSIVE);
        staging[i].set_create_queue_families(&this->setup->get_rt_queue_info().queueFamilyIndex, 1);
        staging[i].set_memory_properties(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        staging[i].set_device(this->setup->get_device());
        staging[i].set_physical_device(this->setup->get_physical_device());
    }
    staging[0].set_create_size(mesh.vertex_size());
    staging[1].set_create_size(attrib_size);
    staging[2].set_create_size(mesh.indices_size());

    if(staging[0].create() != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_render_mesh]: Failed to create staging buffer for vertices.");
    if(staging[1].create() != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_render_mesh]: Failed to create staging buffer for vertex attributes.");
    if(staging[2].create() != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_render_mesh]: Failed to create staging buffer for indices.");

    // stream data into the staging buffers
    void* map[3];
    #pragma unroll_completely
    for(uint32_t i = 0; i < 3; i++) map[i] = staging[i].map(staging[i].size(), 0);

    memcpy(map[0], mesh.pvertices(),            staging[0].size());
    memcpy(map[1], vertex_attributes.data(),    staging[1].size());
    memcpy(map[2], mesh.pindices(),             staging[2].size());

    #pragma unroll_completely
    for(uint32_t i = 0; i < 3; i++) staging[i].unmap();

    // secondary command buffers for copy operation
    VkCommandBuffer cbo[3];
    cbo[0] = vka::Buffer::enqueue_copy(this->setup->get_device(), this->cmd_pool, 1, staging + 0, &rmesh.vectices);
    cbo[1] = vka::Buffer::enqueue_copy(this->setup->get_device(), this->cmd_pool, 1, staging + 1, &rmesh.attributes);
    cbo[2] = vka::Buffer::enqueue_copy(this->setup->get_device(), this->cmd_pool, 1, staging + 2, &rmesh.indices);

    // execute copy operations
    if(vka::utility::execute_scb(this->setup->get_device(), this->cmd_pool, this->setup->get_rt_queue(), 3, cbo) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_render_mesh]: Failed to copy staging buffers.");
    vkFreeCommandBuffers(this->setup->get_device(), this->cmd_pool, 3, cbo);
}
