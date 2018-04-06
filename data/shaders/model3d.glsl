uniform   mat4  u_model;
uniform   mat4  u_view;
uniform   mat4  u_proj;
uniform   vec4  u_color;
uniform   vec2  u_uv_scale;
uniform   vec3  u_l_dir;
uniform   float u_l_diff;
uniform   float u_l_emit;

uniform sampler2D u_tex;
uniform float     u_strip;
uniform float     u_time;

varying   vec4 v_color;
varying   vec2 v_uv;

#ifdef VERTEX_SHADER

/************************************************************************/
attribute vec3  a_pos;
attribute vec4  a_color;
attribute vec3  a_normal;
attribute vec2  a_uv;

void main()
{
    vec4 col = u_color * a_color;
    vec3 pos = (u_view * u_model * vec4(a_pos, 1.0)).xyz;
    gl_Position = u_proj * vec4(pos, 1.0);
    float diff = max(0.0, dot(u_l_dir, a_normal));
    col.rgb *= (u_l_emit + u_l_diff * diff);
    v_color = col;
    v_uv = a_uv * u_uv_scale;
}
/************************************************************************/

#endif

#ifdef FRAGMENT_SHADER

/************************************************************************/
void main()
{
    gl_FragColor = v_color * texture2D(u_tex, v_uv);
    if (u_strip > 0.0) {
       float p = gl_FragCoord.x + gl_FragCoord.y + u_time * 4.0;
       if (mod(p, 8.0) < 4.0) gl_FragColor.rgb *= 0.5;
    }
}
/************************************************************************/

#endif
