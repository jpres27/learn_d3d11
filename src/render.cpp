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

#include "strings.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
// #define STBI_ONLY_PNG

#define internal static
#define local_persist static
#define global_variable static

#define assert(expression) if(expression == false) {*(int *)0 = 0;}
#define AssertHR(hr) assert(SUCCEEDED(hr))
#define array_count(array) (sizeof(array) / sizeof((array)[0]))

typedef int32_t int32;
typedef int64_t int64;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int32_t bool32;
typedef float real32;
typedef double real64;
typedef real32 RGBA[4];

struct Vertex
{
    Vertex(){}
    Vertex(float x, float y, float z,
        float u, float v)
        : pos(x,y,z), tex_coord(u, v){}

    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 tex_coord;
};

D3D11_INPUT_ELEMENT_DESC layout[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },  
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },  
};

IDXGISwapChain1* swap_chain;
ID3D11Device1* device;
ID3D11DeviceContext1* device_context;
ID3D11RenderTargetView* render_target_view;

ID3D11Buffer* sq_index_buffer;
ID3D11Buffer* sq_vert_buffer;
ID3D11DepthStencilView* depth_stencil_view;
ID3D11Texture2D* depth_stencil_buffer;

ID3D11VertexShader* vertex_shader;
ID3D11PixelShader* pixel_shader;
ID3D11InputLayout* vertex_layout;

ID3D11Buffer* cb_per_object_buffer;
ID3D11Buffer* cb_per_frame_buffer;

struct Texture_Info
{
    ID3D11Texture2D *texture;
    ID3D11ShaderResourceView *shader_resource_view;
    ID3D11SamplerState *sampler_state;
};

real32 red = 0.0f;
real32 green = 0.0f;
real32 blue = 0.0f;
int colormodr = 1;
int colormodg = 1;
int colormodb = 1;

LPCTSTR WndClassName = "firstwindow";
HWND window = NULL;

const int WIDTH  = 1400;
const int HEIGHT = 1050;

DirectX::XMMATRIX wvp;
DirectX::XMMATRIX cam_view;
DirectX::XMMATRIX cam_projection;
DirectX::XMVECTOR cam_position;
DirectX::XMVECTOR cam_target;
DirectX::XMVECTOR cam_up;

real32 rotation_state = 0.01f;

// Constant buffers consist of shader constants which are 16 bytes (4*32-buit components)
// and when using offsets to bind various parts of a CB it must be a multiple of 16
// constants in the range [0, 4096]
struct CB_Per_Object
{
    DirectX::XMMATRIX cube1; // 64 bytes
    DirectX::XMMATRIX pad1;
    DirectX::XMMATRIX pad2;
    DirectX::XMMATRIX pad3; //256 bytes (16 shader constants)

    DirectX::XMMATRIX cube2;
    DirectX::XMMATRIX pad4;
    DirectX::XMMATRIX pad5;
    DirectX::XMMATRIX pad6; // 512 bytes

    DirectX::XMMATRIX cube3;
    DirectX::XMMATRIX pad7;
    DirectX::XMMATRIX pad8;
    DirectX::XMMATRIX pad9;

    DirectX::XMMATRIX cube4;
    DirectX::XMMATRIX pad10;
    DirectX::XMMATRIX pad11;
    DirectX::XMMATRIX pad12; // 1024 bytes

    DirectX::XMMATRIX cube5;
    DirectX::XMMATRIX pad13;
    DirectX::XMMATRIX pad14;
    DirectX::XMMATRIX pad15;

    DirectX::XMMATRIX cube6;
    DirectX::XMMATRIX pad16;
    DirectX::XMMATRIX pad17;
    DirectX::XMMATRIX pad18;

    DirectX::XMMATRIX cube7;
    DirectX::XMMATRIX pad19;
    DirectX::XMMATRIX pad20;
    DirectX::XMMATRIX pad21;

    DirectX::XMMATRIX cube8;
    DirectX::XMMATRIX pad22;
    DirectX::XMMATRIX pad23;
    DirectX::XMMATRIX pad24;

