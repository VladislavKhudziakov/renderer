

#include "vk_app.hpp"

#include <GLFW/glfw3.h>
#include <vk_utils/context.hpp>

using namespace app;

namespace
{
    const char* device_extensions[]{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

vk_app::vk_app(const char* app_name, int argc, const char** argv)
    : m_app_info{}
{
    m_app_info.app_name = app_name;
    m_app_info.argc = argc;
    m_app_info.argv = argv;
}

ERROR_TYPE vk_app::run()
{
    return base_app::run();
}

ERROR_TYPE vk_app::init_window()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(800, 600, m_app_info.app_name.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(m_window, this);

    glfwSetWindowSizeCallback(m_window, window_resized_callback);

    if (m_window == nullptr) {
        RAISE_ERROR_FATAL(-1, "Failed to create window.");
    }

    PASS_ERROR(init_vulkan());
    RAISE_ERROR_OK();
}


ERROR_TYPE vk_app::run_main_loop()
{
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        draw_frame();
    }

    vkDeviceWaitIdle(vk_utils::context::get().device());

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_app::cleanup()
{
    RAISE_ERROR_OK();
}


ERROR_TYPE vk_app::init_vulkan()
{
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions;
    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    m_app_info.instance_extensions.assign(glfw_extensions, glfw_extensions + glfw_extension_count);

    m_app_info.device_extensions.assign(std::begin(device_extensions), std::end(device_extensions));

    vk_utils::context::context_init_info context_info{};
    context_info.required_instance_extensions.names = m_app_info.instance_extensions.data();
    context_info.required_instance_extensions.count = m_app_info.instance_extensions.size();
    context_info.required_device_extensions.names = m_app_info.device_extensions.data();
    context_info.required_device_extensions.count = m_app_info.device_extensions.size();

    context_info.surface_create_callback = [this](VkInstance instance, VkSurfaceKHR* surface) {
        auto res = glfwCreateWindowSurface(instance, m_window, nullptr, surface);
        return res;
    };

    PASS_ERROR(vk_utils::context::init(m_app_info.app_name.c_str(), context_info));
    PASS_ERROR(create_swapchain());
    PASS_ERROR(request_swapchain_images());

    m_swapchain_data.image_acquired_semaphores.resize(m_swapchain_data.swapchain_images.size());
    m_swapchain_data.render_finished_semaphores.resize(m_swapchain_data.swapchain_images.size());
    m_swapchain_data.render_finished_fences.resize(m_swapchain_data.swapchain_images.size());

    VkSemaphoreCreateInfo semaphores_info{};
    semaphores_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphores_info.pNext = nullptr;
    semaphores_info.flags = 0;

    VkFenceCreateInfo fences_info{};
    fences_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fences_info.pNext = nullptr;
    fences_info.flags = 0;

    for (size_t i = 0; i < m_swapchain_data.swapchain_images.size(); ++i) {
        m_swapchain_data.image_acquired_semaphores[i].init(vk_utils::context::get().device(), &semaphores_info);
        m_swapchain_data.render_finished_semaphores[i].init(vk_utils::context::get().device(), &semaphores_info);
        m_swapchain_data.render_finished_fences[i].init(vk_utils::context::get().device(), &fences_info);
    }

    m_swapchain_data.frames_in_flight_fences.resize(m_swapchain_data.swapchain_images.size(), nullptr);
    m_swapchain_data.frames_count = m_swapchain_data.swapchain_images.size();

    PASS_ERROR(on_vulkan_initialized());

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_app::create_swapchain()
{
    if (m_swapchain_data.swapchain_info.has_value()) {
        m_swapchain_data.swapchain_info->oldSwapchain = m_swapchain_data.swapchain;
        m_swapchain_data.swapchain_info->imageExtent = get_surface_images_extent();
        m_swapchain_data.swapchain_info->preTransform =
            m_surface_capabilities.currentTransform;

        if (m_surface_capabilities.maxImageCount == 0) {
            m_swapchain_data.swapchain_info->minImageCount = m_surface_capabilities.minImageCount + 1;
        } else {
            m_swapchain_data.swapchain_info->minImageCount =
                std::clamp(
                    m_surface_capabilities.minImageCount + 1,
                    m_surface_capabilities.minImageCount,
                    m_surface_capabilities.maxImageCount);
        }

        PASS_ERROR(errors::handle_error_code(
            m_swapchain_data.swapchain.reset(vk_utils::context::get().device(), &*m_swapchain_data.swapchain_info)));

        RAISE_ERROR_OK();
    }

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_utils::context::get().gpu(), vk_utils::context::get().surface(), &m_surface_capabilities);

    uint32_t surface_formats_count{};
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_utils::context::get().gpu(), vk_utils::context::get().surface(), &surface_formats_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(surface_formats_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        vk_utils::context::get().gpu(), vk_utils::context::get().surface(), &surface_formats_count, formats.data());

    uint32_t surface_present_modes_count{};
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        vk_utils::context::get().gpu(), vk_utils::context::get().surface(), &surface_present_modes_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(surface_present_modes_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        vk_utils::context::get().gpu(), vk_utils::context::get().surface(), &surface_present_modes_count, present_modes.data());

    if (!m_swapchain_data.swapchain_info.has_value()) {
        m_swapchain_data.swapchain_info.emplace();
    }

    m_swapchain_data.swapchain_info->sType =
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    m_swapchain_data.swapchain_info->pNext = nullptr;
    m_swapchain_data.swapchain_info->oldSwapchain = nullptr;
    m_swapchain_data.swapchain_info->surface = vk_utils::context::get().surface();
    m_swapchain_data.swapchain_info->preTransform =
        m_surface_capabilities.currentTransform;

    auto select_format = [this, &formats]() {
        const VkSurfaceFormatKHR* last_success_color_space_fmt = nullptr;
        for (const auto& fmt : formats) {
            if (fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                last_success_color_space_fmt = &fmt;

                auto success_fmt = fmt.format == VK_FORMAT_R8G8B8A8_SRGB || fmt.format == VK_FORMAT_B8G8R8A8_SRGB;
                if (success_fmt) {
                    m_swapchain_data.swapchain_info->imageColorSpace = fmt.colorSpace;
                    m_swapchain_data.swapchain_info->imageFormat = fmt.format;
                    return;
                }
            }
        }

        if (last_success_color_space_fmt != nullptr) {
            m_swapchain_data.swapchain_info->imageColorSpace =
                last_success_color_space_fmt->colorSpace;
            m_swapchain_data.swapchain_info->imageFormat =
                last_success_color_space_fmt->format;
        } else {
            m_swapchain_data.swapchain_info->imageColorSpace = formats.front().colorSpace;
            m_swapchain_data.swapchain_info->imageFormat = formats.front().format;
        }
    };

    auto select_present_mode = [this, present_modes]() {
        if (std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != present_modes.end()) {
            m_swapchain_data.swapchain_info->presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        } else {
            m_swapchain_data.swapchain_info->presentMode = VK_PRESENT_MODE_FIFO_KHR;
        }
    };

    select_format();
    select_present_mode();

    m_swapchain_data.swapchain_info->imageExtent = get_surface_images_extent();
    if (m_surface_capabilities.maxImageCount == 0) {
        m_swapchain_data.swapchain_info->minImageCount = m_surface_capabilities.minImageCount + 1;
    } else {
        m_swapchain_data.swapchain_info->minImageCount =
            std::clamp(m_surface_capabilities.minImageCount + 1, m_surface_capabilities.minImageCount, m_surface_capabilities.maxImageCount);
    }

    m_swapchain_data.swapchain_info->imageArrayLayers = 1;
    m_swapchain_data.swapchain_info->imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // VK_IMAGE_USAGE_TRANSFER_DST_BIT
    m_swapchain_data.swapchain_info->imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    m_swapchain_data.swapchain_info->compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    m_swapchain_data.swapchain_info->clipped = VK_TRUE;

    PASS_ERROR(errors::handle_error_code(
        m_swapchain_data.swapchain.init(vk_utils::context::get().device(), &*m_swapchain_data.swapchain_info)));

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_app::request_swapchain_images()
{
    uint32_t images_count;
    vkGetSwapchainImagesKHR(vk_utils::context::get().device(), m_swapchain_data.swapchain, &images_count, nullptr);

    m_swapchain_data.swapchain_images.clear();
    m_swapchain_data.swapchain_images_views.clear();

    m_swapchain_data.swapchain_images.resize(images_count);
    m_swapchain_data.swapchain_images_views.reserve(images_count);

    vkGetSwapchainImagesKHR(
        vk_utils::context::get().device(),
        m_swapchain_data.swapchain,
        &images_count,
        m_swapchain_data.swapchain_images.data());

    VkImageViewCreateInfo img_view_create_info{};
    img_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    img_view_create_info.pNext = nullptr;
    img_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    img_view_create_info.components.r = VK_COMPONENT_SWIZZLE_R;
    img_view_create_info.components.g = VK_COMPONENT_SWIZZLE_G;
    img_view_create_info.components.b = VK_COMPONENT_SWIZZLE_B;
    img_view_create_info.components.a = VK_COMPONENT_SWIZZLE_A;
    img_view_create_info.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    img_view_create_info.subresourceRange.baseArrayLayer = 0;
    img_view_create_info.subresourceRange.layerCount = 1;
    img_view_create_info.subresourceRange.baseMipLevel = 0;
    img_view_create_info.subresourceRange.levelCount = 1;
    img_view_create_info.format = m_swapchain_data.swapchain_info->imageFormat;

    for (int i = 0; i < images_count; ++i) {
        img_view_create_info.image = m_swapchain_data.swapchain_images[i];
        auto& img_view = m_swapchain_data.swapchain_images_views.emplace_back();
        HANDLE_ERROR(errors::handle_error_code(
            img_view.init(vk_utils::context::get().device(), &img_view_create_info),
            [this](int32_t code) {
                m_swapchain_data.swapchain_images.clear();
                m_swapchain_data.swapchain_images_views.clear();
                return true;
            }));
    }

    RAISE_ERROR_OK();
}


VkExtent2D vk_app::get_surface_images_extent()
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_utils::context::get().gpu(), vk_utils::context::get().surface(), &m_surface_capabilities);
    if (m_surface_capabilities.currentExtent.width != UINT32_MAX) {
        return m_surface_capabilities.currentExtent;
    } else {
        int w, h;
        glfwGetWindowSize(m_window, &w, &h);
        return {
            std::clamp(uint32_t(w), m_surface_capabilities.minImageExtent.width, m_surface_capabilities.maxImageExtent.width),
            std::clamp(uint32_t(h), m_surface_capabilities.minImageExtent.height, m_surface_capabilities.maxImageExtent.height),
        };
    }
}


void vk_app::window_resized_callback(GLFWwindow* window, int w, int h)
{
    auto self = reinterpret_cast<vk_app*>(glfwGetWindowUserPointer(window));
    self->on_window_size_changed(w, h);
}


ERROR_TYPE vk_app::begin_frame()
{
    auto result = vkAcquireNextImageKHR(
        vk_utils::context::get().device(),
        m_swapchain_data.swapchain,
        UINT64_MAX,
        m_swapchain_data.image_acquired_semaphores[m_swapchain_data.current_frame],
        nullptr,
        &m_swapchain_data.current_image);

    while (result != VK_SUCCESS) {
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            vkDeviceWaitIdle(vk_utils::context::get().device());
            HANDLE_ERROR(create_swapchain());
            HANDLE_ERROR(request_swapchain_images());
            HANDLE_ERROR(on_swapchain_recreated());
            result = vkAcquireNextImageKHR(
                vk_utils::context::get().device(),
                m_swapchain_data.swapchain,
                UINT64_MAX,
                m_swapchain_data.image_acquired_semaphores[m_swapchain_data.current_frame],
                nullptr,
                &m_swapchain_data.current_image);
        } else {
            RAISE_ERROR_FATAL(-1, "cannot acquire image.");
        }
    }

