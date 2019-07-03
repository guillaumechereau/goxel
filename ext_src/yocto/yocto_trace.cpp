//
// Implementation for Yocto/Trace.
//

//
// LICENSE:
//
// Copyright (c) 2016 -- 2019 Fabio Pellacini
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "yocto_trace.h"

#include <future>
#include <thread>

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR PATH TRACING SUPPORT FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

// Schlick approximation of the Fresnel term
vec3f fresnel_schlick(const vec3f& specular, float direction_cosine) {
  if (specular == zero3f) return zero3f;
  return specular + (1 - specular) *
                        pow(clamp(1 - abs(direction_cosine), 0.0f, 1.0f), 5.0f);
}

// Evaluates the GGX distribution and geometric term
float eval_microfacetD(
    float roughness, const vec3f& normal, const vec3f& half_vector, bool ggx) {
  auto cosine = dot(normal, half_vector);
  if (cosine <= 0) return 0;
  auto roughness_square = roughness * roughness;
  auto cosine_square    = cosine * cosine;
  auto tangent_square   = clamp(1 - cosine_square, 0.0f, 1.0f) / cosine_square;
  if (ggx) {
    return roughness_square / (pif * cosine_square * cosine_square *
                                  (roughness_square + tangent_square) *
                                  (roughness_square + tangent_square));
  } else {
    return exp(-tangent_square / roughness_square) /
           (pif * roughness_square * cosine_square * cosine_square);
  }
}
float evaluate_microfacetG1(float roughness, const vec3f& normal,
    const vec3f& half_vector, const vec3f& direction, bool ggx) {
  auto cosine = dot(normal, direction);
  if (dot(half_vector, direction) * cosine <= 0) return 0;
  auto roughness_square = roughness * roughness;
  auto cosine_square    = cosine * cosine;
  auto tangent_square   = clamp(1 - cosine_square, 0.0f, 1.0f) / cosine_square;
  if (ggx) {
    return 2 / (1 + sqrt(1.0f + roughness_square * tangent_square));
  } else {
    auto tangent       = sqrt(tangent_square);
    auto inv_rt        = 1 / (roughness * tangent);
    auto inv_rt_square = 1 / (roughness_square * tangent_square);
    if (inv_rt < 1.6f) {
      return (3.535f * inv_rt + 2.181f * inv_rt_square) /
             (1.0f + 2.276f * inv_rt + 2.577f * inv_rt_square);
    } else {
      return 1.0f;
    }
  }
}
float eval_microfacetG(float roughness, const vec3f& normal,
    const vec3f& half_vector, const vec3f& outgoing, const vec3f& incoming,
    bool ggx) {
  return evaluate_microfacetG1(roughness, normal, half_vector, outgoing, ggx) *
         evaluate_microfacetG1(roughness, normal, half_vector, incoming, ggx);
}
vec3f sample_microfacet(
    float roughness, const vec3f& normal, const vec2f& rn, bool ggx) {
  auto phi              = 2 * pif * rn.x;
  auto roughness_square = roughness * roughness;
  auto tangent_square   = 0.0f;
  if (ggx) {
    tangent_square = -roughness_square * log(1 - rn.y);
  } else {
    tangent_square = roughness_square * rn.y / (1 - rn.y);
  }
  auto cosine_square     = 1 / (1 + tangent_square);
  auto cosine            = 1 / sqrt(1 + tangent_square);
  auto radius            = sqrt(clamp(1 - cosine_square, 0.0f, 1.0f));
  auto local_half_vector = vec3f{cos(phi) * radius, sin(phi) * radius, cosine};
  return transform_direction(basis_fromz(normal), local_half_vector);
}
float sample_microfacet_pdf(
    float roughness, const vec3f& normal, const vec3f& half_vector, bool ggx) {
  auto cosine = dot(normal, half_vector);
  if (cosine < 0) return 0;
  return eval_microfacetD(roughness, normal, half_vector, ggx) * cosine;
}

// Phong exponent to roughness.
float exponent_to_roughness(float exponent) {
  return sqrtf(2 / (exponent + 2));
}

// Specular to  eta.
vec3f reflectivity_to_eta(const vec3f& reflectivity) {
  return (1 + sqrt(reflectivity)) / (1 - sqrt(reflectivity));
}

// Specular to fresnel eta.
pair<vec3f, vec3f> reflectivity_to_eta(
    const vec3f& reflectivity, const vec3f& edge_tint) {
  auto r = clamp(reflectivity, 0.0f, 0.99f);
  auto g = edge_tint;

  auto r_sqrt = sqrt(r);
  auto n_min  = (1 - r) / (1 + r);
  auto n_max  = (1 + r_sqrt) / (1 - r_sqrt);

  auto n  = lerp(n_max, n_min, g);
  auto k2 = ((n + 1) * (n + 1) * r - (n - 1) * (n - 1)) / (1 - r);
  k2      = max(k2, 0.0f);
  auto k  = sqrt(k2);
  return {n, k};
}

vec3f eta_to_reflectivity(const vec3f& eta) {
  return ((eta - 1) * (eta - 1)) / ((eta + 1) * (eta + 1));
}
vec3f eta_to_reflectivity(const vec3f& eta, const vec3f& etak) {
  return ((eta - 1) * (eta - 1) + etak * etak) /
         ((eta + 1) * (eta + 1) + etak * etak);
}
vec3f eta_to_edge_tint(const vec3f& eta, const vec3f& etak) {
  auto r     = eta_to_reflectivity(eta, etak);
  auto numer = (1 + sqrt(r)) / (1 - sqrt(r)) - eta;
  auto denom = (1 + sqrt(r)) / (1 - sqrt(r)) - (1 - r) / (1 + r);
  return numer / denom;
}

// Compute the fresnel term for dielectrics. Implementation from
// https://seblagarde.wordpress.com/2013/04/29/memo-on-fresnel-equations/
vec3f fresnel_dielectric(const vec3f& eta_, float cosw) {
  auto eta = eta_;
  if (cosw < 0) {
    eta  = 1 / eta;
    cosw = -cosw;
  }

  auto sin2 = 1 - cosw * cosw;
  auto eta2 = eta * eta;

  auto cos2t = 1 - sin2 / eta2;
  if (cos2t.x < 0 || cos2t.y < 0 || cos2t.z < 0) return {1, 1, 1};  // tir

  auto t0 = sqrt(cos2t);
  auto t1 = eta * t0;
  auto t2 = eta * cosw;

  auto rs = (cosw - t1) / (cosw + t1);
  auto rp = (t0 - t2) / (t0 + t2);

  return (rs * rs + rp * rp) / 2;
}

// Compute the fresnel term for metals. Implementation from
// https://seblagarde.wordpress.com/2013/04/29/memo-on-fresnel-equations/
vec3f fresnel_conductor(const vec3f& eta, const vec3f& etak, float cosw) {
  if (etak == zero3f) return fresnel_dielectric(eta, cosw);

  cosw       = clamp(cosw, (float)-1, (float)1);
  auto cos2  = cosw * cosw;
  auto sin2  = clamp(1 - cos2, (float)0, (float)1);
  auto eta2  = eta * eta;
  auto etak2 = etak * etak;

  auto t0       = eta2 - etak2 - sin2;
  auto a2plusb2 = sqrt(t0 * t0 + 4 * eta2 * etak2);
  auto t1       = a2plusb2 + cos2;
  auto a        = sqrt((a2plusb2 + t0) / 2);
  auto t2       = 2 * a * cosw;
  auto rs       = (t1 - t2) / (t1 + t2);

  auto t3 = cos2 * a2plusb2 + sin2 * sin2;
  auto t4 = t2 * sin2;
  auto rp = rs * (t3 - t4) / (t3 + t4);

  return (rp + rs) / 2;
}

pair<float, int> sample_distance(const vec3f& density, float rl, float rd) {
  auto channel         = clamp((int)(rl * 3), 0, 2);
  auto density_channel = density[channel];
  if (density_channel == 0 || rd == 0)
    return {flt_max, channel};
  else
    return {-log(rd) / density_channel, channel};
}

float sample_distance_pdf(const vec3f& density, float distance, int channel) {
  auto density_channel = density[channel];
  return exp(-density_channel * distance);
}

vec3f eval_transmission(const vec3f& density, float distance) {
  return exp(-density * distance);
}

vec3f sample_phasefunction(float g, const vec2f& u) {
  auto cos_theta = 0.0f;
  if (abs(g) < 1e-3f) {
    cos_theta = 1 - 2 * u.x;
  } else {
    float square = (1 - g * g) / (1 - g + 2 * g * u.x);
    cos_theta    = (1 + g * g - square * square) / (2 * g);
  }

  auto sin_theta = sqrt(max(0.0f, 1 - cos_theta * cos_theta));
  auto phi       = 2 * pif * u.y;
  return {sin_theta * cos(phi), sin_theta * sin(phi), cos_theta};
}

float eval_phasefunction(float cos_theta, float g) {
  auto denom = 1 + g * g + 2 * g * cos_theta;
  return (1 - g * g) / (4 * pif * denom * sqrt(denom));
}

