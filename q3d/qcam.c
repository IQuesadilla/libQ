#include "qcam.h"

void updateCameraVectors(qcam_t *cam) {
  // Calculate the new Front vector
  cam->Front[0] = cos(glm_rad(cam->Yaw)) * cos(glm_rad(cam->Pitch));
  cam->Front[1] = sin(glm_rad(cam->Pitch));
  cam->Front[2] = sin(glm_rad(cam->Yaw)) * cos(glm_rad(cam->Pitch));
  glm_normalize(cam->Front);

  // Also re-calculate the Right and Up vector
  glm_cross(cam->Front, cam->WorldUp, cam->Right);
  // Normalize the vectors, because their length gets
  // closer to 0 the more you look up or down which
  // results in slower movement.
  glm_normalize(cam->Right);

  if (cam->UpIsWorldUp) {
    glm_vec3_copy(cam->WorldUp, cam->Up);
  } else {
    glm_cross(cam->Right, cam->Front, cam->Up);
    glm_normalize(cam->Up);
  }
}

void qcam_init_default(qcam_t *cam) {
  glm_vec3_copy((vec3){0.0f, 0.0f, -1.0f}, cam->Front);
  cam->MovementSpeed = SPEED;
  cam->MouseSensitivity = SENSITIVITY;
  cam->Zoom = ZOOM;
  cam->BinarySensitivity = BINARYSENSITIVITY;
  glm_vec3_copy((vec3){0.0f, 0.0f, 0.0f}, cam->Position);
  glm_vec3_copy((vec3){0.0f, 1.0f, 0.0f}, cam->WorldUp);
  cam->Yaw = YAW;
  cam->Pitch = PITCH;
  cam->UpIsWorldUp = true;
  updateCameraVectors(cam);
}

void qcam_init(qcam_t *cam, float posX, float posY, float posZ, float upX,
               float upY, float upZ, float yaw, float pitch) {
  glm_vec3_copy((vec3){0.0f, 0.0f, -1.0f}, cam->Front);
  cam->MovementSpeed = SPEED;
  cam->Zoom = ZOOM;
  cam->BinarySensitivity = BINARYSENSITIVITY;
  cam->Position[0] = posX;
  cam->Position[1] = posY;
  cam->Position[2] = posZ;
  cam->WorldUp[0] = upX;
  cam->WorldUp[1] = upY;
  cam->WorldUp[2] = upZ;
  cam->Yaw = yaw;
  cam->Pitch = pitch;
  cam->UpIsWorldUp = true;
  updateCameraVectors(cam);
}

void qcam_get_view(qcam_t *cam, mat4 m) {
  vec3 center;
  glm_vec3_add(cam->Position, cam->Front, center);
  glm_lookat(cam->Position, center, cam->Up, m);
}

void qcam_get_rot_view(qcam_t *cam, mat4 m) {
  vec3 eye;
  glm_vec3_mul(cam->Front, (vec3){-1.f, -1.f, -1.f}, eye);
  glm_normalize(eye);
  glm_lookat(eye, (vec3){0.f, 0.f, 0.f}, cam->WorldUp, m);
}

void qcam_get_proj(qcam_t *cam, float AspectRatio, float NearDepth,
                   float FarDepth) {
  glm_perspective(glm_rad(cam->Zoom), AspectRatio, NearDepth, FarDepth,
                  cam->ProjectionMatrix);
}

float qcam_set_view(qcam_t *cam, int x, int y) {
  // glViewport(0, 0, x, y);
  return ((float)x) / ((float)y);
}

void qcam_process_keyboard(qcam_t *cam, qcam_direction_t inDirection,
                           bool down) {
  if (inDirection == FORWARD)
    cam->direction.FORWARD = down;
  if (inDirection == BACKWARD)
    cam->direction.BACKWARD = down;
  if (inDirection == LEFT)
    cam->direction.LEFT = down;
  if (inDirection == RIGHT)
    cam->direction.RIGHT = down;
  if (inDirection == UP)
    cam->direction.UP = down;
  if (inDirection == DOWN)
    cam->direction.DOWN = down;
  if (inDirection == V_UP)
    cam->direction.V_UP = down;
  if (inDirection == V_DOWN)
    cam->direction.V_DOWN = down;
  if (inDirection == V_RIGHT)
    cam->direction.V_RIGHT = down;
  if (inDirection == V_LEFT)
    cam->direction.V_LEFT = down;
}

void qcam_process_mouse(qcam_t *cam, float xoffset, float yoffset) {
  bool constrainPitch = true;
  float delta = cam->MouseSensitivity * (cam->Zoom / 45.0f);
  xoffset *= delta;
  yoffset *= delta;

  cam->Yaw += xoffset;
  cam->Pitch -= yoffset;

  // Make sure that when pitch is out of bounds, screen doesn't get flipped
  if (constrainPitch) {
    if (cam->Pitch > 89.0f)
      cam->Pitch = 89.0f;
    if (cam->Pitch < -89.0f)
      cam->Pitch = -89.0f;
  }

  // Update Front, Right and Up Vectors using the updated Euler angles
  updateCameraVectors(cam);
}

void qcam_process_scroll(qcam_t *cam, float yoffset) {
  if (cam->Zoom >= 1.0f && cam->Zoom <= 45.0f)
    cam->Zoom -= yoffset;
  if (cam->Zoom <= 1.0f)
    cam->Zoom = 1.0f;
  if (cam->Zoom >= 45.0f)
    cam->Zoom = 45.0f;
}

void qcam_input_update(qcam_t *cam, float deltaTime) {
  float velocity = cam->MovementSpeed * deltaTime;
  if (cam->direction.FORWARD)
    glm_vec3_muladds(cam->Front, velocity, cam->Position);
  // Position += Front * velocity;
  if (cam->direction.BACKWARD)
    glm_vec3_muladds(cam->Front, -velocity, cam->Position);
  // Position -= Front * velocity;
  if (cam->direction.LEFT)
    glm_vec3_muladds(cam->Right, -velocity, cam->Position);
  // Position -= Right * velocity;
  if (cam->direction.RIGHT)
    glm_vec3_muladds(cam->Right, velocity, cam->Position);
  // Position += Right * velocity;
  if (cam->direction.UP)
    glm_vec3_muladds(cam->Up, velocity, cam->Position);
  // Position += Up * velocity;
  if (cam->direction.DOWN)
    glm_vec3_muladds(cam->Up, -velocity, cam->Position);
  // Position -= Up * velocity;
  if (cam->direction.V_UP)
    qcam_process_mouse(cam, 0.f, -cam->BinarySensitivity);
  if (cam->direction.V_DOWN)
    qcam_process_mouse(cam, 0.f, cam->BinarySensitivity);
  if (cam->direction.V_RIGHT)
    qcam_process_mouse(cam, cam->BinarySensitivity, 0.f);
  if (cam->direction.V_LEFT)
    qcam_process_mouse(cam, -cam->BinarySensitivity, 0.f);
}
