#pragma once

#include <base_obj_viewer_app.hpp>

#include <vector>


class dummy_obj_viewer_app : public base_obj_viewer_app
{
public:
    dummy_obj_viewer_app(const char* app_name, int argc, const char** argv);

protected:
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

    ERROR_TYPE on_vulkan_initialized() override;
    ERROR_TYPE on_swapchain_recreated() override;

    ERROR_TYPE draw_frame() override;
    ERROR_TYPE on_window_size_changed(int w, int h) override;
    ERROR_TYPE init_dummy_shaders(
        const vk_utils::obj_loader::obj_model& model,
        const VkBuffer ubo,
        shader_group& sgroup);
    ERROR_TYPE init_dummy_model_resources(const char* path, const char** textures = nullptr, size_t textures_size = 0);

    ERROR_TYPE init_geom_pipelines(
        const vk_utils::obj_loader::obj_model& model,
        const std::vector<shader>& shaders,
        VkPrimitiveTopology topo,
        VkExtent2D input_viewport,
        std::vector<vk_utils::graphics_pipeline_handler>& pipelines,
        std::vector<vk_utils::pipeline_layout_handler>& pipeline_layouts);

    ERROR_TYPE record_obj_model_dummy_draw_commands(
        const vk_utils::obj_loader::obj_model& model,
        const shader_group& sgroup,
        VkCommandPool cmd_pool,
        vk_utils::cmd_buffers_handler& cmd_buffers,
        std::vector<vk_utils::pipeline_layout_handler>& pipelines_layouts,
        std::vector<vk_utils::graphics_pipeline_handler>& pipelines);


    ERROR_TYPE init_main_render_pass();
    ERROR_TYPE init_main_frame_buffers();
    
    shader_group m_dummy_shader_group{};

    vk_utils::vma_buffer_handler m_vert_staging_buffer{};
    vk_utils::vma_buffer_handler m_index_staging_buffer{};

    std::vector<vk_utils::pipeline_layout_handler> m_pipelines_layout{};
    std::vector<vk_utils::graphics_pipeline_handler> m_graphics_pipelines{};

    vk_utils::pass_handler m_main_render_pass{};
    vk_utils::vma_image_handler m_main_msaa_image{};
    vk_utils::vma_image_handler m_main_depth_image{};

    vk_utils::image_view_handler m_main_msaa_image_view{};
    vk_utils::image_view_handler m_main_depth_image_view{};

    std::vector<vk_utils::framebuffer_handler> m_main_pass_framebuffers{};

    vk_utils::cmd_buffers_handler m_command_buffers{};
};