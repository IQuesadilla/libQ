#ifndef QCAM_H
#define QCAM_H

// #include "glad.h"
// #include "GL/glew.h"
// #include "glm/glm.hpp"
// #include "glm/gtc/matrix_transform.hpp"
#include <cglm/cglm.h>

// Defines several possible options for camera movement. Used as abstraction to
// stay away from window-system specific input methods
enum qcam_direction {
  FORWARD,
  BACKWARD,
  LEFT,
  RIGHT,
  UP,
  DOWN,
  V_UP,
  V_DOWN,
  V_RIGHT,
  V_LEFT,
  NONE
};
typedef enum qcam_direction qcam_direction_t;

struct qcam_movement {
  bool FORWARD;
  bool BACKWARD;
  bool LEFT;
  bool RIGHT;
  bool UP;
  bool DOWN;
  bool V_UP;
  bool V_DOWN;
  bool V_RIGHT;
  bool V_LEFT;
};
typedef struct qcam_movement qcam_movement_t;

// Default camera values
const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 100.0f;
const float SENSITIVITY = 0.1f;
const float ZOOM = 45.0f;
const float BINARYSENSITIVITY = 1.0f;

// An abstract camera class that processes input and calculates the
// corresponding Euler Angles, Vectors and Matrices for use in OpenGL
struct qcam {
  // Camera Attributes
  vec3 Position;
  vec3 Front;
  vec3 Up;
  vec3 Right;
  vec3 WorldUp;
  // Euler Angles
  float Yaw;
  float Pitch;
  // Camera options
  float MovementSpeed;
  float MouseSensitivity;
  float Zoom;
  float BinarySensitivity;

  bool UpIsWorldUp;

  qcam_movement_t direction;
  mat4 ProjectionMatrix;
};
typedef struct qcam qcam_t;

// float viewsizex, viewsizey;

void qcam_init_default(qcam_t *cam);
void qcam_init(qcam_t *qcam, float posX, float posY, float posZ, float upX,
               float upY, float upZ, float yaw, float pitch);

void qcam_get_view(qcam_t *cam, mat4 m);
void qcam_get_rot_view(qcam_t *cam, mat4 m);
void qcam_get_proj(qcam_t *cam, float AspectRatio, float NearDepth,
                   float FarDepth);
#define qcam_get_proj_default(AspectRatio)                                     \
  qcam_get_proj(AspectRatio, 0.1f, 1000.f)

float qcam_set_view(qcam_t *cam, int x, int y);

void qcam_process_keyboard(qcam_t *cam, qcam_direction_t inDirection,
                           bool down);
void qcam_process_mouse(qcam_t *cam, float xoffset, float yoffset);
void qcam_process_scroll(qcam_t *cam, float yoffset);
void qcam_input_update(qcam_t *cam, float deltaTime);

#endif
