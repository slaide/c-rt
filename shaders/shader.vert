#version 450

layout(location = 0) in vec4 i_Position;
layout(location = 1) in vec2 i_Uv;

out gl_PerVertex{
    vec4 gl_Position;
};

layout(location = 0) out vec2 v_Uv;

void main() {
    gl_Position = i_Position;
    float scale_factor=1.0/1.0;
    v_Uv = i_Uv*scale_factor+(1-scale_factor)/2;
}
