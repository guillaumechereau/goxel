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
uniform highp mat4  u_shadow_mvp;
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

uniform mediump sampler2D u_shadow_tex;
uniform mediump float     u_shadow_strength;


varying highp   vec3 v_Position;
varying lowp    vec4 v_color;
varying mediump vec2 v_occlusion_uv;
varying mediump vec2 v_uv;
varying mediump vec4 v_shadow_coord;
varying mediump vec2 v_UVCoord1;
varying mediump vec3 v_gradient;

#ifdef HAS_TANGENTS
varying mediump mat3 v_TBN;
#else
varying mediump vec3 v_Normal;
#endif


const mediump float M_PI = 3.141592653589793;

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

    v_color = pow(a_color.rgba, vec4(2.2));
    v_occlusion_uv = (a_occlusion_uv + 0.5) / (16.0 * VOXEL_TEXTURE_SIZE);
    v_uv = a_uv;
    gl_Position = u_proj * u_view * vec4(v_Position, 1.0);
    gl_Position.z += u_z_ofs;
    v_shadow_coord = u_shadow_mvp * vec4(v_Position, 1.0);

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
}

#endif

#ifdef FRAGMENT_SHADER

precision mediump float;

struct Light
{
    vec3    direction;
    float   intensity;
    vec3    color;
    float   padding;
};

struct MaterialInfo
{
    float perceptualRoughness;   // roughness value
    vec3 reflectance0;           // full reflectance color
    float alphaRoughness;        // roughness mapped to a more linear change
    vec3 diffuseColor;           // color contribution from diffuse lighting
    vec3 reflectance90;          // reflectance color at grazing angle
    vec3 specularColor;          // color contribution from specular lighting
};

struct AngularInfo
{
    float NdotL;    // cos angle between normal and light direction
    float NdotV;    // cos angle between normal and view direction
    float NdotH;    // cos angle between normal and half vector
    float LdotH;    // cos angle between light direction and half vector
    float VdotH;    // cos angle between view direction and half vector
    vec3  padding;
};

