
#pragma once

#include <app/vk_app.hpp>

#include <vk_utils/obj_loader.hpp>
#include <vk_utils/camera.hpp>

#include<glm/mat4x4.hpp>
#include<glm/vec2.hpp>

#include <memory>
#include <vector>
#include <functional>

class base_obj_viewer_app : public app::vk_app
{
public:
    struct args_parser
    {
        args_parser();
        virtual ~args_parser() = default;
        virtual ERROR_TYPE parse_args(int argc, const char** argv);
        vk_utils::obj_loader::obj_model_info& get_model_info();

    protected:
        vk_utils::obj_loader::obj_model_info m_model_info{};
        std::vector<std::function<void(const char*)>> m_parse_functions;
    };

    base_obj_viewer_app(const char* app_name, std::unique_ptr<args_parser> args_parser = std::make_unique<base_obj_viewer_app::args_parser>());
    ERROR_TYPE run(int argc, const char** argv) override;

protected:

    struct global_ubo
    {
        glm::mat4 projection = glm::mat4{1.0f};
        glm::mat4 view_projection = glm::mat4{1.0f};
        glm::mat4 view = glm::mat4{1.0f};
        glm::mat4 model = glm::mat4{1.0f};
        glm::mat4 mvp = glm::mat4{1.0f};
    };

    enum model_state
    {
        MODEL_MOUNT_BIT = 1,
        MODEL_ROTATE_POSITIVE_X_BIT = 2,
        MODEL_ROTATE_NEGATIVE_X_BIT = 4,
        MODEL_ROTATE_POSITIVE_Y_BIT = 8,
        MODEL_ROTATE_NEGATIVE_Y_BIT = 16,
        MODEL_RELEASE_BIT = 32
    };

    ERROR_TYPE on_vulkan_initialized() override;

    ERROR_TYPE on_mouse_scroll(double, double) override;
    ERROR_TYPE on_mouse_button(int, int, int) override;
    ERROR_TYPE on_mouse_moved(uint64_t x, uint64_t y) override;

    ERROR_TYPE draw_frame() override;
     
    std::unique_ptr<args_parser> m_args_parser;

    vk_utils::obj_loader::obj_model m_model{};
    vk_utils::vma_buffer_handler m_ubo{};
    vk_utils::cmd_pool_handler m_command_pool{};
    vk_utils::camera m_camera;

    glm::vec2 m_mouse_pos{0, 0};
    uint64_t m_model_state{0};
};