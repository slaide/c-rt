#version 450

layout(set=0, binding=1) uniform u_Uniform{
    float u_scale_factor;
    float aspect_ratio;
};

layout(location = 0) in vec4 i_Position;
layout(location = 1) in vec2 i_Uv;

out gl_PerVertex{
    vec4 gl_Position;
};

layout(location = 0) out vec2 v_Uv;

void main() {
    gl_Position = i_Position;
    if(aspect_ratio<1.0){
        gl_Position.x*=aspect_ratio;
    }else{
        gl_Position.y/=aspect_ratio;
    }
    float scale_factor=1.0/u_scale_factor;
    v_Uv = i_Uv*scale_factor+(1-scale_factor)/2;
}
