#pragma once

#include <errors/error_handler.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>


namespace vk_utils
{
    class camera
    {
    public:
        enum type {
            TYPE_PERSPECTIVE,
            TYPE_ORTHO,
            TYPE_FISHEYE
        };

        camera() = default;

        ERROR_TYPE init(type);
        ERROR_TYPE update(float extent_width, float extent_height);

        float fov{90};
        float z_near{0.01};
        float z_far{100.0};

        glm::vec3 eye_position{0, 0, -1};
        glm::vec3 target_position{0, 0, 0};
        glm::vec3 up{0, 1, 0};

        glm::mat4 view_matrix{};
        glm::mat4 proj_matrix{};
        glm::mat4 view_proj_matrix{};

    private:
        type m_curr_type{};
    };
}
