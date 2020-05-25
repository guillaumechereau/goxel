/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

// Some of the algos come from glTF Sampler, under Apache Licence V2.

uniform highp mat4  u_model;
uniform highp mat4  u_view;
uniform highp mat4  u_proj;
uniform lowp  float u_pos_scale;
uniform highp vec3  u_camera;
uniform highp float u_z_ofs; // Used for line rendering.

// Light parameters
uniform lowp    vec3  u_l_dir;
uniform lowp    float u_l_int;
uniform lowp    float u_l_amb; // Ambient light coef.

// Material parameters
uniform lowp float u_m_metallic;
uniform lowp float u_m_roughness;
uniform lowp float u_m_smoothness;
uniform lowp vec4  u_m_base_color;
uniform lowp vec3  u_m_emissive_factor;

uniform mediump sampler2D u_normal_sampler;
uniform lowp    float     u_normal_scale;

#ifdef HAS_OCCLUSION_MAP
uniform mediump sampler2D u_occlusion_tex;
uniform mediump float     u_occlusion_strength;
#endif

#ifdef SHADOW
uniform highp   mat4      u_shadow_mvp;
uniform mediump sampler2D u_shadow_tex;
uniform mediump float     u_shadow_strength;
varying mediump vec4      v_shadow_coord;
#endif


varying highp   vec3 v_Position;
varying lowp    vec4 v_color;
varying mediump vec2 v_occlusion_uv;
varying mediump vec2 v_UVCoord1;
varying mediump vec3 v_gradient;

#ifdef HAS_TANGENTS
varying mediump mat3 v_TBN;
#else
varying mediump vec3 v_Normal;
#endif


const mediump float M_PI = 3.141592653589793;

/*
 * Function: F_Schlick.
 * Compute Fresnel (specular).
 *
 * Optimized variant (presented by Epic at SIGGRAPH '13)
 * https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
 */
mediump vec3 F_Schlick(mediump vec3 f0, mediump float LdotH)
{
    mediump float fresnel = exp2((-5.55473 * LdotH - 6.98316) * LdotH);
    return (1.0 - f0) * fresnel + f0;
}

/*
 * Function: V_SmithGGXCorrelatedFast
 * Compute Geometic occlusion.
 *
 * Fast approximation from
 * https://google.github.io/filament/Filament.html#materialsystem/standardmodel
 */
mediump float V_GGX(mediump float NdotL, mediump float NdotV,
                    mediump float alpha)
{
    mediump float a = alpha;
    mediump float GGXV = NdotL * (NdotV * (1.0 - a) + a);
    mediump float GGXL = NdotV * (NdotL * (1.0 - a) + a);
    return 0.5 / (GGXV + GGXL);
}

/*
 * Function: D_GGX
 * Microfacet distribution
 */
mediump float D_GGX(mediump float NdotH, mediump float alpha)
{
    mediump float a2 = alpha * alpha;
    mediump float f = (NdotH * a2 - NdotH) * NdotH + 1.0;
    return a2 / (M_PI * f * f);
}

mediump vec3 compute_light(mediump vec3 L,
                           mediump float light_intensity,
                           mediump float light_ambient,
                           mediump vec3 base_color,
                           mediump float metallic,
                           mediump float roughness,
                           mediump vec3 N, mediump vec3 V)
{
    mediump vec3 H = normalize(L + V);

    mediump float NdotL = clamp(dot(N, L), 0.0, 1.0);
    mediump float NdotV = clamp(dot(N, V), 0.0, 1.0);
    mediump float NdotH = clamp(dot(N, H), 0.0, 1.0);
    mediump float LdotH = clamp(dot(L, H), 0.0, 1.0);
    mediump float VdotH = clamp(dot(V, H), 0.0, 1.0);

    mediump float a_roughness = roughness * roughness;
    // Schlick GGX model, as used by glTF2.
    mediump vec3 f0 = vec3(0.04);
    mediump vec3 diffuse_color = base_color * (vec3(1.0) - f0) * (1.0 - metallic);
    mediump vec3 specular_color = mix(f0, base_color, metallic);
    mediump vec3  F   = F_Schlick(specular_color, LdotH);
    mediump float Vis = V_GGX(NdotL, NdotV, a_roughness);
    mediump float D   = D_GGX(NdotH, a_roughness);
    // Calculation of analytical lighting contribution
    mediump vec3 diffuseContrib = (1.0 - F) * (diffuse_color / M_PI);
    mediump vec3 specContrib = F * (Vis * D);
    mediump vec3 shade = NdotL * (diffuseContrib + specContrib);
    shade = max(shade, vec3(0.0));
    return light_intensity * shade + light_ambient * base_color;
}

mediump vec3 getNormal()
{
    mediump vec3 ret;
#ifdef HAS_TANGENTS
    mediump mat3 tbn = v_TBN;
    mediump vec3 n = texture2D(u_normal_sampler, v_UVCoord1).rgb;
    n = tbn * ((2.0 * n - 1.0) * vec3(u_normal_scale, u_normal_scale, 1.0));
    ret = normalize(n);
#else
    ret = normalize(v_Normal);
#endif

#ifdef SMOOTHNESS
    ret = normalize(mix(ret, normalize(v_gradient), u_m_smoothness));
#endif
    return ret;
}


