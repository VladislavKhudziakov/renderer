
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
#include <unordered_set>
#include <variant>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

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

vk_utils::vma_buffer_handler g_global_uniform_buffer;

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
    ERROR_TYPE init_window() override
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        m_window = glfwCreateWindow(800, 600, "Vulkan window", nullptr, nullptr);
        glfwSetWindowUserPointer(m_window, this);

        glfwSetWindowSizeCallback(m_window, on_window_resized);

        if (m_window == nullptr) {
            RAISE_ERROR_FATAL(-1, "Failed to create window.");
        }

        if (const auto err_code = init_vulkan(); err_code != 0) {
            RAISE_ERROR_FATAL(-1, "Failed to init vulkan.");
        }

        RAISE_ERROR_OK();
    }

    ERROR_TYPE run_main_loop() override
    {
        if (m_create_callback) {
            if (auto err_code = m_create_callback(this); err_code != VK_SUCCESS) {
                RAISE_ERROR_FATAL(err_code, "create cb failed");
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
                    RAISE_ERROR_FATAL(VK_ERROR_DEVICE_LOST, "device lost occured in vkAcquireNextImageKHR");
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
                    RAISE_ERROR_FATAL(res, "error occured in vkQueuePresentKHR");
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
                    RAISE_ERROR_FATAL(VK_ERROR_DEVICE_LOST, "device lost occured in vkQueuePresentKHR");
                }
            }

            ++curr_frame;
            curr_frame %= frames_in_flight;
        }

        vkDeviceWaitIdle(vk_utils::context::get().device());

        RAISE_ERROR_OK();
    }

    ERROR_TYPE cleanup() override
    {
        if (m_destroy_callback) {
            if (auto err_code = m_destroy_callback(this); err_code != VK_SUCCESS) {
                RAISE_ERROR_FATAL(err_code, "destroy failed.");
            }
        }

        m_pass.destroy();
        m_swapchain_img_views.clear();
        m_framebuffers.clear();
        m_swapchain.destroy();

        glfwDestroyWindow(m_window);
        glfwTerminate();
        RAISE_ERROR_OK();
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

        HANDLE_ERROR(vk_utils::context::init("hello triangle", context_info));

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

        VkAttachmentDescription pass_attachments[]{
            {
                .format = m_swapchain_create_info->imageFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
            {
                .format = VK_FORMAT_D24_UNORM_S8_UINT,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            }};

        VkAttachmentReference refs[]{
            {
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            {
                .attachment = 1,
                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            }};


        VkSubpassDescription subpass_description{};
        subpass_description.colorAttachmentCount = 1;
        subpass_description.pColorAttachments = &refs[0];
        subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_description.pDepthStencilAttachment = &refs[1];

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstSubpass = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        pass_create_info.pAttachments = pass_attachments;
        pass_create_info.attachmentCount = std::size(pass_attachments);
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
        }

        VkImageCreateInfo depth_img_info{};
        depth_img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depth_img_info.pNext = nullptr;

        depth_img_info.imageType = VK_IMAGE_TYPE_2D;
        depth_img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        depth_img_info.extent.width = m_swapchain_create_info->imageExtent.width;
        depth_img_info.extent.height = m_swapchain_create_info->imageExtent.height;
        depth_img_info.extent.depth = 1;
        depth_img_info.mipLevels = 1;
        depth_img_info.arrayLayers = 1;

        depth_img_info.format = VK_FORMAT_D24_UNORM_S8_UINT;
        depth_img_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depth_img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_img_info.samples = VK_SAMPLE_COUNT_1_BIT;
        depth_img_info.tiling = VK_IMAGE_TILING_OPTIMAL;

        VmaAllocationCreateInfo depth_alloc_info{};
        depth_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (const auto err = m_depth_image.init(vk_utils::context::get().allocator(), &depth_img_info, &depth_alloc_info); err != VK_SUCCESS) {
            m_swapchain_img_views.clear();
            m_framebuffers.clear();
            return err;
        }

        VkImageViewCreateInfo depth_img_view_info{};
        depth_img_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depth_img_view_info.pNext = nullptr;
        depth_img_view_info.image = m_depth_image;
        depth_img_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;

        depth_img_view_info.format = depth_img_info.format;

        depth_img_view_info.components.r = VK_COMPONENT_SWIZZLE_R;
        depth_img_view_info.components.g = VK_COMPONENT_SWIZZLE_G;
        depth_img_view_info.components.b = VK_COMPONENT_SWIZZLE_B;
        depth_img_view_info.components.a = VK_COMPONENT_SWIZZLE_A;

        depth_img_view_info.subresourceRange.layerCount = 1;
        depth_img_view_info.subresourceRange.baseArrayLayer = 0;
        depth_img_view_info.subresourceRange.levelCount = 1;
        depth_img_view_info.subresourceRange.baseMipLevel = 0;
        depth_img_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (const auto err = m_depth_image_view.reset(vk_utils::context::get().device(), &depth_img_view_info); err != VK_SUCCESS) {
            m_swapchain_img_views.clear();
            m_framebuffers.clear();
            m_depth_image.destroy();
            return err;
        }

        VkFramebufferCreateInfo fb_create_info{};
        fb_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_create_info.pNext = nullptr;
        fb_create_info.renderPass = m_pass;

        fb_create_info.width = m_swapchain_create_info->imageExtent.width;
        fb_create_info.height = m_swapchain_create_info->imageExtent.height;
        fb_create_info.layers = 1;

        for (int i = 0; i < images_count; ++i) {
            VkImageView fb_attachments[] = {
                m_swapchain_img_views[i],
                m_depth_image_view};

            fb_create_info.pAttachments = fb_attachments;
            fb_create_info.attachmentCount = std::size(fb_attachments);

            vk_utils::framebuffer_handler fb_handle{};

            res = fb_handle.init(vk_utils::context::get().device(), &fb_create_info);

            if (res != VK_SUCCESS) {
                m_swapchain_img_views.clear();
                m_framebuffers.clear();
                m_depth_image.destroy();
                m_depth_image_view.destroy();
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
    vk_utils::vma_image_handler m_depth_image;
    vk_utils::image_view_handler m_depth_image_view;

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


struct global_ubo
{
    glm::mat4 projection = glm::identity<glm::mat4>();
    glm::mat4 view = glm::identity<glm::mat4>();
    glm::mat4 model = glm::identity<glm::mat4>();
    glm::mat4 mvp = glm::identity<glm::mat4>();
};


ERROR_TYPE create_buffer(
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

    RAISE_ERROR_OK();
}


ERROR_TYPE load_image2D(
    const char* path,
    const vk_utils::cmd_pool_handler& cmd_pool,
    bool gen_mips,
    vk_utils::vma_image_handler& image,
    vk_utils::image_view_handler& img_view,
    vk_utils::sampler_handler& sampler)
{
    int w, h, c;
    std::unique_ptr<stbi_uc, std::function<void(stbi_uc*)>> image_handler{
        stbi_load(path, &w, &h, &c, 0),
        [](stbi_uc* ptr) {if (ptr != nullptr) stbi_image_free(ptr); }};

    const auto mip_levels = !gen_mips ? 1 : static_cast<uint32_t>(log2(std::max(w, h))) + 1;

    if (image_handler == nullptr) {
        RAISE_ERROR_WARN(-1, "Cannot load image.");
    }

    uint32_t draw_queue_family = vk_utils::context::get().queue_family_index(vk_utils::context::QUEUE_TYPE_GRAPHICS);


    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(vk_utils::context::get().gpu(), VK_FORMAT_R8G8B8_SRGB, &props);

    bool add_alpha = false;
    if (c == STBI_rgb && !(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
        c = STBI_rgb_alpha;
        add_alpha = true;
    }

    uint8_t* data_ptr = image_handler.get();
    std::vector<uint8_t> bytes;

    if (add_alpha) {
        bytes.reserve(w * h * c);
        for (size_t i = 0; i < w * h; ++i) {
            bytes.push_back(data_ptr[i * 3]);
            bytes.push_back(data_ptr[i * 3 + 1]);
            bytes.push_back(data_ptr[i * 3 + 2]);
            bytes.push_back(255);
        }
        data_ptr = bytes.data();
    }

    vk_utils::vma_buffer_handler staging_buffer{};
    create_buffer(staging_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, w * h * c, data_ptr);

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = nullptr;
    image_info.flags = 0;
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.arrayLayers = 1;
    image_info.mipLevels = mip_levels;
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
            components.a = VK_COMPONENT_SWIZZLE_ONE;
            break;
        case STBI_grey_alpha:
            image_info.format = VK_FORMAT_R8G8_SRGB;
            components.r = VK_COMPONENT_SWIZZLE_R;
            components.g = VK_COMPONENT_SWIZZLE_ZERO;
            components.b = VK_COMPONENT_SWIZZLE_ZERO;
            components.a = VK_COMPONENT_SWIZZLE_G;
            break;
        case STBI_rgb:
            image_info.format = VK_FORMAT_R8G8B8_SRGB;
            components.r = VK_COMPONENT_SWIZZLE_R;
            components.g = VK_COMPONENT_SWIZZLE_G;
            components.b = VK_COMPONENT_SWIZZLE_B;
            components.a = VK_COMPONENT_SWIZZLE_ONE;
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

    if (const auto e = img_view.init(vk_utils::context::get().device(), &img_view_info); e != VK_SUCCESS) {
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

    if (const auto e = sampler.init(vk_utils::context::get().device(), &sampler_info); e != VK_SUCCESS) {
        RAISE_ERROR_WARN(e, "Cannot init sampler.");
    }

    VkCommandBufferAllocateInfo buffer_alloc_info{};
    buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buffer_alloc_info.pNext = nullptr;
    buffer_alloc_info.commandBufferCount = 1;
    buffer_alloc_info.commandPool = cmd_pool;
    buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    vk_utils::cmd_buffers_handler cmd_buffer{};
    cmd_buffer.init(vk_utils::context::get().device(), cmd_pool, &buffer_alloc_info, 1);

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
    img_transfer_barrier.subresourceRange.levelCount = mip_levels;
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
            }
        };

        uint32_t mip_width = w;
        uint32_t mip_height = h;

        for (uint32_t i = 1; i < mip_levels; ++i) {
            mip_gen_barriers.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            mip_gen_barriers.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            mip_gen_barriers.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            mip_gen_barriers.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            mip_gen_barriers.subresourceRange.baseMipLevel = i - 1;

            vkCmdPipelineBarrier(
                cmd_buffer[0],
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
                cmd_buffer[0],
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
            cmd_buffer[0],
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
            cmd_buffer[0],
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

        vkCmdPipelineBarrier(cmd_buffer[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0, nullptr, 1, &img_barrier);
    }

    vkEndCommandBuffer(cmd_buffer[0]);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.pCommandBuffers = cmd_buffer;
    submit_info.commandBufferCount = 1;

    vkQueueSubmit(vk_utils::context::get().queue(vk_utils::context::QUEUE_TYPE_GRAPHICS), 1, &submit_info, nullptr);

    vkQueueWaitIdle(vk_utils::context::get().queue(vk_utils::context::QUEUE_TYPE_GRAPHICS));
    RAISE_ERROR_OK();
}


enum render_technique_type
{
    BLINN_PHONG,
    PBR
};

enum blinn_phong_textures
{
    BLINN_PHONG_DIFFUSE,
    BLINN_PHONG_AMBIENT,
    BLINN_PHONG_SPECULAR,
    BLINN_PHONG_SPECULAR_HIGHLIGHT,
    BLINN_PHONG_BUMP,
    BLINN_PHONG_DISPLACEMENT,
    BLINN_PHONG_ALPHA,
    BLINN_PHONG_REFLECTION,
    BLINN_PHONG_SIZE,
};


struct blinn_phong_material
{
    std::array<int32_t, BLINN_PHONG_SIZE> material_textures{-1, -1, -1, -1, -1, -1, -1, -1};
    std::array<glm::vec3, BLINN_PHONG_SIZE> material_coeffs{};
};

struct pbr_material
{
};


struct bound
{
    glm::vec3 lo;
    glm::vec3 hi;
};

struct subgeometry
{
    std::vector<VkFormat> format{};
    uint32_t indices_offset{0};
    uint32_t indices_size{0};
    uint32_t indices_bias{0};
    uint32_t vertices_offset{0};
    uint32_t image_samplers_count;
    render_technique_type render_technique;
    std::variant<blinn_phong_material, pbr_material> material;
};


struct obj_model
{
    vk_utils::vma_buffer_handler vertex_buffer;
    vk_utils::vma_buffer_handler index_buffer;

    std::vector<vk_utils::vma_image_handler> tex_images;
    std::vector<vk_utils::image_view_handler> tex_image_views;
    std::vector<vk_utils::sampler_handler> tex_samplers;

    std::vector<subgeometry> subgeometries;

    bound model_bound;
};


struct shader
{
    std::vector<VkShaderModule> modules;
    std::vector<VkShaderStageFlagBits> stages;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
    std::vector<vk_utils::descriptor_set_handler> descriptor_sets;
};


struct shader_group
{
    std::vector<vk_utils::shader_module_handler> shader_modules;
    vk_utils::descriptor_pool_handler desc_pool;
    std::vector<vk_utils::descriptor_set_layout_handler> shaders_layouts;
    std::vector<shader> shaders;
};


ERROR_TYPE init_geom_pipelines(
    const std::vector<subgeometry>& subgeoms,
    const std::vector<shader>& shaders,
    VkRenderPass pass,
    uint32_t subpass_index,
    VkPrimitiveTopology topo,
    VkExtent2D input_viewport,
    std::vector<vk_utils::graphics_pipeline_handler>& pipelines,
    std::vector<vk_utils::pipeline_layout_handler>& pipeline_layouts)
{
    for (int i = 0; i < subgeoms.size(); ++i) {
        const auto& subgeom = subgeoms[i];
        const auto& shader = shaders[i];

        std::vector<VkPipelineShaderStageCreateInfo> shader_stages{};
        shader_stages.reserve(shader.stages.size());

        if (shader.stages.size() != shader.modules.size()) {
            RAISE_ERROR_FATAL(-1, "invalid shader modules count");
        }

        for (size_t ii = 0; ii < shader.stages.size(); ++ii) {
            auto& stage = shader_stages.emplace_back();
            stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stage.pNext = nullptr;
            stage.flags = 0;
            stage.module = shader.modules[ii];
            stage.stage = shader.stages[ii];
            stage.pName = "main";
        }

        VkVertexInputAttributeDescription vert_attrs[3] =
        {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0,
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0,
            },
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = 0,
            }
        };
        if (subgeom.format.size() > std::size(vert_attrs)) {
            RAISE_ERROR_FATAL(-1, "invalid vertex format");
        }
        uint32_t stride = 0;
        for (int i = 0; i < subgeom.format.size(); ++i) {
            vert_attrs[i].binding = 0;
            vert_attrs[i].location = i;
            vert_attrs[i].format = subgeom.format[i];
            vert_attrs[i].offset = stride;

            switch (subgeom.format[i]) {
                case VK_FORMAT_R32G32B32_SFLOAT:
                    stride += sizeof(float) * 3;
                    break;
                case VK_FORMAT_R32G32_SFLOAT:
                    stride += sizeof(float) * 2;
                    break;
                default:
                    RAISE_ERROR_FATAL(-1, "invalid vertex attribute format");
            }
        }

        VkVertexInputBindingDescription vert_binding{};
        vert_binding.binding = 0;
        vert_binding.stride = stride;
        vert_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkPipelineVertexInputStateCreateInfo vert_input_info{};
        vert_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vert_input_info.pNext = nullptr;
        vert_input_info.flags = 0;
        vert_input_info.pVertexAttributeDescriptions = vert_attrs;
        vert_input_info.vertexAttributeDescriptionCount = std::size(vert_attrs);
        vert_input_info.pVertexBindingDescriptions = &vert_binding;
        vert_input_info.vertexBindingDescriptionCount = 1;

        VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
        input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_info.pNext = nullptr;
        input_assembly_info.flags = 0;
        input_assembly_info.primitiveRestartEnable = VK_FALSE;
        input_assembly_info.topology = topo;

        VkRect2D scissor;
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent = input_viewport;

        VkViewport viewport;
        viewport.width = input_viewport.width;
        viewport.height = input_viewport.height;
        viewport.x = 0;
        viewport.y = 0;
        viewport.minDepth = 0;
        viewport.maxDepth = 1;

        VkPipelineViewportStateCreateInfo viewport_info{};
        viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_info.pNext = nullptr;
        viewport_info.flags = 0;

        viewport_info.pScissors = &scissor;
        viewport_info.scissorCount = 1;
        viewport_info.pViewports = &viewport;
        viewport_info.viewportCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterization_info{};
        rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_info.pNext = nullptr;
        rasterization_info.flags = 0;
        rasterization_info.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_info.lineWidth = 1;
        rasterization_info.depthClampEnable = VK_FALSE;
        rasterization_info.depthBiasEnable = VK_FALSE;
        rasterization_info.rasterizerDiscardEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisample_info{};
        multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_info.pNext = nullptr;
        multisample_info.flags = 0;
        multisample_info.alphaToCoverageEnable = VK_FALSE;
        multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample_info.sampleShadingEnable = VK_FALSE;
        multisample_info.alphaToOneEnable = VK_FALSE;
        multisample_info.minSampleShading = 1.0f;
        multisample_info.pSampleMask = nullptr;

        VkPipelineDepthStencilStateCreateInfo depth_stencil_info{};
        depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_info.pNext = nullptr;
        depth_stencil_info.flags = 0;
        depth_stencil_info.depthTestEnable = VK_TRUE;
        depth_stencil_info.depthWriteEnable = VK_TRUE;
        depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depth_stencil_info.stencilTestEnable = VK_FALSE;
        depth_stencil_info.depthBoundsTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.blendEnable = VK_FALSE;
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo color_blend_info{};
        color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_info.pNext = nullptr;
        color_blend_info.flags = 0;
        color_blend_info.pAttachments = &color_blend_attachment;
        color_blend_info.attachmentCount = 1;
        color_blend_info.logicOpEnable = VK_FALSE;

        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.pNext = nullptr;
        pipeline_layout_info.flags = 0;

        std::vector<VkDescriptorSetLayout> desc_set_layouts;
        std::transform(shader.descriptor_set_layouts.begin(), shader.descriptor_set_layouts.end(), std::back_inserter(desc_set_layouts), [](VkDescriptorSetLayout l) {return  l;});

        pipeline_layout_info.pSetLayouts = desc_set_layouts.data();
        pipeline_layout_info.setLayoutCount = desc_set_layouts.size();
        pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pPushConstantRanges = nullptr;

        auto& pipeline_layout = pipeline_layouts.emplace_back();
        pipeline_layout.init(vk_utils::context::get().device(), &pipeline_layout_info);

        VkGraphicsPipelineCreateInfo pipeline_create_info{};
        pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create_info.pNext = nullptr;
        pipeline_create_info.flags = 0;

        pipeline_create_info.pStages = shader_stages.data();
        pipeline_create_info.stageCount = shader_stages.size();
        pipeline_create_info.pVertexInputState = &vert_input_info;
        pipeline_create_info.pInputAssemblyState = &input_assembly_info;
        pipeline_create_info.pTessellationState = nullptr;
        pipeline_create_info.pViewportState = &viewport_info;
        pipeline_create_info.pRasterizationState = &rasterization_info;
        pipeline_create_info.pMultisampleState = &multisample_info;
        pipeline_create_info.pDepthStencilState = &depth_stencil_info;
        pipeline_create_info.pColorBlendState = &color_blend_info;
        pipeline_create_info.pDynamicState = nullptr;
        pipeline_create_info.layout = pipeline_layout;

        pipeline_create_info.renderPass = pass;
        pipeline_create_info.subpass = subpass_index;
        pipeline_create_info.basePipelineHandle = nullptr;
        pipeline_create_info.basePipelineIndex = -1;

        auto& pipeline = pipelines.emplace_back();
        if (pipeline.init(vk_utils::context::get().device(), &pipeline_create_info) != VK_SUCCESS) {
            RAISE_ERROR_FATAL(-1, "cannot init pipeline.");
        }
    }
}

ERROR_TYPE init_obj_geometry(
    const tinyobj::attrib_t& attrib,
    const std::vector<tinyobj::shape_t>& shapes,
    const vk_utils::cmd_pool_handler& cmd_pool,
    vk_utils::vma_buffer_handler& v_buffer,
    vk_utils::vma_buffer_handler& i_buffer,
    std::vector<subgeometry>& subgeoms,
    bound& model_bound)
{
    model_bound.lo = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    model_bound.hi = {std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()};

    struct vertex
    {
        std::array<float, 3> position;
        std::optional<std::array<float, 3>> normal;
        std::optional<std::array<float, 3>> texcoord;
    };

    struct geometry
    {
        std::vector<vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<VkFormat> vertex_format;
        size_t vertex_size = 0;
    };

    std::vector<geometry> geometries;
    geometries.reserve(shapes.size());

    auto vert_eq = [](const vertex& v1) {
        return [&v1](const vertex& v2) {
            if (v1.position != v2.position) {
                return false;
            }
            if (v1.normal && v2.normal && (*v1.normal != *v2.normal)) {
                return false;
            }
            if (v1.texcoord && v2.texcoord && (*v1.texcoord != *v2.texcoord)) {
                return false;
            }
            return true;
        };
    };

    for (const auto& shape : shapes) {
        auto& g = geometries.emplace_back();
        g.vertices.reserve(shape.mesh.indices.size());
        auto& i = shape.mesh.indices.front();

        g.vertex_format.emplace_back(VK_FORMAT_R32G32B32_SFLOAT);
        g.vertex_size += sizeof(glm::vec3);

        if (i.normal_index >= 0) {
            g.vertex_format.emplace_back(VK_FORMAT_R32G32B32_SFLOAT);
            g.vertex_size += sizeof(glm::vec3);
        }

        if (i.texcoord_index >= 0) {
            g.vertex_format.emplace_back(VK_FORMAT_R32G32_SFLOAT);
            g.vertex_size += sizeof(glm::vec2);
        }

        for (auto& i : shape.mesh.indices) {
            vertex v{};

            v.position[0] = attrib.vertices[3 * i.vertex_index];
            v.position[1] = attrib.vertices[3 * i.vertex_index + 1];
            v.position[2] = attrib.vertices[3 * i.vertex_index + 2];

            if (i.normal_index >= 0) {
                v.normal = {attrib.normals[3 * i.normal_index], attrib.vertices[3 * i.normal_index + 1], attrib.vertices[3 * i.normal_index + 2]};
            }

            if (i.texcoord_index >= 0) {
                v.texcoord = {attrib.texcoords[2 * i.texcoord_index], attrib.texcoords[2 * i.texcoord_index + 1]};
            }

            auto vertex_exist = std::find_if(g.vertices.begin(), g.vertices.end(), vert_eq(v));

            if (vertex_exist != g.vertices.end()) {
                g.indices.emplace_back(std::distance(g.vertices.begin(), vertex_exist));
            } else {
                g.indices.emplace_back(g.vertices.size());
                g.vertices.emplace_back(v);

                model_bound.lo.x = std::min(v.position[0], model_bound.lo.x);
                model_bound.lo.y = std::min(v.position[1], model_bound.lo.y);
                model_bound.lo.z = std::min(v.position[2], model_bound.lo.z);

                model_bound.hi.x = std::max(v.position[0], model_bound.hi.x);
                model_bound.hi.y = std::max(v.position[1], model_bound.hi.y);
                model_bound.hi.z = std::max(v.position[2], model_bound.hi.z);
            }
        }
    }
    size_t vert_values_count = 0;
    size_t indices_count = 0;

    std::vector<float> vert_buffer_data;
    std::vector<uint32_t> index_buffer_data;

    for (auto& g : geometries) {
        size_t vertex_floats_count = 3;

        if (g.vertices.front().normal) {
            vertex_floats_count += 3;
        }

        if (g.vertices.front().texcoord) {
            vertex_floats_count += 2;
        }

        vert_values_count += vertex_floats_count * g.vertices.size();
        indices_count += g.indices.size();
    }

    vert_buffer_data.reserve(vert_values_count);
    index_buffer_data.reserve(indices_count);
    uint32_t start_vertex = 0;
    size_t indices_offset = 0;
    size_t vertices_offset = 0;

    subgeoms.reserve(geometries.size());

    for (auto& g : geometries) {
        auto& sg = subgeoms.emplace_back();
        sg.format = g.vertex_format;
        sg.indices_offset = indices_offset;
        sg.vertices_offset = vertices_offset;
        sg.indices_bias = start_vertex;
        sg.indices_size = g.indices.size();

        for (const auto& v : g.vertices) {
            vert_buffer_data.push_back(v.position[0]);
            vert_buffer_data.push_back(v.position[1]);
            vert_buffer_data.push_back(v.position[2]);

            if (v.normal) {
                vert_buffer_data.push_back(v.normal->at(0));
                vert_buffer_data.push_back(v.normal->at(1));
                vert_buffer_data.push_back(v.normal->at(2));
            }

            if (v.texcoord) {
                vert_buffer_data.push_back(v.texcoord->at(0));
                vert_buffer_data.push_back(v.texcoord->at(1));
            }
        }

        for (const uint32_t index : g.indices) {
            index_buffer_data.push_back(index);
        }

        start_vertex += g.vertices.size();
        vertices_offset += g.vertices.size() * g.vertex_size;
        indices_offset += g.indices.size();
    }

    vk_utils::vma_buffer_handler vert_staging_buffer{};
    create_buffer(vert_staging_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, vert_buffer_data.size() * sizeof(float), vert_buffer_data.data());
    vk_utils::vma_buffer_handler index_staging_buffer{};
    create_buffer(index_staging_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, index_buffer_data.size() * sizeof(uint32_t), index_buffer_data.data());

    create_buffer(v_buffer, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, vert_buffer_data.size() * sizeof(float));
    create_buffer(i_buffer, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, index_buffer_data.size() * sizeof(uint32_t));

    vk_utils::cmd_buffers_handler cmd_buffer{};
    VkCommandBufferAllocateInfo cmd_buffer_alloc_info{};
    cmd_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buffer_alloc_info.pNext = nullptr;
    cmd_buffer_alloc_info.commandPool = cmd_pool;
    cmd_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buffer_alloc_info.commandBufferCount = 1;
    cmd_buffer.init(vk_utils::context::get().device(), cmd_pool, &cmd_buffer_alloc_info, 1);

    VkCommandBufferBeginInfo cmd_buffer_begin_info{};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.pNext = nullptr;
    cmd_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    vkBeginCommandBuffer(cmd_buffer[0], &cmd_buffer_begin_info);

    VkBufferCopy vert_region{};
    vert_region.srcOffset = 0;
    vert_region.dstOffset = 0;
    vert_region.size = vert_buffer_data.size() * sizeof(float);
    vkCmdCopyBuffer(cmd_buffer[0], vert_staging_buffer, v_buffer, 1, &vert_region);

    VkBufferCopy index_region{};
    index_region.srcOffset = 0;
    index_region.dstOffset = 0;
    index_region.size = index_buffer_data.size() * sizeof(uint32_t);
    vkCmdCopyBuffer(cmd_buffer[0], index_staging_buffer, i_buffer, 1, &index_region);

    vkEndCommandBuffer(cmd_buffer[0]);
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = cmd_buffer;

    vkQueueSubmit(vk_utils::context::get().queue(vk_utils::context::QUEUE_TYPE_GRAPHICS), 1, &submit_info, nullptr);
    vkQueueWaitIdle(vk_utils::context::get().queue(vk_utils::context::QUEUE_TYPE_GRAPHICS));

    RAISE_ERROR_OK();
}


ERROR_TYPE init_obj_materials(
    const std::vector<tinyobj::shape_t>& shapes,
    const vk_utils::cmd_pool_handler& cmd_pool,
    const std::vector<tinyobj::material_t>& materials, obj_model& model,
    const char** textures,
    size_t textures_size)
{
    std::vector<std::string> load_textures;

    for (int i = 0; i < textures_size; ++i) {
        load_textures.emplace_back(textures[i]);
    }

    for (size_t i = 0; i < shapes.size(); ++i) {
        auto& shape = shapes[i];
        auto& mat = materials[shape.mesh.material_ids.front()];

        auto& m = model.subgeometries[i].material.emplace<blinn_phong_material>();

        int curr_diff_texture_index = 0;

        if (textures_size == 0) {
            if (!mat.diffuse_texname.empty()) {
                auto tex_it = std::find(load_textures.begin(), load_textures.end(), mat.diffuse_texname);
                if (tex_it == load_textures.end()) {
                    m.material_textures[BLINN_PHONG_DIFFUSE] = load_textures.size();
                    load_textures.emplace_back(mat.diffuse_texname);
                } else {
                    m.material_textures[BLINN_PHONG_DIFFUSE] = std::distance(load_textures.begin(), tex_it);
                }
            }
        } else {
            m.material_textures[BLINN_PHONG_DIFFUSE] = std::min(i, textures_size - 1);
        }
    }

    model.tex_images.reserve(load_textures.size());
    model.tex_image_views.reserve(load_textures.size());
    model.tex_samplers.reserve(load_textures.size());

    for (auto& tex : load_textures) {
        auto& i = model.tex_images.emplace_back();
        auto& v = model.tex_image_views.emplace_back();
        auto& s = model.tex_samplers.emplace_back();

        PASS_ERROR(load_image2D(tex.c_str(), cmd_pool, true, i, v, s));
    }

    RAISE_ERROR_OK();
}


ERROR_TYPE load_obj_model(const char* path, const vk_utils::cmd_pool_handler& cmd_pool, obj_model& model, const char** textures, size_t textures_size)
{
    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(path)) {
        RAISE_ERROR_FATAL(-1, reader.Error());
    }

    const tinyobj::attrib_t& attrib = reader.GetAttrib();
    const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
    const std::vector<tinyobj::material_t>& materials = reader.GetMaterials();

    if (!reader.Warning().empty()) {
        LOG_WARN(reader.Warning());
    }

    PASS_ERROR(init_obj_geometry(attrib, shapes, cmd_pool, model.vertex_buffer, model.index_buffer, model.subgeometries, model.model_bound));
    PASS_ERROR(init_obj_materials(shapes, cmd_pool, materials, model, textures, textures_size));

    RAISE_ERROR_OK();
}

ERROR_TYPE init_dummy_shaders(const obj_model& model, const VkBuffer ubo, shader_group& sgroup)
{
    sgroup.shader_modules.reserve(2);
    auto& vs = sgroup.shader_modules.emplace_back();
    VkShaderStageFlagBits vs_stage{};
    auto& fs = sgroup.shader_modules.emplace_back();
    VkShaderStageFlagBits fs_stage{};

    if (load_shader("./shaders/dummy.vert.glsl.spv", vk_utils::context::get().device(), vs, vs_stage) < 0) {
        RAISE_ERROR_FATAL(-1, "cannot load shader dummy.vert.glsl.spv");
    }

    if (load_shader("./shaders/dummy.frag.glsl.spv", vk_utils::context::get().device(), fs, fs_stage) < 0) {
        RAISE_ERROR_FATAL(-1, "cannot load shader dummy.frag.glsl.spv");
    }

    VkDescriptorSetLayoutBinding bindings[] {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        },
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        }
    };

    VkDescriptorSetLayoutCreateInfo desc_set_layout_info{};
    desc_set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    desc_set_layout_info.pNext = nullptr;
    desc_set_layout_info.flags = 0;
    desc_set_layout_info.bindingCount = 1;
    desc_set_layout_info.pBindings = &bindings[0];

    sgroup.shaders_layouts.reserve(2);
    auto& ubo_set_layout = sgroup.shaders_layouts.emplace_back();
    ubo_set_layout.init(vk_utils::context::get().device(), &desc_set_layout_info);

    desc_set_layout_info.pBindings = &bindings[1];
    auto& texture_set_layout = sgroup.shaders_layouts.emplace_back();
    texture_set_layout.init(vk_utils::context::get().device(), &desc_set_layout_info);

    VkDescriptorPoolSize descriptor_pool_sizes[] {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = static_cast<uint32_t>(model.subgeometries.size()),
        },
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = static_cast<uint32_t>(model.subgeometries.size()),
        }
    };

    VkDescriptorPoolCreateInfo desc_pool_info{};
    desc_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    desc_pool_info.pNext = nullptr;
    desc_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    desc_pool_info.maxSets = model.subgeometries.size() * 2;
    desc_pool_info.pPoolSizes = descriptor_pool_sizes;
    desc_pool_info.poolSizeCount = std::size(descriptor_pool_sizes);

    if (sgroup.desc_pool.init(vk_utils::context::get().device(), &desc_pool_info) != VK_SUCCESS) {
        RAISE_ERROR_FATAL(-1, "cannot init desc pool.");
    }


    for(auto& subgeom : model.subgeometries) {
        auto& s = sgroup.shaders.emplace_back();
        s.modules.emplace_back(vs);
        s.modules.emplace_back(fs);
        s.stages.emplace_back(vs_stage);
        s.stages.emplace_back(fs_stage);
        s.descriptor_set_layouts.emplace_back(ubo_set_layout);
        s.descriptor_set_layouts.emplace_back(texture_set_layout);

        VkDescriptorSetAllocateInfo desc_sets_alloc_info{};
        desc_sets_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        desc_sets_alloc_info.pNext = nullptr;
        desc_sets_alloc_info.descriptorPool = sgroup.desc_pool;
        desc_sets_alloc_info.descriptorSetCount = 1;

        desc_sets_alloc_info.pSetLayouts = ubo_set_layout;
        s.descriptor_sets.reserve(2);
        auto& ubo_desc_set = s.descriptor_sets.emplace_back();
        ubo_desc_set.init(vk_utils::context::get().device(), sgroup.desc_pool, &desc_sets_alloc_info, desc_sets_alloc_info.descriptorSetCount);

        desc_sets_alloc_info.pSetLayouts = texture_set_layout;
        auto& tex_desc_set = s.descriptor_sets.emplace_back();
        tex_desc_set.init(vk_utils::context::get().device(), sgroup.desc_pool, &desc_sets_alloc_info, desc_sets_alloc_info.descriptorSetCount);

        VkWriteDescriptorSet write_desc_set{};
        write_desc_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_desc_set.pNext = nullptr;
        write_desc_set.descriptorCount = 1;
        write_desc_set.dstArrayElement = 0;
        write_desc_set.dstBinding = 0;
        write_desc_set.dstSet = ubo_desc_set[0];
        write_desc_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        VkDescriptorBufferInfo ubo_info{};
        ubo_info.offset = 0;
        ubo_info.buffer = ubo;
        ubo_info.range = VK_WHOLE_SIZE;
        write_desc_set.pBufferInfo = &ubo_info;

        vkUpdateDescriptorSets(vk_utils::context::get().device(), 1, &write_desc_set, 0, nullptr);
        VkImageView img_view{nullptr};
        VkSampler sampler{nullptr};

        switch (subgeom.render_technique) {
            case BLINN_PHONG:
                {
                    auto& m = std::get<blinn_phong_material>(subgeom.material);
                    if (m.material_textures[BLINN_PHONG_DIFFUSE] >= 0) {
                        img_view = model.tex_image_views[m.material_textures[BLINN_PHONG_DIFFUSE]];
                        sampler = model.tex_samplers[m.material_textures[BLINN_PHONG_DIFFUSE]];
                    }
                }
                break;
            case PBR:
                break;
        }

        if (img_view != nullptr) {
            VkDescriptorImageInfo img_info{};
            img_info.imageView = img_view;
            img_info.sampler = sampler;
            img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            write_desc_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_desc_set.dstSet = tex_desc_set[0];
            write_desc_set.pBufferInfo = nullptr;
            write_desc_set.pImageInfo = &img_info;
            vkUpdateDescriptorSets(vk_utils::context::get().device(), 1, &write_desc_set, 0, nullptr);
        }
    }

    RAISE_ERROR_OK();
}

