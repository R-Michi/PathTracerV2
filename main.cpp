#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#define VKA_IMPLEMENTATION

#include <iostream>
#include <stb/stb_image_write.h>
#include "src/application.h"

void on_model_load(const std::string& path);
void on_texture_load(const std::string& path);

int main()
{
    pt::Setup setup;
    pt::PathTracer path_tracer;

    try
    {
        pt::Settings settings = {};
        settings.rt_width = 3840;
        settings.rt_height = 2160;
        settings.iterations = 5;

        // information for setup
        pt::SetupCreateInfo setup_ci = {};
        setup_ci.app_name = "Path Tracer";
        setup_ci.version_major = 1;
        setup_ci.version_minor = 0;
        setup_ci.version_minor = 0;
        setup_ci.psettings = &settings;

        // instance layers
        #ifdef VKA_DEBUG
        setup.enable_instance_layer("VK_LAYER_LUNARG_standard_validation");
        setup.enable_instance_layer("VK_LAYER_KHRONOS_validation");
        #endif

        // instance extensions
        #ifdef VKA_DEBUG
        setup.enable_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        #endif
        setup.enable_instance_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        // device extensions
        setup.enable_device_extension(VK_NV_RAY_TRACING_EXTENSION_NAME);
        setup.enable_device_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
        setup.enable_device_extension(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
        setup.enable_device_extension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
        setup.enable_device_extension(VK_KHR_MAINTENANCE3_EXTENSION_NAME);

        // set callbacks
        path_tracer.set_model_load_callback(on_model_load);
        path_tracer.set_texture_load_callback(on_texture_load);

        // load models and environment map
        path_tracer.load_environment("../assets/environment/environment3.hdr");
        path_tracer.load_model("../assets/models/test.obj");

        // initialize setup
        setup.init(setup_ci);

        // print some device info
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(setup.get_physical_device(), &device_properties);

        std::cout << "--------------------------------- GPU Properties: ---------------------------------" << std::endl;
        std::cout << "GPU Name:       " << device_properties.deviceName << std::endl;
        std::cout << "GPU ID:         " << device_properties.deviceID << std::endl;
        std::cout << "Vendor ID:      " << device_properties.vendorID << std::endl;
        std::cout << "Vulkan Version: " << VK_VERSION_MAJOR(device_properties.apiVersion) << "." << VK_VERSION_MINOR(device_properties.apiVersion) << "." << VK_VERSION_PATCH(device_properties.apiVersion) << std::endl;
        std::cout << "Driver Version: " << VK_VERSION_MAJOR(device_properties.driverVersion) << "." << VK_VERSION_MINOR(device_properties.driverVersion) << "." << VK_VERSION_PATCH(device_properties.driverVersion) << std::endl;

        std::cout << "--------------------------------- Ray-Tracing Properties: ---------------------------------" << std::endl;
        std::cout << "Max recursion depth:         " << setup.get_rt_properties().maxRecursionDepth << std::endl;
        std::cout << "Max geometry count:          " << setup.get_rt_properties().maxGeometryCount << std::endl;
        std::cout << "Max instance count:          " << setup.get_rt_properties().maxInstanceCount << std::endl;
        std::cout << "Max triangle count:          " << setup.get_rt_properties().maxTriangleCount << std::endl;
        std::cout << "Max acceleration structures: " << setup.get_rt_properties().maxDescriptorSetAccelerationStructures << std::endl;

        // initialize path tracer application
        path_tracer.init(&setup);

        // run the path tracer, it returns the rendering time in nanoseconds
        uint64_t exectime = path_tracer.run();
        std::cout << "Rendering took: " << exectime / 1e6 << "ms" << std::endl;

        // write image pixel data to file
        size_t stride;
        uint32_t comp;
        const uint8_t* pixels = path_tracer.map_image(stride, comp);
        stbi_write_png("path_tracer_output.png", settings.rt_width, settings.rt_height, comp, pixels, stride);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    // destroy application
    path_tracer.destroy();
    setup.destroy();
    return 0;
}

void on_model_load(const std::string& path)
{
    if(path.size() == 0) return;
    std::cout << "[PathTracer | Info]: Loaded model \"" << path << "\"" << std::endl;
}
void on_texture_load(const std::string& path)
{
    if(path.size() == 0) return;
    std::cout << "[PathTracer | Info]: Loaded texture \"" << path << "\"" << std::endl;
}
