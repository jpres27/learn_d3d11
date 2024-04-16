#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxguid")
#pragma comment (lib, "dxgi")


#include <stdint.h>
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dxgidebug.h>
#include <DirectXMath.h>

#include <time.h>

#include "render.h"
#include "utils.cpp"
#include "geometry.cpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
// #define STBI_ONLY_PNG

using namespace DirectX;

global_variable bool32 running;

real64 counts_per_second = 0.0;
int64 counter_start = 0;
int frame_count = 0;
int fps = 0;
int64 frame_time_old = 0;
real64 frame_time;

IDXGISwapChain1* swap_chain;
ID3D11Device1* device;
ID3D11DeviceContext1* device_context;
ID3D11RenderTargetView* render_target_view;

ID3DUserDefinedAnnotation *event_grouper;

ID3D11Buffer* cube_index_buffer;
ID3D11Buffer* cube_vert_buffer;

ID3D11Buffer* sphere_index_buffer;
ID3D11Buffer* sphere_vert_buffer;

ID3D11Buffer* ground_index_buffer;
ID3D11Buffer* ground_vert_buffer;

ID3D11DepthStencilView* depth_stencil_view;
ID3D11Texture2D* depth_stencil_buffer;

ID3D11VertexShader* vertex_shader;
ID3D11PixelShader* pixel_shader;
ID3D11InputLayout* vertex_layout;

ID3D11Buffer* cb_per_object_buffer;
ID3D11Buffer* cb_per_frame_buffer;

const int WIDTH  = 800;
const int HEIGHT = 600;

XMMATRIX wvp;
XMMATRIX cam_view;
XMMATRIX cam_projection;
XMVECTOR cam_position;
XMVECTOR cam_target;
XMVECTOR cam_up;

ID3D11BlendState* transparency;
ID3D11RasterizerState *ccw_cull;
ID3D11RasterizerState *cw_cull;

real64 rotation_state = 0.01f;

Light light = {};

// TODO: Setup a way to make rendering calls outside of the object lists for skybox and ground ... 

// TODO: Make shader loading not ad hoc, once it makes sense to do so
#include "d3d11_vshader.h"
#include "d3d11_pshader.h"

void start_timer()
{
    LARGE_INTEGER frequency_count;
    QueryPerformanceFrequency(&frequency_count);

    counts_per_second = double(frequency_count.QuadPart);

    QueryPerformanceCounter(&frequency_count);
    counter_start = frequency_count.QuadPart;
}

real64 get_time()
{
    LARGE_INTEGER current_time;
    QueryPerformanceCounter(&current_time);
    return real64(current_time.QuadPart - counter_start)/counts_per_second;
}

real64 get_frame_time()
{
    LARGE_INTEGER current_time;
    int64 tick_count;
    QueryPerformanceCounter(&current_time);

    tick_count = current_time.QuadPart - frame_time_old;
    frame_time_old = current_time.QuadPart;

    if(tick_count < 0)
        tick_count = 0;

    return real32(tick_count)/counts_per_second;
}

internal real32 find_dist_from_cam(XMMATRIX cube)
{
    XMVECTOR cube_pos = XMVectorZero();
    cube_pos = XMVector2TransformCoord(cube_pos, cube);

    real32 dist_x = XMVectorGetX(cube_pos) - XMVectorGetX(cam_position);
    real32 dist_y = XMVectorGetY(cube_pos) - XMVectorGetY(cam_position);
    real32 dist_z = XMVectorGetZ(cube_pos) - XMVectorGetZ(cam_position);

    real32 cube_dist = dist_x*dist_x + dist_y*dist_y + dist_z*dist_z;
    return(cube_dist);
}

internal void set_debug_name(ID3D11DeviceChild *child, char *name)
{
    if(child && name)
    {
        child->SetPrivateData(WKPDID_D3DDebugObjectName, string_length(name), name);
    }
}

void init_shape(Shape *shapes, int num_cube, int num_sphere)
{
    int32 num_objects = num_cube + num_sphere;

    for(int i = 0; i < num_cube; ++i)
    {
        shapes[i].shape_type = cube_mesh;
    }

    for(int i = num_cube; i < num_objects; ++i)
    {
        shapes[i].shape_type = sphere_mesh;
    }
}

