/*
 * I followed those name conventions.  All the vectors are expressed in eye
 * coordinates.
 *
 *         reflection         light source
 *
 *               r              s
 *                 ^         ^
 *                  \   n   /
 *  eye              \  ^  /
 *     v  <....       \ | /
 *              -----__\|/
 *                  ----+----
 *
 *
 */

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;
uniform mat4 u_shadow_mvp;
uniform float u_pos_scale;

// Light parameters
uniform vec3 u_l_dir;
uniform float u_l_int;

// Material parameters
uniform float u_m_amb; // Ambient light coef.
uniform float u_m_dif; // Diffuse light coef.
uniform float u_m_spe; // Specular light coef.
uniform float u_m_shi; // Specular light shininess.
uniform float u_m_smo; // Smoothness.

uniform sampler2D u_bshadow_tex;
uniform sampler2D u_bump_tex;
uniform float     u_bshadow;
uniform sampler2D u_shadow_tex;
uniform float     u_shadow_k;

varying lowp vec3 v_pos;
varying lowp vec4 v_color;
varying lowp vec2 v_bshadow_uv;
varying lowp vec2 v_uv;
varying lowp vec2 v_bump_uv;
varying lowp vec3 v_normal;
varying      vec4 v_shadow_coord;

#ifdef VERTEX_SHADER

/************************************************************************/
attribute vec3 a_pos;
attribute vec3 a_normal;
attribute vec4 a_color;
attribute vec2 a_bshadow_uv;
attribute vec2 a_bump_uv;   // bump tex base coordinates [0,255]
attribute vec2 a_uv;        // uv coordinates [0,1]

void main()
{
    v_normal = a_normal;
    v_color = a_color;
    v_bshadow_uv = (a_bshadow_uv + 0.5) / (16.0 * VOXEL_TEXTURE_SIZE);
    v_pos = a_pos * u_pos_scale;
    v_uv = a_uv;
    v_bump_uv = a_bump_uv;
    gl_Position = u_proj * u_view * u_model * vec4(v_pos, 1.0);
    v_shadow_coord = (u_shadow_mvp * u_model * vec4(v_pos, 1.0));
}
/************************************************************************/

#endif

#ifdef FRAGMENT_SHADER

/************************************************************************/
#ifdef GL_ES
precision highp float;
#endif

vec2 uv, bump_uv;
vec3 n, s, r, v, bump;
float s_dot_n;
float l_amb, l_dif, l_spe;
float bshadow;
float visibility;
vec2 PS[4]; // Poisson offsets used for the shadow map.
int i;

void main()
{
    // clamp uv so to prevent overflow with multismapling.
    uv = clamp(v_uv, 0.0, 1.0);
    s = u_l_dir;
    n = normalize((u_view * u_model * vec4(v_normal, 0.0)).xyz);
    bump_uv = (v_bump_uv + 0.5 + uv * 15.0) / 256.0;
    bump = texture2D(u_bump_tex, bump_uv).xyz - 0.5;
    bump = normalize((u_view * u_model * vec4(bump, 0.0)).xyz);
    n = mix(bump, n, u_m_smo);
    s_dot_n = dot(s, n);
    l_dif = u_m_dif * max(0.0, s_dot_n);
    l_amb = u_m_amb;

    // Specular light.
    v = normalize(-(u_view * u_model * vec4(v_pos, 1.0)).xyz);
    r = reflect(-s, n);
    l_spe = u_m_spe * pow(max(dot(r, v), 0.0), u_m_shi);
    l_spe = s_dot_n > 0.0 ? l_spe : 0.0;

    bshadow = texture2D(u_bshadow_tex, v_bshadow_uv).r;
    bshadow = sqrt(bshadow);
    bshadow = mix(1.0, bshadow, u_bshadow);
    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    gl_FragColor.rgb += (l_dif + l_amb) * u_l_int * v_color.rgb;
    gl_FragColor.rgb += l_spe * u_l_int * vec3(1.0);
    gl_FragColor.rgb *= bshadow;

    // Shadow map.
    #ifdef SHADOW
    visibility = 1.0;
    vec4 shadow_coord = v_shadow_coord / v_shadow_coord.w;
    float bias = 0.005 * tan(acos(clamp(s_dot_n, 0.0, 1.0)));
    bias = clamp(bias, 0.0, 0.015);
    shadow_coord.z -= bias;
    PS[0] = vec2(-0.94201624, -0.39906216) / 1024.0;
    PS[1] = vec2(+0.94558609, -0.76890725) / 1024.0;
    PS[2] = vec2(-0.09418410, -0.92938870) / 1024.0;
    PS[3] = vec2(+0.34495938, +0.29387760) / 1024.0;
    for (i = 0; i < 4; i++)
        if (texture2D(u_shadow_tex, v_shadow_coord.xy +
           PS[i]).z < shadow_coord.z) visibility -= 0.2;
    if (s_dot_n <= 0.0) visibility = 0.5;
    gl_FragColor.rgb *= mix(1.0, visibility, u_shadow_k);
    #endif

}

/************************************************************************/

#endif
