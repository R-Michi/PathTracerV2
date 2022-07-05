#pragma once

namespace pt
{
    struct Settings
    {
        uint32_t rt_width;
        uint32_t rt_height;
        uint32_t iterations;
    };

    struct SetupCreateInfo
    {
        std::string app_name;
        uint32_t version_major;
        uint32_t version_minor;
        uint32_t version_patch;
        const Settings* psettings;
    };

    class Setup
    {
    private:
        constexpr static uint32_t GPU_MEMORY                        = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        constexpr static uint32_t CPU_MEMORY                        = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        constexpr static uint32_t CACHED_MEMORY                     = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        constexpr static VkPhysicalDeviceType PRIMARY_DEVICE_TYPE   = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        constexpr static VkPhysicalDeviceType SECONDARY_DEVICE_TYPE = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
        constexpr static VkQueueFlags RAY_TRACING_QUEUE_FLAGS       = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;

        bool initialized;
        SetupCreateInfo info;

        std::vector<std::string> instance_layers, device_layers;
        std::vector<std::string> instance_extensions, device_extensions;

        // properties
        VkPhysicalDeviceRayTracingPropertiesNV rt_properties;
        vka::QueueInfo rtq_info;

        // handles
        VkInstance instance;
        VkPhysicalDevice physical_device;
        VkDevice device;
        VkQueue rtq;

        void create_instance(void);
        void create_physical_device(void);
        void create_queues(void);
        void create_device(void);

    public:
        Setup(void);
        virtual ~Setup(void)
        { this->destroy(); }

        Setup(const Setup&) = delete;
        Setup& operator= (const Setup&) = delete;

        Setup(Setup&&) = delete;
        Setup& operator= (Setup&&) = delete;

        /**
         * @brief Initializes the setup.
         * @param info setup create info
         */
        void init(const SetupCreateInfo& info);

        /**
         * @brief Destroys all internal objects
         */
        void destroy(void);

        /**
         * @brief       Enables a layer at instance level.
         * @param name  Name of the layer.
         * @throw       invalid_argument if layer is not supported.
         * @throw       runtime_error if called after initialization.
         */
        void enable_instance_layer(const std::string& name);
        
        /**
         * @brief       Enables an extension at instance level.
         * @param name  Name of the extension.
         * @throw       invalid_argument if extension is not supported.
         * @throw       runtime_error if called after initialization.
         */
        void enable_instance_extension(const std::string& name);

        /**
         * @brief       Enables a layer at device level.
         * @param name  Name of the layer.
         * @throw       invalid_argument if layer is not supported.
         * @throw       runtime_error if called after initialization.
         */
        void enable_device_layer(const std::string& name);
        
        /**
         * @brief       Enables an extension at device level.
         * @param name  Name of the extension.
         * @throw       invalid_argument if extension is not supported.
         * @throw       runtime_error if called after initialization.
         */
        void enable_device_extension(const std::string& name);

        /**
         * @return vulkan instance
         */
        VkInstance get_instance(void) const;

        /**
         * @return used physical device
         */
        VkPhysicalDevice get_physical_device(void) const;

        /**
         * @return used logical device
         */
        VkDevice get_device(void) const;

        /**
         * @return queue for ray tracing
         */
        VkQueue get_rt_queue(void) const;

        /**
         * @return queue info of the ray tracing queue
         */
        const vka::QueueInfo& get_rt_queue_info(void) const;

        /**
         * @return ray tracing properties of current physical device
         */
        const VkPhysicalDeviceRayTracingPropertiesNV& get_rt_properties(void) const;

        /**
         * @return wether the setup is initialized.
         */
        inline bool is_initialized(void) const noexcept
        { return this->initialized; }

        inline const Settings* get_settings(void) const noexcept
        { return this->info.psettings; }
    };
}
