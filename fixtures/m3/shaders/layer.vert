// [COMBO] {"combo":"TINT","default":0}
attribute vec3 a_Position;
attribute vec2 a_TexCoord;
uniform float4x4 g_ModelViewProjectionMatrix;
varying vec2 v_TexCoord;
void main() {
  gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);
  v_TexCoord = a_TexCoord;
}
