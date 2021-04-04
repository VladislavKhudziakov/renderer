#pragma once

#include <vk_utils/handlers.hpp>
#include <errors/error_handler.hpp>

#include <functional>

namespace vk_utils
{
    class context
    {
    public:
        struct extensions_info
        {
            const char** names;
            uint32_t count;
        };

        struct layers_info
        {
            const char** names;
            uint32_t count;
        };

        struct memory_alloc_info
        {
            int32_t memory_type_index{-1};
            size_t allocation_size{0};
            size_t allocation_alignment{0};
        };

        enum queue_type {
            QUEUE_TYPE_GRAPHICS,
            QUEUE_TYPE_COMPUTE,
            QUEUE_TYPE_PRESENT,
            QUEUE_TYPE_SIZE
        };

        struct context_init_info
        {
            const char* device_name{nullptr};

            extensions_info required_instance_extensions{};
            extensions_info additional_instance_extensions{};

            layers_info required_instance_layers{};
            layers_info additional_instance_layers{};

            extensions_info required_device_extensions{};
            extensions_info additional_device_extensions{};

            layers_info required_device_layers{};
            layers_info additional_device_layers{};

            std::function<VkResult(VkInstance instance, VkSurfaceKHR* surface)> surface_create_callback;
        };

        ~context();
        static errors::error init(const char* app_name, const context_init_info& info);
        static const context& get();

        VkDevice device() const;
        VmaAllocator allocator() const;
        VkPhysicalDevice gpu() const;
        VkInstance instance() const;
        VkQueue queue(queue_type type) const;
        VkSurfaceKHR surface() const;
        const char* app_name() const;
        int32_t queue_family_index(queue_type) const;
        memory_alloc_info get_memory_alloc_info(VkBuffer buffer, VkMemoryPropertyFlags props) const;

    private:
        static VkDebugUtilsMessengerCreateInfoEXT get_debug_messenger_create_info();
        static errors::error init_instance(const char* app_name, const context_init_info& info);
        static errors::error init_debug_messenger(const context_init_info& info);
        static errors::error select_physical_device(const context_init_info& info);
        static errors::error init_device(const context_init_info& info);
        static errors::error request_queues();
        static errors::error init_memory_allocator();

        instance_handler m_instance{};
        surface_handler m_surface{};
        debug_messenger_handler m_debug_messenger{};

        VkPhysicalDevice m_physical_device{};

        device_handler m_device{};
        vma_allocator_handler m_allocator{};

        int32_t m_queue_families_indices[QUEUE_TYPE_SIZE]{};
        VkQueue m_queues[QUEUE_TYPE_SIZE]{};

        const char* m_app_name;
    };
} // namespace vk_utils
