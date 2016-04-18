
cbuffer cbPerObject
{
	float4x4 WVP;
    //Exercise: Add another variable to the constant buffer which will update the pixels color.
    float4 PCOLOR;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float4 Color : COLOR;
};

VS_OUTPUT VS(float4 inPos : POSITION, float4 inColor : COLOR)
{
    VS_OUTPUT output;

    output.Pos = mul(inPos, WVP);
    //Exercise: Add another variable to the constant buffer which will update the pixels color.
    output.Color = inColor*PCOLOR;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    return input.Color* PCOLOR;
}