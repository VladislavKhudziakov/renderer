#pragma once

#include <vk_utils/handlers.hpp>
#include <errors/error_handler.hpp>


namespace vk_utils
{
    struct sampler_info
    {
        bool tiled = false;
        VkFilter fitering = VK_FILTER_LINEAR;
        float max_anisatropy = 0;
    };

    ERROR_TYPE load_texture(
      const char*, 
      VkQueue transfer_queue, 
      uint32_t transfer_queue_family_index, 
      VkCommandPool cmd_pool, 
      const sampler_info& sampler, 
      vk_utils::vma_image_handler& out_image, 
      vk_utils::image_view_handler& out_image_view, 
      vk_utils::sampler_handler& out_image_sampler);

    ERROR_TYPE load_texture_2D(
        const char*,
        VkQueue transfer_queue,
        uint32_t transfer_queue_family_index,
        VkCommandPool cmd_pool,
        const sampler_info& sampler,
        vk_utils::vma_image_handler& out_image,
        vk_utils::image_view_handler& out_image_view,
        vk_utils::sampler_handler& out_image_sampler,
        bool gen_mips = true);

    ERROR_TYPE create_texture_2D(
        VkQueue transfer_queue,
        uint32_t transfer_queue_family_index,
        VkCommandPool command_pool,
        const sampler_info& sampler,
        uint32_t width,
        uint32_t height,
        VkFormat format,
        bool gen_mips,
        const void* data,
        vk_utils::vma_image_handler& out_image,
        vk_utils::image_view_handler& out_image_view,
        vk_utils::sampler_handler& out_image_sampler);

    ERROR_TYPE create_buffer(
        vk_utils::vma_buffer_handler& buffer,
        VkBufferUsageFlags buffer_usage,
        VmaMemoryUsage memory_usage,
        uint32_t size,
        const void* data = nullptr);

    ERROR_TYPE load_shader(
        const char* shader_path,
        vk_utils::shader_module_handler& handle,
        VkShaderStageFlagBits& stage);

    ERROR_TYPE create_shader_module(
        const uint32_t* code,
        uint32_t code_size,
        vk_utils::shader_module_handler& handle);

    vk_utils::fence_handler create_fence(VkFenceCreateFlagBits flags = static_cast<VkFenceCreateFlagBits>(0));
    vk_utils::semaphore_handler create_semaphore();

    bool check_opt_tiling_format(VkFormat req_fmt, VkFormatFeatureFlagBits features_flags);
    bool check_linear_tiling_format(VkFormat req_fmt, VkFormatFeatureFlagBits features_flags);

    ERROR_TYPE load_ktx_texture(
        const char* path,
        VkQueue transfer_queue,
        uint32_t transfer_queue_family_index,
        VkCommandPool cmd_pool,
        const sampler_info& sampler,
        vk_utils::vma_image_handler& out_image,
        vk_utils::image_view_handler& out_image_view,
        vk_utils::sampler_handler& out_image_sampler);

    ERROR_TYPE create_ktx_texture(
        const void* data,
        size_t data_size,
        VkQueue transfer_queue,
        uint32_t transfer_queue_family_index,
        VkCommandPool cmd_pool,
        const sampler_info& sampler,
        vk_utils::vma_image_handler& out_image,
        vk_utils::image_view_handler& out_image_view,
        vk_utils::sampler_handler& out_image_sampler);
}
