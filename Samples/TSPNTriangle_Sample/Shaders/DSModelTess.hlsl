//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

// Include structures and functions for lighting.
#include "..\..\Common\LightingUtil.hlsli"

Texture2D    gDiffuseMap      : register(t0);
Texture2D    gRoughnessMap    : register(t1);
Texture2D    gNormalMap       : register(t2);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

// Constant data that varies per frame.
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
};

cbuffer cbMaterial : register(b1)
{
    float4   gDiffuseAlbedo;
    float3   gFresnelR0;
    float    gRoughness;
    float4x4 gMatTransform;
    int      gUseTexture;
};

// Constant data that varies per material.
cbuffer cbPass : register(b2)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gViewProj;
    float4x4 gInvView;
    float4x4 gInvProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];

    float4 gFogColor;
    float  gFogStart;
    float  gFogRange;
    float  gTessLod;
    float  gReserve;
};

struct VertexIn
{
    float3 PosL     : POSITION;
    float3 NormalL  : NORMAL;
    float3 TangentL : TANGENT;
    float2 TexC     : TEXCOORD;
};

struct VertexOut
{
    float3 PosL     : POSITION;
    float3 NormalL  : NORMAL;
    float3 TangentL : TANGENT;
    float2 TexC     : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;
    vout.PosL = vin.PosL;
    vout.NormalL = vin.NormalL;
    vout.TangentL = vin.TangentL;
    vout.TexC = vin.TexC;
    return vout;
}

struct PatchTess
{
    float EdgeTess[3]   : SV_TessFactor;
    float InsideTess[1] : SV_InsideTessFactor;

    float3 b300 : COEFFIENTP1;
    float3 b030 : COEFFIENTP2;
    float3 b003 : COEFFIENTP3;
    float3 b210 : COEFFIENTP4;
    float3 b120 : COEFFIENTP5;
    float3 b021 : COEFFIENTP6;
    float3 b012 : COEFFIENTP7;
    float3 b102 : COEFFIENTP8;
    float3 b201 : COEFFIENTP9;
    float3 b111 : COEFFIENTP10;

    float3 n200 : COEFFIENTN1;
    float3 n020 : COEFFIENTN2;
    float3 n002 : COEFFIENTN3;
    float3 n110 : COEFFIENTN4;
    float3 n011 : COEFFIENTN5;
    float3 n101 : COEFFIENTN6;
};

PatchTess ConstantHS(InputPatch<VertexOut, 3> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess patchTess;

    float tess = min(64.f, gTessLod);
    tess = max(1.0f, tess);

    // Uniformly tessellate the patch.
    patchTess.EdgeTess[0] = tess;
    patchTess.EdgeTess[1] = tess;
    patchTess.EdgeTess[2] = tess;
    patchTess.InsideTess[0] = tess;

    float3 normal1 = normalize(patch[0].NormalL);
    float3 normal2 = normalize(patch[1].NormalL);
    float3 normal3 = normalize(patch[2].NormalL);

    //float3 normal1 = gNormalMap.SampleLevel(gsamAnisotropicWrap, patch[0].TexC, 0).xyz;
    //float3 normal2 = gNormalMap.SampleLevel(gsamAnisotropicWrap, patch[0].TexC, 0).xyz;
    //float3 normal3 = gNormalMap.SampleLevel(gsamAnisotropicWrap, patch[0].TexC, 0).xyz;

#define P1 (patch[0].PosL)
#define P2 (patch[1].PosL)
#define P3 (patch[2].PosL)
#define N1 (normal1)
#define N2 (normal2)
#define N3 (normal3)
    float w12 = dot((P2 - P1), N1);
    float w21 = dot((P1 - P2), N2);
    float w23 = dot((P3 - P2), N2);
    float w32 = dot((P2 - P3), N3);
    float w31 = dot((P1 - P3), N3);
    float w13 = dot((P3 - P1), N1);
    patchTess.b300 = P1;
    patchTess.b030 = P2;
    patchTess.b003 = P3;
    patchTess.b210 = (2 * P1 + P2 - w12 * N1) / 3.f;
    patchTess.b120 = (2 * P2 + P1 - w21 * N2) / 3.f;
    patchTess.b021 = (2 * P2 + P3 - w23 * N2) / 3.f;
    patchTess.b012 = (2 * P3 + P2 - w32 * N3) / 3.f;
    patchTess.b102 = (2 * P3 + P1 - w31 * N3) / 3.f;
    patchTess.b201 = (2 * P1 + P3 - w13 * N1) / 3.f;
    float3 E = (patchTess.b210 + patchTess.b120 + patchTess.b021 + patchTess.b012 + patchTess.b102 + patchTess.b201) / 6.f;
    float3 V = (P1 + P2 + P3) / 3.f;
    patchTess.b111 = E + (E - V) / 2.f;

    patchTess.n200 = N1;
    patchTess.n020 = N2;
    patchTess.n002 = N3;
    float v12 = 2.f * dot(P2 - P1, N1 + N2) / dot(P2 - P1, P2 - P1);
    float v23 = 2.f * dot(P3 - P2, N2 + N3) / dot(P3 - P2, P3 - P2);
    float v31 = 2.f * dot(P1 - P3, N3 + N1) / dot(P1 - P3, P1 - P3);
    patchTess.n110 = normalize(N1 + N2 - v12 * (P2 - P1));
    patchTess.n011 = normalize(N2 + N3 - v23 * (P3 - P2));
    patchTess.n101 = normalize(N3 + N1 - v31 * (P1 - P3));
#undef P1
#undef P2
#undef P3
#undef N1
#undef N2
#undef N3

    return patchTess;
}