#ifdef VERTEX_SHADER

/************************************************************************/
attribute highp   vec3 a_pos;
attribute mediump vec3 a_normal;
attribute mediump vec3 a_tangent;
attribute mediump vec3 a_gradient;
attribute lowp    vec4 a_color;
attribute mediump vec2 a_occlusion_uv;
attribute mediump vec2 a_bump_uv;   // bump tex base coordinates [0,255]
attribute mediump vec2 a_uv;        // uv coordinates [0,1]

// Must match the value in goxel.h
#define VOXEL_TEXTURE_SIZE 8.0

void main()
{
    vec4 pos = u_model * vec4(a_pos * u_pos_scale, 1.0);
    v_Position = vec3(pos.xyz) / pos.w;

    v_color = a_color.rgba * a_color.rgba; // srgb to linear (fast).
    v_occlusion_uv = (a_occlusion_uv + 0.5) / (16.0 * VOXEL_TEXTURE_SIZE);
    gl_Position = u_proj * u_view * vec4(v_Position, 1.0);
    gl_Position.z += u_z_ofs;

#ifdef SHADOW
    v_shadow_coord = u_shadow_mvp * vec4(v_Position, 1.0);
#endif

#ifdef HAS_TANGENTS
    mediump vec4 tangent = vec4(normalize(a_tangent), 1.0);
    mediump vec3 normalW = normalize(a_normal);
    mediump vec3 tangentW = normalize(vec3(u_model * vec4(tangent.xyz, 0.0)));
    mediump vec3 bitangentW = cross(normalW, tangentW) * tangent.w;
    v_TBN = mat3(tangentW, bitangentW, normalW);
#else
    v_Normal = normalize(a_normal);
#endif

    v_gradient = a_gradient;
    v_UVCoord1 = (a_bump_uv + 0.5 + a_uv * 15.0) / 256.0;

#ifdef VERTEX_LIGHTNING
    mediump vec3 N = getNormal();
    v_color.rgb = compute_light(normalize(u_l_dir), u_l_int, u_l_amb,
                                (v_color * u_m_base_color).rgb,
                                u_m_metallic, u_m_roughness, N,
                                normalize(u_camera - v_Position));
#endif
}

#endif

#ifdef FRAGMENT_SHADER

#ifdef GL_ES
precision mediump float;
#endif


/************************************************************************/
vec3 toneMap(vec3 color)
{
    // color *= u_exposure;
    return sqrt(color); // Gamma correction.
}

void main()
{

#ifdef ONLY_EDGES
    mediump vec3 n = 2.0 * texture2D(u_normal_sampler, v_UVCoord1).rgb - 1.0;
    if (n.z > 0.75)
        discard;
#endif

    float metallic = u_m_metallic;
    float roughness = u_m_roughness;
    vec4 base_color = u_m_base_color * v_color;

#ifdef MATERIAL_UNLIT
    gl_FragColor = vec4(sqrt(base_color.rgb), base_color.a);
    return;
#endif

    vec3 N = getNormal();
    vec3 V = normalize(u_camera - v_Position);
    vec3 L = normalize(u_l_dir);

#ifndef VERTEX_LIGHTNING
    vec3 color = compute_light(L, u_l_int, u_l_amb, base_color.rgb,
                               metallic, roughness,
                               N, V);
#else
    vec3 color = v_color.rgb;
#endif

    // Shadow map.
#ifdef SHADOW
    float NdotL = clamp(dot(N, L), 0.0, 1.0);
    lowp vec2 PS[4]; // Poisson offsets used for the shadow map.
    float visibility = 1.0;
    mediump vec4 shadow_coord = v_shadow_coord / v_shadow_coord.w;
    lowp float bias = 0.005 * tan(acos(clamp(NdotL, 0.0, 1.0)));
    bias = clamp(bias, 0.0015, 0.015);
    shadow_coord.z -= bias;
    PS[0] = vec2(-0.94201624, -0.39906216) / 1024.0;
    PS[1] = vec2(+0.94558609, -0.76890725) / 1024.0;
    PS[2] = vec2(-0.09418410, -0.92938870) / 1024.0;
    PS[3] = vec2(+0.34495938, +0.29387760) / 1024.0;
    for (int i = 0; i < 4; i++)
        if (texture2D(u_shadow_tex, v_shadow_coord.xy +
           PS[i]).z < shadow_coord.z) visibility -= 0.2;
    if (NdotL <= 0.0) visibility = 0.5;
    float shade = mix(1.0, visibility, u_shadow_strength);
    color *= shade;
#endif // SHADOW

#ifdef HAS_OCCLUSION_MAP
    lowp float ao;
    ao = texture2D(u_occlusion_tex, v_occlusion_uv).r;
    color = mix(color, color * ao, u_occlusion_strength);
#endif

    color += u_m_emissive_factor;

    gl_FragColor = vec4(toneMap(color), 1.0);
}

#endif
