#include "../include/camera.hpp"
#include <cmath>

/*

   camera.cpp contains the implementations for camera.hpp — deriving the
   direction vectors from the camera's yaw/pitch. Everything else (view and
   projection matrices) is left to the user.

*/

glm::vec3 cameraForward(const Camera &camera) {
  // Spherical coordinates mapped to Cartesian, with yaw = 0, pitch = 0
  // looking down -z:
  //   forward = (cos(pitch) * sin(yaw), sin(pitch), -cos(pitch) * cos(yaw))
  return glm::vec3(std::cos(camera.pitch) * std::sin(camera.yaw),
                   std::sin(camera.pitch),
                   -std::cos(camera.pitch) * std::cos(camera.yaw));
}

glm::vec3 cameraRight(const Camera &camera) {
  // Degenerate at pitch = +/-90 degrees (looking straight up/down): forward
  // becomes parallel to world up and the cross product collapses to zero.
  return glm::normalize(
      glm::cross(cameraForward(camera), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 cameraUp(const Camera &camera) {
  return glm::cross(cameraRight(camera), cameraForward(camera));
}
