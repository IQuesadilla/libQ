#pragma once

// #include "glad.h"
#include "GL/glew.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

// Defines several possible options for camera movement. Used as abstraction to stay away from window-system specific input methods
enum Camera_Direction {
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

struct Camera_Movement {
    bool FORWARD = false;
    bool BACKWARD = false;
    bool LEFT = false;
    bool RIGHT = false;
    bool UP = false;
    bool DOWN = false;
    bool V_UP = false;
    bool V_DOWN = false;
    bool V_RIGHT = false;
    bool V_LEFT = false;
};

// Default camera values
const float YAW         = -90.0f;
const float PITCH       =  0.0f;
const float SPEED       =  100.0f;
const float SENSITIVITY =  0.1f;
const float ZOOM        =  45.0f;
const float BINARYSENSITIVITY = 1.0f;


// An abstract camera class that processes input and calculates the corresponding Euler Angles, Vectors and Matrices for use in OpenGL
class Camera
{
public:
    // Camera Attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
    // Euler Angles
    float Yaw;
    float Pitch;
    // Camera options
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;
    float BinarySensitivity;

    bool UpIsWorldUp;

    Camera_Movement direction;
    glm::mat4 ProjectionMatrix;

    //float viewsizex, viewsizey;

    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH);
    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch);

    glm::mat4 GetViewMatrix();
    glm::mat4 GetRotViewMatrix();
	  void BuildProjectionMatrix(float AspectRatio, float NearDepth = 0.1f, float FarDepth = 1000.0f);

    static float setViewSize(int x, int y);

    void ProcessKeyboard(Camera_Direction inDirection, bool down);
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void ProcessMouseScroll(float yoffset);
    void InputUpdate(float deltaTime);

private:
    void updateCameraVectors();
};
