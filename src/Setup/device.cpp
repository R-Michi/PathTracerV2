#include "../application.h"
#include <iostream>

void pt::Setup::enable_device_layer(const std::string& name)
{
    if(this->initialized)
        throw std::runtime_error("[pt::Setup::enable_device_layer]: pt::Setup::enable_instance_layer was called after initialization!");
    this->device_layers.push_back(name);
}

void pt::Setup::enable_device_extension(const std::string& name)
{
    if(this->initialized)
        throw std::runtime_error("[pt::Setup::enable_device_extension]: pt::Setup::enable_instance_extension was called after initialization!");
    this->device_extensions.push_back(name);
}

void pt::Setup::create_physical_device(void)
{
    std::vector<VkPhysicalDevice> physical_devices;
    vka::device::get(this->instance, physical_devices);

    // find best matching physical device
    vka::PhysicalDeviceFilter filter = {};
    filter.sequence             = nullptr;
    filter.memoryPropertyFlags  = { GPU_MEMORY, CPU_MEMORY, CACHED_MEMORY };
    filter.deviceTypeHirachy    = { PRIMARY_DEVICE_TYPE, SECONDARY_DEVICE_TYPE };
    filter.pSurface             = nullptr;
    filter.queueFamilyFlags     = { RAY_TRACING_QUEUE_FLAGS };

    size_t index = vka::device::find(physical_devices, 0, filter);
    if(index == VKA_NPOS)
        throw std::runtime_error("[pt::Setup::create_physical_device]: Unable to find physical device.");
    this->physical_device = physical_devices.at(index);

    // quere ray tracing properties
    VkPhysicalDeviceProperties2 properties2;
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &this->rt_properties;
    this->rt_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
    this->rt_properties.pNext = nullptr;
    vkGetPhysicalDeviceProperties2(this->physical_device, &properties2);
}

void pt::Setup::create_device(void)
{
    // map queue famnily index to vector of queue priorities, as there can be multiple queues from a queue family
    std::map<uint32_t, std::vector<float>> queue_priorities;

    // get priority vector of ray tracing queue family
    std::vector<float>& rtqp = queue_priorities[this->rtq_info.queueFamilyIndex];

    // insert queue priority for every used ray tracing queue
    rtqp.insert(rtqp.begin() + this->rtq_info.queueBaseIndex, this->rtq_info.usedQueueCount, 1.0f);

    // device queue create info for every queue family index that is in use
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    for(auto iter = queue_priorities.begin(); iter != queue_priorities.end(); iter++)
    {
        VkDeviceQueueCreateInfo queue_ci = {};
        queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_ci.pNext = nullptr;
        queue_ci.flags = 0;
        queue_ci.queueFamilyIndex = iter->first;
        queue_ci.queueCount = iter->second.size();
        queue_ci.pQueuePriorities = iter->second.data();
        queue_infos.push_back(queue_ci);
    }

    // convert layers and extensions to char* array
    std::vector<const char*> layers, extensions;
    for(const std::string& layer : this->device_layers)
    {
        if(!vka::device::is_layer_supported(this->physical_device, layer.c_str()))
            throw std::invalid_argument("[pt::Setup::create_device]: Device layer " + layer + " is not supported!");
        layers.push_back(layer.c_str());
    }
    for(const std::string& extension : this->device_extensions)
    {
        if(!vka::device::is_extension_supported(this->physical_device, extension.c_str()))
            throw std::invalid_argument("[pt::Setup::create_device]: Device extension " + extension + " is not supported!");
        extensions.push_back(extension.c_str());
    }

    // enabled device features
    VkPhysicalDeviceDescriptorIndexingFeatures indexing_features = {};
    indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    indexing_features.pNext = nullptr;
    indexing_features.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    indexing_features.runtimeDescriptorArray = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2 = {};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &indexing_features;
    features2.features = {};

    VkDeviceCreateInfo device_ci = {};
    device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_ci.pNext = &features2;   // pNext chain contains device-features2 instead of pEnabledFeatures
    device_ci.flags = 0;
    device_ci.queueCreateInfoCount = queue_infos.size();
    device_ci.pQueueCreateInfos = queue_infos.data();
    device_ci.enabledLayerCount = layers.size();
    device_ci.ppEnabledLayerNames = layers.data();
    device_ci.enabledExtensionCount = extensions.size();
    device_ci.ppEnabledExtensionNames = extensions.data();
    device_ci.pEnabledFeatures = nullptr;

    if(vkCreateDevice(this->physical_device, &device_ci, nullptr, &this->device) != VK_SUCCESS)
        throw std::runtime_error("[pt::Setup::create_device]: Failed to create device.");

    // get ray tracing queue
    vkGetDeviceQueue(this->device, this->rtq_info.queueFamilyIndex, this->rtq_info.queueBaseIndex + 0, &this->rtq);

    /* ---------- CLEANUP ---------- */
    // global vector of extensions is not needed any longer
    this->device_layers.clear();
    this->device_extensions.clear();
}