AngularInfo getAngularInfo(vec3 pointToLight, vec3 normal, vec3 view)
{
    // Standard one-letter names
    vec3 n = normalize(normal);           // Outward direction of surface point
    vec3 v = normalize(view);             // Direction from surface point to view
    vec3 l = normalize(pointToLight);     // Direction from surface point to light
    vec3 h = normalize(l + v);            // Direction of the vector between l and v

    float NdotL = clamp(dot(n, l), 0.0, 1.0);
    float NdotV = clamp(dot(n, v), 0.0, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

    return AngularInfo(
        NdotL,
        NdotV,
        NdotH,
        LdotH,
        VdotH,
        vec3(0, 0, 0)
    );
}

/************************************************************************/
mediump vec3 getNormal()
{
#ifdef HAS_TANGENTS
    mediump mat3 tbn = v_TBN;
    mediump vec3 n = texture2D(u_normal_sampler, v_UVCoord1).rgb;
    n = tbn * ((2.0 * n - 1.0) * vec3(u_normal_scale, u_normal_scale, 1.0));
    n = mix(normalize(n), normalize(v_gradient), u_m_smoothness);
    return normalize(n);
#else
    return v_Normal;
#endif
}

// The following equation models the Fresnel reflectance term of the spec
// equation (aka F()) Implementation of fresnel from [4], Equation 15
vec3 specularReflection(MaterialInfo materialInfo, AngularInfo angularInfo)
{
    return materialInfo.reflectance0 +
            (materialInfo.reflectance90 - materialInfo.reflectance0) *
            pow(clamp(1.0 - angularInfo.VdotH, 0.0, 1.0), 5.0);
}

// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in
// Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3 see
// Real-Time Rendering. Page 331 to 336.
float visibilityOcclusion(MaterialInfo materialInfo, AngularInfo angularInfo)
{
    float NdotL = angularInfo.NdotL;
    float NdotV = angularInfo.NdotV;
    float alphaRoughnessSq = materialInfo.alphaRoughness *
                             materialInfo.alphaRoughness;
    float GGXV = NdotL * sqrt(
            NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(
            NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    return 0.5 / (GGXV + GGXL);
}

// The following equation(s) model the distribution of microfacet normals
// across the area being drawn (aka D()) Implementation from "Average
// Irregularity Representation of a Roughened Surface for Ray Reflection" by T.
// S. Trowbridge, and K. P. Reitz Follows the distribution function recommended
// in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(MaterialInfo materialInfo, AngularInfo angularInfo)
{
    float alphaRoughnessSq = materialInfo.alphaRoughness *
                             materialInfo.alphaRoughness;
    float f = (angularInfo.NdotH * alphaRoughnessSq - angularInfo.NdotH) *
              angularInfo.NdotH + 1.0;
    return alphaRoughnessSq / (M_PI * f * f);
}

vec3 diffuse(MaterialInfo materialInfo)
{
    return materialInfo.diffuseColor / M_PI;
}

vec3 getPointShade(vec3 pointToLight, MaterialInfo materialInfo, vec3 normal,
                   vec3 view)
{
    AngularInfo angularInfo = getAngularInfo(pointToLight, normal, view);
    // If one of the dot products is larger than zero, no division by zero can
    // happen. Avoids black borders.
    if (angularInfo.NdotL <= 0.0 && angularInfo.NdotV <= 0.0)
        return vec3(0.0, 0.0, 0.0);

    // Calculate the shading terms for the microfacet specular shading model
    vec3 F = specularReflection(materialInfo, angularInfo);
    float Vis = visibilityOcclusion(materialInfo, angularInfo);
    float D = microfacetDistribution(materialInfo, angularInfo);

    // Calculation of analytical lighting contribution
    vec3 diffuseContrib = (1.0 - F) * diffuse(materialInfo);
    vec3 specContrib = F * Vis * D;

    // Obtain final intensity as reflectance (BRDF) scaled by the energy of
    // the light (cosine law)
    vec3 ret = angularInfo.NdotL * (diffuseContrib + specContrib);

    // Shadow map.
#ifdef SHADOW
    lowp vec2 PS[4]; // Poisson offsets used for the shadow map.
    float visibility = 1.0;
    mediump vec4 shadow_coord = v_shadow_coord / v_shadow_coord.w;
    lowp float bias = 0.005 * tan(acos(clamp(angularInfo.NdotL, 0.0, 1.0)));
    bias = clamp(bias, 0.0, 0.015);
    shadow_coord.z -= bias;
    PS[0] = vec2(-0.94201624, -0.39906216) / 1024.0;
    PS[1] = vec2(+0.94558609, -0.76890725) / 1024.0;
    PS[2] = vec2(-0.09418410, -0.92938870) / 1024.0;
    PS[3] = vec2(+0.34495938, +0.29387760) / 1024.0;
    for (int i = 0; i < 4; i++)
        if (texture2D(u_shadow_tex, v_shadow_coord.xy +
           PS[i]).z < shadow_coord.z) visibility -= 0.2;
    if (angularInfo.NdotL <= 0.0) visibility = 0.5;
    ret *= mix(1.0, visibility, u_shadow_strength);
#endif // SHADOW

    return ret;
}

vec3 applyDirectionalLight(Light light, MaterialInfo materialInfo,
                           vec3 normal, vec3 view)
{
    vec3 pointToLight = -light.direction;
    vec3 shade = getPointShade(pointToLight, materialInfo, normal, view);
    return light.intensity * light.color * shade;
}

vec3 toneMap(vec3 color)
{
    // color *= u_exposure;
    return pow(color, vec3(1.0 / 2.2));
}

void main()
{

#ifdef ONLY_EDGES
    mediump vec3 n = 2.0 * texture2D(u_normal_sampler, v_UVCoord1).rgb - 1.0;
    if (n.z > 0.75)
        discard;
#endif

    float metallic = u_m_metallic;
    float perceptualRoughness = u_m_roughness;

    vec3 f0 = vec3(0.04);
    vec4 baseColor = u_m_base_color;
    baseColor *= v_color;
    vec3 diffuseColor = baseColor.rgb * (vec3(1.0) - f0) * (1.0 - metallic);
    vec3 specularColor = mix(f0, baseColor.rgb, metallic);

#ifdef MATERIAL_UNLIT
    gl_FragColor = vec4(pow(baseColor.rgb, vec3(1.0 / 2.2)), baseColor.a);
    return;
#endif

    perceptualRoughness = clamp(perceptualRoughness, 0.0, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);
    float alphaRoughness = perceptualRoughness * perceptualRoughness;
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);
    vec3 specularEnvironmentR0 = specularColor.rgb;
    vec3 specularEnvironmentR90 = vec3(clamp(reflectance * 50.0, 0.0, 1.0));

    MaterialInfo materialInfo = MaterialInfo(
        perceptualRoughness,
        specularEnvironmentR0,
        alphaRoughness,
        diffuseColor,
        specularEnvironmentR90,
        specularColor
    );

    vec3 normal = getNormal();
    vec3 view = normalize(u_camera - v_Position);

    Light light = Light(-u_l_dir, u_l_int, vec3(1.0, 1.0, 1.0), 0.0);

    vec3 color = vec3(0.0);
    color += applyDirectionalLight(light, materialInfo, normal, view);

    color += u_l_amb * baseColor.rgb;

#ifdef HAS_OCCLUSION_MAP
    lowp float ao;
    ao = texture2D(u_occlusion_tex, v_occlusion_uv).r;
    color = mix(color, color * ao, u_occlusion_strength);
#endif

    color += u_m_emissive_factor;

    gl_FragColor = vec4(toneMap(color), 1.0);
}

#endif
