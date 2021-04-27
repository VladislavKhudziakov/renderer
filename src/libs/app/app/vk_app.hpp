#pragma once

#include <app/base_app.hpp>

#include <vk_utils/handlers.hpp>

#include <optional>

class GLFWwindow;

namespace app
{
    class vk_app : public base_app
    {
    public:
        vk_app(const char* app_name, int argc, const char** argv);
        ERROR_TYPE run() override;

    protected:
        enum input_action
        {
            VK_APP_RELEASE,
            VK_APP_PRESS
        };

        enum action
        {
            VK_APP_MOUSE_LEFT,
            VK_APP_MOUSE_RIGHT,
            VK_APP_MOUSE_MIDDLE,
            VK_APP_MOUSE3,
            VK_APP_MOUSE4,
            VK_APP_MOUSE5,
            VK_APP_MOUSE6,
            VK_APP_MOUSE7,
            VK_APP_MOUSE8
        };

        struct app_data
        {
            std::string app_name;
            uint32_t argc;
            const char** argv;

            std::vector<const char*> instance_extensions{};
            std::vector<const char*> device_extensions{};
        };

        struct swapchain_data
        {
            vk_utils::swapchain_handler swapchain{};

            std::optional<VkSwapchainCreateInfoKHR> swapchain_info{};

            std::vector<VkImage> swapchain_images{};
            std::vector<vk_utils::img_view_handler> swapchain_images_views{};
            std::vector<vk_utils::semaphore_handler> image_acquired_semaphores{};
            std::vector<vk_utils::semaphore_handler> render_finished_semaphores{};
            std::vector<vk_utils::fence_handler> render_finished_fences{};
            std::vector<VkFence> frames_in_flight_fences{};

            uint32_t current_image{0};
            uint32_t current_frame{0};
            uint32_t frames_count{0};
        };

        ERROR_TYPE init_window() override;
        ERROR_TYPE run_main_loop() override;
        ERROR_TYPE cleanup() override;

        ERROR_TYPE init_vulkan();
        ERROR_TYPE create_swapchain();
        ERROR_TYPE request_swapchain_images();

        ERROR_TYPE begin_frame();
        ERROR_TYPE finish_frame(VkCommandBuffer cmd_buffer);

        VkExtent2D get_surface_images_extent();

        virtual ERROR_TYPE on_vulkan_initialized() = 0;
        virtual ERROR_TYPE on_swapchain_recreated() = 0;
        virtual ERROR_TYPE draw_frame() = 0;

        virtual ERROR_TYPE on_window_size_changed(int w, int h)
        {
            RAISE_ERROR_OK();
        }

        virtual ERROR_TYPE on_mouse_moved(int x, int y)
        {
            RAISE_ERROR_OK();
        }

        virtual ERROR_TYPE on_mouse_button(int button, int action, int mods)
        {
            RAISE_ERROR_OK();
        }

        virtual ERROR_TYPE on_mouse_scroll(double xoffset, double yoffset)
        {
            RAISE_ERROR_OK();
        }

         virtual ERROR_TYPE on_mouse_moved(uint64_t x, uint64_t y)
         {
            RAISE_ERROR_OK();
         }

         virtual ERROR_TYPE on_mouse_enter(bool entered)
         {
             RAISE_ERROR_OK();
         }

        VkSurfaceCapabilitiesKHR m_surface_capabilities{};
        swapchain_data m_swapchain_data{};
        app_data m_app_info{};

    private:
        static void window_resized_callback(GLFWwindow* window, int w, int h);
        static void mouse_button_callback(GLFWwindow*, int, int, int);
        static void cursor_pos_callback(GLFWwindow*, double, double);
        static void cursor_enter_callback(GLFWwindow*, int);
        static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

        GLFWwindow* m_window{};
    };
} // namespace app
