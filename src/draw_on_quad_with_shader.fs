#version 150

uniform vec3 iResolution; // viewport resolution in pixels
uniform float iGlobalTime; // shader playback time in seconds
out vec4 oColor;

/*
  this is somewhat[1] compatible with shadertoy shaders:
  - paste it below,
  - replace gl_FragColor with oColor

  [1] we do not support texture channels or mouse input right now
*/

void main()
{
        vec2 uv = gl_FragCoord.xy/iResolution.xy;
        float g = uv.y * (1.0f + 0.5 * sin(8.0*3.141592*(iGlobalTime/4.0 + uv.x)));
        oColor = vec4(uv.x, g, uv.y, 1.00);
}
