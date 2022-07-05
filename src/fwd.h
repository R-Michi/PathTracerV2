#pragma once

namespace pt
{
    namespace detail
    {
        // functions for extension VK_NV_ray_tracing
        inline PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructureNV = nullptr;
        inline PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructureNV = nullptr;
        inline PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirementsNV = nullptr;
        inline PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructureNV = nullptr;
        inline PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemoryNV = nullptr;
        inline PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandleNV = nullptr;
        inline PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV = nullptr;
        inline PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV = nullptr;
        inline PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV= nullptr;

        inline void init_VK_NV_ray_tracing(VkInstance instance)
        {
            vkCreateAccelerationStructureNV = (PFN_vkCreateAccelerationStructureNV)vkGetInstanceProcAddr(instance, "vkCreateAccelerationStructureNV");
            vkDestroyAccelerationStructureNV = (PFN_vkDestroyAccelerationStructureNV)vkGetInstanceProcAddr(instance, "vkDestroyAccelerationStructureNV");
            vkGetAccelerationStructureMemoryRequirementsNV = (PFN_vkGetAccelerationStructureMemoryRequirementsNV)vkGetInstanceProcAddr(instance, "vkGetAccelerationStructureMemoryRequirementsNV");
            vkCmdBuildAccelerationStructureNV = (PFN_vkCmdBuildAccelerationStructureNV)vkGetInstanceProcAddr(instance, "vkCmdBuildAccelerationStructureNV");
            vkBindAccelerationStructureMemoryNV = (PFN_vkBindAccelerationStructureMemoryNV)vkGetInstanceProcAddr(instance, "vkBindAccelerationStructureMemoryNV");
            vkGetAccelerationStructureHandleNV = (PFN_vkGetAccelerationStructureHandleNV)vkGetInstanceProcAddr(instance, "vkGetAccelerationStructureHandleNV");
            vkCreateRayTracingPipelinesNV = (PFN_vkCreateRayTracingPipelinesNV)vkGetInstanceProcAddr(instance, "vkCreateRayTracingPipelinesNV");
            vkGetRayTracingShaderGroupHandlesNV = (PFN_vkGetRayTracingShaderGroupHandlesNV)vkGetInstanceProcAddr(instance, "vkGetRayTracingShaderGroupHandlesNV");
            vkCmdTraceRaysNV = (PFN_vkCmdTraceRaysNV)vkGetInstanceProcAddr(instance, "vkCmdTraceRaysNV");
        }
    };
} // namespace pt
