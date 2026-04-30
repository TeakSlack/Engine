struct DensityProfileLayer
{
    float Width;
    float ExpTerm;
    float ExpScale;
    float LinearTerm;
    float ConstantTerm;
    float3 _pad; // pad to 32 bytes to match C++ layout
};

cbuffer AtmosphereConstants : register(b0)
{
    float PlanetRadius;
    float AtmosphereHeight;
    float2 _pad0;

    float3 RayleighScattering;
    float _pad1;

    float3 MieScattering;
    float _pad2;

    DensityProfileLayer RayleighDensity[2]; // offset 48  (64 bytes)
    DensityProfileLayer MieDensity[2];      // offset 112 (64 bytes)
    DensityProfileLayer OzoneDensity[2];    // offset 176 (64 bytes)
}

// ---- Transmittance pass output ----
RWTexture2D<float4> TransmittanceLUT : register(u0);

// ---- Scattering pass inputs/output ----
Texture2D<float4>   TransmittanceSRV : register(t0);
SamplerState        LinearSampler    : register(s0);
RWTexture3D<float4> ScatteringLUT    : register(u1);

#include "SkyUtil.hlsli"

// ---- Density profile helpers ----

float GetLayerDensity(DensityProfileLayer layer, float altitude)
{
    float density = layer.ExpTerm * exp(layer.ExpScale * altitude)
                  + layer.LinearTerm * altitude
                  + layer.ConstantTerm;
    return clamp(density, 0.0, 1.0);
}

float GetProfileDensity(DensityProfileLayer profile[2], float altitude)
{
    return altitude < profile[0].Width
        ? GetLayerDensity(profile[0], altitude)
        : GetLayerDensity(profile[1], altitude);
}

// ---- Transmittance pass ----

float3 ComputeOpticalDepthToTopAtmosphereBoundary(float r, float mu)
{
    if (RayIntersectsGround(r, mu))
        return float3(1e9, 1e9, 1e9);

    const int    SAMPLE_COUNT     = 500;
    const float3 MIE_EXTINCTION   = MieScattering / 0.9;                                  // single-scatter albedo = 0.9
    const float3 OZONE_ABSORPTION = float3(0.000650, 0.001881, 0.000085);                 // /km (Bruneton 2017)

    float  dx         = DistanceToTopAtmosphereBoundary(r, mu) / float(SAMPLE_COUNT);
    float3 optDepth   = 0;

    for (int i = 0; i <= SAMPLE_COUNT; i++)
    {
        float  di        = float(i) * dx;
        float  ri        = sqrt(di * di + 2.0 * r * mu * di + r * r);
        float  altitude  = ri - PlanetRadius;
        float  weight    = (i == 0 || i == SAMPLE_COUNT) ? 0.5 : 1.0;

        optDepth += (RayleighScattering * GetProfileDensity(RayleighDensity, altitude) +
                     MIE_EXTINCTION     * GetProfileDensity(MieDensity,      altitude) +
                     OZONE_ABSORPTION   * GetProfileDensity(OzoneDensity,    altitude)) * weight * dx;
    }
    return optDepth;
}

// ---- Scattering pass ----

float3 SampleTransmittance(float r, float mu)
{
    float2 uv = GetTransmittanceTextureUvFromRMu(r, mu);
    return TransmittanceSRV.SampleLevel(LinearSampler, uv, 0).rgb;
}

float RayleighPhase(float nu)
{
    return (3.0 / (16.0 * PI)) * (1.0 + nu * nu);
}

float MiePhase(float nu)
{
    const float g  = 0.8;
    const float g2 = g * g;
    float denom = max(1.0 + g2 - 2.0 * g * nu, 1e-5);
    return (3.0 / (8.0 * PI)) * (1.0 - g2) * (1.0 + nu * nu)
           / ((2.0 + g2) * pow(denom, 1.5));
}

// Integrates single in-scattering from (r, mu) toward the top of the atmosphere.
// Returns Rayleigh (RGB) and Mie (RGB, spectrally uniform) density integrals
// with phase function and scattering coefficients already applied.
void ComputeSingleScattering(float r, float mu, float mu_s, float nu,
                              out float3 rayleigh, out float3 mie)
{
    rayleigh = 0;
    mie      = 0;

    if (RayIntersectsGround(r, mu))
        return;

    const int SAMPLE_COUNT = 50;
    float d  = DistanceToTopAtmosphereBoundary(r, mu);
    float dx = d / float(SAMPLE_COUNT);

    float3 T_camera = SampleTransmittance(r, mu);

    for (int i = 0; i <= SAMPLE_COUNT; i++)
    {
        float  t      = float(i) * dx;
        float  r_t    = sqrt(r * r + t * t + 2.0 * r * mu * t);
        float  mu_t   = (r * mu   + t) / r_t;
        float  mu_s_t = (r * mu_s + t * nu) / r_t;
        float  alt    = r_t - PlanetRadius;

        float  density_r = GetProfileDensity(RayleighDensity, alt);
        float  density_m = GetProfileDensity(MieDensity,      alt);

        // T(camera→sample) ≈ T_lut(r,mu) / T_lut(r_t,mu_t)
        float3 T_sample        = SampleTransmittance(r_t, mu_t);
        float3 T_cam_to_sample = T_camera / max(T_sample, 1e-6);
        float3 T_sun           = SampleTransmittance(r_t, mu_s_t);

        float weight = (i == 0 || i == SAMPLE_COUNT) ? 0.5 : 1.0;
        rayleigh += T_cam_to_sample * T_sun * density_r * weight * dx;
        mie      += T_cam_to_sample * T_sun * density_m * weight * dx;
    }

    rayleigh *= RayleighScattering * RayleighPhase(nu);
    mie      *= MieScattering      * MiePhase(nu);
}

// ---- Entry points ----

[numthreads(8, 8, 1)]
void mainTransmittance(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    TransmittanceLUT.GetDimensions(width, height);
    if (id.x >= width || id.y >= height)
        return;

    float2 uv = (float2(id.xy)) / float2(width - 1, height - 1);
    float r, mu;
    GetRMuFromTransmittanceTextureUv(uv, r, mu);

    float3 optDepth = ComputeOpticalDepthToTopAtmosphereBoundary(r, mu);
    TransmittanceLUT[id.xy] = float4(exp(-optDepth), 1.0);
}

[numthreads(8, 8, 8)]
void mainScattering(uint3 id : SV_DispatchThreadID)
{
    uint width, height, depth;
    ScatteringLUT.GetDimensions(width, height, depth);
    if (id.x >= width || id.y >= height || id.z >= depth)
        return;

    float r, mu, mu_s, nu;
    GetRMuMuSNuFromScatteringTextureCoord(id, r, mu, mu_s, nu);

    float3 rayleigh, mie;
    ComputeSingleScattering(r, mu, mu_s, nu, rayleigh, mie);

    // RGB = Rayleigh scattered radiance; A = Mie (spectrally uniform, single channel).
    ScatteringLUT[id] = float4(rayleigh, mie.r);
}
