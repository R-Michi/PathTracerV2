#include "../application.h"

void pt::PathTracer::create_render_images(void)
{
    constexpr VkFormat ROP_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
    constexpr VkFormat OUT_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

    VkExtent3D rop_extent = {
        this->setup->get_settings()->rt_width,
        this->setup->get_settings()->rt_height,
        1
    };

    /* CREATE ROP IMAGE */
    VkImageCreateInfo image_ci = {};
    image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_ci.pNext = nullptr;
    image_ci.flags = 0;
    image_ci.imageType = VK_IMAGE_TYPE_2D;
    image_ci.format = ROP_FORMAT;
    image_ci.extent = rop_extent;
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 1;
    image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_ci.queueFamilyIndexCount = 1;
    image_ci.pQueueFamilyIndices = &this->setup->get_rt_queue_info().queueFamilyIndex;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

    if(vkCreateImage(this->setup->get_device(), &image_ci, nullptr, &this->render_target.image) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_ropi]: Failed to create rop image.");

    // get memory requierements for the image
    VkMemoryRequirements rop_req;
    vkGetImageMemoryRequirements(this->setup->get_device(), this->render_target.image, &rop_req);

    // allocate acutal memory for rops
    VkMemoryAllocateInfo mem_ai = {};
    mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_ai.pNext = nullptr;
    mem_ai.allocationSize = rop_req.size;
    mem_ai.memoryTypeIndex = vka::utility::find_memory_type_index(this->setup->get_physical_device(), rop_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if(vkAllocateMemory(this->setup->get_device(), &mem_ai, nullptr, &this->render_target.mem) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_ropi]: Failed to allocate memory for rop image.");

    // associate image with its memory
    if(vkBindImageMemory(this->setup->get_device(), this->render_target.image, this->render_target.mem, 0) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_ropi]: Failed to bind memory to rop image.");

    // create image view
    VkImageViewCreateInfo ropv_ci = {};
    ropv_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ropv_ci.pNext = nullptr;
    ropv_ci.flags = 0;
    ropv_ci.image = this->render_target.image;
    ropv_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ropv_ci.format = ROP_FORMAT;
    ropv_ci.components = {};
    ropv_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ropv_ci.subresourceRange.baseMipLevel = 0;
    ropv_ci.subresourceRange.levelCount = 1;
    ropv_ci.subresourceRange.baseArrayLayer = 0;
    ropv_ci.subresourceRange.layerCount = 1;

    if(vkCreateImageView(this->setup->get_device(), &ropv_ci, nullptr, &this->render_target.view) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_ropi]: Failed to create rop image view.");

    /* CREATE OUTPUT IMAGE */
    image_ci.format = OUT_FORMAT;
    image_ci.tiling = VK_IMAGE_TILING_LINEAR;
    image_ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if(vkCreateImage(this->setup->get_device(), &image_ci, nullptr, &this->output_image.image) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_ropi]: Failed to create output image.");

    // get memory requierements for the image
    VkMemoryRequirements out_req;
    vkGetImageMemoryRequirements(this->setup->get_device(), this->output_image.image, &out_req);

    mem_ai.allocationSize = out_req.size;
    mem_ai.memoryTypeIndex = vka::utility::find_memory_type_index(this->setup->get_physical_device(), out_req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if(vkAllocateMemory(this->setup->get_device(), &mem_ai, nullptr, &this->output_image.mem) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_ropi]: Failed to allocate memory for output image.");

    // associate image with its memory
    if(vkBindImageMemory(this->setup->get_device(), this->output_image.image, this->output_image.mem, 0) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_ropi]: Failed to bind memory to output image.");

    // make a layout transicon for both images
    // both images need a VK_IMAGE_LAYOUT_GENERAL
    VkCommandBufferAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.pNext = nullptr;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandPool = this->cmd_pool;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmdbuff;
    if(vkAllocateCommandBuffers(this->setup->get_device(), &ai, &cmdbuff) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_ropi]: Failes ro create command buffer for ROP.");

    VkCommandBufferBeginInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.pNext = nullptr;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    bi.pInheritanceInfo = nullptr;

    if(vkBeginCommandBuffer(cmdbuff, &bi) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_ropi]: Failed to record command buffer for ROP's image layout transicon.");

    VkImageMemoryBarrier barriers[2];
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].pNext = nullptr;
    barriers[0].srcAccessMask = VK_ACCESS_NONE_KHR;         // there was no access before
    barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // we want to write to this image in the ray generation shader
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;        // the target of a ray tracing render must have the image layout general
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = this->render_target.image;
    barriers[0].subresourceRange = ropv_ci.subresourceRange;

    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].pNext = nullptr;
    barriers[1].srcAccessMask = VK_ACCESS_NONE_KHR;             // there was no access before
    barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;   // we want to write to the image in the curse of the copy operation
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = this->output_image.image;
    barriers[1].subresourceRange = ropv_ci.subresourceRange;

    vkCmdPipelineBarrier(
        cmdbuff, 
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,             // All previous commands on that image prior in execution must have been finished...
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV,    // ...before the ray tracing stage can continue writing colors to the image.
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        barriers + 0
    );

    vkCmdPipelineBarrier(
        cmdbuff,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // All previous commands on that image prior in execution must have been finished...
        VK_PIPELINE_STAGE_TRANSFER_BIT,     // ... before any transfer operations can start.
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        barriers + 1
    );

    if(vkEndCommandBuffer(cmdbuff) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_ropi]: Failed to end command buffer of ROP's image layout transicon.");

    // execute commands
    VkSubmitInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.pNext = nullptr;
    si.waitSemaphoreCount = 0;
    si.pWaitSemaphores = nullptr;
    si.pWaitDstStageMask = nullptr;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmdbuff;
    si.signalSemaphoreCount = 0;
    si.pSignalSemaphores = nullptr;

    if(vkQueueSubmit(this->setup->get_rt_queue(), 1, &si, VK_NULL_HANDLE) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_ropi]: Failed to execute commands for ROP's image layout transicon.");
        
    if(vkQueueWaitIdle(this->setup->get_rt_queue()) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_ropi]: Failed to wait for queue.");
    vkFreeCommandBuffers(this->setup->get_device(), this->cmd_pool, 1, &cmdbuff);
}
