#include <app/vk_app.hpp>
#include <logger/log.hpp>

#include <vk_utils/context.hpp>
#include <vk_utils/tools.hpp>
#include <vk_utils/obj_loader.hpp>
#include <vk_utils/camera.hpp>
#include <vk_utils/handlers.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <vector>
#include <cstring>

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


struct global_ubo
{
    glm::mat4 projection = glm::identity<glm::mat4>();
    glm::mat4 view_projection = glm::identity<glm::mat4>();
    glm::mat4 view = glm::identity<glm::mat4>();
    glm::mat4 model = glm::identity<glm::mat4>();
    glm::mat4 mvp = glm::identity<glm::mat4>();
};


class dummy_obj_viewer_app : public app::vk_app
{
public:
    dummy_obj_viewer_app(const char* app_name, int argc, const char** argv);


protected:
    enum app_state
    {
        MODEL_MOUNT_BIT = 1,
        MODEL_ROTATE_POSITIVE_X_BIT = 2,
        MODEL_ROTATE_NEGATIVE_X_BIT = 4,
        MODEL_ROTATE_POSITIVE_Y_BIT = 8,
        MODEL_ROTATE_NEGATIVE_Y_BIT = 16,
        MODEL_RELEASE_BIT = 32
    };

    ERROR_TYPE on_vulkan_initialized() override;
    ERROR_TYPE on_swapchain_recreated() override;
    
    ERROR_TYPE on_mouse_scroll(double, double) override;
    ERROR_TYPE on_mouse_button(int, int, int) override;
    ERROR_TYPE on_mouse_moved(uint64_t x, uint64_t y) override;

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

    vk_utils::obj_loader::obj_model m_model{};
    shader_group m_dummy_shader_group{};

    vk_utils::vma_buffer_handler m_vert_staging_buffer{};
    vk_utils::vma_buffer_handler m_index_staging_buffer{};

    std::vector<vk_utils::pipeline_layout_handler> m_pipelines_layout{};
    std::vector<vk_utils::graphics_pipeline_handler> m_graphics_pipelines{};
    vk_utils::vma_buffer_handler m_ubo{};
    vk_utils::cmd_pool_handler m_command_pool{};

    vk_utils::pass_handler m_main_render_pass{};
    vk_utils::vma_image_handler m_main_msaa_image{};
    vk_utils::vma_image_handler m_main_depth_image{};

    vk_utils::image_view_handler m_main_msaa_image_view{};
    vk_utils::image_view_handler m_main_depth_image_view{};

    std::vector<vk_utils::framebuffer_handler> m_main_pass_framebuffers{};

    vk_utils::cmd_buffers_handler m_command_buffers{};
    vk_utils::camera m_camera;

    glm::vec2 m_mouse_pos{0, 0};
    int m_state{0};
};