#include "../application.h"

void pt::Setup::create_queues(void)
{
    std::vector<VkQueueFamilyProperties> properties;
    vka::queue::properties(this->physical_device, properties);

    vka::QueueFamilyFilter rt_filter = {};
    rt_filter.queueFlags = RAY_TRACING_QUEUE_FLAGS;
    rt_filter.queueCount = 1;

    size_t rt_index = vka::queue::find(properties, 0, rt_filter, vka::VKA_QUEUE_FAMILY_PRIORITY_OPTIMAL);
    if(rt_index == VKA_NPOS)
        throw std::runtime_error("[PathTracer | Init]: Unable to find matching queue family for ray tracing.");
    
    this->rtq_info.queueFamilyIndex = rt_index;
    this->rtq_info.usedQueueCount = rt_filter.queueCount;
    this->rtq_info.queueBaseIndex = 0;

    // validate queues
    if(!vka::queue::validate(properties, { this->rtq_info }))
        throw std::runtime_error("[PathTracer | Init]: Too many queues used from a single queue family.");
}