    DirectX::XMMATRIX cube9;
    DirectX::XMMATRIX pad25;
    DirectX::XMMATRIX pad26;
    DirectX::XMMATRIX pad27;

    DirectX::XMMATRIX cube10;
    DirectX::XMMATRIX pad28;
    DirectX::XMMATRIX pad29;
    DirectX::XMMATRIX pad30; // 2560 bytes
};

struct CB_Per_Frame 
{
    DirectX::XMMATRIX view;
    DirectX::XMMATRIX projection;
};

LRESULT CALLBACK WndProc(HWND window,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
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

internal real32 find_dist_from_cam(DirectX::XMMATRIX cube)
{
    DirectX::XMVECTOR cube_pos = DirectX::XMVectorZero();
    cube_pos = DirectX::XMVector2TransformCoord(cube_pos, cube);

    real32 dist_x = DirectX::XMVectorGetX(cube_pos) - DirectX::XMVectorGetX(cam_position);
    real32 dist_y = DirectX::XMVectorGetY(cube_pos) - DirectX::XMVectorGetY(cam_position);
    real32 dist_z = DirectX::XMVectorGetZ(cube_pos) - DirectX::XMVectorGetZ(cam_position);

    real32 cube_dist = dist_x*dist_x + dist_y*dist_y + dist_z*dist_z;
    return(cube_dist);
}

void window_init(HINSTANCE hInstance,
    int ShowWnd,
    int width, int height,
    bool32 windowed)
{
    WNDCLASSEX wc;

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = NULL;
    wc.cbWndExtra = NULL;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = WndClassName;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, "Error registering class",    
            "Error", MB_OK | MB_ICONERROR);
    }

    window = CreateWindowEx(
        NULL,
        WndClassName,
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
}

void d3d11_init(HINSTANCE hInstance)
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

