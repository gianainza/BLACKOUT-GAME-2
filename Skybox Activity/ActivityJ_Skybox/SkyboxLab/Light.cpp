#include "Light.h"

Light::Light()
{
	colour = glm::vec3(1.0f, 1.0f, 1.0f);
	ambientIntensity = 1.0f;
	diffuseIntensity = 0.0f;
	power = 1.0f;
	shadowMap = nullptr;
}

Light::Light(GLuint shadowWidth, GLuint shadowHeight, GLfloat red, GLfloat green, GLfloat blue, GLfloat aIntensity, GLfloat dIntensity)
{
	colour = glm::vec3(red, green, blue);
	ambientIntensity = aIntensity;
	diffuseIntensity = dIntensity;
	power = 1.0f;

	// The shadow map is created by the DERIVED class (a flat ShadowMap for the directional light,
	// an OmniShadowMap for point and spot lights). Activity J created a flat one here as well and then
	// overwrote it, leaking a depth texture and framebuffer per point light.
	shadowMap = nullptr;
}

Light::~Light()
{
}