// Tabulated ior for metals
// https://github.com/tunabrain/tungsten
const unordered_map<string, pair<vec3f, vec3f>> metal_ior_table = {
    {"a-C", {{2.9440999183f, 2.2271502925f, 1.9681668794f},
                {0.8874329109f, 0.7993216383f, 0.8152862927f}}},
    {"Ag", {{0.1552646489f, 0.1167232965f, 0.1383806959f},
               {4.8283433224f, 3.1222459278f, 2.1469504455f}}},
    {"Al", {{1.6574599595f, 0.8803689579f, 0.5212287346f},
               {9.2238691996f, 6.2695232477f, 4.8370012281f}}},
    {"AlAs", {{3.6051023902f, 3.2329365777f, 2.2175611545f},
                 {0.0006670247f, -0.0004999400f, 0.0074261204f}}},
    {"AlSb", {{-0.0485225705f, 4.1427547893f, 4.6697691348f},
                 {-0.0363741915f, 0.0937665154f, 1.3007390124f}}},
    {"Au", {{0.1431189557f, 0.3749570432f, 1.4424785571f},
               {3.9831604247f, 2.3857207478f, 1.6032152899f}}},
    {"Be", {{4.1850592788f, 3.1850604423f, 2.7840913457f},
               {3.8354398268f, 3.0101260162f, 2.8690088743f}}},
    {"Cr", {{4.3696828663f, 2.9167024892f, 1.6547005413f},
               {5.2064337956f, 4.2313645277f, 3.7549467933f}}},
    {"CsI", {{2.1449030413f, 1.7023164587f, 1.6624194173f},
                {0.0000000000f, 0.0000000000f, 0.0000000000f}}},
    {"Cu", {{0.2004376970f, 0.9240334304f, 1.1022119527f},
               {3.9129485033f, 2.4528477015f, 2.1421879552f}}},
    {"Cu2O", {{3.5492833755f, 2.9520622449f, 2.7369202137f},
                 {0.1132179294f, 0.1946659670f, 0.6001681264f}}},
    {"CuO", {{3.2453822204f, 2.4496293965f, 2.1974114493f},
                {0.5202739621f, 0.5707372756f, 0.7172250613f}}},
    {"d-C", {{2.7112524747f, 2.3185812849f, 2.2288565009f},
                {0.0000000000f, 0.0000000000f, 0.0000000000f}}},
    {"Hg", {{2.3989314904f, 1.4400254917f, 0.9095512090f},
               {6.3276269444f, 4.3719414152f, 3.4217899270f}}},
    {"HgTe", {{4.7795267752f, 3.2309984581f, 2.6600252401f},
                 {1.6319827058f, 1.5808189339f, 1.7295753852f}}},
    {"Ir", {{3.0864098394f, 2.0821938440f, 1.6178866805f},
               {5.5921510077f, 4.0671757150f, 3.2672611269f}}},
    {"K", {{0.0640493070f, 0.0464100621f, 0.0381842017f},
              {2.1042155920f, 1.3489364357f, 0.9132113889f}}},
    {"Li", {{0.2657871942f, 0.1956102432f, 0.2209198538f},
               {3.5401743407f, 2.3111306542f, 1.6685930000f}}},
    {"MgO", {{2.0895885542f, 1.6507224525f, 1.5948759692f},
                {0.0000000000f, -0.0000000000f, 0.0000000000f}}},
    {"Mo", {{4.4837010280f, 3.5254578255f, 2.7760769438f},
               {4.1111307988f, 3.4208716252f, 3.1506031404f}}},
    {"Na", {{0.0602665320f, 0.0561412435f, 0.0619909494f},
               {3.1792906496f, 2.1124800781f, 1.5790940266f}}},
    {"Nb", {{3.4201353595f, 2.7901921379f, 2.3955856658f},
               {3.4413817900f, 2.7376437930f, 2.5799132708f}}},
    {"Ni", {{2.3672753521f, 1.6633583302f, 1.4670554172f},
               {4.4988329911f, 3.0501643957f, 2.3454274399f}}},
    {"Rh", {{2.5857954933f, 1.8601866068f, 1.5544279524f},
               {6.7822927110f, 4.7029501026f, 3.9760892461f}}},
    {"Se-e", {{5.7242724833f, 4.1653992967f, 4.0816099264f},
                 {0.8713747439f, 1.1052845009f, 1.5647788766f}}},
    {"Se", {{4.0592611085f, 2.8426947380f, 2.8207582835f},
               {0.7543791750f, 0.6385150558f, 0.5215872029f}}},
    {"SiC", {{3.1723450205f, 2.5259677964f, 2.4793623897f},
                {0.0000007284f, -0.0000006859f, 0.0000100150f}}},
    {"SnTe", {{4.5251865890f, 1.9811525984f, 1.2816819226f},
                 {0.0000000000f, 0.0000000000f, 0.0000000000f}}},
    {"Ta", {{2.0625846607f, 2.3930915569f, 2.6280684948f},
               {2.4080467973f, 1.7413705864f, 1.9470377016f}}},
    {"Te-e", {{7.5090397678f, 4.2964603080f, 2.3698732430f},
                 {5.5842076830f, 4.9476231084f, 3.9975145063f}}},
    {"Te", {{7.3908396088f, 4.4821028985f, 2.6370708478f},
               {3.2561412892f, 3.5273908133f, 3.2921683116f}}},
    {"ThF4", {{1.8307187117f, 1.4422274283f, 1.3876488528f},
                 {0.0000000000f, 0.0000000000f, 0.0000000000f}}},
    {"TiC", {{3.7004673762f, 2.8374356509f, 2.5823030278f},
                {3.2656905818f, 2.3515586388f, 2.1727857800f}}},
    {"TiN", {{1.6484691607f, 1.1504482522f, 1.3797795097f},
                {3.3684596226f, 1.9434888540f, 1.1020123347f}}},
    {"TiO2-e", {{3.1065574823f, 2.5131551146f, 2.5823844157f},
                   {0.0000289537f, -0.0000251484f, 0.0001775555f}}},
    {"TiO2", {{3.4566203131f, 2.8017076558f, 2.9051485020f},
                 {0.0001026662f, -0.0000897534f, 0.0006356902f}}},
    {"VC", {{3.6575665991f, 2.7527298065f, 2.5326814570f},
               {3.0683516659f, 2.1986687713f, 1.9631816252f}}},
    {"VN", {{2.8656011588f, 2.1191817791f, 1.9400767149f},
               {3.0323264950f, 2.0561075580f, 1.6162930914f}}},
    {"V", {{4.2775126218f, 3.5131538236f, 2.7611257461f},
              {3.4911844504f, 2.8893580874f, 3.1116965117f}}},
    {"W", {{4.3707029924f, 3.3002972445f, 2.9982666528f},
              {3.5006778591f, 2.6048652781f, 2.2731930614f}}},
};

// Get a complex ior table with keys the metal name and values (eta, etak)
pair<vec3f, vec3f> get_conductor_eta(const string& name) {
  if (metal_ior_table.find(name) == metal_ior_table.end())
    return {zero3f, zero3f};
  return metal_ior_table.at(name);
}

// Stores sigma_prime_s, sigma_a
static unordered_map<string, pair<vec3f, vec3f>> subsurface_params_table = {
    // From "A Practical Model for Subsurface Light Transport"
    // Jensen, Marschner, Levoy, Hanrahan
    // Proc SIGGRAPH 2001
    {"Apple", {{2.29, 2.39, 1.97}, {0.0030, 0.0034, 0.046}}},
    {"Chicken1", {{0.15, 0.21, 0.38}, {0.015, 0.077, 0.19}}},
    {"Chicken2", {{0.19, 0.25, 0.32}, {0.018, 0.088, 0.20}}},
    {"Cream", {{7.38, 5.47, 3.15}, {0.0002, 0.0028, 0.0163}}},
    {"Ketchup", {{0.18, 0.07, 0.03}, {0.061, 0.97, 1.45}}},
    {"Marble", {{2.19, 2.62, 3.00}, {0.0021, 0.0041, 0.0071}}},
    {"Potato", {{0.68, 0.70, 0.55}, {0.0024, 0.0090, 0.12}}},
    {"Skimmilk", {{0.70, 1.22, 1.90}, {0.0014, 0.0025, 0.0142}}},
    {"Skin1", {{0.74, 0.88, 1.01}, {0.032, 0.17, 0.48}}},
    {"Skin2", {{1.09, 1.59, 1.79}, {0.013, 0.070, 0.145}}},
    {"Spectralon", {{11.6, 20.4, 14.9}, {0.00, 0.00, 0.00}}},
    {"Wholemilk", {{2.55, 3.21, 3.77}, {0.0011, 0.0024, 0.014}}},
    // From "Acquiring Scattering Properties of Participating Media by
    // Dilution",
    // Narasimhan, Gupta, Donner, Ramamoorthi, Nayar, Jensen
    // Proc SIGGRAPH 2006
    {"Lowfat Milk", {{0.89187, 1.5136, 2.532}, {0.002875, 0.00575, 0.0115}}},
    {"Reduced Milk",
        {{2.4858, 3.1669, 4.5214}, {0.0025556, 0.0051111, 0.012778}}},
    {"Regular Milk", {{4.5513, 5.8294, 7.136}, {0.0015333, 0.0046, 0.019933}}},
    {"Espresso", {{0.72378, 0.84557, 1.0247}, {4.7984, 6.5751, 8.8493}}},
    {"Mint Mocha Coffee", {{0.31602, 0.38538, 0.48131}, {3.772, 5.8228, 7.82}}},
    {"Lowfat Soy Milk",
        {{0.30576, 0.34233, 0.61664}, {0.0014375, 0.0071875, 0.035937}}},
    {"Regular Soy Milk",
        {{0.59223, 0.73866, 1.4693}, {0.0019167, 0.0095833, 0.065167}}},
    {"Lowfat Chocolate Milk",
        {{0.64925, 0.83916, 1.1057}, {0.0115, 0.0368, 0.1564}}},
    {"Regular Chocolate Milk",
        {{1.4585, 2.1289, 2.9527}, {0.010063, 0.043125, 0.14375}}},
    {"Coke", {{8.9053e-05, 8.372e-05, 0}, {0.10014, 0.16503, 0.2468}}},
    {"Pepsi", {{6.1697e-05, 4.2564e-05, 0}, {0.091641, 0.14158, 0.20729}}},
    {"Sprite", {{6.0306e-06, 6.4139e-06, 6.5504e-06},
                   {0.001886, 0.0018308, 0.0020025}}},
    {"Gatorade",
        {{0.0024574, 0.003007, 0.0037325}, {0.024794, 0.019289, 0.008878}}},
    {"Chardonnay",
        {{1.7982e-05, 1.3758e-05, 1.2023e-05}, {0.010782, 0.011855, 0.023997}}},
    {"White Zinfandel",
        {{1.7501e-05, 1.9069e-05, 1.288e-05}, {0.012072, 0.016184, 0.019843}}},
    {"Merlot", {{2.1129e-05, 0, 0}, {0.11632, 0.25191, 0.29434}}},
    {"Budweiser Beer",
        {{2.4356e-05, 2.4079e-05, 1.0564e-05}, {0.011492, 0.024911, 0.057786}}},
    {"Coors Light Beer",
        {{5.0922e-05, 4.301e-05, 0}, {0.006164, 0.013984, 0.034983}}},
    {"Clorox",
        {{0.0024035, 0.0031373, 0.003991}, {0.0033542, 0.014892, 0.026297}}},
    {"Apple Juice",
        {{0.00013612, 0.00015836, 0.000227}, {0.012957, 0.023741, 0.052184}}},
    {"Cranberry Juice",
        {{0.00010402, 0.00011646, 7.8139e-05}, {0.039437, 0.094223, 0.12426}}},
    {"Grape Juice", {{5.382e-05, 0, 0}, {0.10404, 0.23958, 0.29325}}},
    {"Ruby Grapefruit Juice",
        {{0.011002, 0.010927, 0.011036}, {0.085867, 0.18314, 0.25262}}},
    {"White Grapefruit Juice",
        {{0.22826, 0.23998, 0.32748}, {0.0138, 0.018831, 0.056781}}},
    {"Shampoo",
        {{0.0007176, 0.0008303, 0.0009016}, {0.014107, 0.045693, 0.061717}}},
    {"Strawberry Shampoo",
        {{0.00015671, 0.00015947, 1.518e-05}, {0.01449, 0.05796, 0.075823}}},
    {"Head & Shoulders Shampoo",
        {{0.023805, 0.028804, 0.034306}, {0.084621, 0.15688, 0.20365}}},
    {"Lemon Tea Powder",
        {{0.040224, 0.045264, 0.051081}, {2.4288, 4.5757, 7.2127}}},
    {"Orange Powder",
        {{0.00015617, 0.00017482, 0.0001762}, {0.001449, 0.003441, 0.007863}}},
    {"Pink Lemonade Powder",
        {{0.00012103, 0.00013073, 0.00012528}, {0.001165, 0.002366, 0.003195}}},
    {"Cappuccino Powder", {{1.8436, 2.5851, 2.1662}, {35.844, 49.547, 61.084}}},
    {"Salt Powder",
        {{0.027333, 0.032451, 0.031979}, {0.28415, 0.3257, 0.34148}}},
    {"Sugar Powder",
        {{0.00022272, 0.00025513, 0.000271}, {0.012638, 0.031051, 0.050124}}},
    {"Suisse Mocha Powder",
        {{2.7979, 3.5452, 4.3365}, {17.502, 27.004, 35.433}}},
    {"Pacific Ocean Surface Water",
        {{0.0001764, 0.00032095, 0.00019617}, {0.031845, 0.031324, 0.030147}}}};

// Get subsurface params
pair<vec3f, vec3f> get_subsurface_params(const string& name) {
  if (subsurface_params_table.find(name) == subsurface_params_table.end())
    return {zero3f, zero3f};
  return subsurface_params_table.at(name);
}

// Check if we are on the same side of the hemisphere
bool same_hemisphere(
    const vec3f& normal, const vec3f& outgoing, const vec3f& incoming) {
  return dot(normal, outgoing) * dot(normal, incoming) > 0;
}
bool other_hemisphere(
    const vec3f& normal, const vec3f& outgoing, const vec3f& incoming) {
  return dot(normal, outgoing) * dot(normal, incoming) < 0;
}

// Minimum roughness for GGX
static const auto trace_min_roughness = 0.03f * 0.03f;

