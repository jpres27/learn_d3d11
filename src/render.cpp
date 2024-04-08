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

global_variable bool32 running;

IDXGISwapChain1* swap_chain;
ID3D11Device1* device;
ID3D11DeviceContext1* device_context;
ID3D11RenderTargetView* render_target_view;

ID3D11Buffer* cube_index_buffer;
ID3D11Buffer* cube_vert_buffer;

ID3D11Buffer* sphere_index_buffer;
ID3D11Buffer* sphere_vert_buffer;

ID3D11DepthStencilView* depth_stencil_view;
ID3D11Texture2D* depth_stencil_buffer;

ID3D11VertexShader* vertex_shader;
ID3D11PixelShader* pixel_shader;
ID3D11InputLayout* vertex_layout;

ID3D11Buffer* cb_per_object_buffer;
ID3D11Buffer* cb_per_frame_buffer;

real32 red = 0.0f;
real32 green = 0.0f;
real32 blue = 0.0f;
int colormodr = 1;
int colormodg = 1;
int colormodb = 1;

const int WIDTH  = 1400;
const int HEIGHT = 1050;

DirectX::XMMATRIX wvp;
DirectX::XMMATRIX cam_view;
DirectX::XMMATRIX cam_projection;
DirectX::XMVECTOR cam_position;
DirectX::XMVECTOR cam_target;
DirectX::XMVECTOR cam_up;

ID3D11BlendState* transparency;
ID3D11RasterizerState *ccw_cull;
ID3D11RasterizerState *cw_cull;

real32 rotation_state = 0.01f;
real32 translation_state = -6.27f;

Light light = {};

// TODO: Make shader loading not ad hoc, once it makes sense to do so
#include "d3d11_vshader.h"
#include "d3d11_pshader.h"

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

    cam_position = DirectX::XMVectorSet(0.0f, 30.0f, -20.0f, 0.0f);
    cam_target = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    cam_up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    cam_view = DirectX::XMMatrixLookAtLH(cam_position, cam_target, cam_up);
    cam_projection = DirectX::XMMatrixPerspectiveFovLH(0.4f*3.14f, (real32)WIDTH/(real32)HEIGHT, 1.0f, 1000.0f);

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

    D3D11_BUFFER_DESC sphere_index_bd = {};
    sphere_index_bd.Usage = D3D11_USAGE_DEFAULT;
    sphere_index_bd.ByteWidth = sizeof(DWORD)*12*3;
    sphere_index_bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    sphere_index_bd.CPUAccessFlags = 0;
    sphere_index_bd.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA sphere_index_init_data ={};
    sphere_index_init_data.pSysMem = indices;

    hr = device->CreateBuffer(&sphere_index_bd, &sphere_index_init_data, &cube_index_buffer);
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

    D3D11_BUFFER_DESC sphere_index_bd = {};
    sphere_index_bd.Usage = D3D11_USAGE_DEFAULT;
    sphere_index_bd.ByteWidth = sizeof(DWORD)*360;
    sphere_index_bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    sphere_index_bd.CPUAccessFlags = 0;
    sphere_index_bd.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA sphere_index_init_data ={};
    sphere_index_init_data.pSysMem = sphere->indices;

    hr = device->CreateBuffer(&sphere_index_bd, &sphere_index_init_data, &sphere_index_buffer);
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

void load_textures(Shape *shapes)
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

        hr = device->CreateTexture2D(&texture_desc, &subresource_data, &shapes[i].texture_info.texture);
        AssertHR(hr);
        hr = device->CreateShaderResourceView(shapes[i].texture_info.texture, 0, &shapes[i].texture_info.shader_resource_view);
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
        
        hr = device->CreateSamplerState(&sampler_desc, &shapes[i].texture_info.sampler_state);
        AssertHR(hr);
    }
}

