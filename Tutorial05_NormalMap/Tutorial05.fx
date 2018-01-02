//--------------------------------------------------------------------------------------
// File: Tutorial05.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
struct Light
{
	float3 dir;
	float zz;
	float4 ambient;
	float4 diffuse;
};

cbuffer ConstantBuffer : register( b0 )
{
	float4x4 WVP;
	float4x4 World;
	Light light;
	
}

Texture2D ObjTexture;
Texture2D ObjNormMap;
SamplerState ObjSamplerState;

TextureCube SkyMap;

//--------------------------------------------------------------------------------------
struct SKYMAP_VS_INPUT
{
	float3 Pos : POSITION;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD;
	float3 tangent : TANGENT;
};



struct SKYMAP_VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float3 texCoord : TEXCOORD1;
};


struct NORMALMAP_VS_INPUT 
{
	float3 Pos : POSITION;
	float3 normal : NORMAL;
	float2 texCoord : TEXCOORD0;
	float3 tangent : TANGENT;
};

struct NORMALMAP_VS_OUTPUT
{
	float4 position          : SV_POSITION;
	float2 texCoord          : TEXCOORD0;
	float3 vLightTS          : TEXCOORD2;   // light vector in tangent space, denormalized
};

//struct NORMALMAP_VS_OUTPUT
//{
//	float4 Pos : SV_POSITION;
//	float4 worldPos : POSITION;
//	float2 TexCoord : TEXCOORD;
//	float3 normal : NORMAL;
//	float3 tangent : TANGENT;
//};

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

NORMALMAP_VS_OUTPUT NORMALMAP_VS(NORMALMAP_VS_INPUT input)
{
	NORMALMAP_VS_OUTPUT Out = (NORMALMAP_VS_OUTPUT)0;
	//Make sure tangent is completely orthogonal to normal
	input.tangent = normalize(input.tangent - dot(input.tangent, input.normal)*input.normal);
	// Transform the normal, tangent and binormal vectors from object space to homogeneous projection space:
	float3 vNormalWS = mul(input.normal, (float3x3) World);
	float3 vTangentWS = mul(input.tangent, (float3x3) World);
	float3 vBiTangentWS = cross(input.normal, input.tangent);
	vBiTangentWS = mul(vBiTangentWS, (float3x3)World);
	vNormalWS = normalize(vNormalWS);
	vTangentWS = normalize(vTangentWS);
	vBiTangentWS = normalize(vBiTangentWS);
	float3x3 TBN = float3x3(vTangentWS, vBiTangentWS, vNormalWS);
	// Propagate the view and the light vectors (in tangent space):
	Out.vLightTS = mul(TBN,light.dir);
	Out.vLightTS = float3(Out.vLightTS.x, -Out.vLightTS.y, Out.vLightTS.z);
	Out.position = mul(float4(input.Pos, 1.0f), WVP);
	Out.texCoord = input.texCoord;
	return Out;
}

float4 NORMALMAP_PS(NORMALMAP_VS_OUTPUT i) : SV_Target
{
	float3 vLightTS = i.vLightTS;
	// Sample the normal from the normal map for the given texture sample:
	float3 vNormalTS = normalize(ObjNormMap.Sample(ObjSamplerState, i.texCoord) * 2 - 1);
	float4 diffuse = ObjTexture.Sample(ObjSamplerState, i.texCoord);

	float3 finalColor;
	finalColor = saturate(dot(vNormalTS, vLightTS)*light.diffuse  * diffuse);
	return float4(finalColor, diffuse.a);
}

//NORMALMAP_VS_OUTPUT NORMALMAP_VS(NORMALMAP_VS_INPUT input)
//{
//	NORMALMAP_VS_OUTPUT output;
//
//	output.Pos = mul(float4(input.Pos, 1.0f), WVP);
//	output.normal = mul(input.normal, (float3x3) World);
//	output.tangent = mul(input.tangent, (float3x3) World);
//	output.TexCoord = input.texCoord;
//
//	return output;
//}
//
//float4 NORMALMAP_PS(NORMALMAP_VS_OUTPUT input) : SV_Target
//{
//	input.normal = normalize(input.normal);
//
//	//Set diffuse color of material
//	float4 diffuse ;
//
//	//If material has a diffuse texture map, set it now
//	diffuse = ObjTexture.Sample(ObjSamplerState, input.TexCoord);
//
//	//If material has a normal map, we can set it now
//	
//	//Load normal from normal map
//	float4 normalMap = ObjNormMap.Sample(ObjSamplerState, input.TexCoord);
//
//	//Change normal map range from [0, 1] to [-1, 1]
//	normalMap = (2.0f*normalMap) - 1.0f;
//
//	//Make sure tangent is completely orthogonal to normal
//	input.tangent = normalize(input.tangent - dot(input.tangent, input.normal)*input.normal);
//
//	//Create the biTangent
//	float3 biTangent = cross(input.tangent,input.normal);
//	//Create the "Texture Space"
//	float3x3 TBN = float3x3(input.tangent, biTangent, input.normal);
//
//	//Convert normal from normal map to texture space and store in input.normal
//	float3 notmalT = normalize(mul(normalMap, TBN));
//	
//	float3 finalColor;
//
//	finalColor = saturate(dot(light.dir, notmalT) *light.diffuse * diffuse);
//
//	return float4(finalColor, diffuse.a);
//}
