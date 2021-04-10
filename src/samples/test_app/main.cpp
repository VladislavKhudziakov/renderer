
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <app/base_app.hpp>
#include <logger/log.hpp>
#include <vk_utils/context.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

#include <shaderc/shaderc.hpp>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

const char* shaders_paths_spv[]{
    "./shaders/triangle/triangle.vert.glsl.spv",
    "./shaders/triangle/triangle.frag.glsl.spv",
};


const char* shaders_paths_glsl[]{
    "./shaders/triangle/triangle.vert.glsl",
    "./shaders/triangle/triangle.frag.glsl",
};

vk_utils::pipeline_layout_handler g_pipeline_layout;
vk_utils::graphics_pipeline_handler g_pipeline;
vk_utils::cmd_buffers_handler g_draw_cmd_buffers;
vk_utils::cmd_buffers_handler g_data_copy_cmd_buffers;
vk_utils::cmd_pool_handler g_cmd_pool;

vk_utils::vma_buffer_handler g_vertex_buffer;
vk_utils::vma_buffer_handler g_vertex_staging_buffer;
vk_utils::vma_buffer_handler g_index_buffer;
vk_utils::vma_buffer_handler g_index_staging_buffer;

std::vector<vk_utils::vma_buffer_handler> g_global_uniform_buffer;

vk_utils::descriptor_set_layout_handler g_ubo_descriptor_set_layout;
vk_utils::descriptor_set_layout_handler g_texture_descriptor_set_layout;


vk_utils::descriptor_pool_handler g_descriptor_pool;
vk_utils::descriptor_set_handler g_ubo_descriptor_sets;
vk_utils::descriptor_set_handler g_texture_descriptor_sets;

vk_utils::shader_module_handler g_shader_modules[std::size(shaders_paths_spv)];

vk_utils::vma_image_handler g_shader_image;
vk_utils::image_view_handler g_shader_img_view;
vk_utils::sampler_handler g_shader_sampler;

const char* validation_layers[]{"VK_LAYER_KHRONOS_validation"};

const char* instance_extensions[]{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
const char* device_extensions[]{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

class hello_triangle_app : public app::base_app
{
public:
    using event_callback = std::function<int32_t(const hello_triangle_app* app)>;
    using update_callback = std::function<int32_t(const hello_triangle_app* app, const VkCommandBuffer**, uint32_t*)>;

    void set_on_create_callback(event_callback cb)
    {
        m_create_callback = std::move(cb);
    }
    void set_on_destroy_callback(event_callback cb)
    {
        m_destroy_callback = std::move(cb);
    }
    void set_on_resize_callback(event_callback cb)
    {
        m_resize_callback = std::move(cb);
    }
    void set_on_update_callback(update_callback cb)
    {
        m_upd_callback = std::move(cb);
    }

    VkExtent2D get_window_size() const
    {
        return m_swapchain_create_info->imageExtent;
    }

    VkSwapchainKHR get_swapchain() const
    {
        return m_swapchain;
    }

    VkRenderPass get_main_pass() const
    {
        return m_pass;
    }

    std::vector<VkFramebuffer> get_main_framebuffers() const
    {
        std::vector<VkFramebuffer> res;
        res.reserve(m_framebuffers.size());
        std::copy(m_framebuffers.begin(), m_framebuffers.end(), std::back_inserter(res));
        return res;
    }

    uint32_t get_current_swapchain_img_index() const
    {
        return m_image_index;
    }

protected:
    errors::error init_window() override
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        m_window = glfwCreateWindow(800, 600, "Vulkan window", nullptr, nullptr);
        glfwSetWindowUserPointer(m_window, this);

        glfwSetWindowSizeCallback(m_window, on_window_resized);

        if (m_window == nullptr) {
            return ERROR(-1, "failed to create window");
        }

        if (const auto err_code = init_vulkan(); err_code != 0) {
            return ERROR(-1, "failed to init vulkan");
        }

        return errors::OK;
    }

    errors::error run_main_loop() override
    {
        if (m_create_callback) {
            if (auto err_code = m_create_callback(this); err_code != VK_SUCCESS) {
                return ERROR(err_code, "create cb failed");
            }
        }

        const auto frames_in_flight = m_swapchain_images.size();

        std::vector<vk_utils::semaphore_handler> image_acquired_semaphores{frames_in_flight};
        std::vector<vk_utils::semaphore_handler> render_finished_semaphores{frames_in_flight};
        std::vector<vk_utils::fence_handler> render_finished_fences{frames_in_flight};
        std::vector<VkFence> frames_in_flight_fences{m_swapchain_images.size(), nullptr};

        uint32_t curr_frame{0};

        auto create_sync_objects = [&image_acquired_semaphores, &render_finished_semaphores, &render_finished_fences, &frames_in_flight_fences]() {
            VkFenceCreateInfo fence_info{};
            fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fence_info.pNext = nullptr;

            VkSemaphoreCreateInfo semaphore_info{};
            semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphore_info.pNext = nullptr;

            for (auto& s : image_acquired_semaphores) {
                s.init(vk_utils::context::get().device(), &semaphore_info);
            }
            for (auto& s : render_finished_semaphores) {
                s.init(vk_utils::context::get().device(), &semaphore_info);
            }
            for (auto& f : render_finished_fences) {
                f.init(vk_utils::context::get().device(), &fence_info);
            }

            for (auto& f : frames_in_flight_fences) {
                f = VK_NULL_HANDLE;
            }
        };

        create_sync_objects();

        auto recreate_swapchain = [this]() {
            vkQueueWaitIdle(vk_utils::context::get().queue(vk_utils::context::QUEUE_TYPE_GRAPHICS));
            vkDeviceWaitIdle(vk_utils::context::get().device());

            create_swapchain();
            request_swapchain_images();

            if (m_resize_callback) {
                m_resize_callback(this);
            }
        };

        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();

            if (m_resized) {
                recreate_swapchain();
                m_resized = false;
            }

            if (frames_in_flight_fences[m_image_index] != nullptr) {
                VkFence f[] = {frames_in_flight_fences[m_image_index]};
                vkWaitForFences(vk_utils::context::get().device(), 1, f, VK_TRUE, UINT64_MAX);
            }

            VkResult res{static_cast<VkResult>(-1)};

            while (res != VK_SUCCESS) {
                res = vkAcquireNextImageKHR(vk_utils::context::get().device(), m_swapchain, UINT64_MAX, image_acquired_semaphores[curr_frame], nullptr, &m_image_index);
                if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
                    LOG_WARN("vkAcquireNextImageKHR failed.");
                    recreate_swapchain();
                } else if (res == VK_ERROR_DEVICE_LOST) {
                    return ERROR_FATAL(VK_ERROR_DEVICE_LOST, "device lost occured in vkAcquireNextImageKHR");
                }
            }

            frames_in_flight_fences[m_image_index] = render_finished_fences[curr_frame];

            const VkCommandBuffer* cmd_buffers = nullptr;
            uint32_t cmd_buffers_count = 0;

            if (m_upd_callback) {
                if (auto err_code = m_upd_callback(this, &cmd_buffers, &cmd_buffers_count); err_code != VK_SUCCESS) {
                    LOG_ERROR("update failed.");
                }
            }

            VkSubmitInfo submit_info{};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.pNext = nullptr;
            VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            submit_info.pSignalSemaphores = render_finished_semaphores[curr_frame];
            submit_info.signalSemaphoreCount = 1;
            submit_info.pWaitSemaphores = image_acquired_semaphores[curr_frame];
            submit_info.waitSemaphoreCount = 1;
            submit_info.pWaitDstStageMask = &wait_stages;
            submit_info.pCommandBuffers = cmd_buffers;
            submit_info.commandBufferCount = 1;


            if (vkResetFences(vk_utils::context::get().device(), 1, render_finished_fences[curr_frame]) != VK_SUCCESS) {
                LOG_ERROR("reset fences failed.");
            }

            if (cmd_buffers != nullptr && cmd_buffers_count > 0) {
                res = vkQueueSubmit(vk_utils::context::get().queue(vk_utils::context::QUEUE_TYPE_GRAPHICS), 1, &submit_info, render_finished_fences[curr_frame]);
                if (res != VK_SUCCESS) {
                    return ERROR_FATAL(res, "error occured in vkQueuePresentKHR");
                }
            }

            VkResult present_res{VK_SUCCESS};

            VkSwapchainKHR swapchain[] = {m_swapchain};
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.pNext = nullptr;
            presentInfo.pWaitSemaphores = render_finished_semaphores[curr_frame];
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pSwapchains = swapchain;
            presentInfo.swapchainCount = std::size(swapchain);
            presentInfo.pImageIndices = &m_image_index;
            presentInfo.pResults = &present_res;

            res = static_cast<VkResult>(-1);

            if (vkQueuePresentKHR(vk_utils::context::get().queue(vk_utils::context::QUEUE_TYPE_PRESENT), &presentInfo) != VK_SUCCESS || present_res != VK_SUCCESS) {
                if (present_res == VK_ERROR_OUT_OF_DATE_KHR || present_res == VK_SUBOPTIMAL_KHR) {
                    LOG_WARN("vkQueuePresentKHR failed.");
                    recreate_swapchain();
                    //ok
                } else if (present_res == VK_ERROR_DEVICE_LOST) {
                    return ERROR_FATAL(VK_ERROR_DEVICE_LOST, "device lost occured in vkQueuePresentKHR");
                }
            }

            ++curr_frame;
            curr_frame %= frames_in_flight;
        }

        vkDeviceWaitIdle(vk_utils::context::get().device());

        return errors::OK;
    }

    errors::error cleanup() override
    {
        if (m_destroy_callback) {
            if (auto err_code = m_destroy_callback(this); err_code != VK_SUCCESS) {
                return ERROR(err_code, "destroy failed.");
            }
        }

        m_pass.destroy();
        m_swapchain_img_views.clear();
        m_framebuffers.clear();
        m_swapchain.destroy();

        glfwDestroyWindow(m_window);
        glfwTerminate();
        return errors::OK;
    }

