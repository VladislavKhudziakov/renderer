#pragma once

#include <vk_utils/handlers.hpp>
#include <errors/error_handler.hpp>


namespace vk_utils
{
    ERROR_TYPE create_image_2D(
        VkQueue transfer_queue,
        VkCommandPool command_pool,
        uint32_t width,
        uint32_t height,
        VkFormat format,
        bool gen_mips,
        void* data,
        vk_utils::vma_image_handler& image,
        vk_utils::image_view_handler& img_view,
        vk_utils::sampler_handler& sampler);

    ERROR_TYPE create_buffer(
        vk_utils::vma_buffer_handler& buffer,
        VkBufferUsageFlags buffer_usage,
        VmaMemoryUsage memory_usage,
        uint32_t size,
        void* data = nullptr);

    vk_utils::fence_handler create_fence(VkFenceCreateFlagBits flags = static_cast<VkFenceCreateFlagBits>(0));
    vk_utils::semaphore_handler create_semaphore();

}
