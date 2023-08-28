#version 450

layout(set=0, binding=0) uniform sampler2D u_Texture;

layout(location = 0) in vec2 v_Texcoord;

layout(location = 0) out vec4 o_Color;

void main() {
  o_Color = texture( u_Texture, v_Texcoord );
  if(o_Color.a==0){
    int x_ind=int(v_Texcoord.x*20   );
    int y_ind=int(v_Texcoord.y*20 +1);
    o_Color.rgb=vec3(0.2);
    o_Color.rgb+=vec3(0.2)*float((x_ind+y_ind)%2);
  }
}