private:
    static void on_window_resized(GLFWwindow* window, int w, int h)
    {
        auto self = reinterpret_cast<hello_triangle_app*>(glfwGetWindowUserPointer(window));
        self->m_resized = true;
    }


    int32_t init_vulkan()
    {
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions;
        glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        std::vector<const char*> instance_ext_list{glfw_extensions, glfw_extensions + glfw_extension_count};
        std::copy(std::begin(instance_extensions), std::end(instance_extensions), std::back_inserter(instance_ext_list));

        std::vector<const char*> device_ext_list{std::begin(device_extensions), std::end(device_extensions)};

        vk_utils::context::context_init_info context_info{};
        context_info.required_instance_extensions.names = instance_ext_list.data();
        context_info.required_instance_extensions.count = instance_ext_list.size();
        context_info.required_device_extensions.names = device_ext_list.data();
        context_info.required_device_extensions.count = device_ext_list.size();

        context_info.surface_create_callback = [this](VkInstance instance, VkSurfaceKHR* surface) {
            auto res = glfwCreateWindowSurface(instance, m_window, nullptr, surface);
            return res;
        };

        errors::handle_error(vk_utils::context::init("hello triangle", context_info));

        if (const auto err_code = create_swapchain();
            err_code != VK_SUCCESS) {
            return err_code;
        }

        if (const auto err_code = create_rpass(); err_code != VK_SUCCESS) {
            return err_code;
        }

        if (const auto err_code = request_swapchain_images();
            err_code != VK_SUCCESS) {
            return err_code;
        }

        return 0;
    }

    int32_t create_swapchain()
    {
        if (m_swapchain_create_info.has_value()) {
            m_swapchain_create_info->oldSwapchain = m_swapchain;
            m_swapchain_create_info->imageExtent = get_image_extent();
            m_swapchain_create_info->preTransform =
                m_surace_capabilities.currentTransform;

            if (m_surace_capabilities.maxImageCount == 0) {
                m_swapchain_create_info->minImageCount = m_surace_capabilities.minImageCount + 1;
            } else {
                m_swapchain_create_info->minImageCount =
                    std::clamp(m_surace_capabilities.minImageCount + 1, m_surace_capabilities.minImageCount, m_surace_capabilities.maxImageCount);
            }

            return m_swapchain.reset(vk_utils::context::get().device(), &*m_swapchain_create_info);
            //            if (res == VK_SUCCESS) {
            //                vkDestroySwapchainKHR(vk_utils::context::get().device(), m_swapchain_create_info->oldSwapchain, nullptr);
            //            }

            //            return res;
        }

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_utils::context::get().gpu(), vk_utils::context::get().surface(), &m_surace_capabilities);

        uint32_t surface_formats_count{};
        vkGetPhysicalDeviceSurfaceFormatsKHR(vk_utils::context::get().gpu(), vk_utils::context::get().surface(), &surface_formats_count, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(surface_formats_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            vk_utils::context::get().gpu(), vk_utils::context::get().surface(), &surface_formats_count, formats.data());

        uint32_t surface_present_modes_count{};
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            vk_utils::context::get().gpu(), vk_utils::context::get().surface(), &surface_present_modes_count, nullptr);
        std::vector<VkPresentModeKHR> present_modes(surface_present_modes_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(vk_utils::context::get().gpu(), vk_utils::context::get().surface(), &surface_present_modes_count, present_modes.data());

        if (!m_swapchain_create_info.has_value()) {
            m_swapchain_create_info.emplace();
        }

        m_swapchain_create_info->sType =
            VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        m_swapchain_create_info->pNext = nullptr;
        m_swapchain_create_info->oldSwapchain = nullptr;
        m_swapchain_create_info->surface = vk_utils::context::get().surface();
        m_swapchain_create_info->preTransform =
            m_surace_capabilities.currentTransform;

        auto select_format = [this, &formats]() {
            const VkSurfaceFormatKHR* last_success_color_space_fmt = nullptr;
            for (const auto& fmt : formats) {
                if (fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    last_success_color_space_fmt = &fmt;

                    auto success_fmt = fmt.format == VK_FORMAT_R8G8B8A8_SRGB || fmt.format == VK_FORMAT_B8G8R8A8_SRGB;
                    if (success_fmt) {
                        m_swapchain_create_info->imageColorSpace = fmt.colorSpace;
                        m_swapchain_create_info->imageFormat = fmt.format;
                        return;
                    }
                }
            }

            if (last_success_color_space_fmt != nullptr) {
                m_swapchain_create_info->imageColorSpace =
                    last_success_color_space_fmt->colorSpace;
                m_swapchain_create_info->imageFormat =
                    last_success_color_space_fmt->format;
            } else {
                m_swapchain_create_info->imageColorSpace = formats.front().colorSpace;
                m_swapchain_create_info->imageFormat = formats.front().format;
            }
        };

        auto select_present_mode = [this, present_modes]() {
            if (std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != present_modes.end()) {
                m_swapchain_create_info->presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            } else {
                m_swapchain_create_info->presentMode = VK_PRESENT_MODE_FIFO_KHR;
            }
        };

        select_format();
        select_present_mode();

        m_swapchain_create_info->imageExtent = get_image_extent();
        if (m_surace_capabilities.maxImageCount == 0) {
            m_swapchain_create_info->minImageCount = m_surace_capabilities.minImageCount + 1;
        } else {
            m_swapchain_create_info->minImageCount =
                std::clamp(m_surace_capabilities.minImageCount + 1, m_surace_capabilities.minImageCount, m_surace_capabilities.maxImageCount);
        }

        m_swapchain_create_info->imageArrayLayers = 1;
        m_swapchain_create_info->imageUsage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // VK_IMAGE_USAGE_TRANSFER_DST_BIT
        m_swapchain_create_info->imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        m_swapchain_create_info->compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        m_swapchain_create_info->clipped = VK_TRUE;

        return m_swapchain.init(vk_utils::context::get().device(), &*m_swapchain_create_info);
    }

    int32_t create_rpass()
    {
        VkRenderPassCreateInfo pass_create_info{};

        pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        pass_create_info.pNext = nullptr;

        VkAttachmentDescription pass_attachment{};

        pass_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        pass_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        pass_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        pass_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        pass_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        pass_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        pass_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        pass_attachment.format = m_swapchain_create_info->imageFormat;

        VkAttachmentReference ref{};
        ref.attachment = 0;
        ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass_description{};
        subpass_description.colorAttachmentCount = 1;
        subpass_description.pColorAttachments = &ref;
        subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstSubpass = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        pass_create_info.pAttachments = &pass_attachment;
        pass_create_info.attachmentCount = 1;
        pass_create_info.pSubpasses = &subpass_description;
        pass_create_info.subpassCount = 1;
        pass_create_info.pDependencies = &dependency;
        pass_create_info.dependencyCount = 0;

        if (auto err_code = m_pass.init(vk_utils::context::get().device(), &pass_create_info);
            err_code != VK_SUCCESS) {
            return err_code;
        }

        return 0;
    }

    VkExtent2D get_image_extent()
    {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_utils::context::get().gpu(), vk_utils::context::get().surface(), &m_surace_capabilities);
        if (m_surace_capabilities.currentExtent.width != UINT32_MAX) {
            return m_surace_capabilities.currentExtent;
        } else {
            int w, h;
            glfwGetWindowSize(m_window, &w, &h);
            return {
                std::clamp(uint32_t(w), m_surace_capabilities.minImageExtent.width, m_surace_capabilities.maxImageExtent.width),
                std::clamp(uint32_t(h), m_surace_capabilities.minImageExtent.height, m_surace_capabilities.maxImageExtent.height),
            };
        }
    }

    uint32_t request_swapchain_images()
    {
        uint32_t images_count;
        vkGetSwapchainImagesKHR(vk_utils::context::get().device(), m_swapchain, &images_count, nullptr);

        m_swapchain_images.clear();
        m_swapchain_img_views.clear();
        m_framebuffers.clear();
        m_swapchain_images.resize(images_count);
        m_swapchain_img_views.reserve(images_count);
        m_framebuffers.reserve(images_count);

        auto res = vkGetSwapchainImagesKHR(vk_utils::context::get().device(), m_swapchain, &images_count, m_swapchain_images.data());

        if (res != VK_SUCCESS) {
            return res;
        }

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
        img_view_create_info.format = m_swapchain_create_info->imageFormat;

        VkFramebufferCreateInfo fb_create_info{};
        fb_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_create_info.pNext = nullptr;
        fb_create_info.renderPass = m_pass;
        fb_create_info.attachmentCount = 1;
        fb_create_info.width = m_swapchain_create_info->imageExtent.width;
        fb_create_info.height = m_swapchain_create_info->imageExtent.height;
        fb_create_info.layers = 1;

        for (int i = 0; i < images_count; ++i) {
            img_view_create_info.image = m_swapchain_images[i];
            vk_utils::img_view_handler h{};
            res = h.init(vk_utils::context::get().device(), &img_view_create_info);

            if (res != VK_SUCCESS) {
                m_swapchain_img_views.clear();
                m_framebuffers.clear();
                return res;
            }

            m_swapchain_img_views.emplace_back(std::move(h));
            VkImageView view = m_swapchain_img_views.back();

            fb_create_info.pAttachments = &view;
            vk_utils::framebuffer_handler fb_handle{};

            res = fb_handle.init(vk_utils::context::get().device(), &fb_create_info);

            if (res != VK_SUCCESS) {
                m_swapchain_img_views.clear();
                m_framebuffers.clear();
                return res;
            }

            m_framebuffers.emplace_back(std::move(fb_handle));
        }

        return VK_SUCCESS;
    }

    GLFWwindow* m_window{};
    vk_utils::swapchain_handler m_swapchain{};

    vk_utils::pass_handler m_pass;

    std::optional<VkSwapchainCreateInfoKHR> m_swapchain_create_info{};
    VkSurfaceCapabilitiesKHR m_surace_capabilities{};

    std::vector<VkImage> m_swapchain_images;
    std::vector<vk_utils::img_view_handler> m_swapchain_img_views;
    std::vector<vk_utils::framebuffer_handler> m_framebuffers;

    uint32_t m_image_index{};

    update_callback m_upd_callback;
    event_callback m_create_callback;
    event_callback m_destroy_callback;
    event_callback m_resize_callback;

    bool m_resized = false;
};

