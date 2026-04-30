#ifndef SKY_UTIL_HLSLI
#define SKY_UTIL_HLSLI

static const float PI = 3.14159265358979;

// Scattering LUT layout: 3D texture encoding 4 dimensions.
// Width  = SCATTERING_NU_SIZE * SCATTERING_MU_S_SIZE  (nu packed into column blocks)
// Height = SCATTERING_MU_SIZE
// Depth  = SCATTERING_R_SIZE
static const uint SCATTERING_R_SIZE    = 32;
static const uint SCATTERING_MU_SIZE   = 128;
static const uint SCATTERING_MU_S_SIZE = 32;
static const uint SCATTERING_NU_SIZE   = 8;

// ---- Math helpers ----

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

// ---- Ray/atmosphere geometry ----

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

// ---- Transmittance LUT coordinate mapping (Bruneton 2017) ----
// Expects PlanetRadius and AtmosphereHeight to be defined in the including shader's cbuffer.

float2 GetTransmittanceTextureUvFromRMu(float r, float mu)
{
    float H    = SafeSqrt((PlanetRadius + AtmosphereHeight) * (PlanetRadius + AtmosphereHeight) - PlanetRadius * PlanetRadius);
    float rho  = SafeSqrt(max(r * r - PlanetRadius * PlanetRadius, 0.0));
    float d     = DistanceToTopAtmosphereBoundary(r, mu);
    float d_min = (PlanetRadius + AtmosphereHeight) - r;
    float d_max = rho + H;
    float x_mu  = (d - d_min) / (d_max - d_min);
    float x_r   = rho / H;

    float2 tex_size = float2(256, 64);
    return float2(
        (0.5 + x_mu * (tex_size.x - 1.0)) / tex_size.x,
        (0.5 + x_r  * (tex_size.y - 1.0)) / tex_size.y);
}

void GetRMuFromTransmittanceTextureUv(float2 uv, out float r, out float mu)
{
    float2 tex_size = float2(256.0, 64.0);
    float x_mu = (uv.x * tex_size.x - 0.5) / (tex_size.x - 1.0);
    float x_r  = (uv.y * tex_size.y - 0.5) / (tex_size.y - 1.0);

    float H   = SafeSqrt((PlanetRadius + AtmosphereHeight) * (PlanetRadius + AtmosphereHeight) - PlanetRadius * PlanetRadius);
    float rho = H * x_r;
    r = sqrt(rho * rho + PlanetRadius * PlanetRadius);

    float d_min = PlanetRadius + AtmosphereHeight - r;
    float d_max = rho + H;
    float d     = d_min + x_mu * (d_max - d_min);
    mu = d == 0.0 ? 1.0 : ((PlanetRadius + AtmosphereHeight) * (PlanetRadius + AtmosphereHeight) - r * r - d * d) / (2.0 * r * d);
    mu = clamp(mu, -1.0, 1.0);
}

// ---- Scattering LUT coordinate mapping ----
// Inverse: dispatchThreadID → (r, mu, mu_s, nu) for writing the LUT.
// r and mu use Bruneton's distance-based non-linear mapping (same as transmittance LUT).
// mu_s and nu use linear mappings packed into the x axis.
// Requires PlanetRadius and AtmosphereHeight from the including shader's cbuffer.
void GetRMuMuSNuFromScatteringTextureCoord(uint3 coord,
    out float r, out float mu, out float mu_s, out float nu)
{
    float H   = SafeSqrt((PlanetRadius + AtmosphereHeight) * (PlanetRadius + AtmosphereHeight) - PlanetRadius * PlanetRadius);
    float rho = (float(coord.z) / float(SCATTERING_R_SIZE - 1)) * H;
    r = sqrt(rho * rho + PlanetRadius * PlanetRadius);

    float x_mu  = float(coord.y) / float(SCATTERING_MU_SIZE - 1);
    float d_min = PlanetRadius + AtmosphereHeight - r;
    float d_max = rho + H;
    float d     = d_min + x_mu * (d_max - d_min);
    mu = (d == 0.0) ? 1.0 : ClampCosine((H * H - rho * rho - d * d) / (2.0 * r * d));

    uint  nu_idx   = coord.x / SCATTERING_MU_S_SIZE;
    uint  mu_s_idx = coord.x % SCATTERING_MU_S_SIZE;
    mu_s = -1.0 + float(mu_s_idx) / float(SCATTERING_MU_S_SIZE - 1) * 2.0;
    nu   = -1.0 + float(nu_idx)   / float(SCATTERING_NU_SIZE   - 1) * 2.0;

    // Clamp nu to the geometrically valid range given mu and mu_s.
    float sin_theta_v  = sqrt(max(1.0 - mu   * mu,   0.0));
    float sin_theta_s  = sqrt(max(1.0 - mu_s * mu_s, 0.0));
    float nu_min = mu * mu_s - sin_theta_v * sin_theta_s;
    float nu_max = mu * mu_s + sin_theta_v * sin_theta_s;
    nu = clamp(nu, nu_min, nu_max);
}

// Forward: (r, mu, mu_s, nu) → uvw for sampling the LUT at runtime.
float3 GetScatteringTextureUvwFromRMuMuSNu(float r, float mu, float mu_s, float nu)
{
    float H   = SafeSqrt((PlanetRadius + AtmosphereHeight) * (PlanetRadius + AtmosphereHeight) - PlanetRadius * PlanetRadius);
    float rho = SafeSqrt(max(r * r - PlanetRadius * PlanetRadius, 0.0));

    float u_r  = rho / H;

    float d     = DistanceToTopAtmosphereBoundary(r, mu);
    float d_min = PlanetRadius + AtmosphereHeight - r;
    float d_max = rho + H;
    float u_mu  = (d - d_min) / max(d_max - d_min, 1e-6);

    float u_mu_s = (mu_s + 1.0) * 0.5;
    float u_nu   = (nu   + 1.0) * 0.5;

    // Pack nu and mu_s into the x UV so it maps to pixel x = nu_idx * MU_S_SIZE + mu_s_pixel.
    float nu_idx  = floor(u_nu * float(SCATTERING_NU_SIZE - 1));
    float u_x     = (nu_idx * float(SCATTERING_MU_S_SIZE) + u_mu_s * float(SCATTERING_MU_S_SIZE - 1))
                    / float(SCATTERING_NU_SIZE * SCATTERING_MU_S_SIZE - 1);

    return float3(u_x, u_mu, u_r);
}

#endif // SKY_UTIL_HLSLI