void init_coords(Shape *shapes, int32 size, bool32 positive)
{
    for(int i = 0; i < size; ++i)
    {
        if(positive)
        {
            shapes[i].x_coord = (real32)i * 3;
            shapes[i].y_coord = 0.0f;

        }
        else 
        {
            shapes[i].x_coord = ((real32)i) * -3;
            shapes[i].y_coord = 0.0f;
        }
    }
}

void attach_textures(Shape *shapes, int32 size, Texture_Info *texture_info)
{
    int k = 0;
    for(int i = 0; i < size; ++i)
    {
        if(k >= 10)
        {
            k = 0;
        }

        shapes[i].texture_info.sampler_state = texture_info[k].sampler_state;
        shapes[i].texture_info.shader_resource_view = texture_info[k].shader_resource_view;
        shapes[i].texture_info.texture = texture_info[k].texture;

        ++k;
    }
}

void d3d11_init(HINSTANCE hInstance, HWND window)
{
    D3D_FEATURE_LEVEL levels[] = 
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };

    HRESULT hr;

    ID3D11Device* base_device;
    ID3D11DeviceContext* base_dc;

    UINT flags = 0;
    
#if DEBUG
    flags = D3D11_CREATE_DEVICE_DEBUG;
#endif

    hr = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, levels, 
        ARRAYSIZE(levels), D3D11_SDK_VERSION, &base_device, 0, &base_dc);
    AssertHR(hr);

    base_device->QueryInterface(__uuidof(ID3D11Device1), (void **)&device);
    assert(device);
    base_device->Release();

    base_dc->QueryInterface(__uuidof(ID3D11DeviceContext1), (void **)&device_context);
    assert(device_context);
    base_dc->Release();

    IDXGIDevice1* dxgi_device;
    hr = device->QueryInterface(__uuidof(IDXGIDevice1), (void **)&dxgi_device);
    AssertHR(hr);
      
    IDXGIAdapter* dxgi_adapter;
    hr = dxgi_device->GetAdapter(&dxgi_adapter);
    AssertHR(hr);

    IDXGIFactory2* dxgi_factory;
    dxgi_adapter->GetParent(__uuidof(IDXGIFactory2), (void **)&dxgi_factory);
    AssertHR(hr);

#if DEBUG
    ID3D11InfoQueue* info;
    device->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&info);
    info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
    info->Release();

    hr = device_context->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (void**)&event_grouper);
    AssertHR(hr);
#endif

    // TODO: Get DXGI debug layer to work by figuring out how to actually call getdebuginterface
#if 0
    IDXGIInfoQueue* dxgi_info;
    DXGIGetDebugInterface1(__uuidof(IDXGIInfoQueue), (void**)&dxgi_info);
    dxgi_info->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    dxgi_info->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE);
    dxgi_info->Release();
#endif

    //Describe our SwapChain
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_chain_desc.Stereo = FALSE;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 2;
    swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

    hr = dxgi_factory->CreateSwapChainForHwnd(device, window, &swap_chain_desc, 0, 0, &swap_chain);
    AssertHR(hr);

    dxgi_factory->Release();
    dxgi_adapter->Release();
    dxgi_device->Release();

    ID3D11Texture2D* backbuffer;
    swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer);

    device->CreateRenderTargetView(backbuffer, 0, &render_target_view);
    // backbuffer->Release();

    D3D11_TEXTURE2D_DESC depth_stencil_desc = {};
    backbuffer->GetDesc(&depth_stencil_desc);
    depth_stencil_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_stencil_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    device->CreateTexture2D(&depth_stencil_desc, 0, &depth_stencil_buffer);
    device->CreateDepthStencilView(depth_stencil_buffer, 0, &depth_stencil_view);
}

