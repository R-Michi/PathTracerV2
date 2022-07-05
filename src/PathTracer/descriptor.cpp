#include "../application.h"

void pt::PathTracer::create_descriptors(void)
{
    // init descriptor manager
    this->descriptors.set_device(this->setup->get_device());
    this->descriptors.set_descriptor_set_count(2);

    // count number of meshes
    uint32_t mesh_count = 0;
    for(size_t i = 0; i < this->models.size(); i++) mesh_count += this->models[i].size();

    // The first set (set = 0) contains the acceleration structure and the output image
    this->descriptors.add_binding(0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV);
    this->descriptors.add_binding(0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);

    // The second set (set = 1) contains the scene description, like vertices, vertex attributes and the materials
    this->descriptors.add_binding(1, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_count, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
    this->descriptors.add_binding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_count, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
    this->descriptors.add_binding(1, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
    this->descriptors.add_binding(1, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, this->materials.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
    this->descriptors.add_binding(1, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, this->materials.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
    this->descriptors.add_binding(1, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, this->materials.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
    this->descriptors.add_binding(1, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, this->materials.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
    this->descriptors.init();
    
    // location (set = 0, binding = 0) contains the output image of the path tracer
    VkDescriptorImageInfo out_image_info = {};
    out_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    out_image_info.imageView = this->render_target.view;
    out_image_info.sampler = VK_NULL_HANDLE;
    this->descriptors.write_image_info(0, 0, 0, 1, &out_image_info);

    // location (set = 0, binding = 1) contains the tlas acceleration structure
    this->descriptors.write_acceleration_structure_NV(0, 1, 0, 1, &this->tlas.as);

    // location (set = 1, binding = 0) contains all the vertex buffers of our meshes
    // location (set = 1, binding = 1) contains all the vertex attribute buffers of or meshes,
    // like normal vectors and texture coordinates
    std::vector<VkDescriptorBufferInfo> vertex_infos, attribute_infos;
    for(const auto& model : this->models)
    {
        for(const RenderMesh& mesh : model)
        {   
            // vertex buffer
            VkDescriptorBufferInfo bi = {};
            bi.buffer = mesh.vectices.handle();
            bi.offset = 0;
            bi.range = mesh.vectices.size();
            vertex_infos.push_back(bi);

            // attribute buffer
            bi.buffer = mesh.attributes.handle();
            bi.offset = 0;
            bi.range = mesh.attributes.size();
            attribute_infos.push_back(bi);
        }
    }

    // only to make sure that the number of descriptors matches the number of meshes
    if(vertex_infos.size() != mesh_count || attribute_infos.size() != mesh_count)
        throw std::runtime_error("[pt::PathTracer::create_descriptors]: The number of meshes do not match the number of "
                                 "vertex-buffer or vertex-attribute descriptors.");
    this->descriptors.write_buffer_info(1, 0, 0, mesh_count, vertex_infos.data());
    this->descriptors.write_buffer_info(1, 1, 0, mesh_count, attribute_infos.data());

    // location (set = 1, binding = 2) contains the buffer of all uniform materials
    VkDescriptorBufferInfo umtl_buffer_info = {};
    umtl_buffer_info.buffer = this->mtl_buffer.handle();
    umtl_buffer_info.offset = 0;
    umtl_buffer_info.range = this->mtl_buffer.size();
    this->descriptors.write_buffer_info(1, 2, 0, 1, &umtl_buffer_info);

    // location (set = 1, binding = 3) contains all emissive texture samplers
    // location (set = 1, binding = 4) contains all albedo texture samplers
    // location (set = 1, binding = 5) contains all rma texture samplers
    // location (set = 1, binding = 6) contains all alpha texture samplers
    std::vector<VkDescriptorImageInfo> material_infos;
    for(const RenderMaterial& mtl : this->materials)
    {
        VkDescriptorImageInfo ii = {};
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ii.imageView = mtl.texture.emission.views().at(0);
        ii.sampler = mtl.texture.emission.sampler();
        material_infos.push_back(ii);
    }
    for(const RenderMaterial& mtl : this->materials)
    {
        VkDescriptorImageInfo ii = {};
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ii.imageView = mtl.texture.arman.views().at(0); // array layer 0 = albedo map
        ii.sampler = mtl.texture.arman.sampler();
        material_infos.push_back(ii);

    }
    for(const RenderMaterial& mtl : this->materials)
    {
        VkDescriptorImageInfo ii = {};
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ii.imageView = mtl.texture.arman.views().at(1); // array layer 1 = roughness, metallic, alpha map
        ii.sampler = mtl.texture.arman.sampler();
        material_infos.push_back(ii);

    }
    for(const RenderMaterial& mtl : this->materials)
    {
        VkDescriptorImageInfo ii = {};
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ii.imageView = mtl.texture.arman.views().at(2); // array layer 2 = normal map
        ii.sampler = mtl.texture.arman.sampler();
        material_infos.push_back(ii);

    }

    // to make sure that there are as many image descriptors as material textures
    if(material_infos.size() != (4 * this->materials.size()))
        throw std::runtime_error("[pt::PathTracer::create_descriptors]: The number of image descriptors must match the number of material textures.");

    for(uint32_t i = 0; i < 4; i++)
        this->descriptors.write_image_info(1, i + 3, 0, this->materials.size(), material_infos.data() + i * this->materials.size());

    // update descriptors
    this->descriptors.update();
}
