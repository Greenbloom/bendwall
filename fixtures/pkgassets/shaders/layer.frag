// [COMBO] {"combo":"TINT","default":0}
#include "common.h"
varying vec2 v_TexCoord;
uniform sampler2D g_Texture0;
uniform float g_Time;
uniform float2 g_PointerPosition;
uniform float2 g_ParallaxPosition;
uniform float g_Boost; // {"material":"boost","default":0}
void main() {
  float4 c = texSample2D(g_Texture0, v_TexCoord);
  float2 p = g_PointerPosition;
  c.g = saturate(c.g + float(TINT) + g_Time + p.x + p.y + g_ParallaxPosition.y + g_Boost * p.x);
  c.r = c.r + EXTRA + lerp(frac(0.0), 0.0, 0.0) + ddx(c.r) * p.x;
  gl_FragColor = c;
}
