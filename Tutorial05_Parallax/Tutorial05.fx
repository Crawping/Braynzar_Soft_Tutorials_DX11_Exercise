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
	vNormalWS = normalize(vNormalWS);
	vTangentWS = normalize(vTangentWS);
	vBiTangentWS = normalize(vBiTangentWS);
	float3x3 TBN = float3x3(vTangentWS, vBiTangentWS, vNormalWS);
	// Propagate the view and the light vectors (in tangent space):
    Out.vLightTS = mul(TBN,light.dir);
	Out.position = mul(float4(input.Pos, 1.0f), WVP);
	Out.texCoord = input.texCoord;


	// Compute position in world space:
    float4 vPositionWS = mul(float4(input.Pos, 1.0f), World);

	// Compute and output the world view vector (unnormalized):
    float3 vViewWS = EyePosition - vPositionWS;
    Out.vViewWS = vViewWS;
    Out.vViewTS = mul(TBN,vViewWS);
   // Out.vViewTS = float3(Out.vViewTS.x, -Out.vViewTS.y, Out.vViewTS.z);
	// Compute the ray direction for intersecting the height field profile with 
	// current view ray. See the above paper for derivation of this computation.

	// Compute initial parallax displacement direction:
    float2 vParallaxDirection = normalize(Out.vViewTS.xy);

	// The length of this vector determines the furthest amount of displacement:
    float fLength = length(Out.vViewTS);
    float fParallaxLength = sqrt(fLength * fLength - Out.vViewTS.z * Out.vViewTS.z) / Out.vViewTS.z;
	// Compute the actual reverse parallax displacement vector:
    Out.vParallaxOffsetTS = vParallaxDirection * fParallaxLength;

	// Need to scale the amount of displacement to account for different height ranges
	// in height maps. This is controlled by an artist-editable parameter:
    Out.vParallaxOffsetTS *= HeightMapScale;

	return Out;
}

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
    int nNumSteps = (int) lerp(50, 8, dot(vViewWS, vNormalWS));
    float fCurrHeight = 0.0;
    float fStepSize = 1.0 / (float) nNumSteps;
    float fPrevHeight = 1.0;
    float fNextHeight = 0.0;

    int nStepIndex = 0;
    bool bCondition = true;

    float2 vTexOffsetPerStep = fStepSize * i.vParallaxOffsetTS;
    float2 vTexCurrentOffset = i.texCoord;
    float fCurrentBound = 1.0;
    float fParallaxAmount = 0.0;

    float2 pt1 = 0;
    float2 pt2 = 0;
       
    float2 texOffset2 = 0;
   // sampler2D sampler2d = ObjSamplerState;
    while (nStepIndex < nNumSteps)
    {
        vTexCurrentOffset -= vTexOffsetPerStep;

         // Sample height map which in this case is stored in the alpha channel of the normal map:
        fCurrHeight = ObjNormMap.SampleGrad(ObjSamplerState, vTexCurrentOffset, dx, dy).a;
        //fCurrHeight = ObjNormMap.Sample(ObjSamplerState, vTexCurrentOffset).a;
        fCurrentBound -= fStepSize;

        if (fCurrHeight > fCurrentBound)
        {
            pt1 = float2(fCurrentBound, fCurrHeight);
            pt2 = float2(fCurrentBound + fStepSize, fPrevHeight);

            texOffset2 = vTexCurrentOffset - vTexOffsetPerStep;

            nStepIndex = nNumSteps + 1;
            fPrevHeight = fCurrHeight;
        }
        else
        {
            nStepIndex++;
            fPrevHeight = fCurrHeight;
        }
    }

    float fDelta2 = pt2.x - pt2.y;
    float fDelta1 = pt1.x - pt1.y;
      
    float fDenominator = fDelta2 - fDelta1;
      
      // SM 3.0 requires a check for divide by zero, since that operation will generate
      // an 'Inf' number instead of 0, as previous models (conveniently) did:
    if (fDenominator == 0.0f)
    {
        fParallaxAmount = 0.0f;
    }
    else
    {
        fParallaxAmount = (pt1.x * fDelta2 - pt2.x * fDelta1) / fDenominator;
    }
      
    float2 vParallaxOffset = i.vParallaxOffsetTS * (1 - fParallaxAmount);

      // The computed texture offset for the displaced point on the pseudo-extruded surface:
    float2 texSampleBase = i.texCoord - vParallaxOffset;
    texSample = texSampleBase;

	// Sample the normal from the normal map for the given texture sample:
    float3 vNormalTS = normalize(ObjNormMap.Sample(ObjSamplerState, texSample) * 2 - 1);
    float4 diffuse = ObjTexture.Sample(ObjSamplerState, texSample);
	float3 finalColor;
	finalColor = saturate(dot(vNormalTS, vLightTS)*light.diffuse  * diffuse);
   // finalColor = ObjNormMap.Sample(ObjSamplerState, texSample).aaaa;
   // finalColor = vViewTS;
	return float4(finalColor, diffuse.a);
}