ERROR_TYPE record_obj_model_dummy_draw_commands(
    const obj_model& model,
    const shader_group& sgroup,
    const hello_triangle_app* app,
    VkCommandPool cmd_pool,
    vk_utils::cmd_buffers_handler& cmd_buffers,
    std::vector<vk_utils::pipeline_layout_handler>& pipelines_layouts,
    std::vector<vk_utils::graphics_pipeline_handler>& pipelines)
{
    init_geom_pipelines(
        model.subgeometries,
        sgroup.shaders,
        app->get_main_pass(),
        0,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        app->get_window_size(),
        pipelines,
        pipelines_layouts);

    auto framebuffers = app->get_main_framebuffers();

    VkCommandBufferAllocateInfo buffers_alloc_info{};
    buffers_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buffers_alloc_info.pNext = nullptr;
    buffers_alloc_info.commandPool = cmd_pool;
    buffers_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    buffers_alloc_info.commandBufferCount = framebuffers.size();

    cmd_buffers.init(vk_utils::context::get().device(), cmd_pool, &buffers_alloc_info, framebuffers.size());

    VkCommandBufferBeginInfo cmd_buffer_begin_info{};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.pNext = nullptr;
    cmd_buffer_begin_info.flags = 0;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    VkClearValue clear_values[] {
        {
            .color {.float32 = {0.2, 0.3, 0.7, 1.}}
        },
        {
            .depthStencil = {.depth = 1.0f, .stencil = 0}
        }
    };

    VkRenderPassBeginInfo pass_begin_info{};
    pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass_begin_info.pNext = nullptr;
    pass_begin_info.renderPass = app->get_main_pass();
    pass_begin_info.pClearValues = clear_values;
    pass_begin_info.clearValueCount = std::size(clear_values);
    pass_begin_info.renderArea = {
        .offset = {0, 0},
        .extent = app->get_window_size()
    };

    for (int i = 0; i < framebuffers.size(); ++i) {
        pass_begin_info.framebuffer = framebuffers[i];

        vkBeginCommandBuffer(cmd_buffers[i], &cmd_buffer_begin_info);
        vkCmdBeginRenderPass(cmd_buffers[i], &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        for (int j = 0; j < model.subgeometries.size(); j++) {
            vkCmdBindPipeline(cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[j]);
            VkDeviceSize vertex_buffer_offset = model.subgeometries[j].vertices_offset;
            vkCmdBindVertexBuffers(cmd_buffers[i], 0, 1, model.vertex_buffer, &vertex_buffer_offset);
            vkCmdBindIndexBuffer(cmd_buffers[i], model.index_buffer, model.subgeometries[j].indices_offset * sizeof(uint32_t), VK_INDEX_TYPE_UINT32);
            std::vector<VkDescriptorSet> desc_sets;
            desc_sets.reserve(sgroup.shaders[j].descriptor_sets.size());
            std::transform(sgroup.shaders[j].descriptor_sets.begin(), sgroup.shaders[j].descriptor_sets.end(), std::back_inserter(desc_sets), [](const vk_utils::descriptor_set_handler& set) {return set[0];});
            vkCmdBindDescriptorSets(cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_layouts[j], 0, sgroup.shaders[j].descriptor_sets.size(), desc_sets.data(), 0, nullptr);
            vkCmdDrawIndexed(cmd_buffers[i], model.subgeometries[j].indices_size, 1, 0, 0, 0);
        }
        vkCmdEndRenderPass(cmd_buffers[i]);
        vkEndCommandBuffer(cmd_buffers[i]);
    }

    RAISE_ERROR_OK();
}


std::optional<obj_model> g_MODEL;
std::optional<shader_group> g_DUMMY_SHADER_GROUP;
std::vector<vk_utils::pipeline_layout_handler> g_PIPELINE_LAYOUTS;
std::vector<vk_utils::graphics_pipeline_handler> g_GRAPHICS_PIPELINES;
vk_utils::vma_buffer_handler g_UBO{};
vk_utils::cmd_pool_handler g_COMMAND_POOL{};
vk_utils::cmd_buffers_handler g_CMD_BUFFERS{};


ERROR_TYPE init_dummy_model_resources(const char* path, const char** textures = nullptr, size_t textures_size = 0)
{
    VkCommandPoolCreateInfo cmd_pool_create_info{};
    cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_create_info.pNext = nullptr;
    cmd_pool_create_info.queueFamilyIndex = vk_utils::context::get().queue_family_index(vk_utils::context::QUEUE_TYPE_GRAPHICS);
    g_COMMAND_POOL.init(vk_utils::context::get().device(), &cmd_pool_create_info);
    g_MODEL.emplace();
    g_DUMMY_SHADER_GROUP.emplace();

    HANDLE_ERROR(create_buffer(g_UBO, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, sizeof(global_ubo)));
    HANDLE_ERROR(load_obj_model(path, g_COMMAND_POOL, *g_MODEL, textures, textures_size));
    HANDLE_ERROR(init_dummy_shaders(*g_MODEL, g_UBO, *g_DUMMY_SHADER_GROUP))
}

int main(int argn, const char** argv)
{
    hello_triangle_app app;

    app.set_on_create_callback([argn, argv](const hello_triangle_app* app) {
        if (argn <= 1) {
            return -1;
        }
        const char** textures_list = nullptr;

        if (argn > 2) {
            textures_list = argv + 2;
        }

        HANDLE_ERROR(init_dummy_model_resources(argv[1], textures_list, argn - 2));
        HANDLE_ERROR(record_obj_model_dummy_draw_commands(*g_MODEL, *g_DUMMY_SHADER_GROUP, app, g_COMMAND_POOL, g_CMD_BUFFERS, g_PIPELINE_LAYOUTS, g_GRAPHICS_PIPELINES));
        return 0;
    });

    app.set_on_update_callback([](const hello_triangle_app* app, const VkCommandBuffer** cmd_bufs, uint32_t* bufs_count) {
        static bool is_first = true;

        static float angle = 0.0;
        angle += 0.01;

        if (angle >= 360.0f) {
            angle = 0.0f;
        }

        static global_ubo ubo_data{};

        const float len = g_MODEL->model_bound.hi.z - g_MODEL->model_bound.lo.z;
        const float h = g_MODEL->model_bound.hi.y - g_MODEL->model_bound.lo.y;
        const float w = g_MODEL->model_bound.hi.x - g_MODEL->model_bound.lo.x;

        glm::vec3 center = g_MODEL->model_bound.lo + glm::vec3{len, h, w} * 0.5f;
//        center.y = -center.y;
        auto model = glm::identity<glm::mat4>();
        model = glm::translate(model, center);
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3{1.0f, 0.0f, 0.0f});
        model = glm::rotate(model, glm::radians(angle), glm::vec3{0.0f, 0.0f, 1.0f});
        ubo_data.model = model;

        ubo_data.projection = glm::perspectiveFov(glm::radians(90.0f), float(app->get_window_size().width), float(app->get_window_size().height), 0.01f, len * 10.0f);

        ubo_data.view = glm::lookAt(glm::vec3{0.0f, 0.0f, -len * 1.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        ubo_data.mvp = ubo_data.projection * ubo_data.view * ubo_data.model;

        void* mapped_data;
        vmaMapMemory(vk_utils::context::get().allocator(), g_UBO, &mapped_data);
        std::memcpy(mapped_data, &ubo_data, sizeof(ubo_data));
        vmaFlushAllocation(vk_utils::context::get().allocator(), g_UBO, 0, VK_WHOLE_SIZE);
        vmaUnmapMemory(vk_utils::context::get().allocator(), g_UBO);

        const VkCommandBuffer* handlers = g_CMD_BUFFERS;
        *cmd_bufs = &handlers[app->get_current_swapchain_img_index()];
        *bufs_count = 1;
        return 0;
    });

    app.set_on_resize_callback([](const hello_triangle_app* app) {
        g_CMD_BUFFERS.destroy();
        g_PIPELINE_LAYOUTS.clear();
        g_GRAPHICS_PIPELINES.clear();
        HANDLE_ERROR(record_obj_model_dummy_draw_commands(*g_MODEL, *g_DUMMY_SHADER_GROUP, app, g_COMMAND_POOL, g_CMD_BUFFERS, g_PIPELINE_LAYOUTS, g_GRAPHICS_PIPELINES));
        return 0;
    });

    app.set_on_destroy_callback([](const hello_triangle_app* app) {
        g_PIPELINE_LAYOUTS.clear();
        g_GRAPHICS_PIPELINES.clear();
        g_CMD_BUFFERS.destroy();
        g_COMMAND_POOL.destroy();
        g_UBO.destroy();
        g_MODEL.reset();
        g_DUMMY_SHADER_GROUP.reset();
        return 0;
    });


    if (app.run(argn, argv)) {
        LOG_ERROR("application run failed.");
        return -1;
    }

    return 0;
}