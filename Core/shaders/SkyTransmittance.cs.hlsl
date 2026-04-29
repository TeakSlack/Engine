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

RWTexture2D<float4> TransmittanceLUT : register(u0);

float ClampCosine(float cosTheta)
{
    return clamp(cosTheta, -1.0, 1.0);
}

float ClampDistance(float distance)
{
    return max(distance, 0.0);
}

float ClampRadius(float radius)
{
    return clamp(radius, PlanetRadius, PlanetRadius + AtmosphereHeight);
}

float SafeSqrt(float value)
{
    return sqrt(max(value, 0.0));
}

float DistanceToTopAtmosphereBoundary(float r, float mu)
{
    float discriminant = r * r * (mu * mu - 1.0) + (PlanetRadius + AtmosphereHeight) * (PlanetRadius + AtmosphereHeight);
    return ClampDistance(-r * mu + SafeSqrt(discriminant));
}

float DistanceToBottomAtmosphereBoundary(float r, float mu)
{
    float discriminant = r * r * (mu * mu - 1.0) + PlanetRadius * PlanetRadius;
    return ClampDistance(-r * mu - SafeSqrt(discriminant));
}

bool RayIntersectsGround(float r, float mu)
{
    float discriminant = r * r * (mu * mu - 1.0) + PlanetRadius * PlanetRadius;
    return mu < 0.0 && discriminant >= 0.0;
}

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

float3 ComputeOpticalDepthToTopAtmosphereBoundary(float r, float mu)
{
    const int SAMPLE_COUNT = 500;
    const float3 MIE_EXTINCTION = float3(0.00444, 0.00444, 0.00444); // in km
    const float3 ABSORPTION_EXTINCTION = float3(0.000650, 0.00188, 0.000085); // Placeholder for absorption extinction
    float dx = DistanceToTopAtmosphereBoundary(r, mu) / float(SAMPLE_COUNT);
    float3 opticalDepth = 0;
    
    for (int i = 0; i <= SAMPLE_COUNT; i++)
    {
        float di = float(i) * dx;
        float ri = sqrt(di * di + 2.0 * r * mu * di + r * r);
        float weight = (i == 0 || i == SAMPLE_COUNT) ? 0.5 : 1.0;
        opticalDepth += (RayleighScattering * GetProfileDensity(RayleighDensity, ri) +
                          MIE_EXTINCTION * GetProfileDensity(MieDensity, ri) +
                          ABSORPTION_EXTINCTION * GetProfileDensity(OzoneDensity, ri)) *
                          weight * dx;
    }
    return opticalDepth;
}

// ---- Twxture coordinate mapping ----
float2 GetTransmittanceTextureUvFromRMu(float r, float mu)
{
    float H = SafeSqrt((PlanetRadius + AtmosphereHeight) * (PlanetRadius + AtmosphereHeight) - PlanetRadius * PlanetRadius);
    float rho = SafeSqrt(max(r * r - PlanetRadius * PlanetRadius, 0.0));
    float d = DistanceToTopAtmosphereBoundary(r, mu);
    float d_min = (PlanetRadius + AtmosphereHeight) - r;
    float d_max = rho + H;
    float x_mu = (d - d_min) / (d_max - d_min);
    float x_r = rho / H;
    
    // LUT size constraints
    float2 tex_size = float2(256, 64); // Example LUT size
    return float2(
    (0.5 + x_mu * (tex_size.x - 1.0)) / tex_size.x,
    (0.5 + x_r *  (tex_size.y - 1.0)) / tex_size.y);
}

void GetRMuFromTransmittanceTextureUv(float2 uv, out float r, out float mu)
{
    float2 tex_size = float2(256.0, 64.0);
    float x_mu = (uv.x * tex_size.x - 0.5) / (tex_size.x - 1.0);
    float x_r = (uv.y * tex_size.y - 0.5) / (tex_size.y - 1.0);
    
    float H = sqrt((PlanetRadius + AtmosphereHeight)* (PlanetRadius + AtmosphereHeight)-
                   PlanetRadius* PlanetRadius);
    float rho = H * x_r;
    r = sqrt(rho * rho + PlanetRadius * PlanetRadius);
    
    float d_min = PlanetRadius + AtmosphereHeight - r;
    float d_max = rho + H;
    float d = d_min + x_mu * (d_max - d_min);
    mu = d == 0.0 ? 1.0 : ((PlanetRadius + AtmosphereHeight) * (PlanetRadius + AtmosphereHeight) - r * r - d * d) / (2.0 * r * d);
    mu = clamp(mu, -1.0, 1.0);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint width, height;
    TransmittanceLUT.GetDimensions(width, height);

    if (dispatchThreadID.x >= width || dispatchThreadID.y >= height)
        return;

    float2 uv = float2(dispatchThreadID.xy) / float2(width - 1, height - 1);
    float r, mu;
    GetRMuFromTransmittanceTextureUv(uv, r, mu);

    float3 optical_depth = ComputeOpticalDepthToTopAtmosphereBoundary(r, mu);
    TransmittanceLUT[dispatchThreadID.xy] = float4(exp(-optical_depth), 1.0);
}