void scene_init(uint32 num_cubes, Texture_Info *texture_infos)
{
    HRESULT hr;

    #include "d3d11_vshader.h"
    #include "d3d11_pshader.h"

    hr = device->CreateVertexShader(d3d11_vshader, sizeof(d3d11_vshader), 0, &vertex_shader);
    hr = device->CreatePixelShader(d3d11_pshader, sizeof(d3d11_pshader), 0, &pixel_shader);

    device_context->VSSetShader(vertex_shader, 0, 0);
    device_context->PSSetShader(pixel_shader, 0, 0);

        Vertex v[] =
        {
            // Front Face
            Vertex(-1.0f, -1.0f, -1.0f, 0.0f, 1.0f),
            Vertex(-1.0f,  1.0f, -1.0f, 0.0f, 0.0f),
            Vertex( 1.0f,  1.0f, -1.0f, 1.0f, 0.0f),
            Vertex( 1.0f, -1.0f, -1.0f, 1.0f, 1.0f),
    
            // Back Face
            Vertex(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f),
            Vertex( 1.0f, -1.0f, 1.0f, 0.0f, 1.0f),
            Vertex( 1.0f,  1.0f, 1.0f, 0.0f, 0.0f),
            Vertex(-1.0f,  1.0f, 1.0f, 1.0f, 0.0f),
    
            // Top Face
            Vertex(-1.0f, 1.0f, -1.0f, 0.0f, 1.0f),
            Vertex(-1.0f, 1.0f,  1.0f, 0.0f, 0.0f),
            Vertex( 1.0f, 1.0f,  1.0f, 1.0f, 0.0f),
            Vertex( 1.0f, 1.0f, -1.0f, 1.0f, 1.0f),
    
            // Bottom Face
            Vertex(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f),
            Vertex( 1.0f, -1.0f, -1.0f, 0.0f, 1.0f),
            Vertex( 1.0f, -1.0f,  1.0f, 0.0f, 0.0f),
            Vertex(-1.0f, -1.0f,  1.0f, 1.0f, 0.0f),
    
            // Left Face
            Vertex(-1.0f, -1.0f,  1.0f, 0.0f, 1.0f),
            Vertex(-1.0f,  1.0f,  1.0f, 0.0f, 0.0f),
            Vertex(-1.0f,  1.0f, -1.0f, 1.0f, 0.0f),
            Vertex(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f),
    
            // Right Face
            Vertex( 1.0f, -1.0f, -1.0f, 0.0f, 1.0f),
            Vertex( 1.0f,  1.0f, -1.0f, 0.0f, 0.0f),
            Vertex( 1.0f,  1.0f,  1.0f, 1.0f, 0.0f),
            Vertex( 1.0f, -1.0f,  1.0f, 1.0f, 1.0f),
        };
    
        DWORD indices[] = {
            // Front Face
            0,  1,  2,
            0,  2,  3,
    
            // Back Face
            4,  5,  6,
            4,  6,  7,
    
            // Top Face
            8,  9, 10,
            8, 10, 11,
    
            // Bottom Face
            12, 13, 14,
            12, 14, 15,
    
            // Left Face
            16, 17, 18,
            16, 18, 19,
    
            // Right Face
            20, 21, 22,
            20, 22, 23
        };

    D3D11_BUFFER_DESC index_buffer_desc = {};
    index_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    index_buffer_desc.ByteWidth = sizeof(DWORD)*12*3;
    index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    index_buffer_desc.CPUAccessFlags = 0;
    index_buffer_desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA index_init_data ={};
    index_init_data.pSysMem = indices;

    hr = device->CreateBuffer(&index_buffer_desc, &index_init_data, &sq_index_buffer);
    AssertHR(hr);
    device_context->IASetIndexBuffer(sq_index_buffer, DXGI_FORMAT_R32_UINT, 0);

    D3D11_BUFFER_DESC vert_buffer_desc = {};
    vert_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    vert_buffer_desc.ByteWidth = sizeof(Vertex)*24;
    vert_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vert_buffer_desc.CPUAccessFlags = 0;
    vert_buffer_desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA vert_buffer_data = {};
    vert_buffer_data.pSysMem = v;

    hr = device->CreateBuffer(&vert_buffer_desc, &vert_buffer_data, &sq_vert_buffer);
    AssertHR(hr);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    device_context->IASetVertexBuffers(0, 1, &sq_vert_buffer, &stride, &offset);

    hr = device->CreateInputLayout(layout, ARRAYSIZE(layout), d3d11_vshader, sizeof(d3d11_vshader), &vertex_layout);
    AssertHR(hr);

    device_context->IASetInputLayout(vertex_layout); 

    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

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

    cam_position = DirectX::XMVectorSet(10.0f, 3.0f, -35.0f, 0.0f);
    cam_target = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    cam_up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    cam_view = DirectX::XMMatrixLookAtLH(cam_position, cam_target, cam_up);
    cam_projection = DirectX::XMMatrixPerspectiveFovLH(0.4f*3.14f, (real32)WIDTH/(real32)HEIGHT, 1.0f, 1000.0f);

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

        hr = device->CreateTexture2D(&texture_desc, &subresource_data, &texture_infos[i].texture);
        AssertHR(hr);
        hr = device->CreateShaderResourceView(texture_infos[i].texture, 0, &texture_infos[i].shader_resource_view);
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
        
        hr = device->CreateSamplerState(&sampler_desc, &texture_infos[i].sampler_state);
        AssertHR(hr);
    }
}

void scene_update(uint32 num_cubes, DirectX::XMMATRIX *cubes)
{
    assert(num_cubes > 0 && num_cubes < 11);

    rotation_state += 0.0005f;
    if (rotation_state > 6.28f)
    {
        rotation_state = 0.0f;
    }

#if 0
    cube_1_world = DirectX::XMMatrixIdentity();
    DirectX::XMVECTOR rotation_axis_1 = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    rotation = DirectX::XMMatrixRotationAxis(rotation_axis_1, rotation_state);
    translation = DirectX::XMMatrixTranslation(0.0f, 4.0f, 0.0f);
    cube_1_world = translation*rotation;

    cube_2_world = DirectX::XMMatrixIdentity();
    DirectX::XMVECTOR rotation_axis_2 = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    rotation = DirectX::XMMatrixRotationAxis(rotation_axis_2, -rotation_state);
    scaling = DirectX::XMMatrixScaling(1.3f, 1.3f, 1.3f);
    cube_2_world = rotation*scaling;
#endif

    DirectX::XMMATRIX translation;
    DirectX::XMVECTOR rotation_axis = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

    for(uint32 i = 0; i < num_cubes; ++i)
    {
        cubes[i] = DirectX::XMMatrixIdentity();
        if(i % 2 == 0) 
        {
            real32 x_translate = (i*2) + 4.0f;
            translation = DirectX::XMMatrixTranslation(x_translate, 0.0f, 0.0f);
            rotation_axis = DirectX::XMVectorSet(x_translate, 0.0f, 0.0f, 0.0f);
        }
        else
        {
            real32 y_translate = (i*2) + 4.0f;
            translation = DirectX::XMMatrixTranslation(0.0f, y_translate, 0.0f);
            rotation_axis = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        }
        DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationAxis(rotation_axis, rotation_state);
        cubes[i] = translation*rotation;
    }
}

