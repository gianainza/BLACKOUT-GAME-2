#pragma once
// ---------------------------------------------------------------------------
// Timing.h / Timing.cpp - the GPU half of the timing harness            owner: R5
//
// GpuTimer wraps an OpenGL GL_TIME_ELAPSED query: how long the GPU spent between Begin() and End()
// (used around the shadow passes). The result of a query is only ready a frame or two later, so Poll()
// collects it without stalling the pipeline and the value is "last known".
//
// The whole-frame CPU timing (300 frames, median, P95, CSV files) is FrameTimeLogger.h.
// ---------------------------------------------------------------------------
#include <GL/glew.h>

class GpuTimer
{
public:
	void  Init();                    // call once, after the GL context exists
	void  Begin();                   // start timing (ignored while the previous result is still pending)
	void  End();                     // stop timing
	void  Poll();                    // collect the result if the GPU has finished; call once per frame
	float LastMs() const { return lastMs; }

private:
	GLuint query = 0;
	bool   began = false;
	bool   pending = false;
	float  lastMs = 0.0f;
};