int32_t load_shader(const std::string& path, VkDevice device, vk_utils::shader_module_handler& handle, VkShaderStageFlagBits& stage)
{
    std::unique_ptr<FILE, std::function<void(FILE*)>> f_handle(
        nullptr, [](FILE* f) { fclose(f); });

    bool is_spv = path.find(".spv") != std::string::npos;

    if (is_spv) {
        f_handle.reset(fopen(path.c_str(), "rb"));
    } else {
        f_handle.reset(fopen(path.c_str(), "r"));
    }

    if (f_handle == nullptr) {
        return -1;
    }

    fseek(f_handle.get(), 0L, SEEK_END);
    auto size = ftell(f_handle.get());
    fseek(f_handle.get(), 0L, SEEK_SET);

    VkShaderModuleCreateInfo shader_module_info{};
    shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_info.pNext = nullptr;

    if (is_spv) {
        std::vector<char> bytes(size);
        fread(bytes.data(), 1, size, f_handle.get());
        shader_module_info.codeSize = bytes.size();
        shader_module_info.pCode = reinterpret_cast<const uint32_t*>(bytes.data());

        auto res = handle.init(device, &shader_module_info);

        if (res != VK_SUCCESS) {
            return res;
        }

    } else {
        static shaderc::Compiler compiler{};

        std::pair<shaderc_shader_kind, const char*> kind;

        static std::unordered_map<std::string, std::pair<shaderc_shader_kind, const char*>> kinds{
            {".vert", {shaderc_shader_kind::shaderc_glsl_vertex_shader, "vs"}},
            {".frag", {shaderc_shader_kind::shaderc_glsl_fragment_shader, "fs"}}};

        bool found = false;

        for (const auto& [stage_name, kind_val] : kinds) {
            if (path.find(stage_name) != std::string::npos) {
                kind = kind_val;
                found = true;
                break;
            }
        }

        if (!found) {
            return -1;
        }

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
        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, kind.first, kind.second, options);
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            LOG_ERROR(result.GetErrorMessage());
            return -1;
        }

        std::vector<uint32_t> shaderSPRV;
        shaderSPRV.assign(result.begin(), result.end());

        shader_module_info.codeSize = shaderSPRV.size() * sizeof(uint32_t);
        shader_module_info.pCode = shaderSPRV.data();

        auto res = handle.init(device, &shader_module_info);

        if (res != VK_SUCCESS) {
            return res;
        }
    }

    static std::unordered_map<std::string, VkShaderStageFlagBits> stages{
        {".vert", VK_SHADER_STAGE_VERTEX_BIT},
        {".frag", VK_SHADER_STAGE_FRAGMENT_BIT}};

    for (const auto& [stage_name, stage_val] : stages) {
        if (path.find(stage_name) != std::string::npos) {
            stage = stage_val;
            return 0;
        }
    }

    return -1;
}

