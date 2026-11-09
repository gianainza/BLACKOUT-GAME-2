#pragma once

#include "stdio.h"

#include <GL\glew.h>
#include <GLFW\glfw3.h>

class Window
{
public:
	Window();

	Window(GLint windowWidth, GLint windowHeight, const char* windowTitle = "OBSOLETE");

	int Initialise();

	GLint getBufferWidth() { return bufferWidth; }
	GLint getBufferHeight() { return bufferHeight; }

	bool getShouldClose() { return glfwWindowShouldClose(mainWindow); }

	bool* getsKeys() { return keys; }
	GLfloat getXChange();
	GLfloat getYChange();

	void swapBuffers() { glfwSwapBuffers(mainWindow); }

	GLFWwindow* getGLFWwindow() { return mainWindow; }
	void setVSync(bool on) { glfwSwapInterval(on ? 1 : 0); }
	// true exactly once after a key went down (the key flag is cleared by this call)
	bool consumeKey(int key) { if (key >= 0 && key < 1024 && keys[key]) { keys[key] = false; return true; } return false; }

	~Window();

private:
	GLFWwindow* mainWindow;

	GLint width, height;
	GLint bufferWidth, bufferHeight;
	const char* title;

	bool keys[1024];

	GLfloat lastX;
	GLfloat lastY;
	GLfloat xChange;
	GLfloat yChange;
	bool mouseFirstMoved;

	void createCallbacks();
	static void handleKeys(GLFWwindow* window, int key, int code, int action, int mode);
	static void handleMouse(GLFWwindow* window, double xPos, double yPos);
	static void handleResize(GLFWwindow* window, int newWidth, int newHeight);
};
