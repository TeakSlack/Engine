#include "common.hlsli"

VK_BINDING(0, 0) cbuffer PerObject : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 ViewProjection;
    float3             CameraPosition;
    float              _pad;
    float3             CameraForward;
    float              _pad1;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float4 Tangent  : TANGENT;
};

struct VSOutput
{
    float4 Position     : SV_Position;
    float3 WorldPos     : WORLD_POSITION;
    float3 WorldNormal  : WORLD_NORMAL;
    float4 WorldTangent : WORLD_TANGENT; // .xyz = tangent direction, .w = handedness sign
    float3 ViewDir      : VIEW_DIR;
    float2 TexCoord     : TEXCOORD0;
};

VSOutput main(VSInput IN)
{
    VSOutput OUT;

    float4 worldPos     = mul(float4(IN.Position, 1.0), World);
    OUT.Position        = mul(worldPos, ViewProjection);
    OUT.WorldPos        = worldPos.xyz;
    OUT.WorldNormal     = normalize(mul(IN.Normal,  (float3x3)World));
    OUT.WorldTangent    = float4(normalize(mul(IN.Tangent.xyz, (float3x3)World)), IN.Tangent.w);
    OUT.ViewDir         = normalize(CameraPosition - worldPos.xyz);
    OUT.TexCoord        = IN.TexCoord;

    return OUT;
}
