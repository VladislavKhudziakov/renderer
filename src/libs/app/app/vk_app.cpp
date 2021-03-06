

#include "vk_app.hpp"

#include <vk_utils/context.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>

using namespace app;

namespace
{
    const char* device_extensions[]{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

vk_app::vk_app(const char* app_name)
    : m_app_info{}
{
    m_app_info.app_name = app_name;
}

ERROR_TYPE vk_app::run(int argc, const char** argv)
{
    m_app_info.argc = argc;
    m_app_info.argv = argv;

    return base_app::run(argc, argv);
}

ERROR_TYPE vk_app::init_window()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(800, 600, m_app_info.app_name.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(m_window, this);

    glfwSetWindowSizeCallback(m_window, window_resized_callback);
    glfwSetMouseButtonCallback(m_window, mouse_button_callback);
    glfwSetCursorPosCallback(m_window, cursor_pos_callback);
    glfwSetCursorEnterCallback(m_window, cursor_enter_callback);
    glfwSetScrollCallback(m_window, scroll_callback);
    glfwSetWindowFocusCallback(m_window, window_focus_callback);
    glfwSetWindowMaximizeCallback(m_window, window_maximized_callback);

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
        
    if (m_frame_state & WINDOW_ZERO_SIZE_BIT) {
        continue;
    }

        HANDLE_ERROR(draw_frame());
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

        if (m_swapchain_data.swapchain_info->imageExtent.height == 0 || m_swapchain_data.swapchain_info->imageExtent.width == 0) {
            m_frame_state |= WINDOW_ZERO_SIZE_BIT;
            RAISE_ERROR_WARN(WINDOW_ZERO_SIZE_BIT, "cannot create swapchain in zero sized window");
        } else {
            m_frame_state &= ~WINDOW_ZERO_SIZE_BIT;
        }

        if (m_surface_capabilities.maxImageCount == 0) {
            m_swapchain_data.swapchain_info->minImageCount = m_surface_capabilities.minImageCount + 1;
        } else {
            m_swapchain_data.swapchain_info->minImageCount =
                std::clamp(
                    m_surface_capabilities.minImageCount + 1,
                    m_surface_capabilities.minImageCount,
                    m_surface_capabilities.maxImageCount);
        }

        if (m_swapchain_data.swapchain.reset(vk_utils::context::get().device(), &*m_swapchain_data.swapchain_info) != VK_SUCCESS) {
            RAISE_ERROR_FATAL(-1, "cannot reset swapchain.");
        }

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

    if (m_swapchain_data.swapchain.init(vk_utils::context::get().device(), &*m_swapchain_data.swapchain_info)) {
        RAISE_ERROR_WARN(-1, "cannot create swapchain.");
    }

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_app::request_swapchain_images()
{
    if (m_frame_state & WINDOW_ZERO_SIZE_BIT) {
        RAISE_ERROR_OK();
    }

    uint32_t images_count;
    vkGetSwapchainImagesKHR(vk_utils::context::get().device(), m_swapchain_data.swapchain, &images_count, nullptr);

    m_swapchain_data.swapchain_images.clear();
    m_swapchain_data.swapchain_images.resize(images_count);

    std::vector<vk_utils::image_view_handler> image_views{};
    image_views.reserve(images_count);
    
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
        auto& img_view = image_views.emplace_back();

        if (img_view.init(vk_utils::context::get().device(), &img_view_create_info) != VK_SUCCESS) {
            RAISE_ERROR_FATAL(-1, "cannot init swapchain image view.");
        }
    }

    m_swapchain_data.swapchain_images_views = std::move(image_views);

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

ERROR_TYPE app::vk_app::on_window_size_changed(int w, int h)
{
    if (m_frame_state & WINDOW_ZERO_SIZE_BIT && (w != 0 && h != 0)) {
        HANDLE_ERROR(create_swapchain());
        HANDLE_ERROR(request_swapchain_images());
        HANDLE_ERROR(on_swapchain_recreated());
    }

    RAISE_ERROR_OK();
}


ERROR_TYPE app::vk_app::on_window_focus_changed(int focused)
{
    RAISE_ERROR_OK();
}


ERROR_TYPE app::vk_app::on_window_maximized(int maximized)
{
    RAISE_ERROR_OK();
}


void vk_app::window_resized_callback(GLFWwindow* window, int w, int h)
{
    auto self = reinterpret_cast<vk_app*>(glfwGetWindowUserPointer(window));
    HANDLE_ERROR(self->on_window_size_changed(w, h));
}


void app::vk_app::mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    auto self = reinterpret_cast<vk_app*>(glfwGetWindowUserPointer(window));
    HANDLE_ERROR(self->on_mouse_button(button, action, mods));
}


void app::vk_app::cursor_pos_callback(GLFWwindow* window, double x, double y)
{
    auto self = reinterpret_cast<vk_app*>(glfwGetWindowUserPointer(window));
    HANDLE_ERROR(self->on_mouse_moved(static_cast<uint64_t>(x), static_cast<uint64_t>(y)));
}


void app::vk_app::cursor_enter_callback(GLFWwindow* window, int entered)
{
    auto self = reinterpret_cast<vk_app*>(glfwGetWindowUserPointer(window));
    HANDLE_ERROR(self->on_mouse_enter(static_cast<bool>(entered)));
}


void app::vk_app::scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto self = reinterpret_cast<vk_app*>(glfwGetWindowUserPointer(window));
    HANDLE_ERROR(self->on_mouse_scroll(xoffset, yoffset));
}


void app::vk_app::window_focus_callback(GLFWwindow* window, int focused)
{
    auto self = reinterpret_cast<vk_app*>(glfwGetWindowUserPointer(window));
    HANDLE_ERROR(self->on_window_focus_changed(focused));
}



void app::vk_app::window_maximized_callback(GLFWwindow* window, int maximized)
{
    auto self = reinterpret_cast<vk_app*>(glfwGetWindowUserPointer(window));
    HANDLE_ERROR(self->on_window_maximized(maximized));
}


ERROR_TYPE vk_app::begin_frame()
{
    VkResult result;
    size_t acquire_image_tries{0};

    do {
        result = vkAcquireNextImageKHR(
            vk_utils::context::get().device(),
            m_swapchain_data.swapchain,
            UINT64_MAX,
            m_swapchain_data.image_acquired_semaphores[m_swapchain_data.current_frame],
            nullptr,
            &m_swapchain_data.current_image);

        if (result != VK_SUCCESS) {
            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                vkDeviceWaitIdle(vk_utils::context::get().device());
                PASS_ERROR(create_swapchain());
                HANDLE_ERROR(request_swapchain_images());
                HANDLE_ERROR(on_swapchain_recreated());
            } else {
                RAISE_ERROR_FATAL(-1, "cannot acquire image.");
            }
        }
        acquire_image_tries++;
    } while (result != VK_SUCCESS && acquire_image_tries < 100);

