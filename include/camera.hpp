#pragma once

// Includes
#include <glm/glm.hpp>

/*

   camera.hpp holds the basic camera struct for the library. It is a pure
   data holder: position + orientation (yaw/pitch). Building view and
   projection matrices from it is left up to the user, so they stay in full
   control of their MVP math.

   Convention: yaw = 0, pitch = 0 looks down -z (matching the engine's
   shaders). pitch rotates around the camera's horizontal axis, yaw around
   the world Y axis.

*/

struct Camera {
  glm::vec3 position = glm::vec3(0.0f);
  float yaw = 0.0f;   // radians, rotation around the world Y axis
  float pitch = 0.0f; // radians, rotation around the horizontal axis
};

// Where the camera is looking (unit vector)
glm::vec3 cameraForward(const Camera &camera);

// The camera's local right (+x when looking down -z) and up vectors
glm::vec3 cameraRight(const Camera &camera);
glm::vec3 cameraUp(const Camera &camera);
