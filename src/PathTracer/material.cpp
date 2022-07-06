#include "../application.h"
#define STB_IMAGE_IMPLEMENTATION
#ifdef __clang__	// suppress waring "empty body" for clang for include file stb_image.h
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wempty-body"
        #include <stb/stb_image.h>
    #pragma clang diagnostic pop
#else
    #include <stb/stb_image.h>
#endif

void pt::PathTracer::load_environment(const std::string& path)
{
    this->environment_path = path;
}

void pt::PathTracer::create_render_materials(void)
{
    // load the non-texture materials into a uniform buffer
    this->load_mtl_buffer(this->materials);

    // load every texture material
    for(RenderMaterial& mtl : this->materials)
    {
        this->load_emissive_texture(mtl);
        this->load_arman_texture(mtl);

        // delete all texture string paths to free memory
        mtl.properties.map_albedo.clear();
        mtl.properties.map_alpha.clear();
        mtl.properties.map_emission.clear();
        mtl.properties.map_metallic.clear();
        mtl.properties.map_normal.clear();
        mtl.properties.map_roughness.clear();
    }

    // load the environment map texture
    this->load_environment_texture();
    this->environment_path.clear();
}

void pt::PathTracer::load_mtl_buffer(const material_array_t& mtlarray)
{
    const size_t mtl_buff_size = mtlarray.size() * sizeof(RenderMaterialUniform);

    // init material buffer
    this->mtl_buffer.set_create_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    this->mtl_buffer.set_memory_properties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // create staging buffer
    vka::Buffer staging(this->setup->get_physical_device(), this->setup->get_device());
    staging.set_create_flags(0);
    staging.set_create_size(mtl_buff_size);
    staging.set_create_usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    staging.set_create_sharing_mode(VK_SHARING_MODE_EXCLUSIVE);
    staging.set_create_queue_families(&this->setup->get_rt_queue_info().queueFamilyIndex, 1);
    staging.set_memory_properties(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(staging.create() != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_mtl_buffer]: Failed to create material staging buffer.");

    // stream data into the staging buffer
    RenderMaterialUniform* map = (RenderMaterialUniform*)staging.map(staging.size(), 0);
    for(size_t i = 0; i < mtlarray.size(); i++)
        map[i] = mtlarray.at(i).uniform;
    staging.unmap();

    // copy buffer
    VkCommandBuffer cbo = vka::Buffer::enqueue_copy(this->setup->get_device(), this->cmd_pool, 1, &staging, &this->mtl_buffer);
    if(vka::utility::execute_scb(this->setup->get_device(), this->cmd_pool, this->setup->get_rt_queue(), 1, &cbo) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_mtl_buffer]: Failed to copy staging buffer.");
}

void pt::PathTracer::load_emissive_texture(RenderMaterial& mtl)
{
    vka::Texture& tex = mtl.texture.emission;
    VkComponentMapping component_mapping = {};
    component_mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    component_mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    component_mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    component_mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    // load textue
    // If instead of a textrue there is only a single value,
    // there is still a texture loaded with an extent of 1x1x1.
    // Then this single pixel stored inside the texture contains the
    // HDR-color of the emission value.
    // If a path is set to a emission texture, the texture is loaded as usual
    VkExtent3D extent;
    float* data;
    if(mtl.properties.map_emission.size() == 0)
    {
        extent = {1, 1, 1};
        data = new float[4];    // eventough the extent contains one pixel, there is one padding element requiered
                                // for the glm2::vec3::load, as it has internally 4 components.
                                // Loaded to the texture are still only 3 elements.
        mtl.properties.single_emission.store(data);
    }
    else
    {
        int w, h;
        // force load 4 channels as only RGBA textures can be created
        data = stbi_loadf(mtl.properties.map_emission.c_str(), &w, &h, nullptr, 4);
        if(data == nullptr)
            throw std::runtime_error("[pt::PathTracer::load_emissive_texture]: Failed to load emissive image: " + mtl.properties.map_emission);
    }

    this->init_texture(tex);
    tex.set_image_format(VK_FORMAT_R32G32B32A32_SFLOAT);   // emission map is a HDR texture
    tex.set_image_extent(extent);
    tex.set_image_array_layers(1);
    tex.set_sampler_lod(0.0f, vka::Texture::to_mip_levels(extent));

    VkImageViewCreateInfo view;
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = nullptr;
    view.flags = 0;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    view.components = component_mapping;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = 1;

    tex.add_view(view);
    if(tex.load(0, data) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_emissive_texture]: Failed to load emissive image into texture.");
    stbi_image_free(data);

    if(tex.create(true, VK_FILTER_NEAREST) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_emissive_texture]: Failed to create emissive texture.");

    if(this->texture_load_callback != nullptr) this->texture_load_callback(mtl.properties.map_emission);
}

void pt::PathTracer::load_arman_texture(RenderMaterial& mtl)
{
    vka::Texture& tex = mtl.texture.arman;
    VkComponentMapping component_mapping = {};
    component_mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    component_mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    component_mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    component_mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    // data[0]: albedo image data
    // data[1]: roughness, metallic, alpha image data
    // data[2]: normal image data
    uint8_t* data[3];

    VkExtent3D extent;
    int w1, w2, h1, h2;
    data[0] = stbi_load(mtl.properties.map_albedo.c_str(), &w1, &h1, nullptr, 4);
    if(data[0] == nullptr)
        throw std::runtime_error("[pt::PathTracer::load_arman_texture]: Failed to load albedo image: " + mtl.properties.map_albedo);
    if(this->texture_load_callback != nullptr) this->texture_load_callback(mtl.properties.map_albedo);
    
    data[2] = stbi_load(mtl.properties.map_normal.c_str(), &w2, &h2, nullptr, 4);
    if(data[2] == nullptr)
        throw std::runtime_error("[pt::PathTracer::load_arman_texture]: Failed to load normal map image: " + mtl.properties.map_normal);
    if(w1 != w2 || h1 != h2)
        throw std::runtime_error("[pt::PathTracer::load_arman_texture]: All images from the same material MUST have the same extent.");
    if(this->texture_load_callback != nullptr) this->texture_load_callback(mtl.properties.map_normal);

    // tmp[0] = roughnes image data
    // tmp[1] = metallic image data
    // tmp[2] = alpha image data
    uint8_t* tmp[3];
    tmp[0] = stbi_load(mtl.properties.map_roughness.c_str(), &w2, &h2, nullptr, 1);
    if(tmp[0] == nullptr)
        throw std::runtime_error("[pt::PathTracer::load_arman_texture]: Failed to load roughness map image: " + mtl.properties.map_roughness);
    if(w1 != w2 || h1 != h2)
        throw std::runtime_error("[pt::PathTracer::load_arman_texture]: All images from the same material MUST have the same extent.");
    if(this->texture_load_callback != nullptr) this->texture_load_callback(mtl.properties.map_roughness);

    tmp[1] = stbi_load(mtl.properties.map_metallic.c_str(), &w2, &h2, nullptr, 1);
    if(tmp[1] == nullptr)
        throw std::runtime_error("[pt::PathTracer::load_arman_texture]: Failed to load metallic map image: " + mtl.properties.map_metallic);
    if(w1 != w2 || h1 != h2)
        throw std::runtime_error("[pt::PathTracer::load_arman_texture]: All images from the same material MUST have the same extent.");
    if(this->texture_load_callback != nullptr) this->texture_load_callback(mtl.properties.map_metallic);

    tmp[2] = stbi_load(mtl.properties.map_alpha.c_str(), &w2, &h2, nullptr, 1);
    if(tmp[2] == nullptr)
        throw std::runtime_error("[pt::PathTracer::load_arman_texture]: Failed to load alpha map image: " + mtl.properties.map_alpha);
    if(w1 != w2 || h1 != h2)
        throw std::runtime_error("[pt::PathTracer::load_arman_texture]: All images from the same material MUST have the same extent.");
    if(this->texture_load_callback != nullptr) this->texture_load_callback(mtl.properties.map_alpha);
    
    extent = {static_cast<uint32_t>(w1), static_cast<uint32_t>(h1), 1};
    // merge roughness, metallic and alpha
    data[1] = new uint8_t[extent.width * extent.height * 4];
    for(uint32_t i = 0; i < 3; i++)
    {
        for(uint32_t j = 0; j < (extent.width * extent.height); j++)
            data[1][j * 4 + i] = tmp[i][j];
        stbi_image_free(tmp[i]);
    }

    this->init_texture(tex);
    tex.set_image_format(VK_FORMAT_R8G8B8A8_UNORM);   // emission map is a HDR texture
    tex.set_image_extent(extent);
    tex.set_image_array_layers(3);
    tex.set_sampler_lod(0.0f, vka::Texture::to_mip_levels(extent));

    VkImageViewCreateInfo view;
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = nullptr;
    view.flags = 0;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = VK_FORMAT_R8G8B8A8_UNORM;
    view.components = component_mapping;
    view.subresourceRange.layerCount = 1;

    for(uint32_t i = 0; i < 3; i++)
    {
        view.subresourceRange.baseArrayLayer = i;
        tex.add_view(view);
        if(tex.load(i, data[i]) != VK_SUCCESS)
            throw std::runtime_error("[pt::PathTracer::load_emissive_texture]: Failed to load arman image into texture.");
        stbi_image_free(data[i]);
    }

    if(tex.create(true, VK_FILTER_NEAREST) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_emissive_texture]: Failed to create arman texture.");

}

void pt::PathTracer::load_environment_texture(void)
{
    VkExtent3D extent;
    int w, h;
    float* data = stbi_loadf(this->environment_path.c_str(), &w, &h, nullptr, 4);
    if(data == nullptr)
        throw std::runtime_error("[pt::PathTracer::load_environment_texture]: Failed to load environment image: " + this->environment_path);

    extent.width    = static_cast<uint32_t>(w);
    extent.height   = static_cast<uint32_t>(h);
    extent.depth    = 1;

    this->init_texture(this->environment);
    this->environment.set_image_format(VK_FORMAT_R32G32B32A32_SFLOAT);
    this->environment.set_image_extent(extent);
    this->environment.set_image_array_layers(1);
    this->environment.set_sampler_lod(0.0f, 0.0f);

    VkComponentMapping component_mapping = {};
    component_mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    component_mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    component_mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    component_mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    VkImageViewCreateInfo view;
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = nullptr;
    view.flags = 0;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    view.components = component_mapping;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = 1;

    this->environment.add_view(view);
    if(this->environment.load(0, data) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_emissive_texture]: Failed to load environment image into texture.");
    stbi_image_free(data);

    if(this->environment.create(false, VK_FILTER_NEAREST) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_emissive_texture]: Failed to create environment texture.");

    if(this->texture_load_callback != nullptr) this->texture_load_callback(this->environment_path);
}

void pt::PathTracer::init_texture(vka::Texture& tex)
{
    // initialitze texture
    tex.set_device(this->setup->get_device());
    tex.set_pyhsical_device(this->setup->get_physical_device());
    tex.set_command_pool(this->cmd_pool);
    tex.set_queue(this->setup->get_rt_queue());

    tex.set_image_flags(0);
    tex.set_image_type(VK_IMAGE_TYPE_2D);
    // format is not set here
    // extent is not set here
    // array layers are not set here
    tex.set_image_queue_families(this->setup->get_rt_queue_info().queueFamilyIndex);

    tex.set_sampler_mag_filter(VK_FILTER_LINEAR);
    tex.set_sampler_min_filter(VK_FILTER_LINEAR);
    tex.set_sampler_mipmap_mode(VK_SAMPLER_MIPMAP_MODE_LINEAR);
    tex.set_sampler_address_mode(VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    tex.set_sampler_mip_lod_bias(0.0f);
    tex.set_sampler_anisotropy_enable(false);
    tex.set_sampler_max_anisotropy(0.0f);
    tex.set_sampler_compare_enable(false);
    tex.set_sampler_compare_op(VK_COMPARE_OP_ALWAYS);
    // lod is not set here
    tex.set_sampler_border_color(VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);
    tex.set_sampler_unnormalized_coordinates(false);
}
