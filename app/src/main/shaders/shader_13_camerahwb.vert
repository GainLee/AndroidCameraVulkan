#version 450

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec2 inUVPos;

layout (binding = 0) uniform UBO
{
    mat4 projectionMatrix;
    mat4 modelMatrix;
    mat4 viewMatrix;
} ubo;

layout (location = 0) out vec2 texturePos;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    texturePos = inUVPos;
    gl_Position = ubo.projectionMatrix * ubo.viewMatrix * ubo.modelMatrix * inPos;
}