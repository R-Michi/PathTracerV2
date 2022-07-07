#include "../application.h"

void pt::PathTracer::create_acceleration_structure(void)
{
    std::vector<VkGeometryNV> geometry;
    this->load_geometry(geometry);
    this->load_blas(geometry, this->blas);

    this->load_instances(this->blas);
    this->load_tlas(this->tlas);
}

void pt::PathTracer::load_geometry(std::vector<VkGeometryNV>& geometry)
{
    // create one geometry for every mesh
    for(const auto& model : this->models)
    {
        for(const RenderMesh& mesh : model)
        {
            VkGeometryTrianglesNV triangles = {};
            triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
            triangles.pNext = nullptr;
            triangles.vertexData = mesh.vectices.handle();
            triangles.vertexOffset = 0;
            triangles.vertexCount = mesh.properties.vertex_count;
            triangles.vertexStride = mesh.properties.vertex_stride;
            triangles.vertexFormat = mesh.properties.vertex_format;
            triangles.indexData = mesh.indices.handle();
            triangles.indexOffset = 0;
            triangles.indexCount = mesh.properties.index_count;
            triangles.indexType = VK_INDEX_TYPE_UINT32;
            triangles.transformData = VK_NULL_HANDLE;
            triangles.transformOffset = 0;

            VkGeometryDataNV data = {};
            data.triangles = triangles;
            data.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
            data.aabbs.pNext = nullptr;

            VkGeometryNV geom = {};
            geom.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
            geom.pNext = nullptr;
            geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
            geom.geometry = data;
            geom.flags = VK_GEOMETRY_OPAQUE_BIT_NV;

            geometry.push_back(geom);
        }
    }
}

void pt::PathTracer::load_instances(const AccelerationStructure& blas)
{
    // there is only one instance of the blas
    VkAccelerationStructureInstanceNV instance;
    instance.transform = glm2transformNV(identity_transform());
    instance.flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_NV;
    instance.mask = 0xFF;
    instance.instanceCustomIndex = 0;   // instanceID of this geometry is 0
    instance.instanceShaderBindingTableRecordOffset = 0; // the first hit group of this instance is the first in the SBT
    instance.accelerationStructureReference = blas.handle;

    // init instance buffer
    this->tlibo.set_create_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV);
    this->tlibo.set_memory_properties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // statging buffer for instance buffer
    vka::Buffer tmp(this->setup->get_physical_device(), this->setup->get_device());
    tmp.set_create_flags(0);
    tmp.set_create_size(sizeof(VkAccelerationStructureInstanceNV));
    tmp.set_create_usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    tmp.set_create_sharing_mode(VK_SHARING_MODE_EXCLUSIVE);
    tmp.set_create_queue_families(&this->setup->get_rt_queue_info().queueFamilyIndex, 1);
    tmp.set_memory_properties(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(tmp.create() != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_instances]: Failed to create staging buffer for instance buffer.");

    // copy instance data indo staging buffer
    void* map = tmp.map(tmp.size(), 0);
    memcpy(map, &instance, sizeof(VkAccelerationStructureInstanceNV));
    tmp.unmap();
    
    // copy staging buffer to instance buffer
    VkCommandBuffer cbo = vka::Buffer::enqueue_copy(this->setup->get_device(), this->cmd_pool, 1, &tmp, &this->tlibo);
    if(vka::utility::execute_scb(this->setup->get_device(), this->cmd_pool, this->setup->get_rt_queue(), 1, &cbo) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_instances]: Failed to create copy staging buffer to instance buffer.");
}

