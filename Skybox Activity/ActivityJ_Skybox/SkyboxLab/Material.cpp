#include "Material.h"

Material::Material()
{
	specularIntensity = 0.0f;
	shininess = 0.0f;
	colour = glm::vec3(1.0f);
	emissive = 0.0f;
}

Material::Material(GLfloat sIntensity, GLfloat shine)
{
	specularIntensity = sIntensity;
	shininess = shine;
	colour = glm::vec3(1.0f);
	emissive = 0.0f;
}

Material::Material(GLfloat sIntensity, GLfloat shine, const glm::vec3& tint, GLfloat emit)
{
	specularIntensity = sIntensity;
	shininess = shine;
	colour = tint;
	emissive = emit;
}

void Material::UseMaterial(GLuint specularIntensityLocation, GLuint shininessLocation)
{
	glUniform1f(specularIntensityLocation, specularIntensity);
	glUniform1f(shininessLocation, shininess);
}

Material::~Material()
{
}
