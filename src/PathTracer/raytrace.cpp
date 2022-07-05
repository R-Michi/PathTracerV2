#include "../application.h"

VkCommandBuffer pt::PathTracer::record(void)
{   
    // Memory barrier to ensure synchronization between the image rendering and
    // the image copy after the render.
    VkImageMemoryBarrier render2copy_barrier = {};
    render2copy_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    render2copy_barrier.pNext = nullptr;
    render2copy_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    render2copy_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    render2copy_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    render2copy_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    render2copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    render2copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    render2copy_barrier.image = this->render_target.image;
    render2copy_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    render2copy_barrier.subresourceRange.baseMipLevel = 0;
    render2copy_barrier.subresourceRange.levelCount = 1;
    render2copy_barrier.subresourceRange.baseArrayLayer = 0;
    render2copy_barrier.subresourceRange.layerCount = 1;


    // extent of the rendered image
    VkExtent3D rop_extent = {
        this->setup->get_settings()->rt_width,
        this->setup->get_settings()->rt_height,
        1
    };

    // info for the copy operation of the render target image to the output image
    VkImageCopy copy = {};
    copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.srcSubresource.mipLevel = 0;
    copy.srcSubresource.baseArrayLayer = 0;
    copy.srcSubresource.layerCount = 1;
    copy.srcOffset = { 0, 0, 0 };
    copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.dstSubresource.mipLevel = 0;
    copy.dstSubresource.baseArrayLayer = 0;
    copy.dstSubresource.layerCount = 1;
    copy.dstOffset = { 0, 0, 0 };
    copy.extent = rop_extent;

    // record ray tracing commands
    VkCommandBuffer cbo;
    VkCommandBufferAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.pNext = nullptr;
    ai.commandPool = this->cmd_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    if(vkAllocateCommandBuffers(this->setup->get_device(), &ai, &cbo) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::record]: Failed to allocate command buffer.");
    
    VkCommandBufferBeginInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.pNext = nullptr;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    bi.pInheritanceInfo = nullptr;

    if(vkBeginCommandBuffer(cbo, &bi) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::record]: Failed to start recording command buffer.");

    vkCmdResetQueryPool(cbo, this->timer_query_pool, 0, 2);
    vkCmdBindPipeline(cbo, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, this->rtp.pipeline);
    vkCmdBindDescriptorSets(cbo, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, this->rtp.layout, 0, this->descriptors.descriptor_set_count(), this->descriptors.descriptor_sets().data(), 0, nullptr);
    vkCmdWriteTimestamp(cbo, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, this->timer_query_pool, 0);   // query ID 0 = start time
    detail::vkCmdTraceRaysNV(
        cbo,
        this->sbt.buff.handle(),
        this->sbt.rgen_offset,
        this->sbt.buff.handle(),
        this->sbt.miss_offset,
        this->sbt.record_stride,
        this->sbt.buff.handle(),
        this->sbt.hg_offset,
        this->sbt.record_stride,
        VK_NULL_HANDLE,
        0,
        0,
        rop_extent.width,
        rop_extent.height,
        rop_extent.depth
    );
    vkCmdWriteTimestamp(cbo, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, this->timer_query_pool, 1); // query ID 1 = end time
    vkCmdPipelineBarrier(cbo, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &render2copy_barrier);
    vkCmdCopyImage(cbo, this->render_target.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, this->output_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    if(vkEndCommandBuffer(cbo) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::record]: Failed to stop recording command buffer.");
    return cbo;
}

void pt::PathTracer::trace(VkCommandBuffer cbo)
{
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
        throw std::runtime_error("[pt::PathTracer::trace]: Failed to execute ray tracetrace commands.");
    if(vkQueueWaitIdle(this->setup->get_rt_queue()) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::trace]: Failed to wait ray tracing process to finish.");
    vkFreeCommandBuffers(this->setup->get_device(), this->cmd_pool, 1, &cbo);
}
