// Interpret cbuffer data as column-major to match GLM's memory layout.
// Without this, HLSL defaults to row-major and the matrix would be transposed.
#pragma pack_matrix(column_major)

[[vk::binding(0, 0)]] cbuffer MVP : register(b0)
{
    float4x4 g_MVP;
};

struct VSIn
{
    float3 position : POSITION;
    float3 color    : COLOR;
};

struct VSOut
{
    float4 position : SV_Position;
    float3 color    : COLOR;
};

VSOut main(VSIn input)
{
    VSOut output;
    output.position = mul(g_MVP, float4(input.position, 1.0f));
    output.color    = input.color;
    return output;
}
