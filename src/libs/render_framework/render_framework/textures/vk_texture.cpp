#include "vk_texture.hpp"

#include <vk_utils/context.hpp>


using namespace render_framework;
using namespace detail;

detail::vk_texture_impl::vk_texture_impl(
    vk_utils::vma_image_handler image,
    vk_utils::image_view_handler image_view,
    vk_utils::sampler_handler image_sampler)
    : m_image(std::move(image))
    , m_image_view(std::move(image_view))
    , m_image_sampler(std::move(image_sampler))
{
}


VkImage vk_texture_impl::get_image() const
{
    return m_image;
}


VkImageView vk_texture_impl::get_image_view() const
{
    return m_image_view;
}


VkSampler vk_texture_impl::get_sampler() const
{
    return m_image_sampler;
}


vk_texture_builder::vk_texture_builder(uint32_t queue_family)
{
    set_queue_family(queue_family);
}


vk_texture_builder& vk_texture_builder::set_command_pool(VkCommandPool command_pool)
{
    m_command_pool = command_pool;
    return *this;
}


vk_texture_builder& vk_texture_builder::set_queue(VkQueue queue)
{
    m_queue = queue;
    return *this;
}


vk_texture_builder& vk_texture_builder::set_command_buffer(VkCommandBuffer command_buffer)
{
    m_command_buffer = command_buffer;
    return *this;
}


vk_texture_builder& vk_texture_builder::set_queue_family(uint32_t queue_family)
{
    m_queue_family = queue_family;
    return *this;
}


ERROR_TYPE vk_texture_builder::create(texture& result)
{
    if (m_queue_family == -1) {
        RAISE_ERROR_WARN(-1, "invalid queue family.");
    }

    vk_utils::cmd_buffers_handler cmd_buffer{};

    if (m_command_buffer == nullptr) {
        if (m_queue == nullptr) {
            RAISE_ERROR_WARN(-1, "not queue, nor command buffer wasn't passed.");
        }

        if (m_command_pool == nullptr) {
            RAISE_ERROR_WARN(-1, "command pool wasn't passed.");
        }

        VkCommandBufferAllocateInfo cmd_buffer_alloc_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_command_pool,
            .level  = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        cmd_buffer.init(vk_utils::context::get().device(), m_command_pool, &cmd_buffer_alloc_info, 1);
        m_command_buffer = cmd_buffer[0];
    }

    vk_utils::vma_image_handler image{};
    vk_utils::image_view_handler image_view{};
    vk_utils::sampler_handler image_sampler{};

    PASS_ERROR(create_vk_texture(image, image_view, image_sampler, m_queue_family, m_command_buffer));

    if (m_queue == nullptr) {
        m_command_buffer = nullptr;
        result = texture::create<vk_texture_impl>(std::move(image), std::move(image_view), std::move(image_sampler));
        RAISE_ERROR_OK();
    }

    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .commandBufferCount = 1,
        .pCommandBuffers = &m_command_buffer,
        .signalSemaphoreCount = 0
    };

    vkQueueSubmit(m_queue, 1, &submit_info, nullptr);
    vkQueueWaitIdle(m_queue);

    if (cmd_buffer.handlers_count() > 0) {
        m_command_buffer = nullptr;
    }

    result = texture::create<vk_texture_impl>(std::move(image), std::move(image_view), std::move(image_sampler));

    RAISE_ERROR_OK();
}
