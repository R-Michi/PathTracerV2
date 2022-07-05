#include "../application.h"

void pt::PathTracer::create_shaders(void)
{
    vka::Shader rgen(this->setup->get_device()),
                rmiss(this->setup->get_device()),
                rchit(this->setup->get_device());

    rgen.load("./assets/shaders/out/main.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_NV);
    rmiss.load("./assets/shaders/out/main.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_NV);
    rchit.load("./assets/shaders/out/main.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);

    this->stages.attach(rgen);  // shader has index 0
    this->stages.attach(rmiss); // shader has index 1
    this->stages.attach(rchit); // shader has index 2
}

void pt::PathTracer::create_shader_groups(void)
{
    VkRayTracingShaderGroupCreateInfoNV group;
    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
    group.pNext = nullptr;

    // ray generation hit group (consisting of the ray generation shader)
    // this shader along with the miss shader are called general shader
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    group.generalShader = 0;    // index of the shader stages of the ray generation shader 
    group.closestHitShader = VK_SHADER_UNUSED_NV;
    group.anyHitShader = VK_SHADER_UNUSED_NV;
    group.intersectionShader = VK_SHADER_UNUSED_NV;
    this->shader_groups.push_back(group);

    // miss hit group (consisting of the miss shader)
    // this shader is also called a general shader
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    group.generalShader = 1;    // index of the shader stages of the miss shader
    group.closestHitShader = VK_SHADER_UNUSED_NV;
    group.anyHitShader = VK_SHADER_UNUSED_NV;
    group.intersectionShader = VK_SHADER_UNUSED_NV;
    this->shader_groups.push_back(group);

    // hit groups (consisting of the closest hit shader, as we don't use a any hit or intersection shader)
    // As every geometry invokes a different hit group record in the SBT, at least one shader group 
    // for every geometry is requiered. There may also be more than one, if we decide to have multiple
    // ray types. For example one hit group for a normal scene ray and one for a shadow ray, both
    // ray types could then invoke different shaders, if they hit any geometry.
    for(uint32_t i = 0; i < this->models.size(); i++)
    {
        for(uint32_t j = 0; j < this->models[i].size(); j++)
        {
            // this shaders are used to trace triangles
            group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
            group.generalShader = VK_SHADER_UNUSED_NV;    
            group.closestHitShader = 2; // index of the shader stages of the closest hit shader
            group.anyHitShader = VK_SHADER_UNUSED_NV;
            group.intersectionShader = VK_SHADER_UNUSED_NV;
            this->shader_groups.push_back(group);
        }
    }
}

void pt::PathTracer::create_sbt(void)
{
    uint32_t mesh_count = 0;
    for(size_t i = 0; i < this->models.size(); i++) mesh_count += this->models[i].size();

    const uint32_t base_alignment   = this->setup->get_rt_properties().shaderGroupBaseAlignment;
    const uint32_t max_stride       = this->setup->get_rt_properties().maxShaderGroupStride;
    const uint32_t handle_size      = this->setup->get_rt_properties().shaderGroupHandleSize;
    const uint32_t parameter_size   = static_cast<uint32_t>(sizeof(RecordParameter));           // additinal parameter to a sbt record
    const uint32_t record_size      = handle_size + parameter_size;                             // the size (stride) of a sbt record is the hadle size + the parameter size
    const uint32_t ray_type_count   = 1;                                                        // number of ray types, number of hit groups per geometry, also refered to as the sbt record stride
    const uint32_t rgen_count       = 1;                                                        // number of ray generation shaders, there is always one
    const uint32_t hg_count         = mesh_count * ray_type_count;                              // number of hit groups: (number of geometries) * (number of hit groups per geometry)
    const uint32_t miss_count       = this->shader_groups.size() - hg_count - rgen_count;       // everything thats left from the shader groups must be the number of miss shaders

    // combute sbt buffer layout
    // the size (stride) of a sbt record must be a multiple of 'shaderGroupHandleSize'
    this->sbt.record_stride = (record_size - 1) / handle_size * handle_size + handle_size;

    // the ray generation record is at the very begin of the buffer
    this->sbt.rgen_offset = 0;

    // the offset for the miss records must be a multiple of 'shaderGroupBaseAlignment'
    this->sbt.miss_offset = ((this->sbt.rgen_offset + rgen_count * this->sbt.record_stride) - 1) / base_alignment * base_alignment + base_alignment;

    // the offset for the hit group records must also be a multiple of 'shaderGroupBaseAlignment'
    this->sbt.hg_offset = ((this->sbt.miss_offset + miss_count * this->sbt.record_stride) - 1) / base_alignment * base_alignment + base_alignment;

    // sbt record stride must not exeed 'maxShaderGroupStride'
    if(this->sbt.record_stride > max_stride)
        throw std::runtime_error("[pt::PathTracer::create_sbt]: SBT-record stride exeeds the maximum size allowed for shader group records.");

    // combute size for sbt buffer
    const size_t sbt_buff_size = this->sbt.hg_offset + hg_count * this->sbt.record_stride;

    // init sbt buffer
    this->sbt.buff.set_create_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV);
    this->sbt.buff.set_memory_properties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // create staging buffer for sbt
    vka::Buffer staging(this->setup->get_physical_device(), this->setup->get_device());
    staging.set_create_flags(0);
    staging.set_create_usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    staging.set_create_size(sbt_buff_size);
    staging.set_create_sharing_mode(VK_SHARING_MODE_EXCLUSIVE);
    staging.set_create_queue_families(&this->setup->get_rt_queue_info().queueFamilyIndex, 1);
    staging.set_memory_properties(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(staging.create() != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_sbt]: Failed to create staging buffer for SBT-buffer.");

    // query shader group handles
    uint8_t sgh[handle_size * this->shader_groups.size()];
    detail::vkGetRayTracingShaderGroupHandlesNV(this->setup->get_device(), this->rtp.pipeline, 0, this->shader_groups.size(), sizeof(sgh), sgh);

    const size_t sgh_rgen   = 0;                                    // offset to the first ray generation shader group handle
    const size_t sgh_miss   = sgh_rgen + rgen_count * handle_size;  // offset to the first miss shader group handle
    const size_t sgh_hg     = sgh_miss + miss_count * handle_size;  // offset to the first hit-group shader group handle

    // copy ray generation record into SBT
    uint8_t* map_rgen = (uint8_t*)staging.map((rgen_count * this->sbt.record_stride), this->sbt.rgen_offset);
    RecordParameter dummy = {0, 0};
    #pragma unroll_completely
    for(uint32_t i = 0; i < rgen_count; i++)
    {   
        uint8_t* const cur_handle = sgh + (sgh_rgen + i * handle_size);
        uint8_t* const cur_record = map_rgen + i * this->sbt.record_stride;
        memcpy(cur_record + 0, cur_handle, handle_size);            // copy shader group handle at the very begin of very SBT record
        memcpy(cur_record + handle_size, &dummy, parameter_size);   // copy in addition to the handle, the SBT record parameter
    }
    staging.unmap();

    // copy miss records into SBT
    uint8_t* map_miss = (uint8_t*)staging.map((miss_count * this->sbt.record_stride), this->sbt.miss_offset);
    for(uint32_t i = 0; i < miss_count; i++)
    {   
        uint8_t* const cur_handle = sgh + (sgh_miss + i * handle_size);
        uint8_t* const cur_record = map_miss + i * this->sbt.record_stride;
        memcpy(cur_record + 0, cur_handle, handle_size);            // copy shader group handle at the very begin of very SBT record
        memcpy(cur_record + handle_size, &dummy, parameter_size);   // copy in addition to the handle, the SBT record parameter
    }
    staging.unmap();

    // copy hit-group records into SBT
    uint8_t* map_hg = (uint8_t*)staging.map((hg_count * this->sbt.record_stride), this->sbt.hg_offset);
    uint32_t idx = 0;
    for(const auto& model : this->models)
    {
        for(const RenderMesh& mesh : model)
        {
            for(uint32_t i = 0; i < ray_type_count; i++, idx++)
            {
                uint8_t* const cur_handle = sgh + (sgh_hg + idx * handle_size);
                uint8_t* const cur_record = map_hg + idx * this->sbt.record_stride;
                memcpy(cur_record + 0, cur_handle, handle_size);                            // copy shader group handle at the very begin of very SBT record
                memcpy(cur_record + handle_size, &mesh.properties.record, parameter_size);  // copy in addition to the handle, the SBT record parameter
            }
        }
    }
    staging.unmap();

    // copy staging buffer to SBT buffer
    VkCommandBuffer cbo = vka::Buffer::enqueue_copy(this->setup->get_device(), this->cmd_pool, 1, &staging, &this->sbt.buff);
    if(vka::utility::execute_scb(this->setup->get_device(), this->cmd_pool, this->setup->get_rt_queue(), 1, &cbo) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_sbt]: Failed to copy staging buffer to SBT-buffer.");
    vkFreeCommandBuffers(this->setup->get_device(), this->cmd_pool, 1, &cbo);
}