void scene_init()
{
    HRESULT hr;

    hr = device->CreateVertexShader(d3d11_vshader, sizeof(d3d11_vshader), 0, &vertex_shader);
    hr = device->CreatePixelShader(d3d11_pshader, sizeof(d3d11_pshader), 0, &pixel_shader);

    device_context->VSSetShader(vertex_shader, 0, 0);
    device_context->PSSetShader(pixel_shader, 0, 0);

    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = WIDTH;
    viewport.Height = HEIGHT;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    device_context->RSSetViewports(1, &viewport);

    D3D11_BUFFER_DESC cb_per_object_bd = {};
    cb_per_object_bd.Usage = D3D11_USAGE_DEFAULT;
    cb_per_object_bd.ByteWidth = sizeof(CB_Per_Object);
    cb_per_object_bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = device->CreateBuffer(&cb_per_object_bd, 0, &cb_per_object_buffer);

    D3D11_BUFFER_DESC cb_per_frame_bd = {};
    cb_per_frame_bd.Usage = D3D11_USAGE_DEFAULT;
    cb_per_frame_bd.ByteWidth = sizeof(CB_Per_Frame);
    cb_per_frame_bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = device->CreateBuffer(&cb_per_frame_bd, 0, &cb_per_frame_buffer);

    cam_position = XMVectorSet(0.0f, 3.0f, -8.0f, 0.0f);
    cam_target = XMVectorSet(2.0f, 0.0f, 0.0f, 0.0f);
    cam_up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    cam_view = XMMatrixLookAtLH(cam_position, cam_target, cam_up);
    cam_projection = XMMatrixPerspectiveFovLH(0.4f*3.14f, (real32)WIDTH/(real32)HEIGHT, 1.0f, 1000.0f);

    D3D11_BLEND_DESC blend_desc = {};
    D3D11_RENDER_TARGET_BLEND_DESC rt_blend_desc = {};
    rt_blend_desc.BlendEnable = true;
    rt_blend_desc.SrcBlend = D3D11_BLEND_SRC_COLOR;
    rt_blend_desc.DestBlend = D3D11_BLEND_BLEND_FACTOR;
    rt_blend_desc.BlendOp = D3D11_BLEND_OP_ADD;
    rt_blend_desc.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt_blend_desc.DestBlendAlpha = D3D11_BLEND_ZERO;
    rt_blend_desc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt_blend_desc.RenderTargetWriteMask = D3D10_COLOR_WRITE_ENABLE_ALL;
    blend_desc.AlphaToCoverageEnable = false;
    blend_desc.RenderTarget[0] = rt_blend_desc;
    device->CreateBlendState(&blend_desc, &transparency);

    D3D11_RASTERIZER_DESC cmdesc = {};
    cmdesc.FillMode = D3D11_FILL_SOLID;
    cmdesc.CullMode = D3D11_CULL_BACK;
    cmdesc.FrontCounterClockwise = true;
    hr = device->CreateRasterizerState(&cmdesc, &ccw_cull);
    cmdesc.FrontCounterClockwise = false;
    hr = device->CreateRasterizerState(&cmdesc, &cw_cull);
}