void scene_render(uint32 num_cubes, DirectX::XMMATRIX *cubes, Texture_Info *texture_infos)
{
    device_context->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);

    real32 bgColor[4] = {(0.0f, 0.0f, 0.0f, 0.0f)};
    device_context->ClearRenderTargetView(render_target_view, bgColor);
    device_context->ClearDepthStencilView(depth_stencil_view, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

    CB_Per_Frame cb_per_frame = {};
    cb_per_frame.view = DirectX::XMMatrixTranspose(cam_view);
    cb_per_frame.projection = DirectX::XMMatrixTranspose(cam_projection);
    device_context->UpdateSubresource(cb_per_frame_buffer, 0, 0, &cb_per_frame, 0, 0);
    device_context->VSSetConstantBuffers(1, 1, &cb_per_frame_buffer);

    CB_Per_Object cb_per_object = {};
    DirectX::XMMATRIX *cbpo_index = (DirectX::XMMATRIX *)&cb_per_object.cube1;

    for(uint32 i = 0; i < num_cubes; ++i)
    {
            *cbpo_index = DirectX::XMMatrixTranspose(cubes[i]);
            cbpo_index = cbpo_index + 4;
    }
    device_context->UpdateSubresource(cb_per_object_buffer, 0, 0, &cb_per_object, 0, 0);

    UINT offset = 0;
    UINT size = (sizeof(cb_per_object.cube1)*4) / 16;
    for(uint32 i = 0; i < num_cubes; ++i)
    {
        offset = ((sizeof(cb_per_object.cube1)*4) / 16) * i;
        device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &offset, &size);
        device_context->PSSetShaderResources(0, 1, &texture_infos[i].shader_resource_view);
        device_context->PSSetSamplers(0, 1, &texture_infos[i].sampler_state);
        device_context->DrawIndexed(36, 0, 0);
    }

    swap_chain->Present(0, 0);
}

int messageloop(uint32 num_cubes, DirectX::XMMATRIX *cubes, Texture_Info *texture_infos)
{
    MSG msg;
    ZeroMemory(&msg, sizeof(MSG));
    while(true)
    {
        BOOL PeekMessageL( 
            LPMSG lpMsg,
            HWND window,
            UINT wMsgFilterMin,
            UINT wMsgFilterMax,
            UINT wRemoveMsg
            );

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;
            TranslateMessage(&msg);    
            DispatchMessage(&msg);
        }
        else{
            scene_update(num_cubes, cubes);
            scene_render(num_cubes, cubes, texture_infos);
            }
    }
    return msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance, 
    LPSTR lpCmdLine,
    int nShowCmd)
{

    srand((unsigned int)time(0));
    uint32 num_cubes = (rand() % 10) + 1;

    DirectX::XMMATRIX *cubes;
    cubes = (DirectX::XMMATRIX *)VirtualAlloc(0, num_cubes*sizeof(DirectX::XMMATRIX), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

    // TODO: Fix the state madness
    Texture_Info texture_infos[10];


    window_init(hInstance, nShowCmd, WIDTH, HEIGHT, true);
    d3d11_init(hInstance);
    scene_init(num_cubes, texture_infos);
    messageloop(num_cubes, cubes, texture_infos);
    
    return 0;
}
