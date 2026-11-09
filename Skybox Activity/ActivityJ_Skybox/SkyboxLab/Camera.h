#pragma once

#include <GL\glew.h>

#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>

#include <GLFW\glfw3.h>

class Camera
{
public:
	Camera();
	Camera(glm::vec3 startPosition, glm::vec3 startUp, GLfloat startYaw, GLfloat startPitch, GLfloat startMoveSpeed, GLfloat startTurnSpeed);

	void keyControl(bool* keys, GLfloat deltaTime);
	void mouseControl(GLfloat xChange, GLfloat yChange);

	// ---- OBSOLETE: third-person orbit mode (R4). The original fly-camera functions are unchanged.
	void orbitControl(GLfloat xChange, GLfloat yChange);          // mouse: turn around the droid
	void followTarget(const glm::vec3& target, GLfloat distance); // place the camera behind the target
	GLfloat getYawRadians() const { return glm::radians(yaw); }
	void setView(GLfloat newYaw, GLfloat newPitch) { yaw = newYaw; pitch = newPitch; update(); }

	glm::vec3 getCameraPosition();
	glm::vec3 getCameraDirection();

	glm::mat4 calculateViewMatrix();

	~Camera();

private:
	glm::vec3 position;
	glm::vec3 front;
	glm::vec3 up;
	glm::vec3 right;
	glm::vec3 worldUp;

	GLfloat yaw;
	GLfloat pitch;

	GLfloat moveSpeed;
	GLfloat turnSpeed;

	void update();
};
