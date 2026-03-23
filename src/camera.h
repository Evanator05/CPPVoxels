#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RIGHT_HANDED
#include "glm/vec3.hpp"
#include "glm/mat4x4.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

enum ProjectionTypes { Perspective, Orthographic };

typedef struct _Camera {
    glm::vec3 position;
    glm::qua<float> rotation;
    float near;
    float far;
    float fov;
    ProjectionTypes projectionType;
    // Perspective
    // fov = vertical field of view (radians)

    // Orthographic
    // fov = vertical view size (world units)
} Camera;

glm::mat4 getInverseProjectionMatrix(Camera camera);