// Evaluates/sample the BRDF scaled by the cosine of the incoming direction.
vec3f eval_diffuse_reflection(float roughness, const vec3f& normal,
    const vec3f& outgoing, const vec3f& incoming) {
  if (!same_hemisphere(normal, outgoing, incoming)) return zero3f;
  return vec3f{abs(dot(normal, incoming)) / pif};
}
vec3f eval_diffuse_translucency(float roughness, const vec3f& normal,
    const vec3f& outgoing, const vec3f& incoming) {
  if (!other_hemisphere(normal, outgoing, incoming)) return zero3f;
  return vec3f{abs(dot(normal, incoming)) / pif};
}
vec3f eval_microfacet_reflection(float roughness, const vec3f& eta,
    const vec3f& etak, const vec3f& normal, const vec3f& outgoing,
    const vec3f& incoming) {
  if (!same_hemisphere(normal, outgoing, incoming)) return zero3f;
  roughness      = clamp(roughness, trace_min_roughness, 1.0f);
  auto up_normal = dot(normal, outgoing) > 0 ? normal : -normal;
  auto halfway   = normalize(incoming + outgoing);
  auto F         = etak == zero3f
               ? fresnel_dielectric(eta, abs(dot(halfway, outgoing)))
               : fresnel_conductor(eta, etak, abs(dot(halfway, outgoing)));
  auto D = eval_microfacetD(roughness, up_normal, halfway);
  auto G = eval_microfacetG(roughness, up_normal, halfway, outgoing, incoming);
  return F * D * G / abs(4 * dot(normal, outgoing) * dot(normal, incoming)) *
         abs(dot(normal, incoming));
}
vec3f eval_delta_reflection(const vec3f& eta, const vec3f& etak,
    const vec3f& normal, const vec3f& outgoing, const vec3f& incoming) {
  if (!same_hemisphere(normal, outgoing, incoming)) return zero3f;
  auto F = etak == zero3f
               ? fresnel_dielectric(eta, abs(dot(normal, outgoing)))
               : fresnel_conductor(eta, etak, abs(dot(normal, outgoing)));
  return F;
}
vec3f eval_microfacet_transmission(float roughness, const vec3f& eta,
    const vec3f& normal, const vec3f& outgoing, const vec3f& incoming) {
  if (!other_hemisphere(normal, outgoing, incoming)) return zero3f;
  roughness           = clamp(roughness, trace_min_roughness, 1.0f);
  auto up_normal      = dot(outgoing, normal) > 0 ? normal : -normal;
  auto halfway_vector = dot(outgoing, normal) > 0 ? -(outgoing + eta * incoming)
                                                  : (eta * outgoing + incoming);
  auto halfway = normalize(halfway_vector);
  auto F       = fresnel_dielectric(eta, abs(dot(halfway, outgoing)));
  auto D       = eval_microfacetD(roughness, up_normal, halfway);
  auto G = eval_microfacetG(roughness, up_normal, halfway, outgoing, incoming);

  auto dot_terms = (dot(outgoing, halfway) * dot(incoming, halfway)) /
                   (dot(outgoing, normal) * dot(incoming, normal));

  auto numerator   = (1 - F) * D * G;
  auto denominator = dot(halfway_vector, halfway_vector);

  // [Walter 2007] equation 21
  return abs(dot_terms) * numerator / denominator * abs(dot(normal, incoming));
}
vec3f eval_delta_transmission(const vec3f& eta, const vec3f& normal,
    const vec3f& outgoing, const vec3f& incoming) {
  if (!other_hemisphere(normal, outgoing, incoming)) return zero3f;
  auto F = fresnel_dielectric(eta, abs(dot(normal, outgoing)));
  return (1 - F);
}
vec3f eval_microfacet_transparency(float roughness, const vec3f& eta,
    const vec3f& normal, const vec3f& outgoing, const vec3f& incoming) {
  if (!other_hemisphere(normal, outgoing, incoming)) return zero3f;
  roughness      = clamp(roughness, trace_min_roughness, 1.0f);
  auto up_normal = dot(outgoing, normal) > 0 ? normal : -normal;
  auto ir        = reflect(-incoming, up_normal);
  auto halfway   = normalize(ir + outgoing);
  auto F         = fresnel_dielectric(eta, abs(dot(halfway, outgoing)));
  auto D         = eval_microfacetD(roughness, up_normal, halfway);
  auto G = eval_microfacetG(roughness, up_normal, halfway, outgoing, ir);
  return (1 - F) * D * G /
         abs(4 * dot(normal, outgoing) * dot(normal, incoming)) *
         abs(dot(normal, incoming));
}
vec3f eval_delta_transparency(const vec3f& eta, const vec3f& normal,
    const vec3f& outgoing, const vec3f& incoming) {
  if (!other_hemisphere(normal, outgoing, incoming)) return zero3f;
  if (eta == zero3f || eta == vec3f{1, 1, 1}) {
    return {1, 1, 1};
  } else {
    auto F = fresnel_dielectric(eta, abs(dot(normal, outgoing)));
    return (1 - F);
  }
}
vec3f eval_delta_passthrough(
    const vec3f& normal, const vec3f& outgoing, const vec3f& incoming) {
  if (!other_hemisphere(normal, outgoing, incoming)) return zero3f;
  return {1, 1, 1};
}
vec3f eval_volume_scattering(const vec3f& albedo, float phaseg,
    const vec3f& outgoing, const vec3f& incoming) {
  return albedo * eval_phasefunction(dot(outgoing, incoming), phaseg);
}

// Picks a direction based on the BRDF
vec3f sample_diffuse_reflection(float roughness, const vec3f& normal,
    const vec3f& outgoing, const vec2f& rn) {
  auto up_normal = dot(normal, outgoing) > 0 ? normal : -normal;
  return sample_hemisphere(up_normal, rn);
}
vec3f sample_diffuse_translucency(float roughness, const vec3f& normal,
    const vec3f& outgoing, const vec2f& rn) {
  auto up_normal = dot(normal, outgoing) > 0 ? normal : -normal;
  return sample_hemisphere(-up_normal, rn);
}
vec3f sample_microfacet_reflection(float roughness, const vec3f& eta,
    const vec3f& normal, const vec3f& outgoing, const vec2f& rn) {
  roughness      = clamp(roughness, trace_min_roughness, 1.0f);
  auto up_normal = dot(normal, outgoing) > 0 ? normal : -normal;
  auto halfway   = sample_microfacet(roughness, up_normal, rn);
  return reflect(outgoing, halfway);
}
vec3f sample_delta_reflection(
    const vec3f& eta, const vec3f& normal, const vec3f& outgoing) {
  auto up_normal = dot(normal, outgoing) > 0 ? normal : -normal;
  return reflect(outgoing, up_normal);
}
vec3f sample_microfacet_transmission(float roughness, const vec3f& eta,
    const vec3f& normal, const vec3f& outgoing, const vec2f& rn) {
  roughness      = clamp(roughness, trace_min_roughness, 1.0f);
  auto up_normal = dot(normal, outgoing) > 0 ? normal : -normal;
  auto halfway   = sample_microfacet(roughness, up_normal, rn);
  return refract(
      outgoing, halfway, dot(normal, outgoing) > 0 ? 1 / mean(eta) : mean(eta));
}
vec3f sample_delta_transmission(
    const vec3f& eta, const vec3f& normal, const vec3f& outgoing) {
  auto up_normal = dot(normal, outgoing) > 0 ? normal : -normal;
  return refract(outgoing, up_normal,
      dot(normal, outgoing) > 0 ? 1 / mean(eta) : mean(eta));
}
vec3f sample_microfacet_transparency(float roughness, const vec3f& eta,
    const vec3f& normal, const vec3f& outgoing, const vec2f& rn) {
  roughness      = clamp(roughness, trace_min_roughness, 1.0f);
  auto up_normal = dot(normal, outgoing) > 0 ? normal : -normal;
  auto halfway   = sample_microfacet(roughness, up_normal, rn);
  auto ir        = reflect(outgoing, halfway);
  return -reflect(ir, up_normal);
}
vec3f sample_delta_transparency(
    const vec3f& eta, const vec3f& normal, const vec3f& outgoing) {
  return -outgoing;
}
vec3f sample_delta_passthrough(const vec3f& normal, const vec3f& outgoing) {
  return -outgoing;
}
vec3f sample_volume_scattering(
    const vec3f& albedo, float phaseg, const vec3f& outgoing, const vec2f& rn) {
  auto direction = sample_phasefunction(phaseg, rn);
  return basis_fromz(-outgoing) * direction;
}

