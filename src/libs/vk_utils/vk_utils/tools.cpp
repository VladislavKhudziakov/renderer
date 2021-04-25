
#include "tools.hpp"

#include <vk_utils/context.hpp>

#include <stb/stb_image.h>
#include <shaderc/shaderc.hpp>

#include <cmath>
#include <cstring>
#include <memory>
#include <algorithm>


ERROR_TYPE vk_utils::load_texture_2D(
    const char* path,
    VkQueue transfer_queue,
    VkCommandPool cmd_pool,
    vk_utils::vma_image_handler& out_image,
    vk_utils::image_view_handler& out_img_view,
    vk_utils::sampler_handler& out_sampler,
    bool gen_mips)
{
    int w, h, c;
    std::unique_ptr<stbi_uc, std::function<void(stbi_uc*)>> image_handler{
        stbi_load(path, &w, &h, &c, 0),
        [](stbi_uc* ptr) {if (ptr != nullptr) stbi_image_free(ptr); }};

    if (image_handler == nullptr) {
        RAISE_ERROR_WARN(-1, "cannot load image.");
    }

    std::vector<uint8_t> rgba_image_buffer;
    const uint8_t* img_data_ptr = image_handler.get();

    if (c == STBI_rgb && !check_opt_tiling_format(VK_FORMAT_R8G8B8_SRGB, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) && !check_opt_tiling_format(VK_FORMAT_R8G8B8_UINT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
        c = STBI_rgb_alpha;

        rgba_image_buffer.reserve(w * h * 4);

        for (size_t i = 0; i < w * h; ++i) {
            rgba_image_buffer.push_back(img_data_ptr[i * 3]);
            rgba_image_buffer.push_back(img_data_ptr[i * 3 + 1]);
            rgba_image_buffer.push_back(img_data_ptr[i * 3 + 2]);
            rgba_image_buffer.push_back(255);
        }

        img_data_ptr = rgba_image_buffer.data();
    }

    VkFormat fmt{};

    switch (c) {
        case STBI_grey: {
            if (check_opt_tiling_format(VK_FORMAT_R8_SRGB, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8_SRGB;
            } else if (check_opt_tiling_format(VK_FORMAT_R8_UINT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8_UINT;
            } else {
                RAISE_ERROR_WARN(-1, "unsupported grey image format.");
            }
            break;
        }
        case STBI_grey_alpha: {
            if (check_opt_tiling_format(VK_FORMAT_R8G8_SRGB, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8G8_SRGB;
            } else if (check_opt_tiling_format(VK_FORMAT_R8G8_UINT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8G8_UINT;
            } else {
                RAISE_ERROR_WARN(-1, "unsupported grey_alpha image format.");
            }
            break;
        }
        case STBI_rgb: {
            if (check_opt_tiling_format(VK_FORMAT_R8G8B8_SRGB, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8G8B8_SRGB;
            } else if (check_opt_tiling_format(VK_FORMAT_R8G8B8_UINT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8G8B8_UINT;
            } else {
                RAISE_ERROR_WARN(-1, "unsupported rgb image format.");
            }
            break;
        }
        case STBI_rgb_alpha: {
            if (check_opt_tiling_format(VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8G8B8A8_SRGB;
            } else if (check_opt_tiling_format(VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                fmt = VK_FORMAT_R8G8B8A8_UINT;
            } else {
                RAISE_ERROR_WARN(-1, "unsupported rgba image format.");
            }
            break;
        }
        default:
            RAISE_ERROR_WARN(-1, "invalid img format.");
    }

    PASS_ERROR(create_texture_2D(transfer_queue, cmd_pool, w, h, fmt, gen_mips, img_data_ptr, out_image, out_img_view, out_sampler));

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_utils::create_texture_2D(
    VkQueue transfer_queue,
    VkCommandPool command_pool,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    bool gen_mips,
    const void* data,
    vk_utils::vma_image_handler& out_image,
    vk_utils::image_view_handler& out_image_view,
    vk_utils::sampler_handler& out_image_sampler)
{
    vk_utils::vma_image_handler image;
    vk_utils::image_view_handler image_view;
    vk_utils::sampler_handler image_sampler;

    size_t pixel_size = 1;
    VkComponentMapping components;

    switch (format) {
        case VK_FORMAT_R8_SRGB:
            [[fallthrough]];
        case VK_FORMAT_R8_UINT:
            pixel_size = 1;
            components = {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_R,
                .b = VK_COMPONENT_SWIZZLE_R,
                .a = VK_COMPONENT_SWIZZLE_ONE};
            break;
        case VK_FORMAT_R8G8_SRGB:
            [[fallthrough]];
        case VK_FORMAT_R8G8_UINT:
            pixel_size = 2;
            components = {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_ZERO,
                .b = VK_COMPONENT_SWIZZLE_ZERO,
                .a = VK_COMPONENT_SWIZZLE_G};
            break;
        case VK_FORMAT_R8G8B8_SRGB:
            [[fallthrough]];
        case VK_FORMAT_R8G8B8_UINT:
            pixel_size = 3;
            components = {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_ONE};
            break;
        case VK_FORMAT_R8G8B8A8_SRGB:
            [[fallthrough]];
        case VK_FORMAT_R8G8B8A8_UINT:
            pixel_size = 4;
            components = {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_A};
            break;
        default:
            RAISE_ERROR_FATAL(-1, "unsupported pixels format.");
    }

    const uint32_t mip_levels = gen_mips ? log2(std::max(width, height)) : 1;

    vk_utils::vma_buffer_handler staging_buffer{};

    if (data != nullptr) {
        create_buffer(staging_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, width * height * pixel_size, data);
    }

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = nullptr;
    image_info.flags = 0;
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = format;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.arrayLayers = 1;
    image_info.mipLevels = mip_levels;
    image_info.extent = {
        .width = width,
        .height = height,
        .depth = 1};

    image_info.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo image_alloc_info{};
    image_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (const auto e = image.init(vk_utils::context::get().allocator(), &image_info, &image_alloc_info); e != VK_SUCCESS) {
        RAISE_ERROR_WARN(e, "Cannot init image.");
    }

    VkImageViewCreateInfo img_view_info{};
    img_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    img_view_info.pNext = nullptr;
    img_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    img_view_info.format = image_info.format;
    img_view_info.components = components;
    img_view_info.image = image;
    img_view_info.subresourceRange.baseArrayLayer = 0;
    img_view_info.subresourceRange.layerCount = 1;
    img_view_info.subresourceRange.baseMipLevel = 0;
    img_view_info.subresourceRange.levelCount = mip_levels;
    img_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    if (const auto e = image_view.init(vk_utils::context::get().device(), &img_view_info); e != VK_SUCCESS) {
        image.destroy();
        RAISE_ERROR_WARN(e, "Cannot init image view.");
    }

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.pNext = nullptr;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 0;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.minLod = 0;
    sampler_info.maxLod = mip_levels - 1;
    sampler_info.mipLodBias = 0;
    sampler_info.compareEnable = VK_FALSE;

    if (const auto e = image_sampler.init(vk_utils::context::get().device(), &sampler_info); e != VK_SUCCESS) {
        image.destroy();
        image_view.destroy();
        RAISE_ERROR_WARN(e, "Cannot init sampler.");
    }

    VkCommandBufferAllocateInfo buffer_alloc_info{};
    buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buffer_alloc_info.pNext = nullptr;
    buffer_alloc_info.commandBufferCount = 1;
    buffer_alloc_info.commandPool = command_pool;
    buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    vk_utils::cmd_buffers_handler images_data_transfer_buffer;
    images_data_transfer_buffer.init(vk_utils::context::get().device(), command_pool, &buffer_alloc_info, 1);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = nullptr;
    begin_info.pInheritanceInfo = nullptr;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(images_data_transfer_buffer[0], &begin_info);

    VkImageMemoryBarrier img_transfer_barrier{};
    img_transfer_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    img_transfer_barrier.pNext = nullptr;
    img_transfer_barrier.image = image;
    img_transfer_barrier.srcAccessMask = 0;
    img_transfer_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    img_transfer_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_transfer_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_transfer_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    img_transfer_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    img_transfer_barrier.subresourceRange.layerCount = 1;
    img_transfer_barrier.subresourceRange.baseArrayLayer = 0;
    img_transfer_barrier.subresourceRange.layerCount = 1;
    img_transfer_barrier.subresourceRange.baseMipLevel = 0;
    img_transfer_barrier.subresourceRange.levelCount = mip_levels;
    img_transfer_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCmdPipelineBarrier(images_data_transfer_buffer[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &img_transfer_barrier);

    VkBufferImageCopy img_copy{};
    img_copy.imageExtent = image_info.extent;
    img_copy.imageOffset = {0, 0, 0};
    img_copy.bufferRowLength = 0;
    img_copy.bufferImageHeight = 0;
    img_copy.bufferOffset = 0;
    img_copy.imageSubresource.mipLevel = 0;
    img_copy.imageSubresource.baseArrayLayer = 0;
    img_copy.imageSubresource.layerCount = 1;
    img_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCmdCopyBufferToImage(images_data_transfer_buffer[0], staging_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &img_copy);
    if (gen_mips) {
        std::vector<VkImageMemoryBarrier> barriers_list{};
        barriers_list.reserve(mip_levels + 1);

        VkImageMemoryBarrier mip_gen_barriers{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }};

        uint32_t mip_width = width;
        uint32_t mip_height = height;

        for (uint32_t i = 1; i < mip_levels; ++i) {
            mip_gen_barriers.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            mip_gen_barriers.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            mip_gen_barriers.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            mip_gen_barriers.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            mip_gen_barriers.subresourceRange.baseMipLevel = i - 1;

            vkCmdPipelineBarrier(
                images_data_transfer_buffer[0],
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &mip_gen_barriers);

            VkImageBlit blit_region{};

            blit_region.srcOffsets[0].x = 0;
            blit_region.srcOffsets[0].y = 0;
            blit_region.srcOffsets[0].z = 0;
            blit_region.srcOffsets[1].x = mip_width;
            blit_region.srcOffsets[1].y = mip_height;
            blit_region.srcOffsets[1].z = 1;

            blit_region.srcSubresource.baseArrayLayer = 0;
            blit_region.srcSubresource.layerCount = 1;
            blit_region.srcSubresource.mipLevel = i - 1;
            blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            mip_width = std::max(1u, mip_width / 2);
            mip_height = std::max(1u, mip_height / 2);

            blit_region.dstOffsets[0].x = 0;
            blit_region.dstOffsets[0].y = 0;
            blit_region.dstOffsets[0].z = 0;
            blit_region.dstOffsets[1].x = mip_width;
            blit_region.dstOffsets[1].y = mip_height;
            blit_region.dstOffsets[1].z = 1;

            blit_region.dstSubresource.baseArrayLayer = 0;
            blit_region.dstSubresource.layerCount = 1;
            blit_region.dstSubresource.mipLevel = i;
            blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            vkCmdBlitImage(
                images_data_transfer_buffer[0],
                image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &blit_region,
                VK_FILTER_LINEAR);
        }
        mip_gen_barriers.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        mip_gen_barriers.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        mip_gen_barriers.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        mip_gen_barriers.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        mip_gen_barriers.subresourceRange.baseMipLevel = 0;
        mip_gen_barriers.subresourceRange.levelCount = mip_levels - 1;

        vkCmdPipelineBarrier(
            images_data_transfer_buffer[0],
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &mip_gen_barriers);

        mip_gen_barriers.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        mip_gen_barriers.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        mip_gen_barriers.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        mip_gen_barriers.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        mip_gen_barriers.subresourceRange.baseMipLevel = mip_levels - 1;
        mip_gen_barriers.subresourceRange.levelCount = 1;

        vkCmdPipelineBarrier(
            images_data_transfer_buffer[0],
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &mip_gen_barriers);
    } else {
        VkImageMemoryBarrier img_barrier{};
        img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        img_barrier.pNext = nullptr;
        img_barrier.image = image;
        img_barrier.srcAccessMask = 0;
        img_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        img_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        img_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        img_barrier.subresourceRange.layerCount = 1;
        img_barrier.subresourceRange.baseArrayLayer = 0;
        img_barrier.subresourceRange.layerCount = 1;
        img_barrier.subresourceRange.baseMipLevel = 0;
        img_barrier.subresourceRange.levelCount = mip_levels;
        img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        vkCmdPipelineBarrier(images_data_transfer_buffer[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0, nullptr, 1, &img_barrier);
    }

    vkEndCommandBuffer(images_data_transfer_buffer[0]);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.pCommandBuffers = images_data_transfer_buffer;
    submit_info.commandBufferCount = 1;

    const auto fence = create_fence();
    vkQueueSubmit(transfer_queue, 1, &submit_info, fence);
    vkWaitForFences(vk_utils::context::get().device(), 1, fence, VK_TRUE, UINT64_MAX);

    out_image = std::move(image);
    out_image_view = std::move(image_view);
    out_image_sampler = std::move(image_sampler);

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_utils::create_buffer(
    vk_utils::vma_buffer_handler& out_buffer,
    VkBufferUsageFlags buffer_usage,
    VmaMemoryUsage memory_usage,
    uint32_t size,
    const void* data)
{
    vk_utils::vma_buffer_handler buffer;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.pNext = nullptr;
    buffer_info.size = size;
    buffer_info.usage = buffer_usage;
    buffer_info.queueFamilyIndexCount = 1;
    const uint32_t family_index = vk_utils::context::get().queue_family_index(vk_utils::context::QUEUE_TYPE_GRAPHICS);
    buffer_info.pQueueFamilyIndices = &family_index;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = memory_usage;
    alloc_info.flags = 0;

    if (const auto err = buffer.init(vk_utils::context::get().allocator(), &buffer_info, &alloc_info); err != VK_SUCCESS) {
        RAISE_ERROR_WARN(err, "cannot init buffer.");
    }

    if (data != nullptr) {
        void* mapped_data;
        vmaMapMemory(vk_utils::context::get().allocator(), buffer, &mapped_data);
        std::memcpy(mapped_data, data, size);
        vmaUnmapMemory(vk_utils::context::get().allocator(), buffer);
        vmaFlushAllocation(
            vk_utils::context::get().allocator(),
            buffer,
            0,
            VK_WHOLE_SIZE);
    }

    out_buffer = std::move(buffer);

    RAISE_ERROR_OK();
}


vk_utils::fence_handler vk_utils::create_fence(VkFenceCreateFlagBits flags)
{
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.pNext = nullptr;
    fence_info.flags = flags;
    vk_utils::fence_handler fence;
    fence.init(vk_utils::context::get().device(), &fence_info);

    return fence;
}


vk_utils::semaphore_handler vk_utils::create_semaphore()
{
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext = nullptr;
    semaphore_info.flags = 0;

    vk_utils::semaphore_handler semaphore;
    semaphore.init(vk_utils::context::get().device(), &semaphore_info);

    return semaphore;
}


bool vk_utils::check_opt_tiling_format(VkFormat req_fmt, VkFormatFeatureFlagBits features_flags)
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(vk_utils::context::get().gpu(), req_fmt, &props);
    return props.optimalTilingFeatures & features_flags;
}


bool vk_utils::check_linear_tiling_format(VkFormat req_fmt, VkFormatFeatureFlagBits features_flags)
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(vk_utils::context::get().gpu(), req_fmt, &props);
    return props.linearTilingFeatures & features_flags;
}


ERROR_TYPE vk_utils::load_shader(
    const char* shader_path,
    vk_utils::shader_module_handler& handle,
    VkShaderStageFlagBits& stage)
{
    struct shader_kind
    {
        shaderc_shader_kind shaderc_kind{};
        const char* shaderc_string{};
        VkShaderStageFlagBits vk_shader_stage{};
    };

    shader_kind curr_shader_kind{};

    static std::unordered_map<const char*, shader_kind> kinds{
        {".vert", {shaderc_shader_kind::shaderc_glsl_vertex_shader, "vs", VK_SHADER_STAGE_VERTEX_BIT}},
        {".frag", {shaderc_shader_kind::shaderc_glsl_fragment_shader, "fs", VK_SHADER_STAGE_FRAGMENT_BIT}},
        {".geom", {shaderc_shader_kind::shaderc_glsl_geometry_shader, "gs", VK_SHADER_STAGE_GEOMETRY_BIT}},
        {".tesc", {shaderc_shader_kind::shaderc_glsl_tess_control_shader, "tesc", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT}},
        {".tese", {shaderc_shader_kind::shaderc_glsl_tess_evaluation_shader, "tese", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT}},
        {".comp", {shaderc_shader_kind::shaderc_glsl_compute_shader, "comp", VK_SHADER_STAGE_COMPUTE_BIT}},
        {".mesh", {shaderc_shader_kind::shaderc_glsl_mesh_shader, "mesh", VK_SHADER_STAGE_MESH_BIT_NV}},
        {".task", {shaderc_shader_kind::shaderc_glsl_task_shader, "task", VK_SHADER_STAGE_TASK_BIT_NV}},
        {".rgen", {shaderc_shader_kind::shaderc_glsl_raygen_shader, "rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR}},
        {".rint", {shaderc_shader_kind::shaderc_glsl_intersection_shader, "rint", VK_SHADER_STAGE_INTERSECTION_BIT_KHR}},
        {".rahit", {shaderc_shader_kind::shaderc_glsl_anyhit_shader, "rahit", VK_SHADER_STAGE_ANY_HIT_BIT_KHR}},
        {".rchit", {shaderc_shader_kind::shaderc_glsl_closesthit_shader, "rchit", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}},
        {".rmiss", {shaderc_shader_kind::shaderc_glsl_miss_shader, "rmiss", VK_SHADER_STAGE_MESH_BIT_NV}},
        {".rcall", {shaderc_shader_kind::shaderc_glsl_callable_shader, "rcall", VK_SHADER_STAGE_CALLABLE_BIT_KHR}}};

    bool shader_kind_found = false;

    for (const auto& [stage_name, kind_val] : kinds) {
        if (strstr(shader_path, stage_name) != nullptr) {
            curr_shader_kind = kind_val;
            shader_kind_found = true;
            break;
        }
    }

    if (!shader_kind_found) {
        RAISE_ERROR_WARN(-1, "cannot find actual shader kind.");
    }

    std::unique_ptr<FILE, std::function<void(FILE*)>> f_handle(
        nullptr, [](FILE* f) { fclose(f); });

    bool is_spv = strstr(shader_path, ".spv") != nullptr;

    if (is_spv) {
        f_handle.reset(fopen(shader_path, "rb"));
    } else {
        f_handle.reset(fopen(shader_path, "r"));
    }

    if (f_handle == nullptr) {
        RAISE_ERROR_WARN(-1, "cannot load shader file.");
    }

    fseek(f_handle.get(), 0L, SEEK_END);
    auto size = ftell(f_handle.get());
    fseek(f_handle.get(), 0L, SEEK_SET);

    if (is_spv) {
        std::vector<char> code(size);
        fread(code.data(), 1, size, f_handle.get());
        PASS_ERROR(create_shader_module(reinterpret_cast<const uint32_t*>(code.data()), code.size(), handle));
    } else {
        static shaderc::Compiler compiler{};

        std::string source;
        source.resize(size);
        fread(source.data(), 1, size, f_handle.get());

        shaderc::CompileOptions options;

#ifndef NDEBUG
        options.SetOptimizationLevel(
            shaderc_optimization_level_zero);
        options.SetGenerateDebugInfo();
#else
        options.SetOptimizationLevel(
            shaderc_optimization_level_performance);
#endif
        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, curr_shader_kind.shaderc_kind, curr_shader_kind.shaderc_string, options);
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            LOG_ERROR(result.GetErrorMessage());
            RAISE_ERROR_WARN(-1, "cannot load shader file.");
        }

        std::vector<uint32_t> shaderSPRV;
        shaderSPRV.assign(result.begin(), result.end());
        PASS_ERROR(create_shader_module(shaderSPRV.data(), shaderSPRV.size() * sizeof(uint32_t), handle));
    }

    stage = curr_shader_kind.vk_shader_stage;
    RAISE_ERROR_OK();
}


ERROR_TYPE vk_utils::create_shader_module(
    const uint32_t* code,
    uint32_t code_size,
    vk_utils::shader_module_handler& handle)
{
    VkShaderModuleCreateInfo shader_module_info{};
    shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_info.pNext = nullptr;
    shader_module_info.pCode = code;
    shader_module_info.codeSize = code_size;

    vk_utils::shader_module_handler module;

    if (module.init(vk_utils::context::get().device(), &shader_module_info) != VK_SUCCESS) {
        RAISE_ERROR_WARN(-1, "cannot create shader module.");
    }

    handle = std::move(module);

    RAISE_ERROR_OK();
}
