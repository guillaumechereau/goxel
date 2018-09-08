#if defined(GL_ES) && defined(FRAGMENT_SHADER)
#extension GL_OES_standard_derivatives : enable
#endif

uniform highp   mat4  u_model;
uniform highp   mat4  u_view;
uniform highp   mat4  u_proj;
uniform highp   mat4  u_clip;
uniform lowp    vec4  u_color;
uniform mediump vec2  u_uv_scale;
uniform lowp    float u_grid_alpha;
uniform mediump vec3  u_l_dir;
uniform mediump float u_l_diff;
uniform mediump float u_l_emit;

uniform mediump sampler2D u_tex;
uniform mediump float     u_strip;
uniform mediump float     u_time;

varying mediump vec3 v_normal;
varying highp   vec3 v_pos;
varying lowp    vec4 v_color;
varying mediump vec2 v_uv;
varying mediump vec4 v_clip_pos;

#ifdef VERTEX_SHADER

/************************************************************************/
attribute highp   vec3  a_pos;
attribute lowp    vec4  a_color;
attribute mediump vec3  a_normal;
attribute mediump vec2  a_uv;

void main()
{
    lowp vec4 col = u_color * a_color;
    highp vec3 pos = (u_view * u_model * vec4(a_pos, 1.0)).xyz;
    if (u_clip[3][3] > 0.0)
        v_clip_pos = u_clip * u_model * vec4(a_pos, 1.0);
    gl_Position = u_proj * vec4(pos, 1.0);
    mediump float diff = max(0.0, dot(u_l_dir, a_normal));
    col.rgb *= (u_l_emit + u_l_diff * diff);
    v_color = col;
    v_uv = a_uv * u_uv_scale;
    v_pos = (u_model * vec4(a_pos, 1.0)).xyz;
    v_normal = a_normal;
}
/************************************************************************/

#endif

#ifdef FRAGMENT_SHADER

/************************************************************************/
void main()
{
    gl_FragColor = v_color * texture2D(u_tex, v_uv);
    if (u_strip > 0.0) {
       mediump float p = gl_FragCoord.x + gl_FragCoord.y + u_time * 4.0;
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
#if !defined(GL_ES) || defined(GL_OES_standard_derivatives)
    if (u_grid_alpha > 0.0) {
        mediump vec2 c;
        if (abs((u_model * vec4(v_normal, 0.0)).x) > 0.5) c = v_pos.yz;
        if (abs((u_model * vec4(v_normal, 0.0)).y) > 0.5) c = v_pos.zx;
        if (abs((u_model * vec4(v_normal, 0.0)).z) > 0.5) c = v_pos.xy;
        mediump vec2 grid = abs(fract(c - 0.5) - 0.5) / fwidth(c);
        mediump float line = min(grid.x, grid.y);
        gl_FragColor.rgb *= mix(1.0 - u_grid_alpha, 1.0, min(line, 1.0));
    }
#endif

}
/************************************************************************/

#endif