void load_cube_mesh()
{
    HRESULT hr;

    #include "cube.h"

    D3D11_BUFFER_DESC ground_index_bd = {};
    ground_index_bd.Usage = D3D11_USAGE_DEFAULT;
    ground_index_bd.ByteWidth = sizeof(DWORD)*12*3;
    ground_index_bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ground_index_bd.CPUAccessFlags = 0;
    ground_index_bd.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA ground_index_init_data ={};
    ground_index_init_data.pSysMem = indices;

    hr = device->CreateBuffer(&ground_index_bd, &ground_index_init_data, &cube_index_buffer);
    AssertHR(hr);
    device_context->IASetIndexBuffer(cube_index_buffer, DXGI_FORMAT_R32_UINT, 0);

    D3D11_BUFFER_DESC vert_buffer_desc = {};
    vert_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    vert_buffer_desc.ByteWidth = sizeof(Vertex)*24;
    vert_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vert_buffer_desc.CPUAccessFlags = 0;
    vert_buffer_desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA vert_buffer_data = {};
    vert_buffer_data.pSysMem = v;

    hr = device->CreateBuffer(&vert_buffer_desc, &vert_buffer_data, &cube_vert_buffer);
    AssertHR(hr);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    device_context->IASetVertexBuffers(0, 1, &cube_vert_buffer, &stride, &offset);

    hr = device->CreateInputLayout(layout, ARRAYSIZE(layout), d3d11_vshader, sizeof(d3d11_vshader), &vertex_layout);
    AssertHR(hr);

    device_context->IASetInputLayout(vertex_layout); 

    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void load_sphere_mesh(Sphere *sphere)
{
    HRESULT hr;

    D3D11_BUFFER_DESC ground_index_bd = {};
    ground_index_bd.Usage = D3D11_USAGE_DEFAULT;
    ground_index_bd.ByteWidth = sizeof(DWORD)*360;
    ground_index_bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ground_index_bd.CPUAccessFlags = 0;
    ground_index_bd.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA ground_index_init_data ={};
    ground_index_init_data.pSysMem = sphere->indices;

    hr = device->CreateBuffer(&ground_index_bd, &ground_index_init_data, &sphere_index_buffer);
    AssertHR(hr);
    device_context->IASetIndexBuffer(sphere_index_buffer, DXGI_FORMAT_R32_UINT, 0);

    D3D11_BUFFER_DESC vert_buffer_desc = {};
    vert_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    vert_buffer_desc.ByteWidth = sizeof(Vertex)*91;
    vert_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vert_buffer_desc.CPUAccessFlags = 0;
    vert_buffer_desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA vert_buffer_data = {};
    vert_buffer_data.pSysMem = sphere->vertices;

    hr = device->CreateBuffer(&vert_buffer_desc, &vert_buffer_data, &sphere_vert_buffer);
    AssertHR(hr);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    device_context->IASetVertexBuffers(0, 1, &sphere_vert_buffer, &stride, &offset);

    hr = device->CreateInputLayout(layout, ARRAYSIZE(layout), d3d11_vshader, sizeof(d3d11_vshader), &vertex_layout);
    AssertHR(hr);

    device_context->IASetInputLayout(vertex_layout); 

    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void load_ground_mesh()
{
    #include "ground.h"

    HRESULT hr;

    D3D11_BUFFER_DESC ground_index_bd = {};
    ground_index_bd.Usage = D3D11_USAGE_DEFAULT;
    ground_index_bd.ByteWidth = sizeof(DWORD)*2*3;
    ground_index_bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ground_index_bd.CPUAccessFlags = 0;
    ground_index_bd.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA ground_index_init_data ={};
    ground_index_init_data.pSysMem = indices;

    hr = device->CreateBuffer(&ground_index_bd, &ground_index_init_data, &ground_index_buffer);
    AssertHR(hr);
    device_context->IASetIndexBuffer(ground_index_buffer, DXGI_FORMAT_R32_UINT, 0);

    D3D11_BUFFER_DESC vert_buffer_desc = {};
    vert_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    vert_buffer_desc.ByteWidth = sizeof(Vertex)*4;
    vert_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vert_buffer_desc.CPUAccessFlags = 0;
    vert_buffer_desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA vert_buffer_data = {};
    vert_buffer_data.pSysMem = v;

    hr = device->CreateBuffer(&vert_buffer_desc, &vert_buffer_data, &ground_vert_buffer);
    AssertHR(hr);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    device_context->IASetVertexBuffers(0, 1, &ground_vert_buffer, &stride, &offset);

    hr = device->CreateInputLayout(layout, ARRAYSIZE(layout), d3d11_vshader, sizeof(d3d11_vshader), &vertex_layout);
    AssertHR(hr);

    device_context->IASetInputLayout(vertex_layout); 

    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

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

        unsigned char *image_data = stbi_load("03.png",
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

void update_and_render(Object_Lists *object_lists, Sphere *sphere, real64 time)
{
    int32 num_objects = object_lists->opaque_size + object_lists->transparent_size;
    assert(num_objects > 0 && num_objects < 10);


    light.dir = XMFLOAT3(0.0f, 1.0f, 0.0f);
    light.ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
    light.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    rotation_state += 1.0f * time;
    if (rotation_state > 6.28f)
    {
        rotation_state = 0.0f;
    }

    XMMATRIX translation;
    XMMATRIX scaling;
    XMVECTOR rotation_axis_y = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR rotation_axis_z = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    XMVECTOR rotation_axis_x = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    XMMATRIX rotation = XMMatrixRotationAxis(rotation_axis_y, rotation_state);

    for(int32 i = 0; i < object_lists->opaque_size; ++i)
    {
        object_lists->opaque_objects[i].world = XMMatrixIdentity();
        real32 x_translate = 0;
        if(object_lists->opaque_objects[i].shape_type == cube_mesh) 
        {
            x_translate = object_lists->opaque_objects[i].x_coord;
            translation = XMMatrixTranslation(x_translate, 0.0f, 4.0f);
            rotation_axis_y = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            rotation = XMMatrixRotationAxis(rotation_axis_y, rotation_state);
            object_lists->opaque_objects[i].world = translation*rotation;
        }
        else if (object_lists->opaque_objects[i].shape_type == sphere_mesh)
        {
            x_translate = object_lists->opaque_objects[i].x_coord;
            translation = XMMatrixTranslation(x_translate, 0.0f, 0.0f);
            rotation_axis_y = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            rotation = XMMatrixRotationAxis(rotation_axis_y, -rotation_state);
            scaling = XMMatrixScaling(1.3f, 1.3f, 1.3f);
            object_lists->opaque_objects[i].world = rotation*scaling*translation;
        }
    }

    for(int32 i = 0; i < object_lists->transparent_size; ++i)
    {
        object_lists->transparent_objects[i].world = XMMatrixIdentity();
        real32 x_translate = 0;
        real32 y_translate = 0;
        if(object_lists->transparent_objects[i].shape_type == cube_mesh) 
        {

            x_translate = object_lists->transparent_objects[i].x_coord;
            translation = XMMatrixTranslation(x_translate, y_translate, 4.0f);
            rotation_axis_y = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            rotation = XMMatrixRotationAxis(rotation_axis_y, rotation_state);
            object_lists->transparent_objects[i].world = translation*rotation;
        }
        else if (object_lists->transparent_objects[i].shape_type == sphere_mesh)
        {

            x_translate = object_lists->transparent_objects[i].x_coord;
            translation = XMMatrixTranslation(x_translate, y_translate, 0.0f);
            rotation_axis_y = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            rotation = XMMatrixRotationAxis(rotation_axis_y, -rotation_state);
            scaling = XMMatrixScaling(1.3f, 1.3f, 1.3f);
            object_lists->transparent_objects[i].world = rotation*scaling*translation;
        }
    }

    device_context->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);

#if DEBUG
    event_grouper->BeginEvent(L"Clear screen");
#endif
    FLOAT color[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    device_context->ClearRenderTargetView(render_target_view, color);
    device_context->ClearDepthStencilView(depth_stencil_view, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);
#if DEBUG
    event_grouper->EndEvent();
#endif

#if DEBUG
    event_grouper->BeginEvent(L"Setup cb_per_frame");
#endif
    CB_Per_Frame cb_per_frame = {};
    cb_per_frame.view = XMMatrixTranspose(cam_view);
    cb_per_frame.projection = XMMatrixTranspose(cam_projection);
    cb_per_frame.light = light;
    device_context->UpdateSubresource(cb_per_frame_buffer, 0, 0, &cb_per_frame, 0, 0);
    device_context->VSSetConstantBuffers(1, 1, &cb_per_frame_buffer);
    device_context->PSSetConstantBuffers(1, 1, &cb_per_frame_buffer);
#if DEBUG
    event_grouper->EndEvent();
#endif

    for(int i = 0; i < object_lists->opaque_size; ++i)
    {
        object_lists->opaque_objects[i].dist_from_cam = find_dist_from_cam(object_lists->opaque_objects[i].world);
    }

    sort_opaque_dist(object_lists->opaque_objects, object_lists->opaque_size);

    CB_Per_Object cb_per_object = {};
    XMMATRIX *cbpo_index = (XMMATRIX *)&cb_per_object.cube1;

    for(int32 i = 0; i < object_lists->opaque_size; ++i)
    {
            *cbpo_index = XMMatrixTranspose(object_lists->opaque_objects[i].world);
            cbpo_index += 4;
    }

    for(int i = 0; i < object_lists->transparent_size; ++i)
    {
        object_lists->transparent_objects[i].dist_from_cam = find_dist_from_cam(object_lists->transparent_objects[i].world);
    }

    sort_transparent_dist(object_lists->transparent_objects, object_lists->transparent_size);

    for(int32 i = 0; i < object_lists->transparent_size; ++i)
    {
            *cbpo_index = XMMatrixTranspose(object_lists->transparent_objects[i].world);
            cbpo_index += 4;
    }
    device_context->UpdateSubresource(cb_per_object_buffer, 0, 0, &cb_per_object, 0, 0);

    UINT offset = 0;
    UINT size = (sizeof(cb_per_object.cube1)*4) / 16;

#if DEBUG
    event_grouper->BeginEvent(L"Render opaque objects");
#endif

    device_context->OMSetBlendState(0, 0, 0xFFFFFFFF);

    for(int32 i = 0; i < object_lists->opaque_size; ++i)
    {
        if(object_lists->opaque_objects[i].shape_type == cube_mesh)
        {
            load_cube_mesh();
            offset = ((sizeof(cb_per_object.cube1)*4) / 16) * i;
            device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &offset, &size);
            device_context->PSSetShaderResources(0, 1, &object_lists->opaque_objects[i].texture_info.shader_resource_view);
            device_context->PSSetSamplers(0, 1, &object_lists->opaque_objects[i].texture_info.sampler_state);
            device_context->DrawIndexed(36, 0, 0);
        }
        else if(object_lists->opaque_objects[i].shape_type == sphere_mesh)
        {
            load_sphere_mesh(sphere);
            offset = ((sizeof(cb_per_object.cube1)*4) / 16) * i;
            device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &offset, &size);
            device_context->PSSetShaderResources(0, 1, &object_lists->opaque_objects[i].texture_info.shader_resource_view);
            device_context->PSSetSamplers(0, 1, &object_lists->opaque_objects[i].texture_info.sampler_state);
            device_context->DrawIndexed(360, 0, 0);
        }
    }
#if DEBUG
    event_grouper->EndEvent();
#endif

#if DEBUG
    event_grouper->BeginEvent(L"Render transparent objects");
#endif
    real32 blend_factor[] = {0.75f, 0.75f, 0.75f, 1.0f};
    device_context->OMSetBlendState(transparency, blend_factor, 0xFFFFFFFF);

    for(int32 i = 0; i < object_lists->transparent_size; ++i)
    {
        if(object_lists->transparent_objects[i].shape_type == cube_mesh)
        {
            #if DEBUG
            event_grouper->BeginEvent(L"Render transparent cube");
            #endif
            load_cube_mesh();
            offset = ((sizeof(cb_per_object.cube1)*4) / 16) * i;
            device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &offset, &size);
            device_context->PSSetShaderResources(0, 1, &object_lists->transparent_objects[i].texture_info.shader_resource_view);
            device_context->PSSetSamplers(0, 1, &object_lists->transparent_objects[i].texture_info.sampler_state);
            device_context->RSSetState(ccw_cull);
            device_context->DrawIndexed(36, 0, 0);
            device_context->RSSetState(cw_cull);
            device_context->DrawIndexed(36, 0, 0);
            #if DEBUG
            event_grouper->EndEvent();
            #endif
        }
        else if(object_lists->transparent_objects[i].shape_type == sphere_mesh)
        {
            #if DEBUG
            event_grouper->BeginEvent(L"Render transparent sphere");
            #endif
            load_sphere_mesh(sphere);
            offset = ((sizeof(cb_per_object.cube1)*4) / 16) * i;
            device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &offset, &size);
            device_context->PSSetShaderResources(0, 1, &object_lists->transparent_objects[i].texture_info.shader_resource_view);
            device_context->PSSetSamplers(0, 1, &object_lists->transparent_objects[i].texture_info.sampler_state);
            device_context->RSSetState(ccw_cull);
            device_context->DrawIndexed(360, 0, 0);
            device_context->RSSetState(cw_cull);
            device_context->DrawIndexed(360, 0, 0);
            #if DEBUG
            event_grouper->EndEvent();
            #endif
        }
    }
#if DEBUG
    event_grouper->EndEvent();
#endif


    swap_chain->Present(0, 0);
}

