#include "../application.h"

void pt::PathTracer::create_pipeline(void)
{
    VkPipelineLayoutCreateInfo layout_ci = {};
    layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.pNext = nullptr;
    layout_ci.flags = 0;
    layout_ci.setLayoutCount = this->descriptors.layouts().size();
    layout_ci.pSetLayouts = this->descriptors.layouts().data();
    layout_ci.pushConstantRangeCount = 0;
    layout_ci.pPushConstantRanges = nullptr;

    if(vkCreatePipelineLayout(this->setup->get_device(), &layout_ci, nullptr, &rtp.layout) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_pipeline]: Failed to create ray tracing pipeline layout.");

    VkRayTracingPipelineCreateInfoNV pipeline_ci = {};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
    pipeline_ci.pNext = nullptr;
    pipeline_ci.flags = 0;
    pipeline_ci.stageCount = this->stages.count();
    pipeline_ci.pStages = this->stages.get_stages();
    pipeline_ci.groupCount = this->shader_groups.size();
    pipeline_ci.pGroups = this->shader_groups.data();
    pipeline_ci.maxRecursionDepth = this->setup->get_settings()->iterations;
    pipeline_ci.layout = rtp.layout;
    pipeline_ci.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_ci.basePipelineIndex = 0;

    if(detail::vkCreateRayTracingPipelinesNV(this->setup->get_device(), VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &rtp.pipeline) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_pipeline]: Failed to create ray tracing pipeline.");
}
