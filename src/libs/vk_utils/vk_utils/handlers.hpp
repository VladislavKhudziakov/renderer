#pragma once

#if defined(__WIN32__) || defined(__WIN32) || defined(WIN32)
    #define NOMINMAX
#endif // defined(__WIN32__) || defined(__WIN32) || defeind(WIN32)


#include <vk_utils/defs.hpp>
#include <VulkanMemoryAllocator/src/vk_mem_alloc.h>

#include <utility>
#include <vector>

#define VK_CHECK(code) PASS_ERROR(errors::handler_error_code((code)))

namespace vk_utils
{
    namespace detail
    {
        inline VkResult vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger)
        {
            auto function_impl = (PFN_vkCreateDebugUtilsMessengerEXT) (vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
            return function_impl(instance, pCreateInfo, pAllocator, pMessenger);
        }

        inline void vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT pMessenger, const VkAllocationCallbacks* pAllocator)
        {
            auto function_impl = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
            //            function_impl(instance, pMessenger, pAllocator);
        }
    } // namespace detail


    template<typename InitializerType, typename InitStructType, typename StructType, VkResult (*init_func)(InitializerType, const InitStructType*, const VkAllocationCallbacks*, StructType*), void (*destroy_func)(InitializerType, StructType, const VkAllocationCallbacks*)>
    class vulkan_base_handler
    {
    public:
        vulkan_base_handler() = default;
        ~vulkan_base_handler()
        {
            destroy();
        }
        vulkan_base_handler(const vulkan_base_handler&) = delete;
        vulkan_base_handler& operator=(const vulkan_base_handler&) = delete;
        vulkan_base_handler(vulkan_base_handler&& src) noexcept
        {
            *this = std::move(src);
        }

        vulkan_base_handler& operator=(vulkan_base_handler&& src) noexcept
        {
            if (this == &src) {
                return *this;
            }
            std::swap(m_handler, src.m_handler);
            std::swap(m_initializer, src.m_initializer);
            return *this;
        }

        virtual VkResult init(InitializerType initializer, InitStructType* info)
        {
            m_initializer = initializer;
            return init_func(m_initializer, info, nullptr, &m_handler);
        }

        virtual VkResult reset(InitializerType initializer, InitStructType* info)
        {
            InitializerType old_initializer = m_initializer;
            StructType old_handler = m_handler;

            auto init_res = init(initializer, info);

            if (init_res == VK_SUCCESS) {
                destroy_impl(old_initializer, old_handler);
            } else {
                m_initializer = old_initializer;
                m_handler = old_handler;
            }

            return init_res;
        }

        virtual void reset(InitializerType initializer, StructType handler)
        {
            destroy();
            m_initializer = initializer;
            m_handler = handler;
        }

        virtual void destroy()
        {
            destroy_impl(m_initializer, m_handler);
            m_handler = nullptr;
            m_initializer = nullptr;
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
        void destroy_impl(InitializerType initializer, StructType handler)
        {
            if (initializer != nullptr && handler != nullptr) {
                destroy_func(initializer, handler, nullptr);
            }
        }

    protected:
        InitializerType m_initializer{nullptr};
        StructType m_handler{nullptr};
    };


    template<typename InitStructType, typename StructType, VkResult (*init_func)(VkDevice, const InitStructType*, const VkAllocationCallbacks*, StructType*), void (*destroy_func)(VkDevice, StructType, const VkAllocationCallbacks*)>
    class default_device_scoped_handler : public vulkan_base_handler<VkDevice, InitStructType, StructType, init_func, destroy_func>
    {
    };

    template<typename InitStructType, typename StructType, VkResult (*init_func)(VkInstance, const InitStructType*, const VkAllocationCallbacks*, StructType*), void (*destroy_func)(VkInstance, StructType, const VkAllocationCallbacks*)>
    class default_instance_scoped_handler : public vulkan_base_handler<VkInstance, InitStructType, StructType, init_func, destroy_func>
    {
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


    template<typename InitializerType, typename PoolType, typename AllocStructType, typename StructType, typename FreeResType, VkResult (*alloc_function)(InitializerType, const AllocStructType*, StructType*), FreeResType (*free_function)(InitializerType, PoolType, uint32_t, const StructType*)>
    class allocated_handler
    {
    public:
        allocated_handler() = default;
        ~allocated_handler()
        {
            destroy();
        }
        allocated_handler(const allocated_handler&) = delete;
        allocated_handler& operator=(const allocated_handler&) = delete;
        allocated_handler(allocated_handler&& src) noexcept
        {
            *this = std::move(src);
        }
        allocated_handler& operator=(allocated_handler&& src) noexcept
        {
            if (this == &src) {
                return *this;
            }

            std::swap(m_handlers, src.m_handlers);
            std::swap(m_initializer, src.m_initializer);
            std::swap(m_pool, src.m_pool);

            return *this;
        }

        VkResult init(InitializerType initializer, PoolType pool, AllocStructType* info, uint32_t handlers_count)
        {
            m_initializer = initializer;
            m_handlers.resize(handlers_count);
            m_pool = pool;
            return alloc_function(m_initializer, info, m_handlers.data());
        }

        void destroy()
        {
            if (m_initializer != nullptr && m_pool != nullptr && !m_handlers.empty()) {
                free_function(m_initializer, m_pool, m_handlers.size(), m_handlers.data());
                m_handlers.clear();
                m_initializer = nullptr;
                m_pool = nullptr;
            }
        }

        uint32_t handlers_count() const
        {
            return m_handlers.size();
        }

        operator const StructType*() const
        {
            return m_handlers.data();
        }

        StructType operator[](size_t index)
        {
            if (index < m_handlers.size()) {
                return m_handlers[index];
            }
            return nullptr;
        }

    private:
        InitializerType m_initializer{nullptr};
        PoolType m_pool{nullptr};
        std::vector<StructType> m_handlers{};
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
            VkDevice old_device = m_device;
            VkPipeline old_handler = m_handler;

            auto res = init(i, info);

            if (res == VK_SUCCESS) {
                destroy_impl(old_device, old_handler);
            }

            return res;
        }

        void reset(VkDevice d, VkPipeline p)
        {
            destroy();
            m_device = d;
            m_handler = p;
        }

        void destroy()
        {
            destroy_impl(m_device, m_handler);
            m_device = nullptr;
            m_handler = nullptr;
        }

        operator VkPipeline() const
        {
            return m_handler;
        }

    private:
        void destroy_impl(VkDevice device, VkPipeline pipeline)
        {
            if (device != nullptr && pipeline != nullptr) {
                vkDestroyPipeline(device, pipeline, nullptr);
            }
        }

        VkDevice m_device{nullptr};
        VkPipeline m_handler{nullptr};
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

    template<
        typename InitStructType,
        typename StructType,
        VkResult (*init_func)(VmaAllocator, const InitStructType*, const VmaAllocationCreateInfo*, StructType*, VmaAllocation*, VmaAllocationInfo*),
        void (*destroy_func)(VmaAllocator, StructType, VmaAllocation)>
    class vma_resource_handler
    {
    public:
        vma_resource_handler() = default;
        ~vma_resource_handler()
        {
            destroy();
        }
        vma_resource_handler(const vma_resource_handler&) = delete;
        vma_resource_handler& operator=(const vma_resource_handler&) = delete;
        vma_resource_handler(vma_resource_handler&& src) noexcept
        {
            *this = std::move(src);
        }

        vma_resource_handler& operator=(vma_resource_handler&& src) noexcept
        {
            if (this == &src) {
                return *this;
            }
            std::swap(m_resource, src.m_resource);
            std::swap(m_allocation, src.m_allocation);
            std::swap(m_allocator, src.m_allocator);
            return *this;
        }

        VkResult init(VmaAllocator allocator, InitStructType* init_info, VmaAllocationCreateInfo* alloc_info)
        {
            destroy();
            m_allocator = allocator;
            return init_func(m_allocator, init_info, alloc_info, &m_resource, &m_allocation, &m_alloc_info);
        }

        void destroy()
        {
            if (m_allocator != nullptr && m_resource != nullptr && m_allocation != nullptr) {
                destroy_func(m_allocator, m_resource, m_allocation);

                m_allocator = nullptr;
                m_resource = nullptr;
                m_allocation = nullptr;
            }
        }

        const VmaAllocationInfo& get_alloc_info() const
        {
            return m_alloc_info;
        }

        operator StructType() const
        {
            return m_resource;
        }

        operator VmaAllocation() const
        {
            return m_allocation;
        }

        operator const StructType*() const
        {
            return &m_resource;
        }

        operator const VmaAllocation*() const
        {
            return &m_allocation;
        }

    private:
        VmaAllocator m_allocator{nullptr};
        StructType m_resource{nullptr};
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

    using descriptor_set_layout_handler =
        default_device_scoped_handler<VkDescriptorSetLayoutCreateInfo, VkDescriptorSetLayout, vkCreateDescriptorSetLayout, vkDestroyDescriptorSetLayout>;

    using cmd_pool_handler =
        default_device_scoped_handler<VkCommandPoolCreateInfo, VkCommandPool, vkCreateCommandPool, vkDestroyCommandPool>;

    using cmd_buffers_handler =
        allocated_handler<VkDevice, VkCommandPool, VkCommandBufferAllocateInfo, VkCommandBuffer, void, vkAllocateCommandBuffers, vkFreeCommandBuffers>;

    using descriptor_pool_handler =
        default_device_scoped_handler<VkDescriptorPoolCreateInfo, VkDescriptorPool, vkCreateDescriptorPool, vkDestroyDescriptorPool>;

    using image_view_handler =
        default_device_scoped_handler<VkImageViewCreateInfo, VkImageView, vkCreateImageView, vkDestroyImageView>;

    using sampler_handler =
        default_device_scoped_handler<VkSamplerCreateInfo, VkSampler, vkCreateSampler, vkDestroySampler>;

    using descriptor_set_handler =
        allocated_handler<VkDevice, VkDescriptorPool, VkDescriptorSetAllocateInfo, VkDescriptorSet, VkResult, vkAllocateDescriptorSets, vkFreeDescriptorSets>;

    using vma_buffer_handler =
        vma_resource_handler<VkBufferCreateInfo, VkBuffer, vmaCreateBuffer, vmaDestroyBuffer>;

    using vma_image_handler =
        vma_resource_handler<VkImageCreateInfo, VkImage, vmaCreateImage, vmaDestroyImage>;

    using surface_handler =
        default_instance_scoped_handler<VK_UTILS_SURFACE_CREATE_INFO_TYPE, VkSurfaceKHR, VK_UTILS_SURFACE_CREATE_FUNCTION, vkDestroySurfaceKHR>;

    using debug_messenger_handler =
        default_instance_scoped_handler<VkDebugUtilsMessengerCreateInfoEXT, VkDebugUtilsMessengerEXT, detail::vkCreateDebugUtilsMessengerEXT, detail::vkDestroyDebugUtilsMessengerEXT>;

} // namespace vk_utils