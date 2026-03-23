#include "camera.h"
#include "i_video.h"
glm::mat4 getInverseProjectionMatrix(const Camera camera) {
    // +Z forward, +Y up, right-handed
    glm::vec3 forward = camera.rotation * glm::vec3(0, 0, 1);
    glm::vec3 up      = camera.rotation * glm::vec3(0, 1, 0);

    // Use glm::lookAt for correct right-handed view
    glm::mat4 view = glm::lookAt(
        camera.position,
        camera.position + forward,
        up
    );

    // Projection
    float aspect = video_get_aspect_ratio();
    glm::mat4 proj;

    if (camera.projectionType == Perspective) {
        proj = glm::perspective(camera.fov, aspect, camera.near, camera.far);
    } else {
        float height = camera.fov * 0.5f;
        float width  = height * aspect;
        proj = glm::ortho(-width, width, -height, height, camera.near, camera.far);
    }

    // Vulkan clip space: Y down, depth 0→1
    proj[1][1] *= -1.0f;

    return glm::inverse(proj * view);
}