// Compute the weight for sampling the BRDF
float sample_diffuse_reflection_pdf(float roughness, const vec3f& normal,
    const vec3f& outgoing, const vec3f& incoming) {
  if (!same_hemisphere(normal, outgoing, incoming)) return 0;
  return abs(dot(normal, incoming)) / pif;
}
float sample_diffuse_translucency_pdf(float roughness, const vec3f& normal,
    const vec3f& outgoing, const vec3f& incoming) {
  if (!other_hemisphere(normal, outgoing, incoming)) return 0;
  return abs(dot(normal, incoming)) / pif;
}
float sample_microfacet_reflection_pdf(float roughness, const vec3f& eta,
    const vec3f& etak, const vec3f& normal, const vec3f& outgoing,
    const vec3f& incoming) {
  if (!same_hemisphere(normal, outgoing, incoming)) return 0;
  roughness      = clamp(roughness, trace_min_roughness, 1.0f);
  auto up_normal = dot(normal, outgoing) >= 0 ? normal : -normal;
  auto halfway   = normalize(incoming + outgoing);
  return sample_microfacet_pdf(roughness, up_normal, halfway) /
         (4 * abs(dot(outgoing, halfway)));
}
float sample_delta_reflection_pdf(const vec3f& eta, const vec3f& etak,
    const vec3f& normal, const vec3f& outgoing, const vec3f& incoming) {
  if (!same_hemisphere(normal, outgoing, incoming)) return 0;
  return 1;
}
float sample_microfacet_transmission_pdf(float roughness, const vec3f& eta,
    const vec3f& normal, const vec3f& outgoing, const vec3f& incoming) {
  if (!other_hemisphere(normal, outgoing, incoming)) return 0;
  roughness           = clamp(roughness, trace_min_roughness, 1.0f);
  auto up_normal      = dot(outgoing, normal) > 0 ? normal : -normal;
  auto halfway_vector = dot(outgoing, normal) > 0 ? -(outgoing + eta * incoming)
                                                  : (eta * outgoing + incoming);
  auto halfway = normalize(halfway_vector);
  // [Walter 2007] equation 17
  return sample_microfacet_pdf(roughness, up_normal, halfway) *
         abs(dot(halfway, incoming)) / dot(halfway_vector, halfway_vector);
}
float sample_delta_transmission_pdf(const vec3f& eta, const vec3f& normal,
    const vec3f& outgoing, const vec3f& incoming) {
  if (!other_hemisphere(normal, outgoing, incoming)) return 0;
  return 1;
}
float sample_microfacet_transparency_pdf(float roughness, const vec3f& eta,
    const vec3f& normal, const vec3f& outgoing, const vec3f& incoming) {
  if (!other_hemisphere(normal, outgoing, incoming)) return 0;
  roughness      = clamp(roughness, trace_min_roughness, 1.0f);
  auto up_normal = dot(outgoing, normal) > 0 ? normal : -normal;
  auto ir        = reflect(-incoming, up_normal);
  auto halfway   = normalize(ir + outgoing);
  auto d         = sample_microfacet_pdf(roughness, up_normal, halfway);
  return d / (4 * abs(dot(outgoing, halfway)));
}
float sample_delta_transparency_pdf(const vec3f& eta, const vec3f& normal,
    const vec3f& outgoing, const vec3f& incoming) {
  if (!other_hemisphere(normal, outgoing, incoming)) return 0;
  return 1;
}
float sample_delta_passthrough_pdf(
    const vec3f& normal, const vec3f& outgoing, const vec3f& incoming) {
  if (!other_hemisphere(normal, outgoing, incoming)) return 0;
  return 1;
}
float sample_volume_scattering_pdf(const vec3f& albedo, float phaseg,
    const vec3f& outgoing, const vec3f& incoming) {
  return eval_phasefunction(dot(outgoing, incoming), phaseg);
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR PATH TRACING
// -----------------------------------------------------------------------------
namespace yocto {

// Set non-rigid frames as default
static const bool trace_non_rigid_frames = true;

// defaults
static const auto coat_roughness = 0.03f * 0.03f;

bool has_brdf(const material_point& material) {
  return material.coat != zero3f || material.specular != zero3f ||
         material.diffuse != zero3f || material.transmission != zero3f;
}
bool has_volume(const material_point& material) {
  return material.voldensity != zero3f;
}
bool is_delta(const material_point& material) {
  return material.roughness == 0;
}

vec3f eval_emission(const material_point& material, const vec3f& normal,
    const vec3f& outgoing) {
  return material.emission;
}
vec3f eval_volemission(const material_point& material, const vec3f& outgoing) {
  return material.volemission;
}

// Evaluates/sample the BRDF scaled by the cosine of the incoming direction.
vec3f eval_brdfcos(const material_point& material, const vec3f& normal,
    const vec3f& outgoing, const vec3f& incoming) {
  if (is_delta(material)) return zero3f;

  auto up_normal = dot(normal, outgoing) > 0 ? normal : -normal;
  auto ceta      = reflectivity_to_eta(material.coat);
  auto eta       = reflectivity_to_eta(material.specular);
  if (!material.thin && material.transmission != zero3f &&
      dot(normal, outgoing) < 0) {
    ceta = 1 / ceta;
    eta  = 1 / eta;
  }

  auto brdfcos = zero3f;

  if (material.coat != zero3f && same_hemisphere(normal, outgoing, incoming)) {
    auto halfway = normalize(incoming + outgoing);
    auto fresnel = fresnel_dielectric(ceta, abs(dot(halfway, outgoing)));
    auto D       = eval_microfacetD(coat_roughness, up_normal, halfway);
    auto G       = eval_microfacetG(
        coat_roughness, up_normal, halfway, outgoing, incoming);
    brdfcos += fresnel * D * G /
               abs(4 * dot(normal, outgoing) * dot(normal, incoming)) *
               abs(dot(normal, incoming));
  }
  if (material.specular != zero3f &&
      same_hemisphere(normal, outgoing, incoming)) {
    auto halfway = normalize(incoming + outgoing);
    auto coat    = fresnel_dielectric(ceta, abs(dot(halfway, outgoing)));
    auto fresnel = fresnel_dielectric(eta, abs(dot(halfway, outgoing)));
    auto D       = eval_microfacetD(material.roughness, up_normal, halfway);
    auto G       = eval_microfacetG(
        material.roughness, up_normal, halfway, outgoing, incoming);
    brdfcos += (1 - coat) * fresnel * D * G /
               abs(4 * dot(normal, outgoing) * dot(normal, incoming)) *
               abs(dot(normal, incoming));
  }
  if (material.diffuse != zero3f &&
      same_hemisphere(normal, outgoing, incoming)) {
    auto halfway  = normalize(incoming + outgoing);
    auto coat     = fresnel_dielectric(ceta, abs(dot(halfway, outgoing)));
    auto specular = fresnel_dielectric(eta, abs(dot(halfway, outgoing)));
    brdfcos += (1 - coat) * (1 - specular) * material.diffuse / pif *
               abs(dot(normal, incoming));
  }
  if (material.transmission != zero3f &&
      other_hemisphere(normal, outgoing, incoming) && !material.thin) {
    auto eta            = mean(reflectivity_to_eta(material.specular));
    auto halfway_vector = dot(outgoing, normal) > 0
                              ? -(outgoing + eta * incoming)
                              : (eta * outgoing + incoming);
    auto halfway = normalize(halfway_vector);
    auto coat    = fresnel_dielectric(ceta, abs(dot(halfway, outgoing)));
    auto fresnel = fresnel_dielectric(
        dot(outgoing, normal) > 0 ? vec3f{eta} : vec3f{1 / eta}, abs(dot(halfway, outgoing)));
    auto D = eval_microfacetD(material.roughness, up_normal, halfway);
    auto G = eval_microfacetG(
        material.roughness, up_normal, halfway, outgoing, incoming);

    auto dot_terms = (dot(outgoing, halfway) * dot(incoming, halfway)) /
                     (dot(outgoing, normal) * dot(incoming, normal));

    // [Walter 2007] equation 21
    brdfcos += (1 - coat) * material.transmission * abs(dot_terms) *
               (1 - fresnel) * D * G / dot(halfway_vector, halfway_vector) *
               abs(dot(normal, incoming));
  }
  if (material.transmission != zero3f &&
      other_hemisphere(normal, outgoing, incoming) && material.thin) {
    auto ir      = reflect(-incoming, up_normal);
    auto halfway = normalize(ir + outgoing);
    auto fresnel = fresnel_dielectric(ceta, abs(dot(halfway, outgoing)));
    auto D = eval_microfacetD(material.roughness, up_normal, halfway);
    auto G = eval_microfacetG(
        material.roughness, up_normal, halfway, outgoing, ir);
    brdfcos += material.transmission * (1 - fresnel) * D * G /
               abs(4 * dot(normal, outgoing) * dot(normal, incoming)) *
               abs(dot(normal, incoming));
  }
  return brdfcos;
}
vec3f eval_delta(const material_point& material, const vec3f& normal,
    const vec3f& outgoing, const vec3f& incoming) {
  if (!is_delta(material)) return zero3f;
  auto brdfcos = zero3f;

  auto ceta = reflectivity_to_eta(material.coat);
  auto eta  = reflectivity_to_eta(material.specular);
  if (!material.thin && material.transmission != zero3f &&
      dot(normal, outgoing) < 0) {
    ceta = 1 / ceta;
    eta  = 1 / eta;
  }

  if (material.coat != zero3f && same_hemisphere(normal, outgoing, incoming)) {
    brdfcos += fresnel_dielectric(ceta, abs(dot(normal, outgoing)));
  }
  if (material.specular != zero3f &&
      same_hemisphere(normal, outgoing, incoming)) {
    auto coat = fresnel_dielectric(ceta, abs(dot(normal, outgoing)));
    brdfcos += (1 - coat) * fresnel_dielectric(eta, abs(dot(normal, outgoing)));
  }
  if (material.transmission != zero3f &&
      other_hemisphere(normal, outgoing, incoming)) {
    auto coat     = fresnel_dielectric(ceta, abs(dot(normal, outgoing)));
    auto specular = fresnel_dielectric(eta, abs(dot(normal, outgoing)));
    brdfcos += (1 - coat) * (1 - specular) * material.transmission;
  }
  return brdfcos;
}

vec4f compute_brdf_pdfs(const material_point& material, const vec3f& normal,
    const vec3f& outgoing) {
  auto ceta = reflectivity_to_eta(material.coat);
  auto eta  = reflectivity_to_eta(material.specular);
  if (!material.thin && material.transmission != zero3f &&
      dot(normal, outgoing) < 0) {
    ceta = 1 / ceta;
    eta  = 1 / eta;
  }

  auto ndo      = abs(dot(outgoing, normal));
  auto coat     = fresnel_dielectric(ceta, ndo);
  auto specular = fresnel_dielectric(eta, ndo);

  auto weights = zero4f;
  weights[0]   = max(coat);
  weights[1]   = max((1 - coat) * specular);
  weights[2]   = max((1 - coat) * (1 - specular) * material.diffuse);
  weights[3]   = max((1 - coat) * (1 - specular) * material.transmission);
  weights /= sum(weights);
  return weights;
}

// Picks a direction based on the BRDF
vec3f sample_brdf(const material_point& material, const vec3f& normal,
    const vec3f& outgoing, float rnl, const vec2f& rn) {
  if (is_delta(material)) return zero3f;

  auto pdfs      = compute_brdf_pdfs(material, normal, outgoing);
  auto up_normal = dot(normal, outgoing) > 0 ? normal : -normal;

  // keep a weight sum to pick a lobe
  auto weight_sum = 0.0f;

  weight_sum += pdfs[0];
  if (rnl < weight_sum) {
    auto halfway = sample_microfacet(coat_roughness, up_normal, rn);
    return reflect(outgoing, halfway);
  }

  weight_sum += pdfs[1];
  if (rnl < weight_sum) {
    auto halfway = sample_microfacet(material.roughness, up_normal, rn);
    return reflect(outgoing, halfway);
  }

  weight_sum += pdfs[2];
  if (rnl < weight_sum) {
    return sample_hemisphere(up_normal, rn);
  }

  weight_sum += pdfs[3];
  if (rnl < weight_sum && !material.thin) {
    auto halfway = sample_microfacet(material.roughness, up_normal, rn);
    auto eta     = mean(reflectivity_to_eta(material.specular));
    return refract_notir(
        outgoing, halfway, dot(normal, outgoing) > 0 ? 1 / eta : eta);
  }
  if (rnl < weight_sum && material.thin) {
    auto halfway = sample_microfacet(material.roughness, up_normal, rn);
    auto ir      = reflect(outgoing, halfway);
    return -reflect(ir, up_normal);
  }

  // something went wrong if we got here
  return zero3f;
}
vec3f sample_delta(const material_point& material, const vec3f& normal,
    const vec3f& outgoing, float rnl) {
  if (!is_delta(material)) return zero3f;

  auto up_normal = dot(normal, outgoing) > 0 ? normal : -normal;
  auto pdfs      = compute_brdf_pdfs(material, normal, outgoing);

  // keep a weight sum to pick a lobe
  auto weight_sum = 0.0f;

  weight_sum += pdfs[0];
  if (rnl < weight_sum) {
    return reflect(outgoing, up_normal);
  }

  weight_sum += pdfs[1];
  if (rnl < weight_sum) {
    return reflect(outgoing, up_normal);
  }

  weight_sum += pdfs[2];
  // skip diffuse

  weight_sum += pdfs[3];
  if (rnl < weight_sum && !material.thin) {
    auto eta = mean(reflectivity_to_eta(material.specular));
    return refract_notir(
        outgoing, up_normal, dot(normal, outgoing) > 0 ? 1 / eta : eta);
  }
  if (rnl < weight_sum && material.thin) {
    return -outgoing;
  }

  // something went wrong if we got here
  return zero3f;
}

// Compute the weight for sampling the BRDF
float sample_brdf_pdf(const material_point& material, const vec3f& normal,
    const vec3f& outgoing, const vec3f& incoming) {
  if (is_delta(material)) return 0;

  auto up_normal = dot(normal, outgoing) >= 0 ? normal : -normal;
  auto pdfs      = compute_brdf_pdfs(material, normal, outgoing);
  auto pdf       = 0.0f;

  if (pdfs[0] && same_hemisphere(normal, outgoing, incoming)) {
    auto halfway = normalize(incoming + outgoing);
    pdf += pdfs[0] * sample_microfacet_pdf(coat_roughness, up_normal, halfway) /
           (4 * abs(dot(outgoing, halfway)));
  }

  if (pdfs[1] && same_hemisphere(normal, outgoing, incoming)) {
    auto halfway = normalize(incoming + outgoing);
    pdf += pdfs[1] *
           sample_microfacet_pdf(material.roughness, up_normal, halfway) /
           (4 * abs(dot(outgoing, halfway)));
  }

  if (pdfs[2] && same_hemisphere(normal, outgoing, incoming)) {
    pdf += pdfs[2] * sample_hemisphere_pdf(up_normal, incoming);
  }

  if (pdfs[3] && other_hemisphere(normal, outgoing, incoming) &&
      !material.thin) {
    auto eta            = mean(reflectivity_to_eta(material.specular));
    auto halfway_vector = dot(outgoing, normal) > 0
                              ? -(outgoing + eta * incoming)
                              : (eta * outgoing + incoming);
    auto halfway = normalize(halfway_vector);
    // [Walter 2007] equation 17
    pdf += pdfs[3] *
           sample_microfacet_pdf(material.roughness, up_normal, halfway) *
           abs(dot(halfway, incoming)) / dot(halfway_vector, halfway_vector);
  }
  if (pdfs[3] && other_hemisphere(normal, outgoing, incoming) &&
      material.thin) {
    auto up_normal = dot(outgoing, normal) > 0 ? normal : -normal;
    auto ir        = reflect(-incoming, up_normal);
    auto halfway   = normalize(ir + outgoing);
    auto d = sample_microfacet_pdf(material.roughness, up_normal, halfway);
    pdf += pdfs[3] * d / (4 * abs(dot(outgoing, halfway)));
  }

  return pdf;
}
float sample_delta_pdf(const material_point& material, const vec3f& normal,
    const vec3f& outgoing, const vec3f& incoming) {
  if (!is_delta(material)) return 0;
  auto pdfs = compute_brdf_pdfs(material, normal, outgoing);

  auto pdf = 0.0f;
  if (pdfs[0] && same_hemisphere(normal, outgoing, incoming)) pdf += pdfs[0];
  if (pdfs[1] && same_hemisphere(normal, outgoing, incoming)) pdf += pdfs[1];
  if (pdfs[3] && other_hemisphere(normal, outgoing, incoming)) pdf += pdfs[3];

  return pdf;
}

vec3f eval_volscattering(const material_point& material, const vec3f& outgoing,
    const vec3f& incoming) {
  if (!has_volume(material)) return zero3f;
  auto scattering = zero3f;
  if (material.voldensity != zero3f) {
    scattering += eval_volume_scattering(
        material.volscatter, material.volanisotropy, outgoing, incoming);
  }
  return scattering;
}
vec3f sample_volscattering(const material_point& material,
    const vec3f& outgoing, float rnl, const vec2f& rn) {
  if (!has_volume(material)) return zero3f;
  auto weights = vec2f{max(material.voldensity), 0};
  if (weights == zero2f) return zero3f;
  weights /= sum(weights);

  // keep a weight sum to pick a lobe
  auto weight_sum = 0.0f;
  weight_sum += weights[0];
  if (rnl < weight_sum) {
    return sample_volume_scattering(
        material.volscatter, material.volanisotropy, outgoing, rn);
  }

  // something went wrong if we got here
  return zero3f;
}
float sample_volscattering_pdf(const material_point& material,
    const vec3f& outgoing, const vec3f& incoming) {
  if (!has_volume(material)) return 0;
  auto weights = vec2f{max(material.voldensity), 0};
  if (weights == zero2f) return 0;
  weights /= sum(weights);

  // commpute pdf
  auto pdf = 0.0f;
  if (weights[0]) {
    pdf += weights[0] * sample_volume_scattering_pdf(material.volscatter,
                            material.volanisotropy, outgoing, incoming);
  }

  return pdf;
}

// Sample pdf for an environment.
float sample_environment_pdf(const yocto_scene& scene,
    const trace_lights& lights, int environment_id, const vec3f& incoming) {
  auto& environment = scene.environments[environment_id];
  if (environment.emission_tex >= 0) {
    auto& cdf          = lights.environment_cdfs[environment.emission_tex];
    auto& emission_tex = scene.textures[environment.emission_tex];
    auto  size         = texture_size(emission_tex);
    auto  texcoord     = eval_texcoord(environment, incoming);
    auto  i            = clamp((int)(texcoord.x * size.x), 0, size.x - 1);
    auto  j            = clamp((int)(texcoord.y * size.y), 0, size.y - 1);
    auto  prob         = sample_discrete_pdf(cdf, j * size.x + i) / cdf.back();
    auto  angle        = (2 * pif / size.x) * (pif / size.y) *
                 sin(pif * (j + 0.5f) / size.y);
    return prob / angle;
  } else {
    return 1 / (4 * pif);
  }
}

// Picks a point on an environment.
vec3f sample_environment(const yocto_scene& scene, const trace_lights& lights,
    int environment_id, float rel, const vec2f& ruv) {
  auto& environment = scene.environments[environment_id];
  if (environment.emission_tex >= 0) {
    auto& cdf          = lights.environment_cdfs[environment.emission_tex];
    auto& emission_tex = scene.textures[environment.emission_tex];
    auto  idx          = sample_discrete(cdf, rel);
    auto  size         = texture_size(emission_tex);
    auto  u            = (idx % size.x + 0.5f) / size.x;
    auto  v            = (idx / size.x + 0.5f) / size.y;
    return eval_direction(environment, {u, v});
  } else {
    return sample_sphere(ruv);
  }
}

// Picks a point on a light.
vec3f sample_light(const yocto_scene& scene, const trace_lights& lights,
    int instance_id, const vec3f& p, float rel, const vec2f& ruv) {
  auto& instance = scene.instances[instance_id];
  auto& shape    = scene.shapes[instance.shape];
  auto& cdf      = lights.shape_cdfs[instance.shape];
  auto  sample   = sample_shape(shape, cdf, rel, ruv);
  auto  element  = sample.first;
  auto  uv       = sample.second;
  return normalize(eval_position(scene, instance, element, uv) - p);
}

// Sample pdf for a light point.
float sample_light_pdf(const yocto_scene& scene, const trace_lights& lights,
    int instance_id, const bvh_scene& bvh, const vec3f& position,
    const vec3f& direction) {
  auto& instance = scene.instances[instance_id];
  auto& material = scene.materials[instance.material];
  if (material.emission == zero3f) return 0;
  auto& cdf = lights.shape_cdfs[instance.shape];
  // check all intersection
  auto pdf           = 0.0f;
  auto next_position = position;
  for (auto bounce = 0; bounce < 100; bounce++) {
    auto isec = intersect_bvh(bvh, instance_id, {next_position, direction});
    if (!isec.hit) break;
    // accumulate pdf
    auto& instance      = scene.instances[isec.instance];
    auto light_position = eval_position(scene, instance, isec.element, isec.uv);
    auto light_normal   = eval_normal(
        scene, instance, isec.element, isec.uv, trace_non_rigid_frames);
    // prob triangle * area triangle = area triangle mesh
    auto area = cdf.back();
    pdf += distance_squared(light_position, position) /
           (abs(dot(light_normal, direction)) * area);
    // continue
    next_position = light_position + direction * 1e-3f;
  }
  return pdf;
}

// Sample lights wrt solid angle
vec3f sample_lights(const yocto_scene& scene, const trace_lights& lights,
    const bvh_scene& bvh, const vec3f& position, float rl, float rel,
    const vec2f& ruv) {
  auto light_id = sample_uniform(
      lights.instances.size() + lights.environments.size(), rl);
  if (light_id < lights.instances.size()) {
    auto instance = lights.instances[light_id];
    return sample_light(scene, lights, instance, position, rel, ruv);
  } else {
    auto environment =
        lights.environments[light_id - (int)lights.instances.size()];
    return sample_environment(scene, lights, environment, rel, ruv);
  }
}

// Sample lights pdf
float sample_lights_pdf(const yocto_scene& scene, const trace_lights& lights,
    const bvh_scene& bvh, const vec3f& position, const vec3f& direction) {
  auto pdf = 0.0f;
  for (auto instance : lights.instances) {
    pdf += sample_light_pdf(scene, lights, instance, bvh, position, direction);
  }
  for (auto environment : lights.environments) {
    pdf += sample_environment_pdf(scene, lights, environment, direction);
  }
  pdf *= sample_uniform_pdf(
      lights.instances.size() + lights.environments.size());
  return pdf;
}

// Trace stats.
std::atomic<uint64_t> _trace_npaths{0};
std::atomic<uint64_t> _trace_nrays{0};

// Sample camera
ray3f sample_camera(const yocto_camera& camera, const vec2i& ij,
    const vec2i& image_size, const vec2f& puv, const vec2f& luv) {
  return eval_camera(camera, ij, image_size, puv, sample_disk(luv));
}
ray3f sample_camera_tent(const yocto_camera& camera, const vec2i& ij,
    const vec2i& image_size, const vec2f& puv, const vec2f& luv) {
  const auto width  = 2.0f;
  const auto offset = 0.5f;
  auto       fuv =
      width *
          vec2f{
              puv.x < 0.5f ? sqrt(2 * puv.x) - 1 : 1 - sqrt(2 - 2 * puv.x),
              puv.y - 0.5f ? sqrt(2 * puv.y) - 1 : 1 - sqrt(2 - 2 * puv.y),
          } +
      offset;
  return eval_camera(camera, ij, image_size, fuv, sample_disk(luv));
}

// Recursive path tracing.
pair<vec3f, bool> trace_path(const yocto_scene& scene, const bvh_scene& bvh,
    const trace_lights& lights, const vec3f& origin_, const vec3f& direction_,
    rng_state& rng, const trace_params& params) {
  // initialize
  auto radiance     = zero3f;
  auto weight       = vec3f{1, 1, 1};
  auto origin       = origin_;
  auto direction    = direction_;
  auto volume_stack = vector<pair<material_point, int>>{};
  auto hit          = false;

  // trace  path
  for (auto bounce = 0; bounce < params.bounces; bounce++) {
    // intersect next point
    _trace_nrays += 1;
    auto intersection = intersect_bvh(bvh, {origin, direction});
    if (!intersection.hit) {
      radiance += weight * eval_environment(scene, direction);
      break;
    }

    // handle transmission if inside a volume
    auto in_volume = false;
    if (!volume_stack.empty()) {
      auto medium              = volume_stack.back().first;
      auto [distance, channel] = sample_distance(
          medium.voldensity, rand1f(rng), rand1f(rng));
      distance = min(distance, intersection.distance);
      weight *= eval_transmission(medium.voldensity, distance) /
                sample_distance_pdf(medium.voldensity, distance, channel);
      in_volume             = distance < intersection.distance;
      intersection.distance = distance;
    }

    // switch between surface and volume
    if (!in_volume) {
      // prepare shading point
      auto  outgoing = -direction;
      auto& instance = scene.instances[intersection.instance];
      auto  position = eval_position(
          scene, instance, intersection.element, intersection.uv);
      auto normal   = eval_shading_normal(scene, instance, intersection.element,
          intersection.uv, direction, trace_non_rigid_frames);
      auto material = eval_material(
          scene, instance, intersection.element, intersection.uv);

      // handle opacity
      if (material.opacity < 1 && rand1f(rng) >= material.opacity) {
        origin = position + direction * 1e-2f;
        bounce -= 1;
        continue;
      }
      hit = true;

      // accumulate emission
      radiance += weight * eval_emission(material, normal, outgoing);

      // next direction
      auto incoming = zero3f;
      if (!is_delta(material)) {
        if (rand1f(rng) < 0.5f) {
          incoming = sample_brdf(
              material, normal, outgoing, rand1f(rng), rand2f(rng));
        } else {
          incoming = sample_lights(scene, lights, bvh, position, rand1f(rng),
              rand1f(rng), rand2f(rng));
        }
        weight *=
            eval_brdfcos(material, normal, outgoing, incoming) /
            (0.5f * sample_brdf_pdf(material, normal, outgoing, incoming) +
                0.5f *
                    sample_lights_pdf(scene, lights, bvh, position, incoming));
      } else {
        incoming = sample_delta(material, normal, outgoing, rand1f(rng));
        weight *= eval_delta(material, normal, outgoing, incoming) /
                  sample_delta_pdf(material, normal, outgoing, incoming);
      }

      // update volume stack
      if (material.voldensity != zero3f &&
          other_hemisphere(normal, outgoing, incoming)) {
        if (volume_stack.empty()) {
          volume_stack.push_back({material, intersection.instance});
        } else {
          volume_stack.pop_back();
        }
      }

      // setup next iteration
      origin    = position;
      direction = incoming;
    } else {
      // prepare shading point
      auto outgoing = -direction;
      auto position = origin + direction * intersection.distance;
      auto material = volume_stack.back().first;

      // handle opacity
      hit = true;

      // accumulate emission
      radiance += weight * eval_volemission(material, outgoing);

      // next direction
      auto incoming = zero3f;
      if (rand1f(rng) < 0.5f) {
        incoming = sample_volscattering(
            material, outgoing, rand1f(rng), rand2f(rng));
      } else {
        incoming = sample_lights(scene, lights, bvh, position, rand1f(rng),
            rand1f(rng), rand2f(rng));
      }
      weight *=
          eval_volscattering(material, outgoing, incoming) /
          (0.5f * sample_volscattering_pdf(material, outgoing, incoming) +
              0.5f * sample_lights_pdf(scene, lights, bvh, position, incoming));

      // setup next iteration
      origin    = position;
      direction = incoming;
    }

    // check weight
    if (weight == zero3f || !isfinite(weight)) break;

    // russian roulette
    if (max(weight) < 1 && bounce > 6) {
      auto rr_prob = max((float)0.05, 1 - max(weight));
      if (rand1f(rng) > rr_prob) break;
      weight *= 1 / rr_prob;
    }
  }

  return {radiance, hit};
}

// Recursive path tracing.
pair<vec3f, bool> trace_naive(const yocto_scene& scene, const bvh_scene& bvh,
    const trace_lights& lights, const vec3f& origin_, const vec3f& direction_,
    rng_state& rng, const trace_params& params) {
  // initialize
  auto radiance  = zero3f;
  auto weight    = vec3f{1, 1, 1};
  auto origin    = origin_;
  auto direction = direction_;
  auto hit       = false;

  // trace  path
  for (auto bounce = 0; bounce < params.bounces; bounce++) {
    // intersect next point
    _trace_nrays += 1;
    auto intersection = intersect_bvh(bvh, {origin, direction});
    if (!intersection.hit) {
      radiance += weight * eval_environment(scene, direction);
      break;
    }

    // prepare shading point
    auto  outgoing = -direction;
    auto  incoming = outgoing;
    auto& instance = scene.instances[intersection.instance];
    auto  position = eval_position(
        scene, instance, intersection.element, intersection.uv);
    auto normal   = eval_shading_normal(scene, instance, intersection.element,
        intersection.uv, direction, trace_non_rigid_frames);
    auto material = eval_material(
        scene, instance, intersection.element, intersection.uv);

    // handle opacity
    if (material.opacity < 1 && rand1f(rng) >= material.opacity) {
      origin = position + direction * 1e-2f;
      bounce -= 1;
      continue;
    }
    hit = true;

    // accumulate emission
    radiance += weight * eval_emission(material, normal, outgoing);

    // next direction
    if (!is_delta(material)) {
      incoming = sample_brdf(
          material, normal, outgoing, rand1f(rng), rand2f(rng));
      weight *= eval_brdfcos(material, normal, outgoing, incoming) /
                sample_brdf_pdf(material, normal, outgoing, incoming);
    } else {
      incoming = sample_delta(material, normal, outgoing, rand1f(rng));
      weight *= eval_delta(material, normal, outgoing, incoming) /
                sample_delta_pdf(material, normal, outgoing, incoming);
    }

    // check weight
    if (weight == zero3f || !isfinite(weight)) break;

    // russian roulette
    if (max(weight) < 1 && bounce > 3) {
      auto rr_prob = max((float)0.05, 1 - max(weight));
      if (rand1f(rng) > rr_prob) break;
      weight *= 1 / rr_prob;
    }

    // setup next iteration
    origin    = position;
    direction = incoming;
  }

  return {radiance, hit};
}

// Eyelight for quick previewing.
pair<vec3f, bool> trace_eyelight(const yocto_scene& scene, const bvh_scene& bvh,
    const trace_lights& lights, const vec3f& origin_, const vec3f& direction_,
    rng_state& rng, const trace_params& params) {
  // initialize
  auto radiance  = zero3f;
  auto weight    = vec3f{1, 1, 1};
  auto origin    = origin_;
  auto direction = direction_;
  auto hit       = false;

  // trace  path
  for (auto bounce = 0; bounce < max(params.bounces, 4); bounce++) {
    // intersect next point
    _trace_nrays += 1;
    auto intersection = intersect_bvh(bvh, {origin, direction});
    if (!intersection.hit) {
      radiance += weight * eval_environment(scene, direction);
      break;
    }

    // prepare shading point
    auto  outgoing = -direction;
    auto& instance = scene.instances[intersection.instance];
    auto  position = eval_position(
        scene, instance, intersection.element, intersection.uv);
    auto normal   = eval_shading_normal(scene, instance, intersection.element,
        intersection.uv, direction, trace_non_rigid_frames);
    auto material = eval_material(
        scene, instance, intersection.element, intersection.uv);

    // handle opacity
    if (material.opacity < 1 && rand1f(rng) >= material.opacity) {
      origin = position + direction * 1e-2f;
      bounce -= 1;
      continue;
    }
    hit = true;

    // accumulate emission
    radiance += weight * eval_emission(material, normal, outgoing);

    // brdf * light
    radiance += weight * pif *
                eval_brdfcos(material, normal, outgoing, outgoing);

    // continue path
    if (!is_delta(material)) break;
    auto incoming = sample_delta(material, normal, outgoing, rand1f(rng));
    weight *= eval_delta(material, normal, outgoing, incoming) /
              sample_delta_pdf(material, normal, outgoing, incoming);
    if (weight == zero3f || !isfinite(weight)) break;

    // setup next iteration
    origin    = position;
    direction = incoming;
  }

  return {radiance, hit};
}

// False color rendering
pair<vec3f, bool> trace_falsecolor(const yocto_scene& scene,
    const bvh_scene& bvh, const trace_lights& lights, const vec3f& origin,
    const vec3f& direction, rng_state& rng, const trace_params& params) {
  // intersect next point
  auto intersection = intersect_bvh(bvh, ray3f{origin, direction});
  if (!intersection.hit) {
    return {zero3f, false};
  }

  // get scene elements
  auto& instance = scene.instances[intersection.instance];
  auto& shape    = scene.shapes[instance.shape];
  // auto& material = scene.materials[instance.material];

  // prepare shading point
  auto outgoing = -direction;
  // auto  position = eval_position(
  //     scene, instance, intersection.element, intersection.uv);
  auto normal   = eval_shading_normal(scene, instance, intersection.element,
      intersection.uv, direction, trace_non_rigid_frames);
  auto material = eval_material(
      scene, instance, intersection.element, intersection.uv);

  switch (params.falsecolor) {
    case trace_params::falsecolor_type::normal: {
      return {normal * 0.5f + 0.5f, 1};
    }
    case trace_params::falsecolor_type::frontfacing: {
      auto frontfacing = dot(normal, outgoing) > 0 ? vec3f{0, 1, 0}
                                                   : vec3f{1, 0, 0};
      return {frontfacing, 1};
    }
    case trace_params::falsecolor_type::gnormal: {
      auto normal = eval_element_normal(
          scene, instance, intersection.element, true);
      return {normal * 0.5f + 0.5f, 1};
    }
    case trace_params::falsecolor_type::gfrontfacing: {
      auto normal = eval_element_normal(
          scene, instance, intersection.element, true);
      auto frontfacing = dot(normal, outgoing) > 0 ? vec3f{0, 1, 0}
                                                   : vec3f{1, 0, 0};
      return {frontfacing, 1};
    }
    case trace_params::falsecolor_type::texcoord: {
      auto texcoord = eval_texcoord(
          shape, intersection.element, intersection.uv);
      return {{texcoord.x, texcoord.y, 0}, 1};
    }
    case trace_params::falsecolor_type::color: {
      auto color = eval_color(shape, intersection.element, intersection.uv);
      return {xyz(color), 1};
    }
    case trace_params::falsecolor_type::emission: {
      return {material.emission, 1};
    }
    case trace_params::falsecolor_type::diffuse: {
      return {material.diffuse, 1};
    }
    case trace_params::falsecolor_type::specular: {
      return {material.specular, 1};
    }
    case trace_params::falsecolor_type::transmission: {
      return {material.transmission, 1};
    }
    case trace_params::falsecolor_type::roughness: {
      return {vec3f{material.roughness}, 1};
    }
    case trace_params::falsecolor_type::material: {
      auto hashed = std::hash<int>()(instance.material);
      auto rng_   = make_rng(trace_default_seed, hashed);
      return {pow(0.5f + 0.5f * rand3f(rng_), 2.2f), 1};
    }
    case trace_params::falsecolor_type::element: {
      auto hashed = std::hash<int>()(intersection.element);
      auto rng_   = make_rng(trace_default_seed, hashed);
      return {pow(0.5f + 0.5f * rand3f(rng_), 2.2f), 1};
    }
    case trace_params::falsecolor_type::shape: {
      auto hashed = std::hash<int>()(instance.shape);
      auto rng_   = make_rng(trace_default_seed, hashed);
      return {pow(0.5f + 0.5f * rand3f(rng_), 2.2f), 1};
    }
    case trace_params::falsecolor_type::instance: {
      auto hashed = std::hash<int>()(intersection.instance);
      auto rng_   = make_rng(trace_default_seed, hashed);
      return {pow(0.5f + 0.5f * rand3f(rng_), 2.2f), 1};
    }
    case trace_params::falsecolor_type::highlight: {
      auto emission = material.emission;
      auto outgoing = -direction;
      if (emission == zero3f) emission = {0.2f, 0.2f, 0.2f};
      return {emission * abs(dot(outgoing, normal)), 1};
    }
    default: {
      return {zero3f, false};
    }
  }
}

// Trace a single ray from the camera using the given algorithm.
using trace_sampler_func = pair<vec3f, bool> (*)(const yocto_scene& scene,
    const bvh_scene& bvh, const trace_lights& lights, const vec3f& position,
    const vec3f& direction, rng_state& rng, const trace_params& params);
trace_sampler_func get_trace_sampler_func(const trace_params& params) {
  switch (params.sampler) {
    case trace_params::sampler_type::path: return trace_path;
    case trace_params::sampler_type::naive: return trace_naive;
    case trace_params::sampler_type::eyelight: return trace_eyelight;
    case trace_params::sampler_type::falsecolor: return trace_falsecolor;
    default: {
      throw std::runtime_error("sampler unknown");
      return nullptr;
    }
  }
}

// Check is a sampler requires lights
bool is_sampler_lit(const trace_params& params) {
  switch (params.sampler) {
    case trace_params::sampler_type::path: return true;
    case trace_params::sampler_type::naive: return true;
    case trace_params::sampler_type::eyelight: return false;
    case trace_params::sampler_type::falsecolor: return false;
    default: {
      throw std::runtime_error("sampler unknown");
      return false;
    }
  }
}

// Get trace pixel
trace_pixel& get_trace_pixel(trace_state& state, int i, int j) {
  return state.pixels[j * state.image_size.x + i];
}

// Trace a block of samples
void trace_region(image<vec4f>& image, trace_state& state,
    const yocto_scene& scene, const bvh_scene& bvh, const trace_lights& lights,
    const image_region& region, int num_samples, const trace_params& params) {
  auto& camera  = scene.cameras.at(params.camera);
  auto  sampler = get_trace_sampler_func(params);
  for (auto j = region.min.y; j < region.max.y; j++) {
    for (auto i = region.min.x; i < region.max.x; i++) {
      auto& pixel = get_trace_pixel(state, i, j);
      for (auto s = 0; s < num_samples; s++) {
        if (params.cancel && *params.cancel) return;
        _trace_npaths += 1;
        auto ray = params.tentfilter
                       ? sample_camera_tent(camera, {i, j}, image.size(),
                             rand2f(pixel.rng), rand2f(pixel.rng))
                       : sample_camera(camera, {i, j}, image.size(),
                             rand2f(pixel.rng), rand2f(pixel.rng));
        auto [radiance, hit] = sampler(
            scene, bvh, lights, ray.o, ray.d, pixel.rng, params);
        if (!hit) {
          if (params.envhidden || scene.environments.empty()) {
            radiance = zero3f;
            hit      = false;
          } else {
            hit = true;
          }
        }
        if (!isfinite(radiance)) {
          // printf("NaN detected\n");
          radiance = zero3f;
        }
        if (max(radiance) > params.clamp)
          radiance = radiance * (params.clamp / max(radiance));
        pixel.radiance += radiance;
        pixel.hits += hit ? 1 : 0;
        pixel.samples += 1;
      }
      auto radiance = pixel.hits ? pixel.radiance / pixel.hits : zero3f;
      auto coverage = (float)pixel.hits / (float)pixel.samples;
      image[{i, j}] = {radiance.x, radiance.y, radiance.z, coverage};
    }
  }
}

// Init a sequence of random number generators.
trace_state make_trace_state(const vec2i& image_size, uint64_t seed) {
  auto state = trace_state{image_size,
      vector<trace_pixel>(image_size.x * image_size.y, trace_pixel{})};
  auto rng   = make_rng(1301081);
  for (auto j = 0; j < state.image_size.y; j++) {
    for (auto i = 0; i < state.image_size.x; i++) {
      auto& pixel = get_trace_pixel(state, i, j);
      pixel.rng   = make_rng(seed, rand1i(rng, 1 << 31) / 2 + 1);
    }
  }
  return state;
}
void make_trace_state(
    trace_state& state, const vec2i& image_size, uint64_t seed) {
  state    = trace_state{image_size,
      vector<trace_pixel>(image_size.x * image_size.y, trace_pixel{})};
  auto rng = make_rng(1301081);
  for (auto j = 0; j < state.image_size.y; j++) {
    for (auto i = 0; i < state.image_size.x; i++) {
      auto& pixel = get_trace_pixel(state, i, j);
      pixel.rng   = make_rng(seed, rand1i(rng, 1 << 31) / 2 + 1);
    }
  }
}

// Init trace lights
trace_lights make_trace_lights(const yocto_scene& scene) {
  auto lights = trace_lights{};
  lights.shape_cdfs.resize(scene.shapes.size());
  lights.environment_cdfs.resize(scene.textures.size());
  for (auto idx = 0; idx < scene.instances.size(); idx++) {
    auto& instance = scene.instances[idx];
    auto& shape    = scene.shapes[instance.shape];
    auto& material = scene.materials[instance.material];
    if (material.emission == zero3f) continue;
    if (shape.triangles.empty() && shape.quads.empty()) continue;
    lights.instances.push_back(idx);
    lights.shape_cdfs[instance.shape] = sample_shape_cdf(shape);
  }
  for (auto idx = 0; idx < scene.environments.size(); idx++) {
    auto& environment = scene.environments[idx];
    if (environment.emission == zero3f) continue;
    lights.environments.push_back(idx);
    if (environment.emission_tex >= 0) {
      lights.environment_cdfs[environment.emission_tex] =
          sample_environment_cdf(scene, environment);
    }
  }
  return lights;
}
void make_trace_lights(trace_lights& lights, const yocto_scene& scene) {
  lights = {};
  lights.shape_cdfs.resize(scene.shapes.size());
  lights.environment_cdfs.resize(scene.textures.size());
  for (auto idx = 0; idx < scene.instances.size(); idx++) {
    auto& instance = scene.instances[idx];
    auto& shape    = scene.shapes[instance.shape];
    auto& material = scene.materials[instance.material];
    if (material.emission == zero3f) continue;
    if (shape.triangles.empty() && shape.quads.empty()) continue;
    lights.instances.push_back(idx);
    sample_shape_cdf(shape, lights.shape_cdfs[instance.shape]);
  }
  for (auto idx = 0; idx < scene.environments.size(); idx++) {
    auto& environment = scene.environments[idx];
    if (environment.emission == zero3f) continue;
    lights.environments.push_back(idx);
    if (environment.emission_tex >= 0) {
      sample_environment_cdf(scene, environment,
          lights.environment_cdfs[environment.emission_tex]);
    }
  }
}

// Progressively compute an image by calling trace_samples multiple times.
image<vec4f> trace_image(const yocto_scene& scene, const bvh_scene& bvh,
    const trace_lights& lights, const trace_params& params) {
  auto image_size = camera_resolution(
      scene.cameras.at(params.camera), params.resolution);
  auto render  = image{image_size, zero4f};
  auto state   = make_trace_state(render.size(), params.seed);
  auto regions = make_regions(render.size(), params.region, true);

  if (params.noparallel) {
    for (auto& region : regions) {
      if (params.cancel && *params.cancel) break;
      trace_region(
          render, state, scene, bvh, lights, region, params.samples, params);
    }
  } else {
    auto                futures  = vector<std::future<void>>{};
    auto                nthreads = std::thread::hardware_concurrency();
    std::atomic<size_t> next_idx(0);
    for (auto thread_id = 0; thread_id < nthreads; thread_id++) {
      futures.emplace_back(std::async(
          std::launch::async, [&render, &state, &scene, &bvh, &lights, &params,
                                  &regions, &next_idx]() {
            while (true) {
              if (params.cancel && *params.cancel) break;
              auto idx = next_idx.fetch_add(1);
              if (idx >= regions.size()) break;
              trace_region(render, state, scene, bvh, lights, regions[idx],
                  params.samples, params);
            }
          }));
    }
    for (auto& f : futures) f.get();
  }

  return render;
}

// Progressively compute an image by calling trace_samples multiple times.
int trace_samples(image<vec4f>& render, trace_state& state,
    const yocto_scene& scene, const bvh_scene& bvh, const trace_lights& lights,
    int current_sample, const trace_params& params) {
  auto regions     = make_regions(render.size(), params.region, true);
  auto num_samples = min(params.batch, params.samples - current_sample);
  if (params.noparallel) {
    for (auto& region : regions) {
      if (params.cancel && *params.cancel) break;
      trace_region(
          render, state, scene, bvh, lights, region, params.samples, params);
    }
  } else {
    auto                futures  = vector<std::future<void>>{};
    auto                nthreads = std::thread::hardware_concurrency();
    std::atomic<size_t> next_idx(0);
    for (auto thread_id = 0; thread_id < nthreads; thread_id++) {
      futures.emplace_back(std::async(
          std::launch::async, [&render, &state, &scene, &bvh, &lights, &params,
                                  &regions, &next_idx, num_samples]() {
            while (true) {
              if (params.cancel && *params.cancel) break;
              auto idx = next_idx.fetch_add(1);
              if (idx >= regions.size()) break;
              trace_region(render, state, scene, bvh, lights, regions[idx],
                  num_samples, params);
            }
          }));
    }
    for (auto& f : futures) f.get();
  }
  return current_sample + num_samples;
}

// Trace statistics for last run used for fine tuning implementation.
// For now returns number of paths and number of rays.
pair<uint64_t, uint64_t> get_trace_stats() {
  return {_trace_nrays, _trace_npaths};
}
void reset_trace_stats() {
  _trace_nrays  = 0;
  _trace_npaths = 0;
}

}  // namespace yocto

// -----------------------------------------------------------------------------
// UNUSED CODE KEPT FOR REFERENCE
// -----------------------------------------------------------------------------
namespace yocto {

#if YOCTO_TRACE_THINSHEET

// Schlick approximation of the Fresnel term
vec3f fresnel_schlick(
    const vec3f& specular, const vec3f& half_vector, const vec3f& incoming) {
  if (specular == zero3f) return zero3f;
  return specular +
         (vec3f{1, 1, 1} - specular) *
             pow(clamp(1.0f - fabs(dot(half_vector, incoming)), 0.0f, 1.0f),
                 5.0f);
}
vec3f fresnel_schlick(const vec3f& specular, const vec3f& half_vector,
    const vec3f& incoming, float roughness) {
  if (specular == zero3f) return zero3f;
  auto fks = fresnel_schlick(specular, half_vector, incoming);
  return specular + (fks - specular) * (1 - sqrt(clamp(roughness, 0.0f, 1.0f)));
}

// Evaluates the GGX distribution and geometric term
float evaluate_ggx_distribution(
    float roughness, const vec3f& normal, const vec3f& half_vector) {
  auto di = (dot(normal, half_vector) * dot(normal, half_vector)) *
                (roughness * roughness - 1) +
            1;
  return roughness * roughness / (pif * di * di);
}
float evaluate_ggx_shadowing(float roughness, const vec3f& normal,
    const vec3f& outgoing, const vec3f& incoming) {
#if 0
    // evaluate G from Heitz
    auto lambda_o = (-1 + sqrt(1 + alpha2 * (1 - ndo * ndo) / (ndo * ndo))) / 2;
    auto lambda_i = (-1 + sqrt(1 + alpha2 * (1 - ndi * ndi) / (ndi * ndi))) / 2;
    auto g = 1 / (1 + lambda_o + lambda_i);
#else
  auto Go = (2 * fabs(dot(normal, outgoing))) /
            (fabs(dot(normal, outgoing)) +
                sqrt(roughness * roughness + (1 - roughness * roughness) *
                                                 dot(normal, outgoing) *
                                                 dot(normal, outgoing)));
  auto Gi = (2 * fabs(dot(normal, incoming))) /
            (fabs(dot(normal, incoming)) +
                sqrt(roughness * roughness + (1 - roughness * roughness) *
                                                 dot(normal, incoming) *
                                                 dot(normal, incoming)));
  return Go * Gi;
#endif
}
// Evaluates the GGX pdf
float sample_ggx_pdf(float rs, float ndh) {
  auto alpha2 = rs * rs;
  auto di     = (ndh * ndh) * (alpha2 - 1) + 1;
  auto d      = alpha2 / (pif * di * di);
  return d * ndh;
}
// Sample the GGX distribution
vec3f sample_ggx(float rs, const vec2f& rn) {
  auto tan2 = rs * rs * rn.y / (1 - rn.y);
  auto rz   = sqrt(1 / (tan2 + 1));
  auto rr   = sqrt(1 - rz * rz);
  auto rphi = 2 * pif * rn.x;
  // set to wh
  auto wh_local = vec3f{rr * cos(rphi), rr * sin(rphi), rz};
  return wh_local;
}

// Evaluates the BRDF scaled by the cosine of the incoming direction.
// - ggx from [Heitz 2014] and [Walter 2007] and [Lagarde 2014]
// "Understanding the Masking-Shadowing Function in Microfacet-Based
// BRDFs" http://jcgt.org/published/0003/02/03/
// - "Microfacet Models for Refraction through Rough Surfaces" EGSR 07
// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
vec3f evaluate_brdf_cosine(const microfacet_brdf& brdf, const vec3f& normal,
    const vec3f& outgoing, const vec3f& incoming) {
  if (is_brdf_delta(brdf)) return zero3f;
  auto brdf_cosine = zero3f;

  // diffuse
  if (brdf.diffuse != zero3f &&
      dot(normal, outgoing) * dot(normal, incoming) > 0) {
    auto half_vector = normalize(incoming + outgoing);
    auto fresnel     = (brdf.fresnel)
                       ? fresnel_schlick(brdf.specular, half_vector, outgoing)
                       : brdf.specular;
    brdf_cosine += brdf.diffuse * (1 - fresnel) / pif;
  }

  // specular
  if (brdf.specular != zero3f &&
      dot(normal, outgoing) * dot(normal, incoming) > 0) {
    auto half_vector = normalize(incoming + outgoing);
    auto fresnel     = (brdf.fresnel)
                       ? fresnel_schlick(brdf.specular, half_vector, outgoing)
                       : brdf.specular;
    auto D = evaluate_ggx_distribution(brdf.roughness, normal, half_vector);
    auto G = evaluate_ggx_shadowing(brdf.roughness, normal, outgoing, incoming);
    brdf_cosine +=
        fresnel * D * G /
        (4 * fabs(dot(normal, outgoing)) * fabs(dot(normal, incoming)));
  }

  // transmission (thin sheet)
  if (brdf.transmission != zero3f && !brdf.refract &&
      dot(normal, outgoing) * dot(normal, incoming) < 0) {
    auto ir = (dot(normal, outgoing) >= 0) ? reflect(-incoming, normal)
                                           : reflect(-incoming, -normal);
    auto half_vector = normalize(ir + outgoing);
    auto fresnel     = (brdf.fresnel)
                       ? fresnel_schlick(brdf.specular, half_vector, outgoing)
                       : brdf.specular;
    auto D = evaluate_ggx_distribution(brdf.roughness, normal, half_vector);
    auto G = evaluate_ggx_shadowing(brdf.roughness, normal, outgoing, ir);
    brdf_cosine += brdf.transmission * (1 - fresnel) * D * G /
                   (4 * fabs(dot(normal, outgoing)) * fabs(dot(normal, ir)));
  }

  // refraction through rough surface
  if (brdf.transmission != zero3f && brdf.refract &&
      dot(normal, outgoing) * dot(normal, incoming) < 0) {
    auto eta            = specular_to_eta(brdf.specular);
    auto halfway_vector = dot(normal, outgoing) > 0
                              ? -(outgoing + eta * incoming)
                              : (eta * outgoing + incoming);
    auto halfway = normalize(halfway_vector);

    auto fresnel = fresnel_schlick(brdf.specular, halfway, outgoing);
    auto D       = evaluate_ggx_distribution(brdf.roughness, normal, halfway);
    auto G = evaluate_ggx_shadowing(brdf.roughness, normal, outgoing, incoming);

    auto dots = dot(outgoing, halfway) * dot(incoming, halfway) /
                (dot(outgoing, normal) * dot(incoming, normal));

    auto numerator   = (1 - fresnel) * D * G;
    auto denominator = dot(halfway_vector, halfway_vector);

    brdf_cosine += abs(dots) * numerator / denominator;
  }
  return brdf_cosine * abs(dot(normal, incoming));
}

// Evaluates the BRDF assuming that it is called only from the directions
// generated by sample_brdf_direction.
vec3f evaluate_delta_brdf_cosine(const microfacet_brdf& brdf,
    const vec3f& normal, const vec3f& outgoing, const vec3f& incoming) {
  if (!is_brdf_delta(brdf)) return zero3f;
  auto microfacet_brdf = zero3f;

  // specular
  if (brdf.specular != zero3f &&
      dot(normal, outgoing) * dot(normal, incoming) > 0) {
    auto fresnel = (brdf.fresnel)
                       ? fresnel_schlick(brdf.specular, normal, outgoing)
                       : brdf.specular;
    microfacet_brdf += fresnel;
  }

  // transmission (thin sheet)
  if (brdf.transmission != zero3f &&
      dot(normal, outgoing) * dot(normal, incoming) < 0) {
    auto fresnel = (brdf.fresnel)
                       ? fresnel_schlick(brdf.specular, normal, outgoing)
                       : brdf.specular;
    microfacet_brdf += brdf.transmission * (1 - fresnel);
  }

  return microfacet_brdf;
}

// Picks a direction based on the BRDF
vec3f sample_brdf_direction(const microfacet_brdf& brdf, const vec3f& normal,
    const vec3f& outgoing, float rnl, const vec2f& rn) {
  if (is_brdf_delta(brdf)) return zero3f;
  auto fresnel = (brdf.fresnel)
                     ? fresnel_schlick(brdf.specular, normal, outgoing)
                     : brdf.specular;
  auto prob = vec3f{max(brdf.diffuse * (1 - fresnel)), max(fresnel),
      max(brdf.transmission * (1 - fresnel))};
  if (prob == zero3f) return zero3f;
  prob /= prob.x + prob.y + prob.z;

  // sample according to diffuse
  if (brdf.diffuse != zero3f && rnl < prob.x) {
    auto rz = sqrtf(rn.y), rr = sqrtf(1 - rz * rz), rphi = 2 * pif * rn.x;
    auto il = vec3f{rr * cosf(rphi), rr * sinf(rphi), rz};
    auto fp = dot(normal, outgoing) >= 0 ? frame_fromz(zero3f, normal)
                                         : frame_fromz(zero3f, -normal);
    return transform_direction(fp, il);
  }
  // sample according to specular GGX
  else if (brdf.specular != zero3f && rnl < prob.x + prob.y) {
    auto hl = sample_ggx(brdf.roughness, rn);
    auto fp = dot(normal, outgoing) >= 0 ? frame_fromz(zero3f, normal)
                                         : frame_fromz(zero3f, -normal);
    auto half_vector = transform_direction(fp, hl);
    return reflect(outgoing, half_vector);
  }
  // transmission hack
  else if (brdf.transmission != zero3f && !brdf.refract &&
           rnl < prob.x + prob.y + prob.z) {
    auto hl = sample_ggx(brdf.roughness, rn);
    auto fp = dot(normal, outgoing) >= 0 ? frame_fromz(zero3f, normal)
                                         : frame_fromz(zero3f, -normal);
    auto half_vector = transform_direction(fp, hl);
    auto ir          = reflect(outgoing, half_vector);
    return dot(normal, outgoing) >= 0 ? reflect(-ir, -normal)
                                      : reflect(-ir, normal);
  }
  // sample according to rough refraction
  else if (brdf.transmission != zero3f && brdf.refract &&
           rnl < prob.x + prob.y + prob.z) {
    auto hl = sample_ggx(brdf.roughness, rn);
    auto fp = dot(normal, outgoing) >= 0 ? frame_fromz(zero3f, normal)
                                         : frame_fromz(zero3f, -normal);
    auto halfway = transform_direction(fp, hl);

    auto eta = specular_to_eta(brdf.specular);
    return refract(
        outgoing, halfway, dot(normal, outgoing) >= 0 ? 1 / eta : eta);
  }

  else {
    return zero3f;
  }
}

// Picks a direction based on the BRDF
vec3f sample_delta_brdf_direction(const microfacet_brdf& brdf,
    const vec3f& normal, const vec3f& outgoing, float rnl, const vec2f& rn) {
  if (!is_brdf_delta(brdf)) return zero3f;
  auto fresnel = (brdf.fresnel)
                     ? fresnel_schlick(brdf.specular, normal, outgoing)
                     : brdf.specular;
  auto prob = vec3f{0, max(fresnel), max(brdf.transmission * (1 - fresnel))};
  if (prob == zero3f) return zero3f;
  prob /= prob.x + prob.y + prob.z;

  // sample according to specular mirror
  if (brdf.specular != zero3f && rnl < prob.x + prob.y) {
    return reflect(outgoing, dot(normal, outgoing) >= 0 ? normal : -normal);
  }
  // sample according to transmission
  else if (brdf.transmission != zero3f && !brdf.refract &&
           rnl < prob.x + prob.y + prob.z) {
    return -outgoing;
  }
  // sample according to transmission
  else if (brdf.transmission != zero3f && brdf.refract &&
           rnl < prob.x + prob.y + prob.z) {
    if (dot(normal, outgoing) >= 0) {
      return refract(outgoing, normal, 1 / specular_to_eta(brdf.specular));
    } else {
      return refract(outgoing, -normal, specular_to_eta(brdf.specular));
    }
  }
  // no sampling
  else {
    return zero3f;
  }
}

// Compute the weight for sampling the BRDF
float sample_brdf_direction_pdf(const microfacet_brdf& brdf,
    const vec3f& normal, const vec3f& outgoing, const vec3f& incoming) {
  if (is_brdf_delta(brdf)) return 0;
  auto fresnel = (brdf.fresnel)
                     ? fresnel_schlick(brdf.specular, normal, outgoing)
                     : brdf.specular;
  auto prob = vec3f{max(brdf.diffuse * (1 - fresnel)), max(fresnel),
      max(brdf.transmission * (1 - fresnel))};
  if (prob == zero3f) return 0;
  prob /= prob.x + prob.y + prob.z;

  auto pdf = 0.0f;

  if (brdf.diffuse != zero3f &&
      dot(normal, outgoing) * dot(normal, incoming) > 0) {
    pdf += prob.x * fabs(dot(normal, incoming)) / pif;
  }
  if (brdf.specular != zero3f &&
      dot(normal, outgoing) * dot(normal, incoming) > 0) {
    auto half_vector = normalize(incoming + outgoing);
    auto d = sample_ggx_pdf(brdf.roughness, fabs(dot(normal, half_vector)));
    pdf += prob.y * d / (4 * fabs(dot(outgoing, half_vector)));
  }
  if (brdf.transmission != zero3f && !brdf.refract &&
      dot(normal, outgoing) * dot(normal, incoming) < 0) {
    auto ir = (dot(normal, outgoing) >= 0) ? reflect(-incoming, normal)
                                           : reflect(-incoming, -normal);
    auto half_vector = normalize(ir + outgoing);
    auto d = sample_ggx_pdf(brdf.roughness, fabs(dot(normal, half_vector)));
    pdf += prob.z * d / (4 * fabs(dot(outgoing, half_vector)));
  }

  // refraction through rough surface
  if (brdf.transmission != zero3f && brdf.refract &&
      dot(normal, outgoing) * dot(normal, incoming) < 0) {
    auto eta            = specular_to_eta(brdf.specular);
    auto outgoing_up    = dot(normal, outgoing) > 0;
    auto halfway_vector = outgoing_up ? -(outgoing + eta * incoming)
                                      : (eta * outgoing + incoming);
    auto halfway = normalize(halfway_vector);

    auto d = sample_ggx_pdf(brdf.roughness, fabs(dot(normal, halfway)));

    auto jacobian = fabs(dot(halfway, incoming)) /
                    dot(halfway_vector, halfway_vector);
    pdf += prob.z * d * jacobian;
  }

  return pdf;
}

// Compute the weight for sampling the BRDF
float sample_delta_brdf_direction_pdf(const microfacet_brdf& brdf,
    const vec3f& normal, const vec3f& outgoing, const vec3f& incoming) {
  if (!is_brdf_delta(brdf)) return 0;
  auto fresnel = (brdf.fresnel)
                     ? fresnel_schlick(brdf.specular, normal, outgoing)
                     : brdf.specular;
  auto prob = vec3f{0, max(fresnel), max(brdf.transmission * (1 - fresnel))};
  if (prob == zero3f) return 0;
  prob /= prob.x + prob.y + prob.z;

  auto pdf = 0.0f;

  if (brdf.specular != zero3f &&
      dot(normal, outgoing) * dot(normal, incoming) > 0) {
    return prob.y;
  }
  if (brdf.transmission != zero3f &&
      dot(normal, outgoing) * dot(normal, incoming) < 0) {
    return prob.z;
  }

  return pdf;
}

#endif

}  // namespace yocto

// -----------------------------------------------------------------------------
// NUMERICAL TESTS FOR MONTE CARLO INTEGRATION
// -----------------------------------------------------------------------------
namespace yocto {

template <typename Func>
float integrate_func_base(
    const Func& f, float a, float b, int nsamples, rng_state& rng) {
  auto integral = 0.0f;
  for (auto i = 0; i < nsamples; i++) {
    auto r = rand1f(rng);
    auto x = a + r * (b - a);
    integral += f(x) * (b - a);
  }
  integral /= nsamples;
  return integral;
}

template <typename Func>
float integrate_func_stratified(
    const Func& f, float a, float b, int nsamples, rng_state& rng) {
  auto integral = 0.0f;
  for (auto i = 0; i < nsamples; i++) {
    auto r = (i + rand1f(rng)) / nsamples;
    auto x = a + r * (b - a);
    integral += f(x) * (b - a);
  }
  integral /= nsamples;
  return integral;
}

template <typename Func>
float integrate_func_importance(const Func& f, const Func& pdf,
    const Func& warp, int nsamples, rng_state& rng) {
  auto integral = 0.0f;
  for (auto i = 0; i < nsamples; i++) {
    auto r = rand1f(rng);
    auto x = warp(r);
    integral += f(x) / pdf(x);
  }
  integral /= nsamples;
  return integral;
}

// compute the integral and error using different Monte Carlo scehems
// example 1: ---------
// auto f = [](double x) { return 1.0 - (3.0 / 4.0) * x * x; };
// auto a = 0.0, b = 1.0;
// auto expected = 3.0 / 4.0;
// auto nsamples = 10000
// example 2: ---------
// auto f = [](double x) { return sin(x); }
// auto a = 0.0, b = (double)M_PI;
// auto expected = (double)M_PI;
template <typename Func>
void print_integrate_func_test(const Func& f, float a, float b, float expected,
    int nsamples, const Func& pdf, const Func& warp) {
  auto rng = rng_state();
  printf("nsamples base base-err stratified-err importance-err\n");
  for (auto ns = 10; ns < nsamples; ns += 10) {
    auto integral_base       = integrate_func_base(f, a, b, ns, rng);
    auto integral_stratified = integrate_func_stratified(f, a, b, ns, rng);
    auto integral_importance = integrate_func_importance(f, pdf, warp, ns, rng);
    auto error_base          = fabs(integral_base - expected) / expected;
    auto error_stratified    = fabs(integral_stratified - expected) / expected;
    auto error_importance    = fabs(integral_importance - expected) / expected;
    printf("%d %g %g %g %g\n", ns, integral_base, error_base, error_stratified,
        error_importance);
  }
}

template <typename Func>
float integrate_func2_base(
    const Func& f, vec2f a, vec2f b, int nsamples, rng_state& rng) {
  auto integral = 0.0f;
  for (auto i = 0; i < nsamples; i++) {
    auto r = rand2f(rng);
    auto x = a + r * (b - a);
    integral += f(x) * (b.x - a.x) * (b.y - a.y);
  }
  integral /= nsamples;
  return integral;
}

template <typename Func>
float integrate_func2_stratified(
    const Func& f, vec2f a, vec2f b, int nsamples, rng_state& rng) {
  auto integral  = 0.0f;
  auto nsamples2 = (int)sqrt(nsamples);
  for (auto i = 0; i < nsamples2; i++) {
    for (auto j = 0; j < nsamples2; j++) {
      auto r = vec2f{
          (i + rand1f(rng)) / nsamples2, (j + rand1f(rng)) / nsamples2};
      auto x = a + r * (b - a);
      integral += f(x) * (b.x - a.x) * (b.y - a.y);
    }
  }
  integral /= nsamples2 * nsamples2;
  return integral;
}

template <typename Func, typename Func2>
float integrate_func2_importance(const Func& f, const Func& pdf,
    const Func2& warp, int nsamples, rng_state& rng) {
  auto integral = 0.0f;
  for (auto i = 0; i < nsamples; i++) {
    auto r = rand2f(rng);
    auto x = warp(r);
    integral += f(x) / pdf(x);
  }
  integral /= nsamples;
  return integral;
}

// compute the integral and error using different Monte Carlo scehems
// example 1: ---------
// auto f = [](double x) { return 1.0 - (3.0 / 4.0) * x * x; };
// auto a = 0.0, b = 1.0;
// auto expected = 3.0 / 4.0;
// auto nsamples = 10000
template <typename Func, typename Func2>
void print_integrate_func2_test(const Func& f, vec2f a, vec2f b, float expected,
    int nsamples, const Func& pdf, const Func2& warp) {
  auto rng = rng_state();
  printf("nsamples base base-err stratified-err importance-err\n");
  for (auto ns = 10; ns < nsamples; ns += 10) {
    auto integral_base       = integrate_func2_base(f, a, b, ns, rng);
    auto integral_stratified = integrate_func2_stratified(f, a, b, ns, rng);
    auto integral_importance = integrate_func2_importance(
        f, pdf, warp, ns, rng);
    auto error_base       = fabs(integral_base - expected) / expected;
    auto error_stratified = fabs(integral_stratified - expected) / expected;
    auto error_importance = fabs(integral_importance - expected) / expected;
    printf("%d %g %g %g %g\n", ns, integral_base, error_base, error_stratified,
        error_importance);
  }
}

}  // namespace yocto
