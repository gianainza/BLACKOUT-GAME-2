#pragma once

#include <GL\glew.h>
#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>

#include "ShadowMap.h"

class Light
{
public:
	Light();
	Light(GLuint shadowWidth, GLuint shadowHeight,
			GLfloat red, GLfloat green, GLfloat blue,
			GLfloat aIntensity, GLfloat dIntensity);

	ShadowMap* getShadowMap() { return shadowMap; }

	void SetColour(const glm::vec3& c) { colour = c; }
	glm::vec3 GetColour() { return colour; }
	void SetPower(GLfloat p) { power = p; }

	~Light();

protected:
	glm::vec3 colour;
	GLfloat ambientIntensity;
	GLfloat diffuseIntensity;
	GLfloat power;

	glm::mat4 lightProj;

	ShadowMap* shadowMap;
};
