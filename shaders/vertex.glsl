#version 300 es

precision highp float;

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texCoord;

out vec2 v_texCoord;

uniform mat4 u_projection;

void main() {
    v_texCoord = a_texCoord;
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
}
