#include "../application.h"

pt::Setup::Setup(void)
{
    this->initialized = false;
    this->info = {};
}

void pt::Setup::init(const SetupCreateInfo& info)
{
    if(this->initialized) return;
    this->info = info;

    this->create_instance();
    detail::init_VK_NV_ray_tracing(this->instance);
    this->create_physical_device();
    this->create_queues();
    this->create_device();
    this->initialized = true;
}

void pt::Setup::destroy(void)
{
    if(!this->initialized) return;
    vkDestroyDevice(this->device, nullptr);
    vkDestroyInstance(this->instance, nullptr);
    this->initialized = false;
}

VkInstance pt::Setup::get_instance(void) const
{
    if(!this->initialized)
        throw std::runtime_error("[pt::Setup::get_instance]: Cannot execute pt::Setup::get_instance before initializing.");
    return this->instance;
}

VkPhysicalDevice pt::Setup::get_physical_device(void) const
{
    if(!this->initialized)
        throw std::runtime_error("[pt::Setup::get_physical_device]: Cannot execute pt::Setup::get_physical_device before initializing.");
    return this->physical_device;
}

VkDevice pt::Setup::get_device(void) const
{
    if(!this->initialized)
        throw std::runtime_error("[pt::Setup::get_device]: Cannot execute pt::Setup::get_device before initializing.");
    return this->device;
}

VkQueue pt::Setup::get_rt_queue(void) const
{
    if(!this->initialized)
        throw std::runtime_error("[pt::Setup::get_rt_queue]: Cannot execute pt::Setup::get_rt_queue before initializing.");
    return this->rtq;
}

const VkPhysicalDeviceRayTracingPropertiesNV& pt::Setup::get_rt_properties(void) const
{
    if(!this->initialized)
        throw std::runtime_error("[pt::Setup::get_rt_properties]: Cannot execute pt::Setup::get_rt_properties before initializing.");
    return this->rt_properties;
}

const vka::QueueInfo& pt::Setup::get_rt_queue_info(void) const
{
    if(!this->initialized)
        throw std::runtime_error("[pt::Setup::get_rt_queue_info]: Cannot execute pt::Setup::get_rt_queue_info before initializing.");
    return this->rtq_info;
}
