//--------------------------------------------------------------------------------------
// File: Tutorial05.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
cbuffer ConstantBuffer : register( b0 )
{
	float4x4 WVP;
	float4x4 World;
	float3 cameraPos;
	
}

Texture2D ObjTexture;
SamplerState ObjSamplerState;
TextureCube SkyMap;

//--------------------------------------------------------------------------------------
struct SKYMAP_VS_INPUT
{
	float3 Pos : POSITION;
	float3 normal : NORMAL;
};


struct SKYMAP_VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float3 texCoord : TEXCOORD1;
};



struct REFLECT_VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float3 PosW : POSITION1;
	float3 normal : POSITION2;
};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
SKYMAP_VS_OUTPUT SKYMAP_VS(SKYMAP_VS_INPUT input )
{
	SKYMAP_VS_OUTPUT output = (SKYMAP_VS_OUTPUT)0;
	output.Pos = mul(float4(input.Pos, 1.0f), WVP).xyww;
	output.texCoord = input.Pos;
    return output;
}

float4 SKYMAP_PS(SKYMAP_VS_OUTPUT input) : SV_Target
{
	return SkyMap.Sample(ObjSamplerState, input.texCoord);
}

REFLECT_VS_OUTPUT REFLECT_VS(float3 inPos : POSITION, float3 normal : NORMAL)
{
	REFLECT_VS_OUTPUT output = (REFLECT_VS_OUTPUT)0;

	output.Pos = mul(float4(inPos, 1.0f), WVP);

	output.normal =  normalize(mul(normal, (float3x3) World));
	output.PosW = mul(float4(inPos, 1.0f), World);

	
	/*float3 posw = mul(float4(inPos, 1.0f), World);
	float3 n = normalize(mul(normal,(float3x3) World));
	float3 I = normalize (posw - cameraPos);
	float3 R =  (reflect(I, n));
	output.normal = R;*/
	
	return output;
}

float4 REFLECT_PS(REFLECT_VS_OUTPUT input) : SV_Target
{

	float3 I = normalize(input.PosW - cameraPos);
	float3 R = reflect(I, input.normal);
	return SkyMap.Sample(ObjSamplerState,R);

	//return SkyMap.Sample(ObjSamplerState, input.normal);

}