struct HullOut
{
    float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
    float3 TangentL: TANGENT;
    float2 TexC    : TEXCOORD;
};

[domain("tri")]
//[partitioning("fractional_odd")]
[partitioning("integer")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.f)]
HullOut HS(InputPatch<VertexOut, 3> p, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
{
    HullOut hout;
    hout.PosL = p[i].PosL;
    hout.NormalL = p[i].NormalL;
    hout.TangentL = p[i].TangentL;
    hout.TexC = p[i].TexC;
    return hout;
}

struct DomainOut
{
    float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW: TANGENT;
    float2 TexC    : TEXCOORD;
};

[domain("tri")]
DomainOut DS(PatchTess patch, float3 uvw : SV_DomainLocation, const OutputPatch<HullOut, 3> tri)
{
    DomainOut dout;

#define u (uvw.y)
#define v (uvw.z)
#define w (uvw.x)
    float3 p = patch.b300 * w * w * w + patch.b030 * u * u * u + patch.b003 * v * v * v
        + patch.b210 * 3.0 * w * w * u + patch.b120 * 3.0 * w * u * u + patch.b201 * 3.0 * w * w * v
        + patch.b021 * 3.0 * u * u * v + patch.b102 * 3.0 * w * v * v + patch.b012 * 3.0 * u * v * v
        + patch.b111 * 6.0 * w * u * v;
    float3 normal = patch.n200*w*w + patch.n020*u*u + patch.n002*v*v
        + patch.n110*w*u + patch.n011*u*v + patch.n101*w*v;
    normal = normalize(normal);
    float2 TexC = uvw.x*tri[0].TexC + uvw.y*tri[1].TexC + uvw.z*tri[2].TexC;

    //p = uvw.x*tri[0].PosL + uvw.y*tri[1].PosL + uvw.z*tri[2].PosL;
    //normal = uvw.x*tri[0].NormalL + uvw.y*tri[1].NormalL + uvw.z*tri[2].NormalL;
    //normal = normalize(normal);
    float3 tangent = uvw.x*tri[0].TangentL + uvw.y*tri[1].TangentL + uvw.z*tri[2].TangentL;
    tangent = normalize(tangent);
#undef u
#undef v
#undef w

    float4 PosW = mul(float4(p, 1.f), gWorld);
    dout.PosW = PosW.xyz;
    dout.NormalW = mul(normal, (float3x3)gWorld);
    dout.TangentW = mul(tangent, (float3x3)gWorld);
    dout.PosH = mul(PosW, gViewProj);

    float4 texC4 = mul(float4(TexC, 0.0f, 1.0f), gTexTransform);
    dout.TexC = mul(texC4, gMatTransform).xy;

    return dout;
}

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
    // Uncompress each component from [0,1] to [-1,1].
    float3 normalT = 2.0f*normalMapSample - 1.0f;
    //float3 normalT = normalMapSample;

    // Build orthonormal basis.
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N)*N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);

    // Transform from tangent space to world space.
    float3 bumpedNormalW = mul(normalT, TBN);

    return bumpedNormalW;
}

float4 PS(DomainOut pin) : SV_Target
{
    float4 ambientDiffuseAlbedo = gDiffuseAlbedo;
    float4 diffuseAlbedo = gDiffuseAlbedo;
    float roughness = gRoughness;
    float3 fresnelR0 = gFresnelR0;
    if (gUseTexture) {
        //ambientDiffuseAlbedo = gRoughnessMap.Sample(gsamAnisotropicWrap, pin.TexC);
        //roughness = gRoughnessMap.Sample(gsamAnisotropicWrap, pin.TexC).r;
        diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC);
        //diffuseAlbedo *= 10.f;
        //fresnelR0 = gRoughnessMap.Sample(gsamAnisotropicWrap, pin.TexC).xyz;
        //diffuseAlbedo = float4(fresnelR0,1.0f);

        //float3 normalMapSample = gNormalMap.Sample(gsamAnisotropicWrap, pin.TexC).xyz;
        //float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.TangentW);
        //normalMapSample = 2.0f*normalMapSample - 1.0f;
        //pin.NormalW += normalMapSample;
        //pin.NormalW = -bumpedNormalW;
    }
    else{
        ambientDiffuseAlbedo = gDiffuseAlbedo;
        diffuseAlbedo = gDiffuseAlbedo;
    }

    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);

    // Vector from point being lit to eye. 
    float3 toEyeW = gEyePosW - pin.PosW;
    float toEyeDis = distance(pin.PosW, gEyePosW);
    toEyeW /= toEyeDis;

    // Light terms.
    float4 ambient = gAmbientLight * ambientDiffuseAlbedo;

    const float shininess = 1.0f - roughness;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;

    return litColor;
}

