// load_texture.cpp

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void load_textures(Texture_Info *texture_info)
{
    HRESULT hr;

    char *filenames[10];
    filenames[0] = "01.png";
    filenames[1] = "02.png";
    filenames[2] = "03.png";
    filenames[3] = "04.png";
    filenames[4] = "05.png";
    filenames[5] = "06.png";
    filenames[6] = "07.png";
    filenames[7] = "08.png";
    filenames[8] = "09.png";
    filenames[9] = "10.png";

    char *shuffled_filenames[10];
    bool32 visited[10];
    for(int i = 0; i < 10; ++i)
    {
        visited[i] = false;
    }

    for (int i = 0; i < 10; ++i)
    {
        int j = rand() % 10;
        while(visited[j])
        {
            j = rand() % 10;
        }
        shuffled_filenames[i] = filenames[j];
        visited[j] = true;
    }    

    int image_width;
    int image_height;
    int image_channels;
    int image_desired_channels = 4;
    int image_pitch;

    // TODO: Completely overhaul the way we load and save textures so that any texture can be applied to any object easily

    for(int i = 0; i < 10; ++i)
    {
        unsigned char *image_data = stbi_load(shuffled_filenames[i],
                                            &image_width, 
                                            &image_height, 
                                            &image_channels, image_desired_channels);
        assert(image_data);
        image_pitch = image_width * 4;

        D3D11_TEXTURE2D_DESC texture_desc = {};
        texture_desc.Width = image_width;
        texture_desc.Height = image_height;
        texture_desc.MipLevels = 1;
        texture_desc.ArraySize = 1;
        texture_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.SampleDesc.Quality = 0;
        texture_desc.Usage = D3D11_USAGE_IMMUTABLE;
        texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        
        D3D11_SUBRESOURCE_DATA subresource_data = {};
        subresource_data.pSysMem = image_data;
        subresource_data.SysMemPitch = image_pitch;

        hr = device->CreateTexture2D(&texture_desc, &subresource_data, &texture_info[i].texture);
        AssertHR(hr);
        hr = device->CreateShaderResourceView(texture_info[i].texture, 0, &texture_info[i].shader_resource_view);
        AssertHR(hr);

        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.MipLODBias = 0.0f;
        sampler_desc.MaxAnisotropy = 1;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampler_desc.BorderColor[0] = 1.0f;
        sampler_desc.BorderColor[1] = 1.0f;
        sampler_desc.BorderColor[2] = 1.0f;
        sampler_desc.BorderColor[3] = 1.0f;
        sampler_desc.MinLOD = -FLT_MAX;
        sampler_desc.MaxLOD = FLT_MAX;
        
        hr = device->CreateSamplerState(&sampler_desc, &texture_info[i].sampler_state);
        AssertHR(hr);
    }
}

void load_ground_texture(Texture_Info *texture_info)
{
    HRESULT hr;

    int image_width;
    int image_height;
    int image_channels;
    int image_desired_channels = 4;
    int image_pitch;

        unsigned char *image_data = stbi_load("04.png",
                                            &image_width, 
                                            &image_height, 
                                            &image_channels, image_desired_channels);
        assert(image_data);
        image_pitch = image_width * 4;

        D3D11_TEXTURE2D_DESC texture_desc = {};
        texture_desc.Width = image_width;
        texture_desc.Height = image_height;
        texture_desc.MipLevels = 1;
        texture_desc.ArraySize = 1;
        texture_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.SampleDesc.Quality = 0;
        texture_desc.Usage = D3D11_USAGE_IMMUTABLE;
        texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        
        D3D11_SUBRESOURCE_DATA subresource_data = {};
        subresource_data.pSysMem = image_data;
        subresource_data.SysMemPitch = image_pitch;

        hr = device->CreateTexture2D(&texture_desc, &subresource_data, &texture_info->texture);
        AssertHR(hr);
        hr = device->CreateShaderResourceView(texture_info->texture, 0, &texture_info->shader_resource_view);
        AssertHR(hr);

        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.MipLODBias = 0.0f;
        sampler_desc.MaxAnisotropy = 1;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampler_desc.BorderColor[0] = 1.0f;
        sampler_desc.BorderColor[1] = 1.0f;
        sampler_desc.BorderColor[2] = 1.0f;
        sampler_desc.BorderColor[3] = 1.0f;
        sampler_desc.MinLOD = -FLT_MAX;
        sampler_desc.MaxLOD = FLT_MAX;
        
        hr = device->CreateSamplerState(&sampler_desc, &texture_info->sampler_state);
        AssertHR(hr);
}

void load_sky_texture(Texture_Info *texture_info)
{

    // TODO: Load cube map textures properly

    HRESULT hr;

    int image_width;
    int image_height;
    int image_channels;
    int image_desired_channels = 4;
    int image_pitch;

        unsigned char *image_data = stbi_load("skybox.png",
                                            &image_width, 
                                            &image_height, 
                                            &image_channels, image_desired_channels);
        assert(image_data);
        image_pitch = image_width * 4;

        D3D11_TEXTURE2D_DESC texture_desc = {};
        texture_desc.Width = image_width;
        texture_desc.Height = image_height;
        texture_desc.MipLevels = 1;
        texture_desc.ArraySize = 6;
        texture_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.SampleDesc.Quality = 0;
        texture_desc.Usage = D3D11_USAGE_IMMUTABLE;
        texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texture_desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
        
        D3D11_SUBRESOURCE_DATA subresource_data = {};
        subresource_data.pSysMem = image_data;
        subresource_data.SysMemPitch = image_pitch;

        hr = device->CreateTexture2D(&texture_desc, &subresource_data, &texture_info->texture);
        AssertHR(hr);

        D3D11_SHADER_RESOURCE_VIEW_DESC skymap_view_desc = {};
        skymap_view_desc.Format = texture_desc.Format;
        skymap_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        skymap_view_desc.TextureCube.MipLevels = texture_desc.MipLevels;
        skymap_view_desc.TextureCube.MostDetailedMip = 0;

        hr = device->CreateShaderResourceView(texture_info->texture, &skymap_view_desc, &texture_info->shader_resource_view);
        AssertHR(hr);

        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_desc.MipLODBias = 0.0f;
        sampler_desc.MaxAnisotropy = 1;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampler_desc.BorderColor[0] = 1.0f;
        sampler_desc.BorderColor[1] = 1.0f;
        sampler_desc.BorderColor[2] = 1.0f;
        sampler_desc.BorderColor[3] = 1.0f;
        sampler_desc.MinLOD = -FLT_MAX;
        sampler_desc.MaxLOD = FLT_MAX;
        
        hr = device->CreateSamplerState(&sampler_desc, &texture_info->sampler_state);
        AssertHR(hr);
}