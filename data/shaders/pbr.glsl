
// Just a test.  Based on the glTF 2 PBR shader.

#define LIGHT_COUNT 1

// KHR_lights_punctual extension.
// see https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_lights_punctual
struct Light
{
    vec3 direction;
    vec3 color;
    float intensity;
    vec2 padding;
};

varying vec2 v_UVCoord1;
varying vec2 v_UVCoord2;
varying vec3 v_Position;
varying vec3 v_Color;
varying mat3 v_TBN;

uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;
uniform mat4 u_normal_matrix = mat4(1.0, 0.0, 0.0, 0.0,
                                    0.0, 1.0, 0.0, 0.0,
                                    0.0, 0.0, 1.0, 0.0,
                                    0.0, 0.0, 0.0, 1.0);

uniform float u_MetallicFactor = 0.0;
uniform float u_RoughnessFactor = 0.5;
uniform vec4 u_BaseColorFactor = vec4(1.0, 1.0, 1.0, 1.0);

uniform vec3 u_camera;

uniform Light u_Lights[LIGHT_COUNT] = {{
    vec3(0.3, 0.1, -1.0),
    vec3(1.0, 1.0, 1.0),
    2.0,
    vec2(0.0, 0.0)
}};

uniform float u_Exposure = 1.0;
uniform float u_Gamma = 2.2;

uniform lowp float u_pos_scale = 1.0;

uniform sampler2D u_occlusion_tex;
uniform int u_OcclusionUVSet = 1;
uniform float u_OcclusionStrength = 0.5;

uniform sampler2D u_bump_tex;
// uniform sampler2D u_NormalSampler;
uniform float u_NormalScale = 1.0;
uniform sampler2D u_brdf_lut;

const float M_PI = 3.141592653589793;


#ifdef VERTEX_SHADER

attribute vec3 a_pos;
attribute vec3 a_normal;
attribute vec3 a_tangent;
attribute vec4 a_color;
attribute vec2 a_occlusion_uv;

attribute vec2 a_uv;
attribute vec2 a_bump_uv;

vec4 getNormal()
{
    vec4 normal = vec4(a_normal, 0.0);
    return normalize(normal);
}


void main()
{
    vec4 pos = u_model * vec4(a_pos, 1.0) * u_pos_scale;
    v_Position = vec3(pos.xyz) / pos.w;

    vec4 tangent = vec4(normalize(a_tangent), 1.0);
    vec3 normalW = normalize(vec3(u_normal_matrix * vec4(getNormal().xyz, 0.0)));
    vec3 tangentW = normalize(vec3(u_model * vec4(tangent.xyz, 0.0)));
    vec3 bitangentW = cross(normalW, tangentW) * tangent.w;
    v_TBN = mat3(tangentW, bitangentW, normalW);

    v_UVCoord1 = (a_bump_uv + 0.5 + a_uv * 15.0) / 256.0;
    v_UVCoord2 = (a_occlusion_uv + 0.5) / (16.0 * VOXEL_TEXTURE_SIZE);
    v_Color = a_color.rgb;
    gl_Position = u_proj * u_view * pos;
}

#endif

#ifdef FRAGMENT_SHADER

struct AngularInfo
{
    float NdotL;                  // cos angle between normal and light direction
    float NdotV;                  // cos angle between normal and view direction
    float NdotH;                  // cos angle between normal and half vector
    float LdotH;                  // cos angle between light direction and half vector

    float VdotH;                  // cos angle between view direction and half vector

    vec3 padding;
};

struct MaterialInfo
{
    float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
    vec3 reflectance0;            // full reflectance color (normal incidence angle)

    float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 diffuseColor;            // color contribution from diffuse lighting

    vec3 reflectance90;           // reflectance color at grazing angle
    vec3 specularColor;           // color contribution from specular lighting
};


vec2 getNormalUV()
{
    vec3 uv = vec3(v_UVCoord1, 1.0);
    return uv.xy;
}

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
vec3 getNormal()
{
    vec2 UV = getNormalUV();
    mat3 tbn = v_TBN;
    vec3 n = texture2D(u_bump_tex, UV).rgb;
    n = normalize(tbn * ((2.0 * n - 1.0) * vec3(u_NormalScale, u_NormalScale, 1.0)));
    return n;
}

