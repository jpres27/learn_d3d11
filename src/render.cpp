#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dinput8")
#pragma comment (lib, "dxguid")
#pragma comment (lib, "dxgi")


#include <stdint.h>
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dxgidebug.h>
#include <DirectXMath.h>
#include <dinput.h>

#include <time.h>

#include "render.h"
#include "utils.cpp"
#include "geometry.cpp"

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

ID3D11VertexShader *skymap_vs;
ID3D11PixelShader *skymap_ps;

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

XMVECTOR default_forward = XMVectorSet(0.0f,0.0f,1.0f, 0.0f);
XMVECTOR default_right = XMVectorSet(1.0f,0.0f,0.0f, 0.0f);
XMVECTOR cam_forward = XMVectorSet(0.0f,0.0f,1.0f, 0.0f);
XMVECTOR cam_right = XMVectorSet(1.0f,0.0f,0.0f, 0.0f);

XMMATRIX cam_rotation_matrix;
XMMATRIX ground_world;

real32 move_left_right = 0.0f;
real32 move_back_forward = 0.0f;
real32 cam_yaw = 0.0f;
real32 cam_pitch = 0.0f;

ID3D11DepthStencilState *ds_less_equal;
ID3D11BlendState* transparency;
ID3D11RasterizerState *cull_none;
ID3D11RasterizerState *ccw_cull;
ID3D11RasterizerState *cw_cull;

IDirectInputDevice8* di_keyboard;
IDirectInputDevice8* di_mouse;
DIMOUSESTATE mouse_last_state;
LPDIRECTINPUT8 direct_input;

real64 rotation_state = 0.01f;

Light light = {};

// TODO: Setup a way to make rendering calls outside of the object lists for skybox and ground ... 

// TODO: Make shader loading not ad hoc, once it makes sense to do so
#include "d3d11_vshader.h"
#include "d3d11_pshader.h"
#include "skymap_vshader.h"
#include "skymap_pshader.h"

#include "load_mesh.cpp"
#include "load_texture.cpp"

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

