#include "../application.h"

void pt::PathTracer::create_pools(void)
{
    VkCommandPoolCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.queueFamilyIndex = this->setup->get_rt_queue_info().queueFamilyIndex;

    if(vkCreateCommandPool(this->setup->get_device(), &ci, nullptr, &this->cmd_pool) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_pools]: Failed to create command pool.");

    VkQueryPoolCreateInfo timer_query_pool_ci = {};
    timer_query_pool_ci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    timer_query_pool_ci.pNext = nullptr;
    timer_query_pool_ci.flags = 0;
    timer_query_pool_ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
    timer_query_pool_ci.queryCount = 2;
    timer_query_pool_ci.pipelineStatistics = 0;

    if(vkCreateQueryPool(this->setup->get_device(), &timer_query_pool_ci, nullptr, &this->timer_query_pool) != VK_SUCCESS)
        throw std::runtime_error("[pt::PathTracer::create_pools]: Failed to create timer query pool.");
}