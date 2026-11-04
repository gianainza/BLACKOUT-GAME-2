#version 460 core
// HUD vertex shader: unit quad placed with a pixel rectangle, glm::ortho projection.
layout (location = 0) in vec2 pos;   // 0..1
layout (location = 1) in vec2 uv;
uniform mat4 projection;
uniform vec4 rect;      // x, y, width, height in pixels (origin bottom-left)
uniform vec4 uvRect;    // u0, v0, du, dv
out vec2 TexCoord;
void main()
{
    gl_Position = projection * vec4(rect.xy + pos * rect.zw, 0.0, 1.0);
    TexCoord = uvRect.xy + uv * uvRect.zw;
}