void init_direct_input(HINSTANCE instance, HWND window)
{
    HRESULT hr;
    hr = DirectInput8Create(instance,
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        (void**)&direct_input,
        NULL); 

    hr = direct_input->CreateDevice(GUID_SysKeyboard,
        &di_keyboard,
        NULL);

    hr = direct_input->CreateDevice(GUID_SysMouse,
        &di_mouse,
        NULL);

    hr = di_keyboard->SetDataFormat(&c_dfDIKeyboard);
    hr = di_keyboard->SetCooperativeLevel(window, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

    hr = di_mouse->SetDataFormat(&c_dfDIMouse);
    hr = di_mouse->SetCooperativeLevel(window, DISCL_EXCLUSIVE | DISCL_NOWINKEY | DISCL_FOREGROUND);
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

void init_coords(Shape *shapes, int32 size, int32 positive)
{
    for(int i = 0; i < size; ++i)
    {
        if(positive == 1)
        {
            shapes[i].x_coord = ((real32)(i)+1.1f) * 3;
            shapes[i].y_coord = 0.0f;

        }
        else if(positive == 0)
        {
            shapes[i].x_coord = ((real32)(i)+1.1f) * -3;
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

    hr = device->CreateVertexShader(skymap_vshader, sizeof(skymap_vshader), 0, &skymap_vs);
    hr = device->CreatePixelShader(skymap_pshader, sizeof(skymap_pshader), 0, &skymap_ps);

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

    cam_position = XMVectorSet(0.0f, 3.0f, -16.0f, 0.0f);
    cam_target = XMVectorSet(2.0f, 0.0f, 0.0f, 0.0f);
    cam_up = XMVectorSet(0.0f, 2.0f, 0.0f, 0.0f);
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
    // Should we clear the struct for this last one?
    cmdesc.CullMode = D3D11_CULL_NONE;
    hr = device->CreateRasterizerState(&cmdesc, &cull_none);

    D3D11_DEPTH_STENCIL_DESC dss_desc = {};
    dss_desc.DepthEnable = true;
    dss_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dss_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    device->CreateDepthStencilState(&dss_desc, &ds_less_equal);
}

void update_camera()
{
    cam_rotation_matrix = XMMatrixRotationRollPitchYaw(cam_pitch, cam_yaw, 0);
    cam_target = XMVector3TransformCoord(default_forward, cam_rotation_matrix);
    cam_target = XMVector3Normalize(cam_target);

    XMMATRIX rotate_y_temp_matrix;
    rotate_y_temp_matrix = XMMatrixRotationY(cam_yaw);

    cam_right = XMVector3TransformCoord(default_right, rotate_y_temp_matrix);
    cam_up = XMVector3TransformCoord(cam_up, rotate_y_temp_matrix);
    cam_forward = XMVector3TransformCoord(default_forward, rotate_y_temp_matrix);

    cam_position += move_left_right*cam_right;
    cam_position += move_back_forward*cam_forward;
    move_left_right = 0.0f;
    move_back_forward = 0.0f;

    cam_target = cam_position + cam_target;
    cam_view = XMMatrixLookAtLH(cam_position, cam_target, cam_up);
}

void detect_input(real64 time, HWND window)
{
    DIMOUSESTATE mouse_current_state;

    BYTE keyboardState[256];

    di_keyboard->Acquire();
    di_mouse->Acquire();

    di_mouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouse_current_state);

    di_keyboard->GetDeviceState(sizeof(keyboardState),(LPVOID)&keyboardState);

    if(keyboardState[DIK_ESCAPE] & 0x80)
        PostMessage(window, WM_DESTROY, 0, 0);

    real32 speed = 15.0f * time;

    if(keyboardState[DIK_A] & 0x80)
    {
        move_left_right -= speed;
    }
    if(keyboardState[DIK_D] & 0x80)
    {
        move_left_right += speed;
    }
    if(keyboardState[DIK_W] & 0x80)
    {
        move_back_forward += speed;
    }
    if(keyboardState[DIK_S] & 0x80)
    {
        move_back_forward -= speed;
    }
    if((mouse_current_state.lX != mouse_last_state.lX) || (mouse_current_state.lY != mouse_last_state.lY))
    {
        cam_yaw += mouse_last_state.lX * 0.001f;

        cam_pitch += mouse_current_state.lY * 0.001f;

        mouse_last_state = mouse_current_state;
    }

    update_camera();

    return;
}

void update_and_render(Object_Lists *object_lists, Sphere *sphere, real64 time)
{
    device_context->VSSetShader(vertex_shader, 0, 0);
    device_context->PSSetShader(pixel_shader, 0, 0);

    int32 num_objects = object_lists->opaque_size + object_lists->transparent_size;
    assert(num_objects > 0 && num_objects <= 10);


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
    real32 roty = 1.0f;
#if 0
    for(int32 i = 0; i < object_lists->opaque_size; ++i)
    {
        object_lists->opaque_objects[i].world = XMMatrixIdentity();
        real32 x_translate = 0;
        if(object_lists->opaque_objects[i].shape_type == cube_mesh) 
        {
            x_translate = object_lists->opaque_objects[i].x_coord;
            translation = XMMatrixTranslation(x_translate, 0.0f, 4.0f);
            rotation_axis_y = XMVectorSet(0.0f, roty, 0.0f, 0.0f);
            rotation = XMMatrixRotationAxis(rotation_axis_y, rotation_state);
            roty = roty + 1.0f;
            object_lists->opaque_objects[i].world = translation*rotation;
        }
        else if (object_lists->opaque_objects[i].shape_type == sphere_mesh)
        {
            x_translate = object_lists->opaque_objects[i].x_coord;
            translation = XMMatrixTranslation(x_translate, 0.0f, 0.0f);
            rotation = XMMatrixRotationAxis(rotation_axis_y, -rotation_state);
            roty = roty + 1.0f;
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
            rotation_axis_y = XMVectorSet(0.0f, roty, 0.0f, 0.0f);
            rotation = XMMatrixRotationAxis(rotation_axis_y, rotation_state);
            roty = roty + 1.0f;
            object_lists->transparent_objects[i].world = translation*rotation;
        }
        else if (object_lists->transparent_objects[i].shape_type == sphere_mesh)
        {

            x_translate = object_lists->transparent_objects[i].x_coord;
            translation = XMMatrixTranslation(x_translate, y_translate, 0.0f);
            rotation_axis_y = XMVectorSet(0.0f, roty, 0.0f, 0.0f);  
            rotation = XMMatrixRotationAxis(rotation_axis_y, -rotation_state);
            roty = roty + 1.0f;
            scaling = XMMatrixScaling(1.3f, 1.3f, 1.3f);
            object_lists->transparent_objects[i].world = rotation*scaling*translation;
        }
    }
#endif

    object_lists->opaque_objects[0].world = XMMatrixIdentity();
    scaling = XMMatrixScaling(500.0f, 10.0f, 500.0f);
    translation = XMMatrixTranslation(0.0f, 10.0f, 0.0f);
    object_lists->opaque_objects[0].world = scaling*translation;

    object_lists->opaque_objects[1].world = XMMatrixIdentity();
    scaling = XMMatrixScaling(5.0f, 5.0f, 5.0f);
    translation = XMMatrixTranslation(XMVectorGetX(cam_position), XMVectorGetY(cam_position), XMVectorGetZ(cam_position));
    object_lists->opaque_objects[1].world = scaling*translation;

#if 0
    for(int32 i = 2; i < object_lists->opaque_size; ++i)
    {
        object_lists->opaque_objects[i].world = XMMatrixIdentity();
        real32 x_translate = object_lists->opaque_objects[i].x_coord;
        translation = XMMatrixTranslation(x_translate, 0.0f, 4.0f);
        rotation = XMMatrixRotationAxis(rotation_axis_y, rotation_state);
        object_lists->opaque_objects[i].world = translation*rotation;
    }
    
    for(int32 i = 0; i < object_lists->transparent_size; ++i)
    {
        object_lists->transparent_objects[i].world = XMMatrixIdentity();
        real32 x_translate = object_lists->transparent_objects[i].x_coord;
        translation = XMMatrixTranslation(x_translate, 0.0f, 4.0f);
        rotation = XMMatrixRotationAxis(rotation_axis_y, rotation_state);
        object_lists->transparent_objects[i].world = translation*rotation;
    }
#endif

    device_context->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);

#if DEBUG
    event_grouper->BeginEvent(L"Clear screen");
#endif
    FLOAT color[] = { 0.3f, 0.3f, 0.3f, 1.0f };
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

    device_context->OMSetBlendState(0, 0, 0xFFFFFFFF);
    int32 k = 0;

#if DEBUG
    event_grouper->BeginEvent(L"Render ground");
#endif

    load_ground_mesh();
    Texture_Info ground_texture_info = {};
    load_ground_texture(&ground_texture_info);
    device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &offset, &size);
    device_context->PSSetShaderResources(0, 1, &ground_texture_info.shader_resource_view);
    device_context->PSSetSamplers(0, 1, &ground_texture_info.sampler_state);
    device_context->RSSetState(ccw_cull);
    device_context->DrawIndexed(6, 0, 0);

    ++k;

    load_sphere_mesh(sphere);
    Texture_Info skymap_texture = {};
    load_sky_texture(&skymap_texture);
    offset = ((sizeof(cb_per_object.cube1)*4) / 16) * k;
    device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &offset, &size);
    device_context->PSSetShaderResources(0, 1, &skymap_texture.shader_resource_view);
    device_context->PSSetSamplers(0, 1, &skymap_texture.sampler_state);
    device_context->VSSetShader(skymap_vs, 0, 0);
    device_context->PSSetShader(skymap_ps, 0, 0);
    device_context->OMSetDepthStencilState(ds_less_equal, 0);
    device_context->RSSetState(cull_none);
    device_context->DrawIndexed(360, 0, 0);

    // Reset state
    device_context->VSSetShader(vertex_shader, 0, 0);
    device_context->PSSetShader(pixel_shader, 0, 0);
    device_context->OMSetDepthStencilState(0, 0);


#if DEBUG
    event_grouper->EndEvent();
#endif

#if 0

#if DEBUG
    event_grouper->BeginEvent(L"Render opaque objects");
#endif

    for(int32 i = 0; i < object_lists->opaque_size; ++i)
    {
        if(object_lists->opaque_objects[i].shape_type == cube_mesh)
        {
            load_cube_mesh();
            offset = ((sizeof(cb_per_object.cube1)*4) / 16) * k;
            device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &offset, &size);
            device_context->PSSetShaderResources(0, 1, &object_lists->opaque_objects[i].texture_info.shader_resource_view);
            device_context->PSSetSamplers(0, 1, &object_lists->opaque_objects[i].texture_info.sampler_state);
            device_context->RSSetState(ccw_cull);
            device_context->DrawIndexed(36, 0, 0);
            ++k;
        }
        else if(object_lists->opaque_objects[i].shape_type == sphere_mesh)
        {
            load_sphere_mesh(sphere);
            offset = ((sizeof(cb_per_object.cube1)*4) / 16) * k;
            device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &offset, &size);
            device_context->PSSetShaderResources(0, 1, &object_lists->opaque_objects[i].texture_info.shader_resource_view);
            device_context->PSSetSamplers(0, 1, &object_lists->opaque_objects[i].texture_info.sampler_state);
            device_context->RSSetState(ccw_cull);
            device_context->DrawIndexed(360, 0, 0);
            ++k;
        }
    }
