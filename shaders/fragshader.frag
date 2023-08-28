#version 450

layout(set=0, binding=0) uniform sampler2D u_Texture;

layout(location = 0) in vec2 v_Texcoord;

layout(location = 0) out vec4 o_Color;

void main() {
  o_Color = texture( u_Texture, v_Texcoord );

  const int x_ind=int(v_Texcoord.x*20   );
  const int y_ind=int(v_Texcoord.y*20 +1);
  const float raster_block_factor=float((x_ind+y_ind)%2);

  if(o_Color.a==0){
    o_Color.rgb=vec3(0.2+0.2*raster_block_factor);
  }else if(o_Color.a<1){
    o_Color.rgb*=1-o_Color.a*0.2*raster_block_factor;
  }
}