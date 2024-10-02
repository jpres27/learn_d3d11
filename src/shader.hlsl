struct Light
{
    float3 dir;
    float3 pos;
    float  range;
    float3 att;
    float4 ambient;
    float4 diffuse;
};

cbuffer CB_Per_Object : register(b0)
{
    float4x4 world;
};

cbuffer Cb_Per_Frame : register(b1)
{
    float4x4 view;
    float4x4 projection;
    Light light;
};

Texture2D cube_texture;
SamplerState tex_sampler;
TextureCube skymap;

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float4 world_pos : POSITION;
    float2 tex_coord : TEXCOORD;
    float3 normal : NORMAL;
};

struct SKYMAP_VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float3 tex_coord : TEXCOORD;
};

VS_OUTPUT vs(float4 in_pos : POSITION, float2 in_tex_coord : TEXCOORD, float3 normal : NORMAL)
{
    VS_OUTPUT output;
    float4x4 wvp = mul(world, view);
    wvp = mul(wvp, projection);

    output.pos = mul(in_pos, wvp);
    output.world_pos = mul(in_pos, world);
    output.normal = mul(float4(normal, 0.0), world).xyz;
    output.tex_coord = in_tex_coord;

    return output;
}

SKYMAP_VS_OUTPUT sky_vs(float3 in_pos : POSITION, float2 in_tex_coord : TEXCOORD, float3 normal : NORMAL)
{
    SKYMAP_VS_OUTPUT output = (SKYMAP_VS_OUTPUT)0;

    float4x4 wvp = mul(world, view);
    wvp = mul(wvp, projection);

    //Set pos to xyww instead of xyzw, so that z will always be 1 (furthest from camera)
    output.pos = mul(float4(in_pos, 1.0f), wvp).xyww;

    output.tex_coord = in_pos;

    return output;
}

float4 ps(VS_OUTPUT input) : SV_TARGET
{
    input.normal = normalize(input.normal);

    float4 diffuse = cube_texture.Sample(tex_sampler, input.tex_coord);

    float3 final_color = float3(0.0f, 0.0f, 0.0f);

    float3 light_to_pixel = light.pos - (float3)input.world_pos;
    float d = length(light_to_pixel);
    float3 final_ambient = (float3)(diffuse*light.ambient);
    if(d > light.range)
    {
        return float4(final_ambient, diffuse.a);
    }

    light_to_pixel /= d;
    float light_amt = dot(light_to_pixel, input.normal);
    if(light_amt > 0.0f)
    {
        final_color += (float3)(light_amt*diffuse*light.diffuse);
        final_color /= light.att[0] + (light.att[1]*d) + (light.att[2]*(d*d));
    }

    final_color = saturate(final_color + final_ambient);
    return float4(final_color, diffuse.a);
}

float4 sky_ps(SKYMAP_VS_OUTPUT input) : SV_TARGET
{
    return skymap.Sample(tex_sampler, input.tex_coord);
}

Texture2D<float4> post_input  : register(t4);
RWTexture2D<float4> post_output : register(u4);

[numthreads(16, 16, 1)]
void postprocessing(uint3 Pos: SV_DispatchThreadID)
{
  post_output[Pos.xy] = 2.0 * post_input[Pos.xy]; // make everything 2x darker, Pos.xy is pixel coordinate of output
}