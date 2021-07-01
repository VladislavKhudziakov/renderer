#include "base_obj_viewer_app.hpp"

#include <vk_utils/context.hpp>
#include <vk_utils/tools.hpp>

#include <glm/gtx/euler_angles.hpp>

base_obj_viewer_app::base_obj_viewer_app(const char* app_name, std::unique_ptr<args_parser> args_parser)
    : vk_app(app_name)
    , m_args_parser(std::move(args_parser))
{
    m_camera.eye_position = {0, 0, -2};
    m_camera.target_position = {0, 0, 0};
    m_camera.fov = 90.0f;
}


ERROR_TYPE base_obj_viewer_app::run(int argc, const char** argv)
{
    HANDLE_ERROR(vk_app::run(argc, argv));
    RAISE_ERROR_OK();
}


ERROR_TYPE base_obj_viewer_app::on_vulkan_initialized()
{
    VkCommandPoolCreateInfo cmd_pool_create_info{};
    cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_create_info.pNext = nullptr;
    cmd_pool_create_info.queueFamilyIndex = vk_utils::context::get().queue_family_index(vk_utils::context::QUEUE_TYPE_GRAPHICS);
    m_command_pool.init(vk_utils::context::get().device(), &cmd_pool_create_info);
    PASS_ERROR(vk_utils::create_buffer(m_ubo, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, sizeof(global_ubo)));

    HANDLE_ERROR(m_args_parser->parse_args(m_app_info.argc, m_app_info.argv));
    vk_utils::obj_loader loader{};
    PASS_ERROR(loader.load_model(
      m_args_parser->get_model_info(), 
      vk_utils::context::get().queue(vk_utils::context::QUEUE_TYPE_GRAPHICS), 
      vk_utils::context::get().queue_family_index(vk_utils::context::QUEUE_TYPE_GRAPHICS), 
      m_command_pool, 
      m_model));

    RAISE_ERROR_OK();
}


ERROR_TYPE base_obj_viewer_app::on_mouse_scroll(double x, double y)
{
    auto dir = glm::normalize(m_camera.target_position - m_camera.eye_position);
    m_camera.eye_position += dir * float(y) * 0.05f;
    return vk_app::on_mouse_scroll(x, y);
}


ERROR_TYPE base_obj_viewer_app::on_mouse_button(int button, int action, int mode)
{
    if (button == VK_APP_MOUSE_LEFT) {
        if (action == VK_APP_PRESS) {
            m_model_state &= ~MODEL_RELEASE_BIT;
            m_model_state |= MODEL_MOUNT_BIT;
        } else {
            m_model_state &= ~MODEL_MOUNT_BIT;
            m_model_state |= MODEL_RELEASE_BIT;
        }
    }

    return vk_app::on_mouse_button(button, action, mode);
}

ERROR_TYPE base_obj_viewer_app::on_mouse_moved(uint64_t x, uint64_t y)
{
    static glm::vec2 last_pos{-1, -1};

    if (m_model_state & MODEL_MOUNT_BIT) {
        if (x != last_pos.x) {
            m_model_state |= (x - last_pos.x > 0 ? MODEL_ROTATE_POSITIVE_X_BIT : MODEL_ROTATE_NEGATIVE_X_BIT);
        }

        if (y != last_pos.y) {
            m_model_state |= (y - last_pos.y > 0 ? MODEL_ROTATE_POSITIVE_Y_BIT : MODEL_ROTATE_NEGATIVE_Y_BIT);
        }
    }

    last_pos = {x, y};

    return vk_app::on_mouse_moved(x, y);
}


ERROR_TYPE base_obj_viewer_app::draw_frame()
{
    static global_ubo ubo_data{};

    auto model = glm::identity<glm::mat4>();

    auto view_transform = glm::identity<glm::mat4>();

    float yAngle = 0.0f;
    float xAngle = 0.0f;

    if (m_model_state & MODEL_ROTATE_POSITIVE_Y_BIT) {
        m_model_state &= ~MODEL_ROTATE_POSITIVE_Y_BIT;
        yAngle += 0.5f;
    }

    if (m_model_state & MODEL_ROTATE_NEGATIVE_Y_BIT) {
        m_model_state &= ~MODEL_ROTATE_NEGATIVE_Y_BIT;
        yAngle -= 0.5f;
    }

    if (m_model_state & MODEL_ROTATE_POSITIVE_X_BIT && yAngle == 0) {
        m_model_state &= ~MODEL_ROTATE_POSITIVE_X_BIT;
        xAngle += 0.5f;
    }

    if (m_model_state & MODEL_ROTATE_NEGATIVE_X_BIT && yAngle == 0) {
        m_model_state &= ~MODEL_ROTATE_NEGATIVE_X_BIT;
        xAngle -= 0.5f;
    }

    if (xAngle != 0 || yAngle != 0) {
        view_transform = glm::yawPitchRoll(glm::radians(xAngle), glm::radians(yAngle), 0.0f);
    }

    m_camera.eye_position = glm::mat3(view_transform) * m_camera.eye_position;

    m_camera.update(m_swapchain_data.swapchain_info->imageExtent.width, m_swapchain_data.swapchain_info->imageExtent.height);
    ubo_data.model = model * m_model.model_transform;
    ubo_data.view_projection = m_camera.view_proj_matrix;
    ubo_data.projection = m_camera.proj_matrix;
    ubo_data.view = m_camera.view_matrix;
    ubo_data.mvp = ubo_data.projection * ubo_data.view * ubo_data.model;

    void* mapped_data;
    vmaMapMemory(vk_utils::context::get().allocator(), m_ubo, &mapped_data);
    std::memcpy(mapped_data, &ubo_data, sizeof(ubo_data));
    vmaFlushAllocation(vk_utils::context::get().allocator(), m_ubo, 0, VK_WHOLE_SIZE);
    vmaUnmapMemory(vk_utils::context::get().allocator(), m_ubo);

    RAISE_ERROR_OK();
}


