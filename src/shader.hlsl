cbuffer CB_Per_Object
{
    float4x4 wvp;
};

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

VS_OUTPUT vs(float4 inPos : POSITION, float4 inColor : COLOR)
{
    VS_OUTPUT output;

    output.Pos = mul(inPos, wvp);
    output.Color = inColor;

    return output;
}

float4 ps(VS_OUTPUT input) : SV_TARGET
{
    return input.Color;
}