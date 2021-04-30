#pragma once

#include <base_obj_viewer_app.hpp>

#include <vector>

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

class test_push_constants_app : public base_obj_viewer_app
{
public:
    test_push_constants_app(const char* app_name, int argc, const char** argv);

protected:
    struct push_constant_data {
        glm::vec4 color;
        glm::mat4 transform;
    };

    ERROR_TYPE on_vulkan_initialized() override;
    ERROR_TYPE on_swapchain_recreated() override;
    ERROR_TYPE draw_frame() override;

    ERROR_TYPE init_render_passes();
    ERROR_TYPE init_framebuffers();
    ERROR_TYPE init_shaders();
    ERROR_TYPE init_pipelines();
    ERROR_TYPE record_command_buffers();

    std::vector<VkPipeline> m_graphics_pipelines{};

    std::unordered_map<uint32_t, vk_utils::graphics_pipeline_handler> m_pipelines_handlers;

    vk_utils::pass_handler m_main_render_pass{};
    std::vector<vk_utils::framebuffer_handler> m_main_pass_framebuffers{};

    vk_utils::vma_image_handler m_main_depth_image{};
    vk_utils::image_view_handler m_main_depth_image_view{};

    vk_utils::pipeline_layout_handler m_pipeline_layout{};
    vk_utils::descriptor_set_layout_handler m_descriptor_set_layout{};
    vk_utils::descriptor_pool_handler m_descriptor_pool{};
    vk_utils::descriptor_set_handler m_descriptor_set{};

    vk_utils::shader_module_handler m_vert_shader{};
    vk_utils::shader_module_handler m_frag_shader{};

    vk_utils::cmd_buffers_handler m_command_buffers{};
};