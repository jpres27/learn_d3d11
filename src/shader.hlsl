cbuffer CB_Per_Object : register(b0)
{
    float4x4 world;
};

cbuffer Cb_Per_Frame : register(b1)
{
    float4x4 view;
    float4x4 projection;
};

Texture2D momo_tex;
SamplerState momo_sampler;

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 tex_coord : TEXCOORD;
};

VS_OUTPUT vs(float4 in_pos : POSITION, float2 in_tex_coord : TEXCOORD)
{
    VS_OUTPUT output;
    float4x4 wvp = mul(world, view);
    wvp = mul(wvp, projection);
    wvp = transpose(wvp);
    
    output.pos = mul(in_pos, wvp);
    output.tex_coord = in_tex_coord;

    return output;
}

float4 ps(VS_OUTPUT input) : SV_TARGET
{
    return momo_tex.Sample(momo_sampler, input.tex_coord);
}