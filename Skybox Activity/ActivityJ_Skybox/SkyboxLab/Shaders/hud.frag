#version 460 core
in vec2 TexCoord;
out vec4 outColour;
uniform vec4 tint;
uniform int  useTexture;
uniform sampler2D hudTexture;     // texture unit 15 (UNIT_HUD) : the digit/letter strip
void main()
{
    if (useTexture != 0)
        outColour = vec4(tint.rgb, tint.a * texture(hudTexture, TexCoord).a);
    else
        outColour = tint;
}
