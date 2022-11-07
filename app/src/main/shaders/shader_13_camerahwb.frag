#version 450
layout (binding = 1) uniform sampler2D samplerImg;
layout (location = 0) in vec2 texturePos;
layout (location = 0) out vec4 outColor;
void main() {
   outColor = texture(samplerImg, texturePos, 0.0);
//   outColor = vec4(1.0, 0.0, 0.0, 1.0);
}