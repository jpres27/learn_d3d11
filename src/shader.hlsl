cbuffer CB_Per_Object
{
    float4x4 wvp;
};

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

VS_OUTPUT vs(float4 inPos : POSITION, float2 inTexCoord : TEXCOORD)
{
    VS_OUTPUT output;

    output.Pos = mul(inPos, wvp);
    output.TexCoord = inTexCoord;

    return output;
}

Texture2D momo_tex;
SamplerState momo_sampler;

float4 ps(VS_OUTPUT input) : SV_TARGET
{
    return momo_tex.Sample(momo_sampler, input.TexCoord);
}