#if DEBUG
    event_grouper->EndEvent();
#endif

#if DEBUG
    event_grouper->BeginEvent(L"Render transparent objects");
#endif
    real32 blend_factor[] = {0.5f, 0.5f, 0.5f, 0.7f};
    device_context->OMSetBlendState(transparency, blend_factor, 0xFFFFFFFF);

    for(int32 i = 0; i < object_lists->transparent_size; ++i)
    {
        assert(k < object_lists->num_objects + 1);
        if(object_lists->transparent_objects[i].shape_type == cube_mesh)
        {
            #if DEBUG
            event_grouper->BeginEvent(L"Render transparent cube");
            #endif
            load_cube_mesh();
            offset = ((sizeof(cb_per_object.cube1)*4) / 16) * k;
            device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &offset, &size);
            device_context->PSSetShaderResources(0, 1, &object_lists->transparent_objects[i].texture_info.shader_resource_view);
            device_context->PSSetSamplers(0, 1, &object_lists->transparent_objects[i].texture_info.sampler_state);
            device_context->RSSetState(ccw_cull);
            device_context->DrawIndexed(36, 0, 0);

            ++k;
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
            offset = ((sizeof(cb_per_object.cube1)*4) / 16) * k;
            device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &offset, &size);
            device_context->PSSetShaderResources(0, 1, &object_lists->transparent_objects[i].texture_info.shader_resource_view);
            device_context->PSSetSamplers(0, 1, &object_lists->transparent_objects[i].texture_info.sampler_state);
            device_context->RSSetState(ccw_cull); 
            device_context->DrawIndexed(360, 0, 0);

            ++k;
            #if DEBUG
            event_grouper->EndEvent();
            #endif
        }
    }
#if DEBUG
    event_grouper->EndEvent();
#endif

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
    init_direct_input(instance, window);

    srand((unsigned int)time(0));

    int32 num_objects = (rand() % 10) + 1;
    if(num_objects < 3)
    {
        num_objects = 3;
    }
    int32 num_transparent = (rand() % num_objects) + 1;
    int32 num_opaque = num_objects - num_transparent;

    int32 num_opaque_sphere = (rand() % num_opaque) + 1;
    int32 num_opaque_cube = num_opaque - num_opaque_sphere;

    int32 num_transparent_sphere = (rand() % num_transparent) + 1;
    int32 num_transparent_cube = num_transparent - num_transparent_sphere;

#if 0
    int32 num_objects = 8;
    int32 num_transparent = 4;
    int32 num_opaque = 4;
    int32 num_opaque_sphere = 2;
    int32 num_opaque_cube = 2;
    int32 num_transparent_sphere = 2;
    int32 num_transparent_cube = 2;
#endif

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
    init_coords(object_lists.opaque_objects, num_opaque, 1);
    init_coords(object_lists.transparent_objects, num_transparent, 0);

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
        detect_input(frame_time, window);
        update_and_render(&object_lists, &sphere, frame_time);
    }
    
    return(0);
}