LRESULT CALLBACK win32_main_window_callback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch( msg )
    {
    case WM_KEYDOWN:
        if( wParam == VK_ESCAPE ){
            DestroyWindow(window);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(window,
        msg,
        wParam,
        lParam);
}

HWND window_init(HINSTANCE hInstance,
    int ShowWnd,
    int width, int height,
    bool32 windowed)
{
    WNDCLASSEX wc;

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = win32_main_window_callback;
    wc.cbClsExtra = NULL;
    wc.cbWndExtra = NULL;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "Render_Window_Class";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, "Error registering class",    
            "Error", MB_OK | MB_ICONERROR);
    }

    HWND window = CreateWindowEx(
        NULL,
        wc.lpszClassName,
        "Window Title",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        NULL,
        NULL,
        hInstance,
        NULL
        );

    if (!window)
    {
        MessageBox(NULL, "Error creating window",
            "Error", MB_OK | MB_ICONERROR);
    }

    ShowWindow(window, ShowWnd);
    UpdateWindow(window);
    return(window);
}

void messageloop(HWND window)
{
    MSG msg;
    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        switch(msg.message)
        {
        case WM_QUIT:
        {
            running = false;
            DestroyWindow(window);
        } break;
        default:
        {
            TranslateMessage(&msg);    
            DispatchMessage(&msg);  
        } break;
        }
    }
}