float vert_data[][3]{
    {-0.5, -0.5, 0.0},
    {1., 1., 1.},
    {0.0, 0.0, 0.0},

    {-0.5, 0.5, 0.0},
    {0., 1., 0.},
    {0.0, 1.0, 0.0},

    {0.5, 0.5, 0.0},
    {0., 0., 1.},
    {1.0, 1.0, 0.0},

    {0.5, -0.5, 0.0},
    {1., 0., 1.},
    {1.0, 0.0, 0.0},
};

uint16_t indices[] = {
    0, 1, 2, 0, 2, 3};


struct global_ubo
{
    glm::mat4 projection = glm::identity<glm::mat4>();
    glm::mat4 view = glm::identity<glm::mat4>();
    glm::mat4 model = glm::identity<glm::mat4>();
    glm::mat4 mvp = glm::identity<glm::mat4>();
};


errors::error create_buffer(
    vk_utils::vma_buffer_handler& buffer,
    VkBufferUsageFlags buffer_usage,
    VmaMemoryUsage memory_usage,
    uint32_t size,
    void* data = nullptr)
{
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
        return ERROR(err, "cannot create buffer.");
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

    return errors::OK;
}


errors::error load_image2D(const char* path, vk_utils::vma_image_handler& image, vk_utils::image_view_handler& img_view, vk_utils::sampler_handler& sampler)
{
    int w, h, c;
    std::unique_ptr<stbi_uc, std::function<void(stbi_uc*)>> image_handler{
        stbi_load(path, &w, &h, &c, 0),
        [](stbi_uc* ptr) {if (ptr != nullptr) stbi_image_free(ptr); }};

    if (image_handler == nullptr) {
        return ERROR(-1, "Cannot load image.");
    }

    uint32_t draw_queue_family = vk_utils::context::get().queue_family_index(vk_utils::context::QUEUE_TYPE_GRAPHICS);

    vk_utils::vma_buffer_handler staging_buffer{};
    create_buffer(staging_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, w * h * c, image_handler.get());

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = nullptr;
    image_info.flags = 0;
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.arrayLayers = 1;
    image_info.mipLevels = 1;
    image_info.extent = {
        .width = static_cast<uint32_t>(w),
        .height = static_cast<uint32_t>(h),
        .depth = 1};

    VkComponentMapping components;

    switch (c) {
        case STBI_grey:
            image_info.format = VK_FORMAT_R8_SRGB;
            components.r = VK_COMPONENT_SWIZZLE_R;
            components.g = VK_COMPONENT_SWIZZLE_R;
            components.b = VK_COMPONENT_SWIZZLE_R;
            components.a = VK_COMPONENT_SWIZZLE_A;
            break;
        case STBI_grey_alpha:
            image_info.format = VK_FORMAT_R8G8_SRGB;
            components.r = VK_COMPONENT_SWIZZLE_R;
            components.g = VK_COMPONENT_SWIZZLE_B;
            components.b = VK_COMPONENT_SWIZZLE_B;
            components.a = VK_COMPONENT_SWIZZLE_G;
            break;
        case STBI_rgb:
            image_info.format = VK_FORMAT_R8G8B8_SRGB;
            components.r = VK_COMPONENT_SWIZZLE_R;
            components.g = VK_COMPONENT_SWIZZLE_G;
            components.b = VK_COMPONENT_SWIZZLE_B;
            components.a = VK_COMPONENT_SWIZZLE_A;
            break;
        case STBI_rgb_alpha:
            image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
            components.r = VK_COMPONENT_SWIZZLE_R;
            components.g = VK_COMPONENT_SWIZZLE_G;
            components.b = VK_COMPONENT_SWIZZLE_B;
            components.a = VK_COMPONENT_SWIZZLE_A;
            break;
    }

    image_info.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo image_alloc_info{};
    image_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (const auto e = image.init(vk_utils::context::get().allocator(), &image_info, &image_alloc_info); e != VK_SUCCESS) {
        return ERROR(e, "Cannot init image.");
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
    img_view_info.subresourceRange.levelCount = 1;
    img_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    if (const auto e = img_view.init(vk_utils::context::get().device(), &img_view_info); e != VK_SUCCESS) {
        return ERROR(e, "Cannot init image view.");
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
    sampler_info.maxLod = 0;
    sampler_info.mipLodBias = 0;
    sampler_info.compareEnable = VK_FALSE;

    if (const auto e = sampler.init(vk_utils::context::get().device(), &sampler_info); e != VK_SUCCESS) {
        return ERROR(e, "Cannot init sampler.");
    }

    VkCommandBufferAllocateInfo buffer_alloc_info{};
    buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buffer_alloc_info.pNext = nullptr;
    buffer_alloc_info.commandBufferCount = 1;
    buffer_alloc_info.commandPool = g_cmd_pool;
    buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    vk_utils::cmd_buffers_handler cmd_buffer{};
    cmd_buffer.init(vk_utils::context::get().device(), g_cmd_pool, &buffer_alloc_info, 1);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = nullptr;
    begin_info.pInheritanceInfo = nullptr;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd_buffer[0], &begin_info);

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
    img_transfer_barrier.subresourceRange.levelCount = 1;
    img_transfer_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCmdPipelineBarrier(cmd_buffer[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &img_transfer_barrier);

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

    vkCmdCopyBufferToImage(cmd_buffer[0], staging_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &img_copy);

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
    img_barrier.subresourceRange.levelCount = 1;
    img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCmdPipelineBarrier(cmd_buffer[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0, nullptr, 1, &img_barrier);

    vkEndCommandBuffer(cmd_buffer[0]);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.pCommandBuffers = cmd_buffer;
    submit_info.commandBufferCount = 1;

    vkQueueSubmit(vk_utils::context::get().queue(vk_utils::context::QUEUE_TYPE_GRAPHICS), 1, &submit_info, nullptr);

    vkQueueWaitIdle(vk_utils::context::get().queue(vk_utils::context::QUEUE_TYPE_GRAPHICS));
    return errors::OK;
}


int32_t init_pipeline(const hello_triangle_app* app)
{
    static VkShaderStageFlagBits shader_stages[std::size(shaders_paths_glsl)];
    static VkPipelineShaderStageCreateInfo
        shader_stages_infos[std::size(shaders_paths_glsl)];


    static bool shaders_loaded = false;

    if (!shaders_loaded) {
        for (int i = 0; i < std::size(shaders_paths_glsl); ++i) {
            if (auto err = load_shader(shaders_paths_spv[i], vk_utils::context::get().device(), g_shader_modules[i], shader_stages[i]);
                err != VK_SUCCESS) {
                return err;
            }

            shader_stages_infos[i].sType =
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_stages_infos[i].pNext = nullptr;
            shader_stages_infos[i].flags = 0;
            shader_stages_infos[i].stage = shader_stages[i];
            shader_stages_infos[i].module = g_shader_modules[i];
            shader_stages_infos[i].pName = "main";
            shader_stages_infos[i].pSpecializationInfo = nullptr;
        }

        shaders_loaded = true;
    }

    static bool vert_buffer_initialized = false;

    if (!vert_buffer_initialized) {
        if (create_buffer(g_vertex_buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, sizeof(vert_data)) != errors::OK) {
            return -1;
        }

        if (create_buffer(g_vertex_staging_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, sizeof(vert_data), vert_data) != errors::OK) {
            return -1;
        }

        if (create_buffer(g_index_buffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, sizeof(indices)) != errors::OK) {
            return -1;
        }

        if (create_buffer(g_index_staging_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, sizeof(indices), indices) != errors::OK) {
            return -1;
        }

        vert_buffer_initialized = true;
    }

    VkVertexInputAttributeDescription attrs[] = {
        {.location = 0,
         .binding = 0,
         .format = VK_FORMAT_R32G32B32_SFLOAT,
         .offset = 0},
        {.location = 1,
         .binding = 0,
         .format = VK_FORMAT_R32G32B32_SFLOAT,
         .offset = sizeof(float[3])},
        {.location = 2,
         .binding = 0,
         .format = VK_FORMAT_R32G32B32_SFLOAT,
         .offset = sizeof(float[3]) * 2}};

    VkVertexInputBindingDescription vert_bind[] = {
        {
            .binding = 0,
            .stride = sizeof(float[3]) * 3,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        }};

    VkPipelineVertexInputStateCreateInfo vert_input_info{};
    vert_input_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vert_input_info.pNext = nullptr;
    vert_input_info.pVertexAttributeDescriptions = nullptr;
    vert_input_info.vertexAttributeDescriptionCount = 0;

    vert_input_info.pVertexAttributeDescriptions = attrs;
    vert_input_info.vertexAttributeDescriptionCount = std::size(attrs);

    vert_input_info.pVertexBindingDescriptions = nullptr;
    vert_input_info.vertexBindingDescriptionCount = 0;

    vert_input_info.pVertexBindingDescriptions = vert_bind;
    vert_input_info.vertexBindingDescriptionCount = std::size(vert_bind);


    VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
    input_assembly_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.pNext = nullptr;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0, viewport.y = 0;
    viewport.width = app->get_window_size().width;
    viewport.height = app->get_window_size().height;
    viewport.minDepth = 0;
    viewport.maxDepth = 1;

    VkRect2D scissor{};
    scissor.extent = app->get_window_size();
    scissor.offset = {0, 0};

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.pNext = nullptr;
    viewport_state.pViewports = &viewport;
    viewport_state.viewportCount = 1;
    viewport_state.pScissors = &scissor;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterization_info{};
    rasterization_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_info.pNext = nullptr;
    rasterization_info.depthClampEnable = VK_FALSE;
    rasterization_info.depthBiasEnable = VK_FALSE;
    rasterization_info.rasterizerDiscardEnable = VK_FALSE;

    rasterization_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample_info{};
    multisample_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_info.pNext = nullptr;
    multisample_info.alphaToCoverageEnable = VK_FALSE;
    multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_info.sampleShadingEnable = VK_FALSE;
    multisample_info.alphaToOneEnable = VK_FALSE;
    multisample_info.minSampleShading = 1.0f;
    multisample_info.pSampleMask = nullptr;

    VkPipelineColorBlendStateCreateInfo color_blend_info{};
    color_blend_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_info.pNext = nullptr;
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_info.pAttachments = &color_blend_attachment;
    color_blend_info.attachmentCount = 1;
    color_blend_info.logicOpEnable = VK_FALSE;

    VkDynamicState dyn_state[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dyn_state_info{};
    dyn_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn_state_info.pNext = nullptr;
    dyn_state_info.pDynamicStates = dyn_state;
    dyn_state_info.dynamicStateCount = std::size(dyn_state);

    static bool pipeline_layout_created = false;

    if (!pipeline_layout_created) {
        VkDescriptorSetLayoutBinding descriptor_set_layout_binding[] = {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .pImmutableSamplers = nullptr,
            },
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr,
            }};

        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info{};
        descriptor_set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptor_set_layout_info.pNext = nullptr;
        descriptor_set_layout_info.pBindings = &descriptor_set_layout_binding[0];
        descriptor_set_layout_info.bindingCount = 1;

        if (auto err_code = g_ubo_descriptor_set_layout.init(vk_utils::context::get().device(), &descriptor_set_layout_info); err_code != VK_SUCCESS) {
            return -1;
        }

        descriptor_set_layout_info.pBindings = &descriptor_set_layout_binding[1];

        if (auto err_code = g_texture_descriptor_set_layout.init(vk_utils::context::get().device(), &descriptor_set_layout_info); err_code != VK_SUCCESS) {
            return -1;
        }

        VkDescriptorSetLayout desc_set_layouts[]{g_ubo_descriptor_set_layout, g_texture_descriptor_set_layout};

        VkPipelineLayoutCreateInfo layout_create_info{};
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.pNext = nullptr;
        layout_create_info.pPushConstantRanges = nullptr;
        layout_create_info.pushConstantRangeCount = 0;
        layout_create_info.pSetLayouts = desc_set_layouts;
        layout_create_info.setLayoutCount = std::size(desc_set_layouts);

        if (auto err_code =
                g_pipeline_layout.init(vk_utils::context::get().device(), &layout_create_info);
            err_code != VK_SUCCESS) {
            return -1;
        }

        pipeline_layout_created = true;
    }

    VkGraphicsPipelineCreateInfo pipeline_create_info{};
    pipeline_create_info.sType =
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_create_info.pNext = nullptr;

    pipeline_create_info.pStages = shader_stages_infos;
    pipeline_create_info.stageCount = std::size(shaders_paths_glsl);
    pipeline_create_info.pVertexInputState = &vert_input_info;
    pipeline_create_info.pInputAssemblyState = &input_assembly_info;
    pipeline_create_info.pViewportState = &viewport_state;
    pipeline_create_info.pRasterizationState = &rasterization_info;
    pipeline_create_info.pMultisampleState = &multisample_info;
    pipeline_create_info.pDepthStencilState = nullptr;
    pipeline_create_info.pColorBlendState = &color_blend_info;
    pipeline_create_info.pDynamicState = nullptr;
    pipeline_create_info.layout = g_pipeline_layout;
    pipeline_create_info.renderPass = app->get_main_pass();
    pipeline_create_info.subpass = 0;
    pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_create_info.basePipelineIndex = -1;

    if (const auto err_code =
            g_pipeline.init(vk_utils::context::get().device(), &pipeline_create_info);
        err_code != VK_SUCCESS) {
        return -1;
    }

    auto fbs = app->get_main_framebuffers();

    static bool cmd_pool_created = false;

    if (!cmd_pool_created) {
        VkCommandPoolCreateInfo cmd_pool_create_info{};
        cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmd_pool_create_info.pNext = nullptr;
        cmd_pool_create_info.queueFamilyIndex = vk_utils::context::get().queue_family_index(vk_utils::context::QUEUE_TYPE_GRAPHICS);
        if (const auto err_code = g_cmd_pool.init(vk_utils::context::get().device(), &cmd_pool_create_info);
            err_code != VK_SUCCESS) {
            return err_code;
        }

        cmd_pool_created = true;
    }

    static bool image_loaded = false;

    if (!image_loaded) {
        errors::handle_error(load_image2D("./images/texture.jpg", g_shader_image, g_shader_img_view, g_shader_sampler));
        image_loaded = true;
    }

    VkCommandBufferAllocateInfo cmd_buff_alloc_info{};
    cmd_buff_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buff_alloc_info.pNext = nullptr;
    cmd_buff_alloc_info.commandBufferCount = fbs.size();
    cmd_buff_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buff_alloc_info.commandPool = g_cmd_pool;

    if (const auto err_code = g_draw_cmd_buffers.init(vk_utils::context::get().device(), g_cmd_pool, &cmd_buff_alloc_info, cmd_buff_alloc_info.commandBufferCount); err_code != VK_SUCCESS) {
        return -1;
    }

    static bool vert_buffer_copied = false;

    if (!vert_buffer_copied) {
        cmd_buff_alloc_info.commandBufferCount = 1;

        if (const auto err_code = g_data_copy_cmd_buffers.init(vk_utils::context::get().device(), g_cmd_pool, &cmd_buff_alloc_info, cmd_buff_alloc_info.commandBufferCount); err_code != VK_SUCCESS) {
            return -1;
        }

        VkCommandBufferBeginInfo data_copy_begin_info{};
        data_copy_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        data_copy_begin_info.pNext = nullptr;
        data_copy_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        data_copy_begin_info.pInheritanceInfo = nullptr;

        vkBeginCommandBuffer(g_data_copy_cmd_buffers[0], &data_copy_begin_info);
        VkBufferCopy copy_info{};
        copy_info.size = sizeof(vert_data);
        copy_info.srcOffset = 0;
        copy_info.dstOffset = 0;
        vkCmdCopyBuffer(g_data_copy_cmd_buffers[0], g_vertex_staging_buffer, g_vertex_buffer, 1, &copy_info);
        copy_info.size = sizeof(indices);
        vkCmdCopyBuffer(g_data_copy_cmd_buffers[0], g_index_staging_buffer, g_index_buffer, 1, &copy_info);

        vkEndCommandBuffer(g_data_copy_cmd_buffers[0]);

        vert_buffer_copied = true;
    }


    static bool ubos_created = false;

    if (!ubos_created) {
        g_global_uniform_buffer.resize(fbs.size());

        for (auto& ubo : g_global_uniform_buffer) {
            create_buffer(ubo, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, sizeof(global_ubo));
        }

        ubos_created = true;
    }

    static bool descriptors_sets_created = false;

    if (!descriptors_sets_created) {
        VkDescriptorPoolCreateInfo descriptor_pool_info{};
        descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descriptor_pool_info.pNext = nullptr;

        VkDescriptorPoolSize pools_size[] = {
            {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             .descriptorCount = static_cast<uint32_t>(fbs.size())},
            {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             .descriptorCount = static_cast<uint32_t>(fbs.size())}};

        descriptor_pool_info.pPoolSizes = pools_size;
        descriptor_pool_info.poolSizeCount = std::size(pools_size);
        descriptor_pool_info.maxSets = fbs.size() * 2;

        if (const auto err_code = g_descriptor_pool.init(vk_utils::context::get().device(), &descriptor_pool_info); err_code != VK_SUCCESS) {
            return -1;
        }

        std::vector<VkDescriptorSetLayout> ubo_desc_layouts{fbs.size(), g_ubo_descriptor_set_layout};
        std::vector<VkDescriptorSetLayout> texture_desc_layouts{fbs.size(), g_texture_descriptor_set_layout};

        VkDescriptorSetAllocateInfo descriptor_alloc_info{};
        descriptor_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptor_alloc_info.pNext = nullptr;
        descriptor_alloc_info.pSetLayouts = ubo_desc_layouts.data();
        descriptor_alloc_info.descriptorSetCount = fbs.size();
        descriptor_alloc_info.descriptorPool = g_descriptor_pool;

        if (const auto err_code = g_ubo_descriptor_sets.init(vk_utils::context::get().device(), g_descriptor_pool, &descriptor_alloc_info, descriptor_alloc_info.descriptorSetCount); err_code != VK_SUCCESS) {
            return -1;
        }

        descriptor_alloc_info.pSetLayouts = texture_desc_layouts.data();

        if (const auto err_code = g_texture_descriptor_sets.init(vk_utils::context::get().device(), g_descriptor_pool, &descriptor_alloc_info, descriptor_alloc_info.descriptorSetCount); err_code != VK_SUCCESS) {
            return -1;
        }

        VkWriteDescriptorSet ubo_write_desc_set{};
        ubo_write_desc_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ubo_write_desc_set.pNext = nullptr;
        ubo_write_desc_set.dstBinding = 0;
        ubo_write_desc_set.descriptorCount = 1;
        ubo_write_desc_set.dstArrayElement = 0;
        ubo_write_desc_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

        VkWriteDescriptorSet tex_write_desc_set{};
        tex_write_desc_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        tex_write_desc_set.pNext = nullptr;
        tex_write_desc_set.dstBinding = 0;
        tex_write_desc_set.descriptorCount = 1;
        tex_write_desc_set.dstArrayElement = 0;
        tex_write_desc_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        VkDescriptorImageInfo tex_write_desc_set_img_info{};
        tex_write_desc_set_img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        tex_write_desc_set_img_info.imageView = g_shader_img_view;
        tex_write_desc_set_img_info.sampler = g_shader_sampler;

        tex_write_desc_set.pImageInfo = &tex_write_desc_set_img_info;

        for (int i = 0; i < fbs.size(); ++i) {
            ubo_write_desc_set.dstSet = g_ubo_descriptor_sets[i];
            tex_write_desc_set.dstSet = g_texture_descriptor_sets[i];
            VkDescriptorBufferInfo buffer_info{};

            buffer_info.buffer = g_global_uniform_buffer[i];
            buffer_info.offset = 0;
            buffer_info.range = VK_WHOLE_SIZE;
            ubo_write_desc_set.pBufferInfo = &buffer_info;

            vkUpdateDescriptorSets(vk_utils::context::get().device(), 1, &ubo_write_desc_set, 0, nullptr);
            vkUpdateDescriptorSets(vk_utils::context::get().device(), 1, &tex_write_desc_set, 0, nullptr);
        }

        descriptors_sets_created = true;
    }

    const VkCommandBuffer* cmd_buffers = g_draw_cmd_buffers;

    VkCommandBufferBeginInfo cmd_buf_begin_info{};
    cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buf_begin_info.pNext = nullptr;
    cmd_buf_begin_info.pInheritanceInfo = nullptr;

    VkRenderPassBeginInfo pass_begin_info{};
    pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass_begin_info.pNext = nullptr;
    pass_begin_info.renderPass = app->get_main_pass();
    pass_begin_info.renderArea = {
        .offset = {0, 0},
        .extent = app->get_window_size()};

    pass_begin_info.clearValueCount = 1;

    VkClearValue clear{};
    clear.color = VkClearColorValue{.float32 = {.2, .3, .7, 1.}};
    pass_begin_info.pClearValues = &clear;

    for (int i = 0; i < fbs.size(); ++i) {
        auto res = vkBeginCommandBuffer(cmd_buffers[i], &cmd_buf_begin_info);

        if (res != VK_SUCCESS) {
            return -1;
        }

        pass_begin_info.framebuffer = fbs[i];
        vkCmdBeginRenderPass(cmd_buffers[i], &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline);
        VkBuffer v_buffer[] = {g_vertex_buffer};
        VkDescriptorSet desc_sets[] = {g_ubo_descriptor_sets[i], g_texture_descriptor_sets[i]};
        VkDeviceSize offsets[] = {0};
        vkCmdBindDescriptorSets(cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline_layout, 0, std::size(desc_sets), desc_sets, 0, nullptr);
        vkCmdBindVertexBuffers(cmd_buffers[i], 0, 1, v_buffer, offsets);
        vkCmdBindIndexBuffer(cmd_buffers[i], g_index_buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd_buffers[i], std::size(indices), 1, 0, 0, 0);
        vkCmdEndRenderPass(cmd_buffers[i]);
        res = vkEndCommandBuffer(cmd_buffers[i]);

        if (res != VK_SUCCESS) {
            return -1;
        }
    }

    return 0;
}

int main(int argn, const char** argv)
{
    hello_triangle_app app;

    app.set_on_create_callback([](const hello_triangle_app* app) {
        return init_pipeline(app);
    });

    app.set_on_update_callback([](const hello_triangle_app* app, const VkCommandBuffer** cmd_bufs, uint32_t* bufs_count) {
        static bool is_first = true;

        static global_ubo ubo_data{};
        ubo_data.model = glm::rotate(ubo_data.model, glm::radians(0.1f), glm::vec3{0.0f, 0.0f, 1.0f});
        ubo_data.projection = glm::perspectiveFov(glm::radians(90.0f), float(app->get_window_size().width), float(app->get_window_size().height), 0.01f, 10.0f);
        ubo_data.projection[1][1] *= -1.0f;
        ubo_data.view = glm::lookAt(glm::vec3{0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        ubo_data.mvp = ubo_data.model * ubo_data.view * ubo_data.projection;

        auto& ubo = g_global_uniform_buffer[app->get_current_swapchain_img_index()];
        void* mapped_data;
        vmaMapMemory(vk_utils::context::get().allocator(), ubo, &mapped_data);
        std::memcpy(mapped_data, &ubo_data, sizeof(ubo_data));
        vmaFlushAllocation(vk_utils::context::get().allocator(), ubo, 0, VK_WHOLE_SIZE);
        vmaUnmapMemory(vk_utils::context::get().allocator(), ubo);

        static VkCommandBuffer handlers[2];
        handlers[1] = g_draw_cmd_buffers[app->get_current_swapchain_img_index()];

        if (is_first) {
            handlers[0] = g_data_copy_cmd_buffers[0];
            *cmd_bufs = handlers;
            *bufs_count = std::size(handlers);
            is_first = false;
        } else {
            const VkCommandBuffer* handlers = g_draw_cmd_buffers;
            *cmd_bufs = &handlers[app->get_current_swapchain_img_index()];
            *bufs_count = 1;
        }

        return 0;
    });

    app.set_on_resize_callback([](const hello_triangle_app* app) {
        g_draw_cmd_buffers.destroy();
        g_pipeline.destroy();

        return init_pipeline(app);
    });

    app.set_on_destroy_callback([](const hello_triangle_app* app) {
        g_draw_cmd_buffers.destroy();
        g_data_copy_cmd_buffers.destroy();

        g_cmd_pool.destroy();
        g_pipeline_layout.destroy();
        g_pipeline.destroy();

        g_vertex_buffer.destroy();
        g_vertex_staging_buffer.destroy();
        g_index_buffer.destroy();
        g_index_staging_buffer.destroy();

        g_global_uniform_buffer.clear();
        g_ubo_descriptor_set_layout.destroy();
        g_texture_descriptor_set_layout.destroy();

        g_ubo_descriptor_sets.destroy();
        g_texture_descriptor_sets.destroy();

        g_descriptor_pool.destroy();

        g_shader_image.destroy();
        g_shader_img_view.destroy();
        g_shader_sampler.destroy();

        for (auto& m : g_shader_modules) {
            m.destroy();
        }
        return 0;
    });


    if (app.run(argn, argv)) {
        LOG_ERROR("application run failed.");
        return -1;
    }

    return 0;
}