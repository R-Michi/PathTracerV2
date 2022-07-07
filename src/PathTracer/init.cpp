#include "../application.h"

pt::PathTracer::PathTracer(void)
{
    this->initialized = false;
    this->setup = nullptr;
    this->model_load_callback = nullptr;
    this->texture_load_callback = nullptr;
}

void pt::PathTracer::set_model_load_callback(loadmsg_callback_t callback)
{
    this->model_load_callback = callback;
}
void pt::PathTracer::set_texture_load_callback(loadmsg_callback_t callback)
{
    this->texture_load_callback = callback;
}

void pt::PathTracer::init(const Setup* setup)
{
    if(this->initialized) return;
    if(setup == nullptr)
        throw std::invalid_argument("[pt::PathTracer::init]: Setup in pt::PathTracer::init must not be a nullptr.");
    this->setup = setup;

    this->create_pools();
    this->create_render_images();
    this->create_render_models();
    this->create_render_materials();
    this->create_acceleration_structure();
    this->create_shaders();
    this->create_shader_groups();
    this->create_descriptors();
    this->create_pipeline();
    this->create_sbt();
    this->initialized = true;
}

void pt::PathTracer::destroy(void)
{
    if(!this->initialized) return;

    // wait for device to finish all operations before destroying objects
    vkDeviceWaitIdle(this->setup->get_device());

    // destroy SBT
    this->sbt.buff.clear();

    // destroy pipeline
    vkDestroyPipelineLayout(this->setup->get_device(), this->rtp.layout, nullptr);
    vkDestroyPipeline(this->setup->get_device(), this->rtp.pipeline, nullptr);

    // destroy descriptors
    this->descriptors.clear();

    // destroy shaders
    this->stages.clear();

    // destroy acceleration structures
    this->tlibo.clear();
    vkFreeMemory(this->setup->get_device(), this->blas.memory, nullptr);
    vkFreeMemory(this->setup->get_device(), this->tlas.memory, nullptr);
    detail::vkDestroyAccelerationStructureNV(this->setup->get_device(), this->blas.as, nullptr);
    detail::vkDestroyAccelerationStructureNV(this->setup->get_device(), this->tlas.as, nullptr);

    // destroy render materials
    this->mtl_buffer.clear();
    for(RenderMaterial& mtl : this->materials)
    {
        mtl.texture.albedo.clear();
        mtl.texture.emission.clear();
        mtl.texture.rman.clear();
    }
    this->environment.clear();

    // destroy render models
    for(auto& model : this->models)
    {
        for(RenderMesh& mesh : model)
        {
            mesh.vectices.clear();
            mesh.attributes.clear();
            mesh.indices.clear();
        }
    }

    // destroy render images
    vkDestroyImageView(this->setup->get_device(), this->render_target.view, nullptr);
    vkFreeMemory(this->setup->get_device(), this->render_target.mem, nullptr);
    vkFreeMemory(this->setup->get_device(), this->output_image.mem, nullptr);
    vkDestroyImage(this->setup->get_device(), this->render_target.image, nullptr);
    vkDestroyImage(this->setup->get_device(), this->output_image.image, nullptr);

    // destroy pool(s)
    vkDestroyQueryPool(this->setup->get_device(), this->timer_query_pool, nullptr);
    vkDestroyCommandPool(this->setup->get_device(), this->cmd_pool, nullptr);
    this->initialized = false;
}

uint64_t pt::PathTracer::run(void)
{
    VkCommandBuffer cbo = this->record();
    this->trace(cbo);
    
    // time_stamps[0]: start time
    // time_stamps[1]: end time
    uint64_t time_stamps[2];
    if(vkGetQueryPoolResults(
        this->setup->get_device(), 
        this->timer_query_pool, 
        0, 
        2, 
        sizeof(time_stamps), 
        time_stamps, 
        sizeof(uint64_t),
        VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT
    ) != VK_SUCCESS)
    { throw std::runtime_error("[pt::PathTracer::run]: Failed to query tracing time."); }

    // get pyhsical device limits
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(this->setup->get_physical_device(), &properties);

    uint64_t time_diff = time_stamps[1] - time_stamps[0];   // difference of start and end time-stamp (t_end - t_start)

    // To get the time difference in nanoseconds we have to multiply the time-stamp difference by the time resolution
    // specified in the VkPhysicalDeviceLimits.
    return time_diff * static_cast<uint64_t>(properties.limits.timestampPeriod);
}

const uint8_t* pt::PathTracer::map_image(size_t& row_stride, uint32_t& component_count) const
{
    void* map;
    if(vkMapMemory(this->setup->get_device(), this->output_image.mem, 0, VK_WHOLE_SIZE, 0, &map) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::map_image]: Failed to map output image.");
    row_stride = this->setup->get_settings()->rt_width * vka::utility::format_sizeof(VK_FORMAT_R8G8B8A8_UNORM);
    component_count = 4;
    return reinterpret_cast<uint8_t*>(map);
}

void pt::PathTracer::unmap_image(void) const noexcept
{
    vkUnmapMemory(this->setup->get_device(), this->output_image.mem);
}