uniform   mat4  u_model;
uniform   mat4  u_view;
uniform   mat4  u_proj;
uniform   mat4  u_clip;
uniform   vec4  u_color;
uniform   vec2  u_uv_scale;
uniform   float u_grid_alpha;
uniform   vec3  u_l_dir;
uniform   float u_l_diff;
uniform   float u_l_emit;

uniform sampler2D u_tex;
uniform float     u_strip;
uniform float     u_time;

varying   vec3 v_normal;
varying   vec3 v_pos;
varying   vec4 v_color;
varying   vec2 v_uv;
varying   vec4 v_clip_pos;

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
    if (u_clip[3][3] > 0.0)
        v_clip_pos = u_clip * u_model * vec4(a_pos, 1.0);
    gl_Position = u_proj * vec4(pos, 1.0);
    float diff = max(0.0, dot(u_l_dir, a_normal));
    col.rgb *= (u_l_emit + u_l_diff * diff);
    v_color = col;
    v_uv = a_uv * u_uv_scale;
    v_pos = (u_model * vec4(a_pos, 1.0)).xyz;
    v_normal = a_normal;
}
/************************************************************************/

#endif

#ifdef FRAGMENT_SHADER

// XXX: need to check if available, and disable the grid if not.
#extension GL_OES_standard_derivatives : enable

/************************************************************************/
void main()
{
    gl_FragColor = v_color * texture2D(u_tex, v_uv);
    if (u_strip > 0.0) {
       float p = gl_FragCoord.x + gl_FragCoord.y + u_time * 4.0;
       if (mod(p, 8.0) < 4.0) gl_FragColor.rgb *= 0.5;
    }
    if (u_clip[3][3] > 0.0) {
        if (    v_clip_pos[0] < -v_clip_pos[3] ||
                v_clip_pos[1] < -v_clip_pos[3] ||
                v_clip_pos[2] < -v_clip_pos[3] ||
                v_clip_pos[0] > +v_clip_pos[3] ||
                v_clip_pos[1] > +v_clip_pos[3] ||
                v_clip_pos[2] > +v_clip_pos[3]) discard;
    }

    // Grid effect.
    if (u_grid_alpha > 0.0) {
        vec2 c;
        if (abs((u_model * vec4(v_normal, 0.0)).x) > 0.5) c = v_pos.yz;
        if (abs((u_model * vec4(v_normal, 0.0)).y) > 0.5) c = v_pos.zx;
        if (abs((u_model * vec4(v_normal, 0.0)).z) > 0.5) c = v_pos.xy;
        vec2 grid = abs(fract(c - 0.5) - 0.5) / fwidth(c);
        float line = min(grid.x, grid.y);
        gl_FragColor.rgb *= mix(1.0 - u_grid_alpha, 1.0, min(line, 1.0));
    }
}
/************************************************************************/

#endif
