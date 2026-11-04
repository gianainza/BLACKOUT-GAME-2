#pragma once

#include <string>

#include <GL/glew.h>

#include "CommonValues.h"

// Activity J's Texture, with three fixes (R3, recorded in the decision log):
//   * the file name is COPIED (it used to keep a raw const char*, which dangles when the name is a temporary)
//   * images are always requested as 4 channels, so an RGB png is no longer read as if it were RGBA
//   * a missing / unreadable file is reported and replaced by a magenta checker instead of drawing black
class Texture
{
public:
	Texture();
	Texture(const char* fileLoc);

	bool LoadTexture();
	bool LoadTextureA();

	void UseTexture();
	void ClearTexture();

	bool IsPlaceholder() const { return placeholder; }
	GLuint GetId() const { return textureID; }

	~Texture();

private:
	GLuint textureID;
	int width, height, bitDepth;
	bool placeholder;

	std::string fileLocation;

	void MakePlaceholder();
};