void update_and_render(Shape *shapes, int32 num_objects, Sphere *sphere)
{
    assert(num_objects > 0 && num_objects < 11);


    light.dir = DirectX::XMFLOAT3(0.25f, 0.5f, -1.0f);
    light.ambient = DirectX::XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
    light.diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    rotation_state += 0.0005f;
    if (rotation_state > 6.28f)
    {
        rotation_state = 0.0f;
    }

    if(translation_state < -4.5f)
    {
        translation_state += 0.001f;
    }
    else if(translation_state > 4.5f)
    {
        translation_state -= 0.001f;
    }

    DirectX::XMMATRIX translation;
    DirectX::XMMATRIX rotation;
    DirectX::XMMATRIX scaling;
    DirectX::XMVECTOR rotation_axis = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

    for(int32 i = 0; i < num_objects; ++i)
    {
        shapes[i].world = DirectX::XMMatrixIdentity();
        real32 x_translate = 0;
        real32 y_translate = 0;
        if(shapes[i].shape_type == cube_mesh) 
        {
            if(i % 2 == 0)
            {
                x_translate = -(translation_state + shapes[i].x_coord);
                y_translate = translation_state + shapes[i].y_coord;
            }
            else
            {
                x_translate = translation_state + shapes[i].x_coord;
                y_translate = translation_state + shapes[i].y_coord;
            }
            translation = DirectX::XMMatrixTranslation(x_translate, y_translate, 4.0f);
            rotation_axis = DirectX::XMVectorSet(0.0f, y_translate, 0.0f, 0.0f);
            rotation = DirectX::XMMatrixRotationAxis(rotation_axis, rotation_state);
            shapes[i].world = translation*rotation;
        }
        else if (shapes[i].shape_type == sphere_mesh)
        {
            if(i % 2 == 0)
            {
                x_translate = (translation_state + shapes[i].x_coord);
                y_translate = -(translation_state + shapes[i].y_coord);
            }
            else
            {
                x_translate = -(translation_state + shapes[i].x_coord);
                y_translate = translation_state + shapes[i].y_coord;
            }
            translation = DirectX::XMMatrixTranslation(x_translate, y_translate, 0.0f);
            rotation_axis = DirectX::XMVectorSet(0.0f, y_translate, 0.0f, 0.0f);
            rotation = DirectX::XMMatrixRotationAxis(rotation_axis, -rotation_state);
            scaling = DirectX::XMMatrixScaling(1.3f, 1.3f, 1.3f);
            shapes[i].world = rotation*scaling*translation;
        }
    }

    device_context->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);

    FLOAT color[] = { 0.392f, 0.584f, 0.929f, 1.f };
    real32 bgColor[4] = {(1.0f, 1.0f, 0.0f, 0.0f)};
    device_context->ClearRenderTargetView(render_target_view, color);
    device_context->ClearDepthStencilView(depth_stencil_view, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

    real32 blend_factor[] = {0.75f, 0.75f, 0.75f, 1.0f};
    device_context->OMSetBlendState(0, 0, 0xFFFFFFFF);
    device_context->OMSetBlendState(transparency, blend_factor, 0xFFFFFFFF);

    for(int i = 0; i < num_objects; ++i)
    {
        shapes[i].dist_from_cam = find_dist_from_cam(shapes[i].world);
    }

    sort_object_dist(shapes, num_objects);

    CB_Per_Frame cb_per_frame = {};
    cb_per_frame.view = DirectX::XMMatrixTranspose(cam_view);
    cb_per_frame.projection = DirectX::XMMatrixTranspose(cam_projection);
    cb_per_frame.light = light;
    device_context->UpdateSubresource(cb_per_frame_buffer, 0, 0, &cb_per_frame, 0, 0);
    device_context->VSSetConstantBuffers(1, 1, &cb_per_frame_buffer);
    device_context->PSSetConstantBuffers(1, 1, &cb_per_frame_buffer);

    CB_Per_Object cb_per_object = {};
    DirectX::XMMATRIX *cbpo_index = (DirectX::XMMATRIX *)&cb_per_object.cube1;

    for(int32 i = 0; i < num_objects; ++i)
    {
            *cbpo_index = DirectX::XMMatrixTranspose(shapes[i].world);
            cbpo_index += 4;
    }
    device_context->UpdateSubresource(cb_per_object_buffer, 0, 0, &cb_per_object, 0, 0);

    load_cube_mesh();
    UINT offset = 0;
    UINT size = (sizeof(cb_per_object.cube1)*4) / 16;
  

    for(int32 i = 0; i < num_objects; ++i)
    {
        if(shapes[i].shape_type == cube_mesh)
        {
            load_cube_mesh();
            offset = ((sizeof(cb_per_object.cube1)*4) / 16) * i;
            device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &offset, &size);
            device_context->PSSetShaderResources(0, 1, &shapes[i].texture_info.shader_resource_view);
            device_context->PSSetSamplers(0, 1, &shapes[i].texture_info.sampler_state);
            device_context->RSSetState(ccw_cull);
            device_context->DrawIndexed(36, 0, 0);
            device_context->RSSetState(cw_cull);
            device_context->DrawIndexed(36, 0, 0);
        }
        else if(shapes[i].shape_type == sphere_mesh)
        {
            load_sphere_mesh(sphere);
            offset = ((sizeof(cb_per_object.cube1)*4) / 16) * i;
            device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &offset, &size);
            device_context->PSSetShaderResources(0, 1, &shapes[i].texture_info.shader_resource_view);
            device_context->PSSetSamplers(0, 1, &shapes[i].texture_info.sampler_state);
            device_context->RSSetState(ccw_cull);
            device_context->DrawIndexed(360, 0, 0);
            device_context->RSSetState(cw_cull);
            device_context->DrawIndexed(360, 0, 0);
        }
    }


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
    int32 num_objects = (rand() % 10) + 1;
    if(num_objects < 5)
    {
        num_objects = 5;
    }
    int32 num_spheres = (rand() % num_objects) + 1;
    int32 num_cubes = num_objects - num_spheres;

    // DirectX::XMMATRIX *cubes;
    // cubes = (DirectX::XMMATRIX *)VirtualAlloc(0, num_objects*sizeof(DirectX::XMMATRIX), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

    Sphere sphere = {};
    build_smooth_sphere(&sphere);

    Shape *shapes;
    shapes = (Shape *)VirtualAlloc(0, num_objects*sizeof(Shape), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

    for(int i = 0; i < num_cubes; ++i)
    {
        shapes[i].shape_type = cube_mesh;
        if(i % 2 == 0)
        {
            shapes[i].x_coord = i * -(real32)(rand() % 8);
            shapes[i].y_coord = i * (real32)(rand() % 8);
        }
        else 
        {
            shapes[i].x_coord = i * -(real32)(rand() % 8);
            shapes[i].y_coord = i * -(real32)(rand() % 8);
        }
    }
    for(int i = num_cubes; i < num_objects; ++i)
    {
        shapes[i].shape_type = sphere_mesh;
        if(i % 2 == 0)
        {
            shapes[i].x_coord = i * (real32)(rand() % 8);
            shapes[i].y_coord = i * -(real32)(rand() % 8);
        }
        else 
        {
            shapes[i].x_coord = i * (real32)(rand() % 8);
            shapes[i].y_coord = i * (real32)(rand() % 8);
        }

    }

#if 0 
    bool32 shuffled_objects[10];
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
        shuffled_objects[i] = objects[j];
        visited[j] = true;
    }  
#endif

    scene_init();
    load_textures(shapes);

    running = true;
    while(running)
    {
        messageloop(window);
        update_and_render(shapes, num_objects, &sphere);
    }
    
    return(0);
}
