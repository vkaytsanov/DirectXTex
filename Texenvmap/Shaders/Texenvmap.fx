//--------------------------------------------------------------------------------------
// File: Texenvmap.fx
//
// DirectX Texture environment map tool shaders
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

Texture2D<float4> Texture : register(t0);
sampler Sampler : register(s0);

cbuffer Parameters : register(b0)
{
    float4x4 WorldViewProj;
}

struct VSInput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 PositionPS : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

// Vertex shader: basic.
VSOutput VSBasic(VSInput vin)
{
    VSOutput vout;

    vout.PositionPS = mul(vin.Position, WorldViewProj);
    vout.TexCoord = vin.TexCoord;

    return vout;
}

// Pixel shader
float4 PSBasic(VSOutput pin) : SV_Target0
{
    return Texture.Sample(Sampler, pin.TexCoord);
}