base_obj_viewer_app::args_parser::args_parser()
{
    m_parse_functions.push_back([this](const char* curr_arg) {
        if (auto rtech = strstr(curr_arg, "--render_technique="); rtech != nullptr) {
            rtech += strlen("--render_technique=");
            if (strcmp(rtech, "PBR")) {
                RAISE_ERROR_FATAL(-1, "PBR currently unsupported.");
            } else if (strcmp(rtech, "PHONG")) {
                m_model_info.model_render_technique = vk_utils::obj_loader::PHONG;
            } else {
                RAISE_ERROR_FATAL(-1, "invalid render technique name.");
            }
        }
    });

    m_parse_functions.push_back([this](const char* curr_arg) {
        static std::unordered_map<std::string, std::function<void(const std::string&, std::array<std::string, vk_utils::obj_loader::PHONG_SIZE>&, std::array<std::string, vk_utils::obj_loader::PBR_SIZE>&)>> textures_loaders{
            {"diffuse", [](const std::string& path, auto& phong_list, auto& pbr_list) { phong_list[vk_utils::obj_loader::PHONG_DIFFUSE] = path; pbr_list[vk_utils::obj_loader::PBR_DIFFUSE] = path; }},
            {"specular", [](const std::string& path, auto& phong_list, auto& pbr_list) { phong_list[vk_utils::obj_loader::PHONG_SPECULAR] = path; }},
            {"specular_highlight", [](const std::string& path, auto& phong_list, auto& pbr_list) { phong_list[vk_utils::obj_loader::PHONG_SPECULAR_HIGHLIGHT] = path; }},
            {"bump", [](const std::string& path, auto& phong_list, auto& pbr_list) { phong_list[vk_utils::obj_loader::PHONG_BUMP] = path; }},
            {"displacement", [](const std::string& path, auto& phong_list, auto& pbr_list) { phong_list[vk_utils::obj_loader::PHONG_DISPLACEMENT] = path; }},
            {"alpha", [](const std::string& path, auto& phong_list, auto& pbr_list) { phong_list[vk_utils::obj_loader::PHONG_ALPHA] = path; }},
            {"reflection", [](const std::string& path, auto& phong_list, auto& pbr_list) { phong_list[vk_utils::obj_loader::PHONG_REFLECTION] = path; }}};

        if (auto subgeom = strstr(curr_arg, "--subgeometry_textures="); subgeom != nullptr) {
            auto subgeom_len = strlen("--subgeometry_textures=");
            subgeom += subgeom_len;
            auto arg_end = subgeom + strlen(subgeom);

            while (subgeom < arg_end && (*subgeom == ' ' || *subgeom == '\n')) {
                subgeom++;
            }

            if (subgeom >= arg_end) {
                return;
            }

            auto subgeom_begin = subgeom;

            while (subgeom < arg_end && (*subgeom != ' ' && *subgeom != '\n')) {
                subgeom++;
            }

            if (subgeom >= arg_end) {
                return;
            }

            auto subgeom_end = subgeom;

            std::string subgeom_name{subgeom_begin, subgeom_end};

            std::array<std::string, vk_utils::obj_loader::PHONG_SIZE>& phong_textures = m_model_info.phong_textures[subgeom_name];
            std::array<std::string, vk_utils::obj_loader::PBR_SIZE>& pbr_textures = m_model_info.pbr_textures[subgeom_name];

            std::string texture_type;

            for (auto ch_begin = subgeom; ch_begin < arg_end; ch_begin++) {
                while (ch_begin < arg_end && (*ch_begin == ' ' || *ch_begin == '\n'))
                    ch_begin++;
                auto tex_data_begin = ch_begin;
                while (ch_begin < arg_end && (*ch_begin != ' ' && *ch_begin != '\n'))
                    ch_begin++;
                auto tex_data_end = ch_begin;

                if (texture_type.empty()) {
                    texture_type = {tex_data_begin, tex_data_end};
                    LOG_INFO(texture_type);
                    continue;
                }

                std::string tex_path{tex_data_begin, tex_data_end};
                auto tex_loader_it = textures_loaders.find(texture_type);

                if (tex_loader_it != textures_loaders.end()) {
                    tex_loader_it->second(tex_path, phong_textures, pbr_textures);
                } else {
                    LOG_WARN("invalid texture type ", texture_type, " assigned to texture ", tex_path);
                }

                texture_type.clear();
            }
        }
    });
}

ERROR_TYPE base_obj_viewer_app::args_parser::parse_args(int argc, const char** argv)
{
    if (argc < 2) {
        RAISE_ERROR_FATAL(-1, "invalid arguments count.");
    }

    m_model_info.model_path = argv[1];

    for (size_t i = 2; i < argc; i++) {
        auto curr_arg = argv[i];
        for (const auto& f : m_parse_functions) {
            f(curr_arg);
        }
    }
   
    RAISE_ERROR_OK();
}

vk_utils::obj_loader::obj_model_info& base_obj_viewer_app::args_parser::get_model_info()
{
    return m_model_info;
}
