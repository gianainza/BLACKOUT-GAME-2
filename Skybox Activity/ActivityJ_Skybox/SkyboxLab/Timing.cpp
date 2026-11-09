#include "Timing.h"


// ------------------------------------------------------------------ GpuTimer
void GpuTimer::Init()
{
	glGenQueries(1, &query);
}

void GpuTimer::Begin()
{
	if (query == 0 || pending) return;           // the last result has not been collected yet: skip this frame
	glBeginQuery(GL_TIME_ELAPSED, query);
	began = true;
}

void GpuTimer::End()
{
	if (!began) return;
	glEndQuery(GL_TIME_ELAPSED);
	began = false;
	pending = true;
}

void GpuTimer::Poll()
{
	if (!pending) return;
	GLint available = 0;
	glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
	if (!available) return;                      // still running on the GPU; try again next frame
	GLuint64 ns = 0;
	glGetQueryObjectui64v(query, GL_QUERY_RESULT, &ns);
	lastMs = (float)((double)ns / 1.0e6);
	pending = false;
}
