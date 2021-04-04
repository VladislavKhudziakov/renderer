#pragma once

#include <vk_utils/defs.hpp>
#include <VulkanMemoryAllocator/src/vk_mem_alloc.h>

#include <utility>
#include <vector>

namespace vk_utils
{
    namespace detail
    {
        inline VkResult vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger)
        {
            auto function_impl = (PFN_vkCreateDebugUtilsMessengerEXT)(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
            return function_impl(instance, pCreateInfo, pAllocator, pMessenger);
        }

        inline void vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT pMessenger, const VkAllocationCallbacks* pAllocator)
        {
            auto function_impl = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
//            function_impl(instance, pMessenger, pAllocator);
        }
    }


    template<typename InitStructType, typename StructType, VkResult (*init_func)(VkDevice, const InitStructType*, const VkAllocationCallbacks*, StructType*), void (*destroy_func)(VkDevice, StructType, const VkAllocationCallbacks*)>
    class default_device_scoped_handler
    {
    public:
        default_device_scoped_handler() = default;
        ~default_device_scoped_handler()
        {
            destroy();
        }
        default_device_scoped_handler(const default_device_scoped_handler&) = delete;
        default_device_scoped_handler& operator=(const default_device_scoped_handler&) = delete;
        default_device_scoped_handler(default_device_scoped_handler&& src) noexcept
        {
            *this = std::move(src);
        }

        default_device_scoped_handler& operator=(default_device_scoped_handler&& src) noexcept
        {
            if (this == &src) {
                return *this;
            }
            std::swap(m_handler, src.m_handler);
            std::swap(m_device, src.m_device);
            return *this;
        }

        VkResult init(VkDevice device, InitStructType* info)
        {
            m_device = device;
            return init_func(m_device, info, nullptr, &m_handler);
        }

        VkResult reset(VkDevice device, InitStructType* info)
        {
            VkDevice old_device = m_device;
            StructType old_handler = m_handler;

            auto init_res = init(device, info);

            if (init_res == VK_SUCCESS) {
                destroy_impl(old_device, old_handler);
            } else {
                m_device = old_device;
                m_handler = old_handler;
            }

            return init_res;
        }

        void reset(VkDevice device, StructType handler)
        {
            destroy();
            m_device = device;
            m_handler = handler;
        }

        void destroy()
        {
            destroy_impl(m_device, m_handler);
            m_handler = nullptr;
            m_device = nullptr;
        }

        operator StructType() const
        {
            return m_handler;
        }

        operator const StructType*() const
        {
            return &m_handler;
        }

    private:
        void destroy_impl(VkDevice device, StructType handler)
        {
            if (device != nullptr && handler != nullptr) {
                destroy_func(device, handler, nullptr);
            }
        }

        VkDevice m_device{nullptr};
        StructType m_handler{nullptr};
    };


    template<typename InitStructType, typename StructType, VkResult (*init_func)(VkInstance, const InitStructType*, const VkAllocationCallbacks*, StructType*), void (*destroy_func)(VkInstance, StructType, const VkAllocationCallbacks*)>
    class default_instance_scoped_handler
    {
    public:
        default_instance_scoped_handler() = default;
        ~default_instance_scoped_handler()
        {
            destroy();
        }
        default_instance_scoped_handler(const default_instance_scoped_handler&) = delete;
        default_instance_scoped_handler& operator=(const default_instance_scoped_handler&) = delete;
        default_instance_scoped_handler(default_instance_scoped_handler&& src) noexcept
        {
            *this = std::move(src);
        }

        default_instance_scoped_handler& operator=(default_instance_scoped_handler&& src) noexcept
        {
            if (this == &src) {
                return *this;
            }
            std::swap(m_handler, src.m_handle);
            std::swap(m_instance, src.m_initializator);
            return *this;
        }

        VkResult init(VkInstance instance, InitStructType* info)
        {
            m_instance = instance;
            auto r = init_func(m_instance, info, nullptr, &m_handler);
            return r;
        }

        VkResult reset(VkInstance instance, InitStructType* info)
        {
            VkInstance old_instance = m_instance;
            StructType old_handler = m_handler;

            auto res = init(instance, info);

            if (res == VK_SUCCESS) {
                destroy_impl(old_instance, old_handler);
            }

            return res;
        }

        void reset(VkInstance instance, StructType handler)
        {
            destroy();
            m_instance = instance;
            m_handler = handler;
        }

        void destroy()
        {
            destroy_impl(m_instance, m_handler);
            m_handler = nullptr;
            m_instance = nullptr;
        }

        operator StructType() const
        {
            return m_handler;
        }

    private:
        void destroy_impl(VkInstance instance, StructType handler)
        {
            if (instance != nullptr && handler != nullptr) {
                destroy_func(instance, handler, nullptr);
            }
        }

        VkInstance m_instance{nullptr};
        StructType m_handler{nullptr};
    };


    class instance_handler
    {
    public:
        instance_handler() = default;
        ~instance_handler()
        {
            destroy();
        }
        instance_handler(const instance_handler&) = delete;
        instance_handler& operator=(const instance_handler&) = delete;
        instance_handler(instance_handler&& src) noexcept
        {
            *this = std::move(src);
        }

        instance_handler& operator=(instance_handler&& src) noexcept
        {
            if (this == &src) {
                return *this;
            }
            std::swap(m_handle, src.m_handle);
            return *this;
        }
        VkResult init(VkInstanceCreateInfo* info)
        {
            return vkCreateInstance(info, nullptr, &m_handle);
        }

        void destroy()
        {
            if (m_handle != nullptr) {
                vkDestroyInstance(m_handle, nullptr);
                m_handle = nullptr;
            }
        }

        operator VkInstance() const
        {
            return m_handle;
        }

    private:
        VkInstance m_handle{nullptr};
    };


    class device_handler
    {
    public:
        device_handler() = default;
        ~device_handler()
        {
            destroy();
        }

        device_handler(const device_handler&) = delete;
        device_handler& operator=(const device_handler&) = delete;
        device_handler(device_handler&& src) noexcept
        {
            *this = std::move(src);
        }

        device_handler& operator=(device_handler&& src) noexcept
        {
            if (this == &src) {
                return *this;
            }
            std::swap(m_handle, src.m_handle);
            return *this;
        }


        VkResult init(VkPhysicalDevice physical_device, VkDeviceCreateInfo* info)
        {
            return vkCreateDevice(physical_device, info, nullptr, &m_handle);
        }

        void destroy()
        {
            if (m_handle != nullptr) {
                vkDestroyDevice(m_handle, nullptr);
                m_handle = nullptr;
            }
        }

        operator VkDevice() const
        {
            return m_handle;
        }

    private:
        VkDevice m_handle{nullptr};
    };


    class cmd_buffers_handler
    {
    public:
        cmd_buffers_handler() = default;
        ~cmd_buffers_handler()
        {
            destroy();
        }
        cmd_buffers_handler(const cmd_buffers_handler&) = delete;
        cmd_buffers_handler& operator=(const cmd_buffers_handler&) = delete;
        cmd_buffers_handler(cmd_buffers_handler&& src) noexcept
        {
            *this = std::move(src);
        }
        cmd_buffers_handler& operator=(cmd_buffers_handler&& src) noexcept
        {
            if (this == &src) {
                return *this;
            }

            std::swap(m_handlers, src.m_handlers);
            std::swap(m_device, src.m_device);
            std::swap(m_cmd_pool, src.m_cmd_pool);

            return *this;
        }

        VkResult init(VkDevice device, VkCommandBufferAllocateInfo* info)
        {
            m_device = device;
            m_handlers.resize(info->commandBufferCount);
            m_cmd_pool = info->commandPool;
            return vkAllocateCommandBuffers(m_device, info, m_handlers.data());
        }

        VkResult reset(VkDevice i, VkCommandBufferAllocateInfo* info)
        {
            destroy();
            return init(i, info);
        }

        void destroy()
        {
            if (m_device != nullptr && m_cmd_pool != nullptr && !m_handlers.empty()) {
                vkFreeCommandBuffers(m_device, m_cmd_pool, m_handlers.size(), m_handlers.data());
                m_handlers.clear();
            }
        }

        uint32_t buffers_count() const
        {
            return m_handlers.size();
        }

        operator const VkCommandBuffer*() const
        {
            return m_handlers.data();
        }

        VkCommandBuffer operator[](size_t index)
        {
            if (index < m_handlers.size()) {
                return m_handlers[index];
            }
            return nullptr;
        }

    private:
        VkDevice m_device{nullptr};
        VkCommandPool m_cmd_pool{nullptr};
        std::vector<VkCommandBuffer> m_handlers{};
    };


    class graphics_pipeline_handler
    {
    public:
        graphics_pipeline_handler() = default;
        ~graphics_pipeline_handler()
        {
            destroy();
        }
        graphics_pipeline_handler(const graphics_pipeline_handler&) = delete;
        graphics_pipeline_handler& operator=(const graphics_pipeline_handler&) = delete;
        graphics_pipeline_handler(graphics_pipeline_handler&& src) noexcept
        {
            *this = std::move(src);
        }
        graphics_pipeline_handler& operator=(graphics_pipeline_handler&& src) noexcept
        {
            if (this == &src) {
                return *this;
            }

            std::swap(m_handler, src.m_handler);
            std::swap(m_device, src.m_device);

            return *this;
        }

        VkResult init(VkDevice device, VkGraphicsPipelineCreateInfo* info, VkPipelineCache cache = nullptr)
        {
            m_device = device;
            return vkCreateGraphicsPipelines(m_device, cache, 1, info, nullptr, &m_handler);
        }

        VkResult reset(VkDevice i, VkGraphicsPipelineCreateInfo* info)
        {
            destroy();
            return init(i, info);
        }

        void destroy()
        {
            if (m_device != nullptr && m_handler != nullptr) {
                vkDestroyPipeline(m_device, m_handler, nullptr);
                m_device = nullptr;
                m_handler = nullptr;
            }
        }

        operator VkPipeline() const
        {
            return m_handler;
        }

    private:
        VkDevice m_device{nullptr};
        VkPipeline m_handler;
    };


    class vma_allocator_handler
    {
    public:
        vma_allocator_handler() = default;
        ~vma_allocator_handler()
        {
            destroy();
        }

        vma_allocator_handler(const vma_allocator_handler&) = delete;
        vma_allocator_handler& operator=(const vma_allocator_handler&) = delete;
        vma_allocator_handler(vma_allocator_handler&& src) noexcept
        {
            *this = std::move(src);
        }

        vma_allocator_handler& operator=(vma_allocator_handler&& src) noexcept
        {
            if (this == &src) {
                return *this;
            }
            std::swap(m_handle, src.m_handle);
            return *this;
        }


        VkResult init(VmaAllocatorCreateInfo* info)
        {
            return vmaCreateAllocator(info, &m_handle);
        }

        void destroy()
        {
            if (m_handle != nullptr) {
                vmaDestroyAllocator(m_handle);
                m_handle = nullptr;
            }
        }

        operator VmaAllocator() const
        {
            return m_handle;
        }

    private:
        VmaAllocator m_handle{nullptr};
    };


    class vma_buffer_handler
    {
    public:
        vma_buffer_handler() = default;
        ~vma_buffer_handler()
        {
            destroy();
        }
        vma_buffer_handler(const vma_buffer_handler&) = delete;
        vma_buffer_handler& operator=(const vma_buffer_handler&) = delete;
        vma_buffer_handler(vma_buffer_handler&& src) noexcept
        {
            *this = std::move(src);
        }

        vma_buffer_handler& operator=(vma_buffer_handler&& src) noexcept
        {
            if (this == &src) {
                return *this;
            }
            std::swap(m_buffer, src.m_buffer);
            std::swap(m_allocation, src.m_allocation);
            std::swap(m_allocator, src.m_allocator);
            return *this;
        }

        VkResult init(VmaAllocator allocator, VkBufferCreateInfo* buffer_info, VmaAllocationCreateInfo* alloc_info)
        {
            destroy();
            m_allocator = allocator;
            return vmaCreateBuffer(m_allocator, buffer_info, alloc_info, &m_buffer, &m_allocation, &m_alloc_info);
        }

        void destroy()
        {
            if (m_allocator != nullptr && m_buffer != nullptr && m_allocation != nullptr) {
                vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);

                m_allocator = nullptr;
                m_buffer = nullptr;
                m_allocation = nullptr;
            }
        }

        const VmaAllocationInfo& get_alloc_info() const
        {
            return m_alloc_info;
        }

        operator VkBuffer() const
        {
            return m_buffer;
        }

        operator VmaAllocation() const
        {
            return m_allocation;
        }

        operator const VkBuffer*() const
        {
            return &m_buffer;
        }

        operator const VmaAllocation*() const
        {
            return &m_allocation;
        }

    private:
        VmaAllocator m_allocator{nullptr};
        VkBuffer m_buffer{nullptr};
        VmaAllocation m_allocation{nullptr};
        VmaAllocationInfo m_alloc_info{};
    };

    using img_view_handler =
        default_device_scoped_handler<VkImageViewCreateInfo, VkImageView, vkCreateImageView, vkDestroyImageView>;

    using shader_module_handler =
        default_device_scoped_handler<VkShaderModuleCreateInfo, VkShaderModule, vkCreateShaderModule, vkDestroyShaderModule>;

    using pipeline_layout_handler =
        default_device_scoped_handler<VkPipelineLayoutCreateInfo, VkPipelineLayout, vkCreatePipelineLayout, vkDestroyPipelineLayout>;

    using pass_handler =
        default_device_scoped_handler<VkRenderPassCreateInfo, VkRenderPass, vkCreateRenderPass, vkDestroyRenderPass>;

    using framebuffer_handler =
        default_device_scoped_handler<VkFramebufferCreateInfo, VkFramebuffer, vkCreateFramebuffer, vkDestroyFramebuffer>;

    using cmd_pool_handler =
        default_device_scoped_handler<VkCommandPoolCreateInfo, VkCommandPool, vkCreateCommandPool, vkDestroyCommandPool>;

    using semaphore_handler =
        default_device_scoped_handler<VkSemaphoreCreateInfo, VkSemaphore, vkCreateSemaphore, vkDestroySemaphore>;

    using fence_handler =
        default_device_scoped_handler<VkFenceCreateInfo, VkFence, vkCreateFence, vkDestroyFence>;

    using swapchain_handler =
        default_device_scoped_handler<VkSwapchainCreateInfoKHR, VkSwapchainKHR, vkCreateSwapchainKHR, vkDestroySwapchainKHR>;

    using buffer_handler =
        default_device_scoped_handler<VkBufferCreateInfo, VkBuffer, vkCreateBuffer, vkDestroyBuffer>;

    using device_memory_handler =
        default_device_scoped_handler<VkMemoryAllocateInfo, VkDeviceMemory, vkAllocateMemory, vkFreeMemory>;

    using surface_handler =
        default_instance_scoped_handler<VK_UTILS_SURFACE_CREATE_INFO_TYPE, VkSurfaceKHR, VK_UTILS_SURFACE_CREATE_FUNCTION, vkDestroySurfaceKHR>;

    using debug_messenger_handler =
        default_instance_scoped_handler<VkDebugUtilsMessengerCreateInfoEXT, VkDebugUtilsMessengerEXT, detail::vkCreateDebugUtilsMessengerEXT, detail::vkDestroyDebugUtilsMessengerEXT>;


} // namespace vk_utils