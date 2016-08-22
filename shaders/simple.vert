#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(std140, binding = 0) uniform buf {
  mat4 MVP;
  vec4 position[3];
  vec4 color[3];
} ubuf;

layout (location = 0) out vec4 vColor;

out gl_PerVertex {
  vec4 gl_Position;
};

void main() 
{
  gl_Position = ubuf.MVP * ubuf.position[gl_VertexIndex];
  vColor = ubuf.color[gl_VertexIndex];

  // GL->VK conventions
  gl_Position.y = -gl_Position.y;
  gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
}