    if (acquire_image_tries >= 100) {
        RAISE_ERROR_FATAL(-1, "image acquire tries exceed.");
    }

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_app::finish_frame(VkCommandBuffer cmd_buffer)
{
    if (m_swapchain_data.frames_in_flight_fences[m_swapchain_data.current_image] != nullptr) {
        vkWaitForFences(
            vk_utils::context::get().device(),
            1,
            &m_swapchain_data.frames_in_flight_fences[m_swapchain_data.current_image],
            true,
            UINT64_MAX);
    }

    m_swapchain_data.frames_in_flight_fences[m_swapchain_data.current_image] = m_swapchain_data.render_finished_fences[m_swapchain_data.current_frame];

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

    VkResult result{};
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

    if (result != VK_SUCCESS) {
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            vkDeviceWaitIdle(vk_utils::context::get().device());
            PASS_ERROR(create_swapchain());
            HANDLE_ERROR(request_swapchain_images());
            HANDLE_ERROR(on_swapchain_recreated());
            RAISE_ERROR_OK();
        } else {
            RAISE_ERROR_WARN(-1, "queue present failed.");
        }
    }

    m_swapchain_data.current_frame = (m_swapchain_data.current_frame + 1) % m_swapchain_data.frames_count;

    RAISE_ERROR_OK();
}
