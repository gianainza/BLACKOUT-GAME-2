#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

// Activity J's Material (specular intensity + shininess) plus a colour tint and an emissive term,
// which the terminal lamps and the scanner beams need.
class Material
{
public:
	Material();
	Material(GLfloat sIntensity, GLfloat shine);
	Material(GLfloat sIntensity, GLfloat shine, const glm::vec3& tint, GLfloat emit);

	void UseMaterial(GLuint specularIntensityLocation, GLuint shininessLocation);

	glm::vec3 GetColour() const { return colour; }
	GLfloat GetEmissive() const { return emissive; }

	~Material();

private:
	GLfloat specularIntensity;
	GLfloat shininess;
	glm::vec3 colour;
	GLfloat emissive;
};