void pt::PathTracer::load_blas(const std::vector<VkGeometryNV>& geometry, AccelerationStructure& blas)
{
    // create acceleration structure
    VkAccelerationStructureInfoNV blas_info = {};
    blas_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
    blas_info.pNext = nullptr;
    blas_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
    blas_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV;
    blas_info.instanceCount = 0;
    blas_info.geometryCount = geometry.size();
    blas_info.pGeometries = geometry.data();

    VkAccelerationStructureCreateInfoNV blas_ci = {};
    blas_ci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
    blas_ci.pNext = nullptr;
    blas_ci.compactedSize = 0;
    blas_ci.info = blas_info;

    if(detail::vkCreateAccelerationStructureNV(this->setup->get_device(), &blas_ci, nullptr, &blas.as) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_blas]: Failed to create blas.");

    // query memory requierements
    VkAccelerationStructureMemoryRequirementsInfoNV blas_memreq = {};
    blas_memreq.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    blas_memreq.pNext = nullptr;
    blas_memreq.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;
    blas_memreq.accelerationStructure = blas.as;

    // scratch memory requierements
    VkMemoryRequirements2 scratch_req;
    scratch_req.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    scratch_req.pNext = nullptr;
    detail::vkGetAccelerationStructureMemoryRequirementsNV(this->setup->get_device(), &blas_memreq, &scratch_req);

    // blas memory requierements
    blas_memreq.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

    VkMemoryRequirements2 object_req = {};
    object_req.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    object_req.pNext = nullptr;
    detail::vkGetAccelerationStructureMemoryRequirementsNV(this->setup->get_device(), &blas_memreq, &object_req);

    // create memory for blas and bind memory to blas
    VkMemoryAllocateInfo mem_ai = {};
    mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_ai.pNext = nullptr;
    mem_ai.allocationSize = object_req.memoryRequirements.size;
    mem_ai.memoryTypeIndex = vka::utility::find_memory_type_index(this->setup->get_physical_device(), object_req.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(vkAllocateMemory(this->setup->get_device(), &mem_ai, nullptr, &blas.memory) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_blas]: Failed to allocate memory for blas.");
    
    VkBindAccelerationStructureMemoryInfoNV bind_info = {};
    bind_info.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
    bind_info.pNext = nullptr;
    bind_info.accelerationStructure = blas.as;
    bind_info.memory = blas.memory;
    bind_info.memoryOffset = 0;
    bind_info.deviceIndexCount = 0;
    bind_info.pDeviceIndices = nullptr;
    if(detail::vkBindAccelerationStructureMemoryNV(this->setup->get_device(), 1, &bind_info) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_blas]: Failed to bind blas to memory.");

    // create scratch buffer
    vka::Buffer scratch_buff(this->setup->get_physical_device(), this->setup->get_device());
    scratch_buff.set_create_flags(0);
    scratch_buff.set_create_usage(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV);
    scratch_buff.set_create_size(scratch_req.memoryRequirements.size);
    scratch_buff.set_create_sharing_mode(VK_SHARING_MODE_EXCLUSIVE);
    scratch_buff.set_create_queue_families(&this->setup->get_rt_queue_info().queueFamilyIndex, 1);
    scratch_buff.set_memory_properties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(scratch_buff.create() != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_blas]: Failed to create scratch memory for blas.");

    // allocate command buffer for blas build
    VkCommandBuffer cbo;
    VkCommandBufferAllocateInfo cbo_ai = {};
    cbo_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbo_ai.pNext = nullptr;
    cbo_ai.commandPool = this->cmd_pool;
    cbo_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbo_ai.commandBufferCount = 1;
    if(vkAllocateCommandBuffers(this->setup->get_device(), &cbo_ai, &cbo) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_blas]: Failed to allocate command buffer for blas.");

    VkCommandBufferBeginInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.pNext = nullptr;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    bi.pInheritanceInfo = nullptr;

    if(vkBeginCommandBuffer(cbo, &bi) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_blas]: Failed to begin command buffer for blas build.");

    detail::vkCmdBuildAccelerationStructureNV(cbo, &blas_info, VK_NULL_HANDLE, 0, VK_FALSE, blas.as, VK_NULL_HANDLE, scratch_buff.handle(), 0);

    if(vkEndCommandBuffer(cbo) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_blas]: Failed to end command buffer for blas build.");

    // execute build command
    VkSubmitInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.pNext = nullptr;
    si.waitSemaphoreCount = 0;
    si.pWaitSemaphores = nullptr;
    si.pWaitDstStageMask = nullptr;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cbo;
    si.signalSemaphoreCount = 0;
    si.pSignalSemaphores = nullptr;

    if(vkQueueSubmit(this->setup->get_rt_queue(), 1, &si, VK_NULL_HANDLE) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_blas]: Failed to execute blas build.");
    if(vkQueueWaitIdle(this->setup->get_rt_queue()) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_blas]: Failed to wait for blas build.");

    // get device-side handle of blas
    detail::vkGetAccelerationStructureHandleNV(this->setup->get_device(), blas.as, sizeof(uint64_t), &blas.handle);
    vkFreeCommandBuffers(this->setup->get_device(), this->cmd_pool, 1, &cbo);
}

void pt::PathTracer::load_tlas(AccelerationStructure& tlas)
{
    // create acceleration structure
    VkAccelerationStructureInfoNV tlas_info = {};
    tlas_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
    tlas_info.pNext = nullptr;
    tlas_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
    tlas_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV;
    tlas_info.instanceCount = 1;
    tlas_info.geometryCount = 0;
    tlas_info.pGeometries = VK_NULL_HANDLE;

    VkAccelerationStructureCreateInfoNV tlas_ci = {};
    tlas_ci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
    tlas_ci.pNext = nullptr;
    tlas_ci.compactedSize = 0;
    tlas_ci.info = tlas_info;

    if(detail::vkCreateAccelerationStructureNV(this->setup->get_device(), &tlas_ci, nullptr, &tlas.as) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_tlas]: Failed to create tlas.");

    // query memory requierements
    VkAccelerationStructureMemoryRequirementsInfoNV tlas_memreq = {};
    tlas_memreq.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
    tlas_memreq.pNext = nullptr;
    tlas_memreq.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;
    tlas_memreq.accelerationStructure = tlas.as;

    // scratch memory requierements
    VkMemoryRequirements2 scratch_req;
    scratch_req.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    scratch_req.pNext = nullptr;
    detail::vkGetAccelerationStructureMemoryRequirementsNV(this->setup->get_device(), &tlas_memreq, &scratch_req);

    // tlas memory requierements
    tlas_memreq.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

    VkMemoryRequirements2 object_req = {};
    object_req.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    object_req.pNext = nullptr;
    detail::vkGetAccelerationStructureMemoryRequirementsNV(this->setup->get_device(), &tlas_memreq, &object_req);

    // create memory for tlas and bind memory to tlas
    VkMemoryAllocateInfo mem_ai = {};
    mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_ai.pNext = nullptr;
    mem_ai.allocationSize = object_req.memoryRequirements.size;
    mem_ai.memoryTypeIndex = vka::utility::find_memory_type_index(this->setup->get_physical_device(), object_req.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(vkAllocateMemory(this->setup->get_device(), &mem_ai, nullptr, &tlas.memory) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_tlas]: Failed to allocate memory for tlas.");
    
    VkBindAccelerationStructureMemoryInfoNV bind_info = {};
    bind_info.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
    bind_info.pNext = nullptr;
    bind_info.accelerationStructure = tlas.as;
    bind_info.memory = tlas.memory;
    bind_info.memoryOffset = 0;
    bind_info.deviceIndexCount = 0;
    bind_info.pDeviceIndices = nullptr;
    if(detail::vkBindAccelerationStructureMemoryNV(this->setup->get_device(), 1, &bind_info) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_tlas]: Failed to bind tlas to memory.");

    // create scratch buffer
    vka::Buffer scratch_buff(this->setup->get_physical_device(), this->setup->get_device());
    scratch_buff.set_create_flags(0);
    scratch_buff.set_create_usage(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV);
    scratch_buff.set_create_size(scratch_req.memoryRequirements.size);
    scratch_buff.set_create_sharing_mode(VK_SHARING_MODE_EXCLUSIVE);
    scratch_buff.set_create_queue_families(&this->setup->get_rt_queue_info().queueFamilyIndex, 1);
    scratch_buff.set_memory_properties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(scratch_buff.create() != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_tlas]: Failed to create scratch memory for tlas.");

    // allocate command buffer for tlas build
    VkCommandBuffer cbo;
    VkCommandBufferAllocateInfo cbo_ai = {};
    cbo_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbo_ai.pNext = nullptr;
    cbo_ai.commandPool = this->cmd_pool;
    cbo_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbo_ai.commandBufferCount = 1;
    if(vkAllocateCommandBuffers(this->setup->get_device(), &cbo_ai, &cbo) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_tlas]: Failed to allocate command buffer for tlas.");

    VkCommandBufferBeginInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.pNext = nullptr;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    bi.pInheritanceInfo = nullptr;

    if(vkBeginCommandBuffer(cbo, &bi) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_tlas]: Failed to begin command buffer for tlas build.");

    detail::vkCmdBuildAccelerationStructureNV(cbo, &tlas_info, this->tlibo.handle(), 0, VK_FALSE, tlas.as, VK_NULL_HANDLE, scratch_buff.handle(), 0);

    if(vkEndCommandBuffer(cbo) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_tlas]: Failed to end command buffer for tlas build.");

    // execute build command
    VkSubmitInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.pNext = nullptr;
    si.waitSemaphoreCount = 0;
    si.pWaitSemaphores = nullptr;
    si.pWaitDstStageMask = nullptr;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cbo;
    si.signalSemaphoreCount = 0;
    si.pSignalSemaphores = nullptr;

    if(vkQueueSubmit(this->setup->get_rt_queue(), 1, &si, VK_NULL_HANDLE) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_tlas]: Failed to execute tlas build.");
    if(vkQueueWaitIdle(this->setup->get_rt_queue()) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::load_tlas]: Failed to wait for tlas build.");

    // get device-side handle of tlas
    detail::vkGetAccelerationStructureHandleNV(this->setup->get_device(), tlas.as, sizeof(uint64_t), &tlas.handle);
    vkFreeCommandBuffers(this->setup->get_device(), this->cmd_pool, 1, &cbo);
}
