#include "common.hlsli"

// t0=albedo  t1=normal  t2=metallic-roughness (G=roughness, B=metallic)
VK_BINDING(2, 0) Texture2D    AlbedoMap           : register(t0);
VK_BINDING(3, 0) Texture2D    NormalMap            : register(t1);
VK_BINDING(4, 0) Texture2D    MetallicRoughnessMap : register(t2);
VK_BINDING(5, 0) SamplerState Sampler              : register(s2);

VK_BINDING(0, 0) cbuffer PerObject : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 ViewProjection;
    float3             CameraPosition;
    float              _pad;
    float3             CameraForward;
    float              _pad1;
};

static const float  PI           = 3.14159265359;
static const float3 LightOffset  = float3(0.2, 0.3, 0.1); // bias above/right of camera
static const float3 LightColor   = float3(1.0, 0.95, 0.85);
static const float3 AmbientColor = float3(0.05, 0.06, 0.08);

struct PSInput
{
    float4 Position     : SV_Position;
    float3 WorldPos     : WORLD_POSITION;
    float3 WorldNormal  : WORLD_NORMAL;
    float4 WorldTangent : WORLD_TANGENT;
    float3 ViewDir      : VIEW_DIR;
    float2 TexCoord     : TEXCOORD0;
};

// GGX / Trowbridge-Reitz NDF
float D_GGX(float NdotH, float a2)
{
    float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Height-correlated Smith GGX visibility (G / (4 NdotV NdotL) baked in)
float V_SmithGGXCorrelated(float NdotV, float NdotL, float a2)
{
    float GV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float GL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / (GV + GL + 1e-5);
}

// Schlick Fresnel
float3 F_Schlick(float VdotH, float3 F0)
{
    float f = pow(saturate(1.0 - VdotH), 5.0);
    return F0 + (1.0 - F0) * f;
}

float4 main(PSInput IN) : SV_Target
{
    float2 uv = IN.TexCoord;

    // --- Material ---
    float3 albedo    = AlbedoMap.Sample(Sampler, uv).rgb;
    float2 mr        = MetallicRoughnessMap.Sample(Sampler, uv).gb; // glTF: G=roughness, B=metallic
    float  roughness = max(mr.x, 0.045); // clamp away specular singularity
    float  metallic  = mr.y;

    // --- Normal mapping ---
    float3 N_geo = normalize(IN.WorldNormal);
    float3 T     = normalize(IN.WorldTangent.xyz);
    T            = normalize(T - dot(T, N_geo) * N_geo); // re-orthogonalise
    float3 B     = cross(N_geo, T) * IN.WorldTangent.w;  // .w flips B for mirrored UVs

    float3 normalSample = NormalMap.Sample(Sampler, uv).xyz * 2.0 - 1.0;
    float3 N = normalize(T * normalSample.x + B * normalSample.y + N_geo * normalSample.z);

    float3 V = normalize(IN.ViewDir);
    float3 L = normalize(CameraForward + LightOffset);
    float3 H = normalize(V + L);

    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    float  a2 = roughness * roughness * roughness * roughness; // (alpha^2, alpha = roughness^2)
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    // --- Cook-Torrance specular BRDF ---
    float  D        = D_GGX(NdotH, a2);
    float  Vis      = V_SmithGGXCorrelated(NdotV, NdotL, a2);
    float3 F        = F_Schlick(VdotH, F0);
    float3 specular = D * Vis * F;

    // --- Lambertian diffuse (energy conserved: metals have no diffuse) ---
    float3 kD      = (1.0 - F) * (1.0 - metallic);
    float3 diffuse = kD * (albedo / PI);

    float3 directLight = (diffuse + specular) * LightColor * NdotL;

    // --- Ambient (simple IBL-less approximation) ---
    float3 ambient = AmbientColor * albedo * lerp(1.0, 0.5, metallic);

    float3 color = ambient + directLight;

    return float4(color, 1.0);
}
