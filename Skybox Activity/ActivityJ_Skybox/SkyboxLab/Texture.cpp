#include "Texture.h"

#include <cstdio>
#include <vector>

Texture::Texture()
{
	textureID = 0;
	width = 0;
	height = 0;
	bitDepth = 0;
	placeholder = false;
	fileLocation = "";
}

Texture::Texture(const char* fileLoc)
{
	textureID = 0;
	width = 0;
	height = 0;
	bitDepth = 0;
	placeholder = false;
	fileLocation = fileLoc ? fileLoc : "";
}

void Texture::MakePlaceholder()
{
	// 8 x 8 magenta / black checker: impossible to miss, so a missing asset is never a silent black object
	const int S = 64;
	std::vector<unsigned char> px(S * S * 4);
	for (int y = 0; y < S; ++y)
		for (int x = 0; x < S; ++x)
		{
			bool on = (((x / 8) + (y / 8)) & 1) != 0;
			unsigned char* p = &px[(y * S + x) * 4];
			p[0] = on ? 255 : 20; p[1] = on ? 0 : 0; p[2] = on ? 255 : 20; p[3] = 255;
		}
	width = height = S;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, S, S, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
	glBindTexture(GL_TEXTURE_2D, 0);
	placeholder = true;
}

bool Texture::LoadTexture()
{
	return LoadTextureA();     // both paths now load 4 channels
}

bool Texture::LoadTextureA()
{
	unsigned char *texData = stbi_load(fileLocation.c_str(), &width, &height, &bitDepth, 4);
	if (!texData)
	{
		printf("Failed to load texture '%s' - using a magenta placeholder\n", fileLocation.c_str());
		MakePlaceholder();
		return false;
	}

	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);

	stbi_image_free(texData);

	return true;
}

void Texture::UseTexture()
{
	glActiveTexture(GL_TEXTURE0 + UNIT_TEXTURE);
	glBindTexture(GL_TEXTURE_2D, textureID);
}

void Texture::ClearTexture()
{
	if (textureID != 0) glDeleteTextures(1, &textureID);
	textureID = 0;
	width = 0;
	height = 0;
	bitDepth = 0;
	fileLocation = "";
}

Texture::~Texture()
{
	ClearTexture();
}
