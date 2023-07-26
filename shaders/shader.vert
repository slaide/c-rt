#version 450

layout(set=0, binding=1) uniform u_Uniform{
    float zoom_factor;
    float image_aspect_ratio;
    float window_aspect_ratio;

    float offset_x;
    float offset_y;
};

layout(location = 0) in vec4 i_Position;
layout(location = 1) in vec2 i_Uv;

out gl_PerVertex{
    vec4 gl_Position;
};

layout(location = 0) out vec2 v_Uv;

void main() {
    v_Uv = i_Uv;
    gl_Position = i_Position;

    gl_Position.xy=gl_Position.xy*zoom_factor;

    float aspect_ratio=image_aspect_ratio/window_aspect_ratio;

    if(aspect_ratio<1.0){
        gl_Position.x*=aspect_ratio;
    }else{
        gl_Position.y/=aspect_ratio;
    }
}