int WINAPI WinMain(HINSTANCE instance,
    HINSTANCE prev_instance, 
    LPSTR cmd_line,
    int show_cmd)
{
    HWND window = window_init(instance, show_cmd, WIDTH, HEIGHT, true);
    d3d11_init(instance, window);

    srand((unsigned int)time(0));
#if 0
    int32 num_objects = (rand() % 10) + 1;
    if(num_objects < 3)
    {
        num_objects = 3;
    }
    int32 num_transparent = 2;
    int32 num_opaque = num_objects - num_transparent;

    int32 num_opaque_sphere = 1;
    int32 num_opaque_cube = num_opaque - num_opaque_sphere;

    int32 num_transparent_sphere = 1;
    int32 num_transparent_cube = num_transparent - num_transparent_sphere;
#endif

    int32 num_objects = 8;
    int32 num_transparent = 4;
    int32 num_opaque = 4;
    int32 num_opaque_sphere = 2;
    int32 num_opaque_cube = 2;
    int32 num_transparent_sphere = 2;
    int32 num_transparent_cube = 2;

    Sphere sphere = {};
    build_smooth_sphere(&sphere);

    Shape *opaques;
    opaques = (Shape *)VirtualAlloc(0, num_opaque*sizeof(Shape), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Shape *transparents;
    transparents = (Shape *)VirtualAlloc(0, num_transparent*sizeof(Shape), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Object_Lists object_lists = {};
    object_lists.opaque_objects = opaques;
    object_lists.opaque_size = num_opaque;
    object_lists.transparent_objects = transparents;
    object_lists.transparent_size = num_transparent;
    object_lists.num_objects = num_objects;

    init_shape(object_lists.opaque_objects, num_opaque_cube, num_opaque_sphere);
    init_shape(object_lists.transparent_objects, num_transparent_cube, num_transparent_sphere);
    init_coords(object_lists.opaque_objects, num_opaque, true);
    init_coords(object_lists.transparent_objects, num_transparent, false);

    scene_init();

    Texture_Info texture_info[10];
    load_textures(texture_info);
    attach_textures(object_lists.opaque_objects, object_lists.opaque_size, texture_info);
    attach_textures(object_lists.transparent_objects, object_lists.transparent_size, texture_info);

    running = true;
    while(running)
    {
        messageloop(window);
        frame_count++;
        if(get_time() > 1.0f)
        {
            fps = frame_count;
            frame_count = 0;
            start_timer();
        }
        frame_time = get_frame_time();
        update_and_render(&object_lists, &sphere, frame_time);
    }
    
    return(0);
}
