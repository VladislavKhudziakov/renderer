

#include "camera.hpp"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>


ERROR_TYPE vk_utils::camera::update(float extent_width, float extent_height)
{
    switch (m_curr_type) {
        case TYPE_PERSPECTIVE:
            proj_matrix = glm::perspectiveFov(glm::radians(fov), extent_width, extent_height, 0.01f, 100.0f);
            break;
        case TYPE_ORTHO:
            proj_matrix = glm::ortho(-extent_width * 0.5f, extent_width * 0.5f, -extent_height * 0.5f, extent_height * 0.5f, z_near, z_far);
            break;
        case TYPE_FISHEYE:
            RAISE_ERROR_WARN(-1, "unsupported porjection type.");
            break;
    }
    view_matrix = glm::lookAt(eye_position, target_position, up);
    view_proj_matrix = proj_matrix * view_matrix;
    RAISE_ERROR_OK();
}


ERROR_TYPE vk_utils::camera::init(vk_utils::camera::type type)
{
    m_curr_type = type;
    RAISE_ERROR_OK();
}
