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
	float4x4 View;                   // View matrix 
	float4   EyePosition;                    // Camera's location
	float    BaseTextureRepeat;      // The tiling factor for base and normal map textures
	float    HeightMapScale;         // Describes the useful range of values for the height field
	float f1;
	float f2;
	Light light;
	
}

Texture2D ObjTexture;
Texture2D ObjNormMap;
SamplerState ObjSamplerState;


//--------------------------------------------------------------------------------------



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
	float3 vViewTS           : TEXCOORD6;   // view vector in tangent space, denormalized
	float2 vParallaxOffsetTS : TEXCOORD3;   // Parallax offset vector in tangent space
	float3 vNormalWS         : TEXCOORD4;   // Normal vector in world space
	float3 vViewWS           : TEXCOORD5;   // View vector in world space
};



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
    Out.vNormalWS = vNormalWS;
	vNormalWS = normalize(vNormalWS);
	vTangentWS = normalize(vTangentWS);
	vBiTangentWS = normalize(vBiTangentWS);
	float3x3 TBN = float3x3(vTangentWS, vBiTangentWS, vNormalWS);
	// Propagate the view and the light vectors (in tangent space):
    Out.vLightTS = mul(TBN,light.dir);
	Out.position = mul(float4(input.Pos, 1.0f), WVP);
    Out.texCoord = input.texCoord * BaseTextureRepeat;
 
	// Compute position in world space:
    float4 vPositionWS = mul(float4(input.Pos, 1.0f), World);

	// Compute and output the world view vector (unnormalized):
    float3 vViewWS = EyePosition - vPositionWS;
    Out.vViewWS = vViewWS;
    Out.vViewTS = mul(TBN,vViewWS);
	// Compute the ray direction for intersecting the height field profile with 
	// current view ray. See the above paper for derivation of this computation.
    // Compute initial parallax displacement direction:
    float2 vParallaxDirection = normalize(Out.vViewTS.xy);


    float fParallaxLimit = -length(Out.vViewTS.xy) / Out.vViewTS.z;
    fParallaxLimit *= HeightMapScale;
    Out.vParallaxOffsetTS = vParallaxDirection * fParallaxLimit;



	return Out;
}

//https://www.gamedev.net/articles/programming/graphics/a-closer-look-at-parallax-occlusion-mapping-r3262/
float4 NORMALMAP_PS(NORMALMAP_VS_OUTPUT i) : SV_Target
{
    float3 vLightTS = normalize(i.vLightTS);
    vLightTS = float3(vLightTS.x, -vLightTS.y, vLightTS.z);
    float3 vViewTS = normalize(i.vViewTS);
    float3 vViewWS = normalize(i.vViewWS);
    float3 vNormalWS = normalize(i.vNormalWS);

	// Compute the current gradients:
    float2 fTexCoordsPerSize = i.texCoord * float2(256,256);
	 // Compute all 4 derivatives in x and y in a single instruction to optimize:
    float2 dxSize, dySize;
    float2 dx, dy;

    float4(dxSize, dx) = ddx(float4(fTexCoordsPerSize, i.texCoord));
    float4(dySize, dy) = ddy(float4(fTexCoordsPerSize, i.texCoord));
   // Start the current sample located at the input texture coordinate, which would correspond
   // to computing a bump mapping result:
    float2 texSample = i.texCoord;
    int nNumSteps = (int) lerp(50, 8, dot(vViewTS, float3(0, 0, 1)));

    float fCurrHeight = 0.0;
    float fStepSize = 1.0 / (float) nNumSteps;
    float fPrevHeight = 1.0;
    float fNextHeight = 0.0;
    int nStepIndex = 0;
    bool bCondition = true;
    float fCurrentBound = 1.0;
    float2 vCurrOffset = float2(0, 0);
    float2 vLastOffset = float2(0, 0);

    while (nStepIndex < nNumSteps)
    {
         // Sample height map which in this case is stored in the alpha channel of the normal map:
        fCurrHeight = ObjNormMap.SampleGrad(ObjSamplerState, i.texCoord + vCurrOffset, dx, dy).a;

        if (fCurrHeight > fCurrentBound)
        {
      
            float delta1 = fCurrHeight - fCurrentBound;
            float delta2 = (fCurrentBound + fStepSize) - fPrevHeight;
            float ratio = delta1 / (delta1 + delta2);
            vCurrOffset = (ratio) * vLastOffset + (1.0 - ratio) * vCurrOffset;
            nStepIndex = nNumSteps + 1;
          
        }
        else
        {
            fCurrentBound -= fStepSize;

            nStepIndex++;
            fPrevHeight = fCurrHeight;
            vLastOffset = vCurrOffset;
            vCurrOffset += fStepSize * i.vParallaxOffsetTS;
        }
    }

    // The computed texture offset for the displaced point on the pseudo-extruded surface:
    texSample = i.texCoord + vCurrOffset;

	// Sample the normal from the normal map for the given texture sample:
    float3 vNormalTS = normalize(ObjNormMap.Sample(ObjSamplerState, texSample) * 2 - 1);
    float4 diffuse = ObjTexture.Sample(ObjSamplerState, texSample);
	float3 finalColor;
	finalColor = saturate(dot(vNormalTS, vLightTS)*light.diffuse  * diffuse);
	return float4(finalColor, diffuse.a);
}