vec4 getVertexColor()
{
    vec4 color = vec4(1.0, 1.0, 1.0, 1.0);
    color.rgb = v_Color;
    return color;
}

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

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specularReflection(MaterialInfo materialInfo, AngularInfo angularInfo)
{
    return materialInfo.reflectance0 + (materialInfo.reflectance90 - materialInfo.reflectance0) * pow(clamp(1.0 - angularInfo.VdotH, 0.0, 1.0), 5.0);
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(MaterialInfo materialInfo, AngularInfo angularInfo)
{
    float alphaRoughnessSq = materialInfo.alphaRoughness * materialInfo.alphaRoughness;
    float f = (angularInfo.NdotH * alphaRoughnessSq - angularInfo.NdotH) * angularInfo.NdotH + 1.0;
    return alphaRoughnessSq / (M_PI * f * f);
}

// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float visibilityOcclusion(MaterialInfo materialInfo, AngularInfo angularInfo)
{
    float NdotL = angularInfo.NdotL;
    float NdotV = angularInfo.NdotV;
    float alphaRoughnessSq = materialInfo.alphaRoughness * materialInfo.alphaRoughness;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    return 0.5 / (GGXV + GGXL);
}

// Lambert lighting
// see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
vec3 diffuse(MaterialInfo materialInfo)
{
    return materialInfo.diffuseColor / M_PI;
}

vec3 getPointShade(vec3 pointToLight, MaterialInfo materialInfo, vec3 normal, vec3 view)
{
    AngularInfo angularInfo = getAngularInfo(pointToLight, normal, view);

    // If one of the dot products is larger than zero, no division by zero can happen. Avoids black borders.
    if (angularInfo.NdotL > 0.0 || angularInfo.NdotV > 0.0)
    {
        // Calculate the shading terms for the microfacet specular shading model
        vec3 F = specularReflection(materialInfo, angularInfo);
        float Vis = visibilityOcclusion(materialInfo, angularInfo);
        float D = microfacetDistribution(materialInfo, angularInfo);

        // Calculation of analytical lighting contribution
        vec3 diffuseContrib = (1.0 - F) * diffuse(materialInfo);
        vec3 specContrib = F * Vis * D;

        // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
        return angularInfo.NdotL * (diffuseContrib + specContrib);
    }

    return vec3(0.0, 0.0, 0.0);
}

vec2 getOcclusionUV()
{
    vec3 uv = vec3(v_UVCoord1, 1.0);
    uv.xy = u_OcclusionUVSet < 1 ? v_UVCoord1 : v_UVCoord2;
    return uv.xy;
}

vec3 applyDirectionalLight(Light light, MaterialInfo materialInfo, vec3 normal, vec3 view)
{
    vec3 pointToLight = -light.direction;
    vec3 shade = getPointShade(pointToLight, materialInfo, normal, view);
    return light.intensity * light.color * shade;
}

// Gamma Correction in Computer Graphics
// see https://www.teamten.com/lawrence/graphics/gamma/
vec3 gammaCorrection(vec3 color)
{
    return pow(color, vec3(1.0 / u_Gamma));
}

vec3 toneMap(vec3 color)
{
    color *= u_Exposure;
    return gammaCorrection(color);
}

vec3 getIBLContribution(MaterialInfo materialInfo, vec3 n, vec3 v)
{
    float NdotV = clamp(dot(n, v), 0.0, 1.0);
    vec3 reflection = normalize(reflect(-v, n));
    vec2 brdfSamplePoint = clamp(vec2(NdotV, materialInfo.perceptualRoughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
    // retrieve a scale and bias to F0. See [1], Figure 3
    vec2 brdf = texture2D(u_brdf_lut, brdfSamplePoint).rg;

    // vec4 diffuseSample = textureCube(u_DiffuseEnvSampler, n);
    // vec4 specularSample = textureCube(u_SpecularEnvSampler, reflection);
    vec4 diffuseSample = vec4(0.1, 0.1, 0.1, 1.0);
    vec4 specularSample = vec4(0.1, 0.1, 0.1, 1.0);

    vec3 diffuseLight = diffuseSample.rgb;
    vec3 specularLight = specularSample.rgb;
    vec3 diffuse = diffuseLight * materialInfo.diffuseColor;
    vec3 specular = specularLight * (materialInfo.specularColor * brdf.x + brdf.y);
    return diffuse + specular;
}

void main()
{
    // Metallic and Roughness material properties are packed together
    // In glTF, these factors can be specified by fixed scalar values
    // or from a metallic-roughness map
    float perceptualRoughness = 0.0;
    float metallic = 0.0;
    vec4 baseColor = vec4(0.0, 0.0, 0.0, 1.0);
    vec3 diffuseColor = vec3(0.0);
    vec3 specularColor= vec3(0.0);
    vec3 f0 = vec3(0.04);

    metallic = u_MetallicFactor;
    perceptualRoughness = u_RoughnessFactor;

    // The albedo may be defined from a base texture or a flat color
    baseColor = u_BaseColorFactor;
    baseColor *= getVertexColor();
    diffuseColor = baseColor.rgb * (vec3(1.0) - f0) * (1.0 - metallic);

    specularColor = mix(f0, baseColor.rgb, metallic);

    baseColor.a = 1.0;

    perceptualRoughness = clamp(perceptualRoughness, 0.0, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness [2].
    float alphaRoughness = perceptualRoughness * perceptualRoughness;

    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    vec3 specularEnvironmentR0 = specularColor.rgb;
    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    vec3 specularEnvironmentR90 = vec3(clamp(reflectance * 50.0, 0.0, 1.0));

    MaterialInfo materialInfo = MaterialInfo(
        perceptualRoughness,
        specularEnvironmentR0,
        alphaRoughness,
        diffuseColor,
        specularEnvironmentR90,
        specularColor
    );

    // LIGHTING

    vec3 color = vec3(0.0, 0.0, 0.0);
    vec3 normal = getNormal();
    vec3 view = normalize(u_camera - v_Position);

    for (int i = 0; i < LIGHT_COUNT; ++i)
    {
        Light light = u_Lights[i];
        color += applyDirectionalLight(light, materialInfo, normal, view);
    }

    color += getIBLContribution(materialInfo, normal, view);

    float ao = 1.0;

    // Apply optional PBR terms for additional (optional) shading
    ao = texture2D(u_occlusion_tex,  getOcclusionUV()).r;
    color = mix(color, color * ao, u_OcclusionStrength);

    // regular shading
    gl_FragColor = vec4(toneMap(color), baseColor.a);
    // gl_FragColor = vec4(color, 1.0);
    // gl_FragColor = vec4(diffuseColor, 1.0);

}

#endif
