#include "../application.h"

void pt::Setup::enable_instance_layer(const std::string& name)
{
    if(this->initialized)
        throw std::runtime_error("[pt::Setup::enable_instance_layer]: pt::Setup::enable_instance_layer was called after initialization!");
    this->instance_layers.push_back(name);
}

void pt::Setup::enable_instance_extension(const std::string& name)
{
    if(this->initialized)
        throw std::runtime_error("[pt::Setup::enable_instance_extension]: pt::Setup::enable_instance_extension was called after initialization!");
    this->instance_extensions.push_back(name);
}

void pt::Setup::create_instance(void)
{
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = this->info.app_name.c_str();
    app_info.applicationVersion = VK_MAKE_VERSION(this->info.version_major, this->info.version_minor, this->info.version_patch);
    app_info.pEngineName = this->info.app_name.c_str();
    app_info.engineVersion = app_info.applicationVersion;
    app_info.apiVersion = 0;    // its not supposed to be an API

    // convert layers and extensions to char* array
    std::vector<const char*> layers, extensions;
    for(const std::string& layer : this->instance_layers)
    {
        if(!vka::instance::is_layer_supported(layer.c_str()))
            throw std::invalid_argument("[pt::Setup::create_instance]: Instance layer " + layer + " is not supported!");
        layers.push_back(layer.c_str());
    }
    for(const std::string& extension : this->instance_extensions)
    {
        if(!vka::instance::is_extension_supported(extension.c_str()))
            throw std::invalid_argument("[pt::Setup::create_instance]: Instance extension " + extension + " is not supported!");
        extensions.push_back(extension.c_str());
    }

    VkInstanceCreateInfo instance_ci = {};
    instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.pNext = nullptr;
    instance_ci.flags = 0;
    instance_ci.pApplicationInfo = &app_info;
    instance_ci.enabledLayerCount = layers.size();
    instance_ci.ppEnabledLayerNames = layers.data();
    instance_ci.enabledExtensionCount = extensions.size();
    instance_ci.ppEnabledExtensionNames = extensions.data();

    if(vkCreateInstance(&instance_ci, nullptr, &this->instance) != VK_SUCCESS)
        throw std::runtime_error("[pt::Setup::create_instance]: Failed to create instance!");

    /* ---------- CLEANUP ---------- */
    // global vector of extensions is not needed any longer
    this->instance_layers.clear();
    this->instance_extensions.clear();
}
