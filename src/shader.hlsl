struct Light
{
    float3 dir;
    float pad;
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

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 tex_coord : TEXCOORD;
    float3 normal : NORMAL;
};

VS_OUTPUT vs(float4 in_pos : POSITION, float2 in_tex_coord : TEXCOORD, float3 normal : NORMAL)
{
    VS_OUTPUT output;
    float4x4 wvp = mul(world, view);
    wvp = mul(wvp, projection);

    output.pos = mul(in_pos, wvp);
    output.normal = mul(float4(normal, 0.0), world).xyz;
    output.tex_coord = in_tex_coord;

    return output;
}

float4 ps(VS_OUTPUT input) : SV_TARGET
{
    input.normal = normalize(input.normal);

    float4 diffuse = cube_texture.Sample(tex_sampler, input.tex_coord);

    float3 final_color;

    final_color = diffuse.xyz * light.ambient.xyz;
    final_color += saturate(dot(light.dir, input.normal) * light.diffuse.xyz * diffuse.xyz);

    return float4(final_color, diffuse.a);
}