    if (m_swapchain_data.frames_in_flight_fences[m_swapchain_data.current_image] != nullptr) {
        vkWaitForFences(
            vk_utils::context::get().device(),
            1,
            &m_swapchain_data.frames_in_flight_fences[m_swapchain_data.current_image],
            true,
            UINT64_MAX);
    }

    m_swapchain_data.frames_in_flight_fences[m_swapchain_data.current_image] = m_swapchain_data.render_finished_fences[m_swapchain_data.current_frame];
}


ERROR_TYPE vk_app::finish_frame(VkCommandBuffer cmd_buffer)
{
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    VkPipelineStageFlags stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit_info.pWaitDstStageMask = &stage_mask;
    submit_info.pWaitSemaphores = m_swapchain_data.image_acquired_semaphores[m_swapchain_data.current_frame];
    submit_info.waitSemaphoreCount = 1;
    submit_info.pSignalSemaphores = m_swapchain_data.render_finished_semaphores[m_swapchain_data.current_frame];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;
    submit_info.commandBufferCount = 1;

    vkResetFences(
        vk_utils::context::get().device(),
        1,
        m_swapchain_data.render_finished_fences[m_swapchain_data.current_frame]);

    vkQueueSubmit(
        vk_utils::context::get().queue(vk_utils::context::QUEUE_TYPE_GRAPHICS),
        1,
        &submit_info,
        m_swapchain_data.render_finished_fences[m_swapchain_data.current_frame]);

    VkResult result;
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = nullptr;

    present_info.pImageIndices = &m_swapchain_data.current_image;

    present_info.pSwapchains = m_swapchain_data.swapchain;
    present_info.swapchainCount = 1;

    present_info.pWaitSemaphores = m_swapchain_data.render_finished_semaphores[m_swapchain_data.current_frame];
    present_info.waitSemaphoreCount = 1;
    present_info.pResults = &result;

    vkQueuePresentKHR(vk_utils::context::get().queue(vk_utils::context::QUEUE_TYPE_GRAPHICS), &present_info);

    PASS_ERROR(errors::handle_error_code(
        result,
        [this](int32_t err_code) {
            if (err_code == VK_ERROR_OUT_OF_DATE_KHR || err_code == VK_SUBOPTIMAL_KHR) {
                vkDeviceWaitIdle(vk_utils::context::get().device());
                create_swapchain();
                request_swapchain_images();
                HANDLE_ERROR(on_swapchain_recreated());
                return false;
            }
            return true;
        }));

    m_swapchain_data.current_frame = (m_swapchain_data.current_frame + 1) % m_swapchain_data.frames_count;
}
