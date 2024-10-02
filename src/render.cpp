#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dinput8")
#pragma comment (lib, "dxguid")
#pragma comment (lib, "dxgi")
#pragma comment (lib, "mfplat")
#pragma comment (lib, "mfreadwrite")

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dxgidebug.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <dinput.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#define IMGUI_IMPLEMENTATION
#include "dear_imgui_single.h"
#include "../external/dearimgui/backends/imgui_impl_dx11.h"
#include "../external/dearimgui/backends/imgui_impl_dx11.cpp"
#include "../external/dearimgui/backends/imgui_impl_win32.h"
#include "../external/dearimgui/backends/imgui_impl_win32.cpp"

#include <time.h>

#include "render.h"
#include "platform.h"
#include "win32_wasapi.h"
#include "utils.cpp"
#include "geometry.cpp"

using namespace DirectX;

global_variable bool32 running;

real64 counts_per_second = 0.0;
int64 counter_start = 0;
int32 frame_count = 0;
int32 fps = 0;
int64 frame_time_old = 0;
real64 frame_time;

global_variable WINDOWPLACEMENT prev_window_position = {sizeof(prev_window_position)};

IDXGISwapChain1* swap_chain;
ID3D11Device1* device;
ID3D11DeviceContext1* device_context;
ID3D11RenderTargetView* offscreen_texture_rtv;
ID3D11RenderTargetView* render_target_view;
ID3D11ShaderResourceView* offscreen_texture_srv;
ID3D11UnorderedAccessView* backbuffer_uav;

ID3DUserDefinedAnnotation *event_grouper;

ID3D11Buffer* cube_index_buffer;
ID3D11Buffer* cube_vert_buffer;
ID3D11Buffer* sphere_index_buffer;
ID3D11Buffer* sphere_vert_buffer;
ID3D11Buffer* ground_index_buffer;
ID3D11Buffer* ground_vert_buffer;
ID3D11Buffer* debug_vert_buffer;

ID3D11DepthStencilView* depth_stencil_view;
ID3D11Texture2D* depth_stencil_buffer;

ID3D11VertexShader* vertex_shader;
ID3D11PixelShader* pixel_shader;
ID3D11InputLayout* vertex_layout;
ID3D11VertexShader *skymap_vs;
ID3D11PixelShader *skymap_ps;
ID3D11ComputeShader *postprocess_shader;

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
ID3D11RasterizerState *wireframe;

IDirectInputDevice8* di_keyboard;
IDirectInputDevice8* di_mouse;
DIMOUSESTATE mouse_last_state;
LPDIRECTINPUT8 direct_input;

real64 rotation_state = 0.01f;

Light light = {};

bool32 show_num_rendered;

// TODO: Make shader loading not ad hoc, once it makes sense to do so
#include "d3d11_vshader.h"
#include "d3d11_pshader.h"
#include "skymap_vshader.h"
#include "skymap_pshader.h"
#include "postprocessing_shader.h"

#include "cube.h"
#include "load_mesh.cpp"
#include "load_texture.cpp"
#include "timing.cpp"
#include "debug_draw.cpp"
#include "sound.cpp"

// TODO: Setup text rendering

internal void init_arena(Memory_Arena *arena, memory_index size, uint8_t *base)
{
    arena->size = size;
    arena->base = base;
    arena->used = 0;
}

// NOTE: Based on Raymond Chen's blog about fullscreen toggling
internal void toggle_fullscreen(HWND window)
{
  DWORD style = GetWindowLong(window, GWL_STYLE);
  if (style & WS_OVERLAPPEDWINDOW) 
  {
    MONITORINFO monitor_info = { sizeof(monitor_info) };
    if (GetWindowPlacement(window, &prev_window_position) &&
        GetMonitorInfo(MonitorFromWindow(window,
                       MONITOR_DEFAULTTOPRIMARY), &monitor_info)) 
    {
      SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
      SetWindowPos(window, HWND_TOP,
                   monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                   monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                   monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                   SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
  }
  else 
  {
    SetWindowLong(window, GWL_STYLE,
                  style | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(window, &prev_window_position);
    SetWindowPos(window, 0, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  }
}

internal Window_Dimensions get_window_dimensions(HWND window)
{
    Window_Dimensions result;
    RECT client_rect;
    GetClientRect(window, &client_rect);
    result.width = client_rect.right - client_rect.left;
    result.height = client_rect.bottom - client_rect.top;

    return (result);
}

// add to WndProc
    // HANDLE_MSG(window, WM_LBUTTONUP, toggle_fullscreen);

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

void calculate_bounding_box(Vertex *vertices, int size, XMFLOAT3 *b_min, XMFLOAT3 *b_max)
{
    XMFLOAT3 min_vertex = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
    XMFLOAT3 max_vertex = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    XMVECTOR min = XMLoadFloat3(&min_vertex);
    XMVECTOR max = XMLoadFloat3(&max_vertex);

    for (int i = 0; i < size; ++i)
    {
        XMVECTOR test_v = XMLoadFloat3(&vertices[i].pos);
        min = XMVectorMin(min, test_v);
        max = XMVectorMax(max, test_v);
    }

    XMStoreFloat3(b_min, min);
    XMStoreFloat3(b_max, max);
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

void init_random_objects(Shape *shapes, int size, int num_cube, int num_sphere)
{
    // Init meshes
    int32 num_objects = num_cube + num_sphere;

    for(int i = 0; i < num_cube; ++i)
    {
        shapes[i].shape_type = CUBE;
    }

    for(int i = num_cube; i < num_objects; ++i)
    {
        shapes[i].shape_type = SPHERE;
    }

    // Init coords
    for(int i = 0; i < size; ++i)
    {
        if(i%2 == 0)
        {
            shapes[i].x_coord = ((real32)(i)+1.1f) * 3;
            shapes[i].y_coord = 0.0f;

        }
        else
        {
            shapes[i].x_coord = ((real32)(i)+1.1f) * -3;
            shapes[i].y_coord = 0.0f;
        }
    }

    // Init transparency
    for(int i = 0; i < size; ++i)
    {
        if(i%2 == 0)
        {
            shapes[i].transparent = false;
        }
        else
        {
            shapes[i].transparent = true;
        }
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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(device, device_context);

    //Describe our SwapChain
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.Height = HEIGHT;
    swap_chain_desc.Width = WIDTH;
    swap_chain_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_chain_desc.Stereo = FALSE;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
    swap_chain_desc.BufferCount = 2;
    swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

    hr = dxgi_factory->CreateSwapChainForHwnd(device, window, &swap_chain_desc, 0, 0, &swap_chain);
    AssertHR(hr);

    dxgi_factory->Release();
    dxgi_adapter->Release();
    dxgi_device->Release();

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = WIDTH;
    desc.Height = HEIGHT;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    ID3D11Texture2D* offscreen_texture;
    device->CreateTexture2D(&desc, NULL, &offscreen_texture);
    D3D11_RENDER_TARGET_VIEW_DESC offscreen_texture_desc = {};
    offscreen_texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    offscreen_texture_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(offscreen_texture, &offscreen_texture_desc, &offscreen_texture_rtv);
    device->CreateShaderResourceView(offscreen_texture, 0, &offscreen_texture_srv);

    ID3D11Texture2D* backbuffer;
    swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer);
    device->CreateRenderTargetView(backbuffer, 0, &render_target_view);
    device->CreateUnorderedAccessView(backbuffer, 0, &backbuffer_uav);

    D3D11_TEXTURE2D_DESC depth_stencil_desc = {};
    offscreen_texture->GetDesc(&depth_stencil_desc);
    depth_stencil_desc.Width = WIDTH;
    depth_stencil_desc.Height = HEIGHT;
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

    hr = device->CreateComputeShader(postprocessing_shader, sizeof(postprocessing_shader), 0, &postprocess_shader);

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

    D3D11_RASTERIZER_DESC cndesc = {};
    cndesc.FillMode = D3D11_FILL_SOLID;
    cndesc.CullMode = D3D11_CULL_NONE;
    hr = device->CreateRasterizerState(&cndesc, &cull_none);

    D3D11_RASTERIZER_DESC wireframe_desc = {};
    wireframe_desc.FillMode = D3D11_FILL_WIREFRAME;
    wireframe_desc.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&wireframe_desc, &wireframe);

    D3D11_DEPTH_STENCIL_DESC dss_desc = {};
    dss_desc.DepthEnable = true;
    dss_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dss_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    device->CreateDepthStencilState(&dss_desc, &ds_less_equal);
}

internal void process_keyboard_event(Game_Button_State *new_state, bool32 is_down)
{
    if(new_state->ended_down != is_down)
    {
        new_state->ended_down = is_down;
        ++new_state->half_transistion_count;
    }
}

internal void process_pending_messages(HWND window, Game_Controller_Input *input, Mouse_State *mouse)
{
    MSG message;
    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
    {
        switch (message.message)
        {
        case WM_QUIT:
        {
            running = false;
        } break;
        case WM_MOUSEMOVE:
        {
            Window_Dimensions wd = get_window_dimensions(window);
            POINT center;
            center.x = wd.width/2;
            center.y = wd.height/2;
            int mouse_x = GET_X_LPARAM(message.lParam); 
            int mouse_y = GET_Y_LPARAM(message.lParam);
            int mouse_speed = 5;
            if(mouse_x != center.x || mouse_y != center.y)
            {
                mouse->rotated = true;
                mouse->rot_x += (mouse_x - center.x);
                mouse->rot_y += (mouse_y - center.y);

                ClientToScreen(window, &center);
                SetCursorPos(center.x, center.y);
            }
        } break;
        case WM_LBUTTONDOWN:
        {
            process_keyboard_event(&input->attack, true);
        } break;
        case WM_RBUTTONDOWN:
        {
            process_keyboard_event(&input->block, true);
        } break;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            uint32_t vk_code = (uint32_t)message.wParam;
            // NOTE: Since we are comparing was_down and is_down directly we need to use
            // == and != to convert them to 0 or 1 values
            bool32 was_down = ((message.lParam & KEY_MESSAGE_WAS_DOWN_BIT) != 0);
            bool32 is_down = ((message.lParam & KEY_MESSAGE_IS_DOWN_BIT) == 0);

            if(was_down != is_down)
            {
                if(vk_code == 'W')
                {
                    process_keyboard_event(&input->move_fwd, is_down);
                }
                else if(vk_code == 'A')
                {
                    process_keyboard_event(&input->move_left, is_down);
                }
                else if(vk_code == 'S')
                {
                    process_keyboard_event(&input->move_back, is_down);
                }
                else if(vk_code == 'D')
                {
                    process_keyboard_event(&input->move_right, is_down);
                }
                else if(vk_code == 'F')
                {
                    process_keyboard_event(&input->inventory, is_down);
                }
                else if(vk_code == 'J')
                {
                    process_keyboard_event(&input->inventory, is_down);
                }
                else if(vk_code == 'E')
                {
                    process_keyboard_event(&input->use, is_down);
                }
                else if(vk_code == VK_SPACE)
                {
                    process_keyboard_event(&input->jump, is_down);
                }
#if DEBUG
                else if(vk_code == 'K')
                {
                    if(is_down)
                    {
                        show_num_rendered = !show_num_rendered;
                    }
                }
#endif
                if(is_down)
                {
                    bool32 alt_key_down = (message.lParam & ALT_KEY_DOWN_BIT);
                    if((vk_code == VK_F4) && alt_key_down)
                    {
                        running = false;
                    }
                    if((vk_code == VK_RETURN) && (alt_key_down))
                    {
                        if(message.hwnd)
                        {
                            toggle_fullscreen(message.hwnd);
                        }
                    }
                }
            }
        } break;
        default:
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        } break;
        }
    }
}

#if 0
// TODO: Stop using dinput and use WM instead.
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

    if(keyboardState[DIK_K] & 0x80)
    {
        show_num_rendered = !show_num_rendered;
    }

    real32 speed = (real32)(15.0f * time);

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
#endif

internal void update_player(real64 time, vec2 dd_player, Mouse_State *mouse)
{
    real32 speed = (real32)(20.0f * time);

    cam_rotation_matrix = XMMatrixRotationRollPitchYaw(cam_pitch, cam_yaw, 0);
    cam_target = XMVector3TransformCoord(default_forward, cam_rotation_matrix);
    cam_target = XMVector3Normalize(cam_target);

    XMMATRIX rotate_y_temp_matrix;
    rotate_y_temp_matrix = XMMatrixRotationY(cam_yaw);

    cam_right = XMVector3TransformCoord(default_right, rotate_y_temp_matrix);
    cam_up = XMVector3TransformCoord(cam_up, rotate_y_temp_matrix);
    cam_forward = XMVector3TransformCoord(default_forward, rotate_y_temp_matrix);

    cam_position += dd_player.x*speed*cam_right;
    cam_position += dd_player.y*speed*cam_forward;

    cam_target = cam_position + cam_target;
    cam_view = XMMatrixLookAtLH(cam_position, cam_target, cam_up);
}



void clean_up()
{
    swap_chain->Release();
    device->Release();
    device_context->Release();
    render_target_view->Release();
    vertex_shader->Release();
    pixel_shader->Release();
    vertex_layout->Release();
    depth_stencil_view->Release();
    depth_stencil_buffer->Release();
    cb_per_object_buffer->Release();
    transparency->Release();
    ccw_cull->Release();
    cw_cull->Release();
    cb_per_frame_buffer->Release();

    di_keyboard->Unacquire();
    di_mouse->Unacquire();
    direct_input->Release();

    sphere_index_buffer->Release();
    sphere_vert_buffer->Release();

    skymap_vs->Release();
    skymap_ps->Release();

    ds_less_equal->Release();
    cull_none->Release();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void update_and_render(Game_Memory *game_memory, Shape *objects_to_render, int otr_size, Texture_Info *texture_list, real64 time)
{
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = WIDTH;
    viewport.Height = HEIGHT;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    device_context->RSSetViewports(1, &viewport);

    int num_opaque = 0;
    int num_transparent = 0;
    for(int i = 0; i < otr_size; ++i)
    {
        if(objects_to_render[i].transparent)
        {
            num_transparent++;
        }
        else
        {
            num_opaque++;
        }
    }

    Memory_Arena opaque_storage;
    Memory_Arena transparent_storage;
    init_arena(&opaque_storage, num_opaque*sizeof(Render_Object), (uint8_t *)game_memory->transient_storage);
    uint8_t *transparent_start = (uint8_t *)(game_memory->transient_storage) + opaque_storage.size;
    init_arena(&transparent_storage, num_opaque*sizeof(Render_Object), transparent_start);

    Render_Object *opaques;
    opaques = (Render_Object *)opaque_storage.base;
    Render_Object *transparents;
    transparents = (Render_Object *)transparent_storage.base;

    Sphere sphere = build_smooth_sphere();

    /*
        View:            World-space to view-space matrix
        Projection:      View-space to clip-space matrix
        View-Projection: World-space to clip-space matrix
    */

    BoundingFrustum frustum;
    BoundingFrustum::CreateFromMatrix(frustum, cam_projection);

    XMFLOAT3 c_min, c_max, s_min, s_max;
    calculate_bounding_box(cube_vertices, array_count(cube_vertices), &c_min, &c_max);
    calculate_bounding_box(sphere.vertices, array_count(sphere.vertices), &s_min, &s_max);
    XMVECTOR cv_min = XMLoadFloat3(&c_min);
    XMVECTOR cv_max = XMLoadFloat3(&c_max);
    XMVECTOR sv_min = XMLoadFloat3(&s_min);
    XMVECTOR sv_max = XMLoadFloat3(&s_max);

    BoundingBox c_aabb;
    BoundingBox s_aabb;
    c_aabb.CreateFromPoints(c_aabb, cv_min, cv_max);
    s_aabb.CreateFromPoints(s_aabb, sv_min, sv_max);

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
    XMMATRIX rotation = XMMatrixRotationAxis(rotation_axis_y, (real32)rotation_state);
    real32 roty = 1.0f;

    int num_rendered_opaque = 0;
    int num_rendered_transparent = 0;

    for(int i = 0; i < num_opaque; ++i)
    {
        if(!objects_to_render->transparent)
        {
            if(objects_to_render[i].shape_type == SPHERE)
            {
                XMMATRIX world = XMMatrixIdentity();
                real32 x_translate = objects_to_render[i].x_coord;
                translation = XMMatrixTranslation(x_translate, 5.0f, 4.0f);
                rotation = XMMatrixRotationAxis(rotation_axis_y, (real32)rotation_state);
                world = translation*rotation;
                XMMATRIX wv = XMMatrixIdentity();
                wv = world*cam_view;
                BoundingBox test;
                s_aabb.Transform(test, wv);
                ContainmentType ct = frustum.Contains(test);
                if(ct != DISJOINT)
                {
                    opaques[num_rendered_opaque].world = world;
                    opaques[num_rendered_opaque].mesh = objects_to_render[i].shape_type;
                    opaques[num_rendered_opaque].texture_info = objects_to_render[i].texture_info;
                    ++num_rendered_opaque;
                }
            }

            if(objects_to_render[i].shape_type == CUBE)
            {
                XMMATRIX world = XMMatrixIdentity();
                real32 x_translate = objects_to_render[i].x_coord;
                translation = XMMatrixTranslation(x_translate, 5.0f, 4.0f);
                rotation = XMMatrixRotationAxis(rotation_axis_y, (real32)rotation_state);
                world = translation*rotation;
                XMMATRIX wv = XMMatrixIdentity();
                wv = world*cam_view;
                BoundingBox test;
                s_aabb.Transform(test, wv);
                ContainmentType ct = frustum.Contains(test);
                if(ct != DISJOINT)
                {
                    opaques[num_rendered_opaque].world = world;
                    opaques[num_rendered_opaque].mesh = objects_to_render[i].shape_type;
                    opaques[num_rendered_opaque].texture_info = objects_to_render[i].texture_info;
                    ++num_rendered_opaque;
                }
            }

            if(objects_to_render[i].shape_type == PLANE)
            {
                opaques[num_rendered_opaque].mesh = objects_to_render[i].shape_type;
                opaques[num_rendered_opaque].texture_info = objects_to_render[i].texture_info;
                opaques[num_rendered_opaque].world = XMMatrixIdentity();
                scaling = XMMatrixScaling(75.0f, 10.0f, 75.0f);
                translation = XMMatrixTranslation(0.0f, 10.0f, 0.0f);
                opaques[num_rendered_opaque].world = scaling*translation;
                ++num_rendered_opaque;
            }

            if(objects_to_render[i].shape_type == SKYBOX)
            {
                opaques[num_rendered_opaque].mesh = objects_to_render[i].shape_type;
                opaques[num_rendered_opaque].texture_info = objects_to_render[i].texture_info;
                opaques[num_rendered_opaque].world = XMMatrixIdentity();
                scaling = XMMatrixScaling(3.0f, 3.0f, 3.0f);
                translation = XMMatrixTranslation(XMVectorGetX(cam_position), XMVectorGetY(cam_position), XMVectorGetZ(cam_position));
                opaques[num_rendered_opaque].world = scaling*translation;
                ++num_rendered_opaque;
            }
        }
    }

    for(int i = 0; i < num_transparent; ++i)
    {
        if(objects_to_render->transparent)
        {
            int j = 0;
            if(objects_to_render[i].shape_type == SPHERE)
            {
                XMMATRIX world = XMMatrixIdentity();
                real32 x_translate = objects_to_render[i].x_coord;
                translation = XMMatrixTranslation(x_translate, 20.0f, 4.0f);
                rotation = XMMatrixRotationAxis(rotation_axis_y, (real32)rotation_state);
                world = translation*rotation;
                XMMATRIX wv = XMMatrixIdentity();
                wv = world*cam_view;
                BoundingBox test;
                s_aabb.Transform(test, wv);
                ContainmentType ct = frustum.Contains(test);
                if(ct != DISJOINT)
                {
                    transparents[num_rendered_transparent].world = world;
                    transparents[num_rendered_transparent].mesh = objects_to_render[i].shape_type;
                    transparents[num_rendered_transparent].texture_info = objects_to_render[i].texture_info;
                    ++num_rendered_transparent;
                }
            }

            if(objects_to_render[i].shape_type == CUBE)
            {
                XMMATRIX world = XMMatrixIdentity();
                real32 x_translate = objects_to_render[i].x_coord;
                translation = XMMatrixTranslation(x_translate, 20.0f, 4.0f);
                rotation = XMMatrixRotationAxis(rotation_axis_y, (real32)rotation_state);
                world = translation*rotation;
                XMMATRIX wv = XMMatrixIdentity();
                wv = world*cam_view;
                BoundingBox test;
                s_aabb.Transform(test, wv);
                ContainmentType ct = frustum.Contains(test);
                if(ct != DISJOINT)
                {
                    transparents[num_rendered_transparent].world = world;
                    transparents[num_rendered_transparent].mesh = objects_to_render[i].shape_type;
                    transparents[num_rendered_transparent].texture_info = objects_to_render[i].texture_info;
                    ++num_rendered_transparent;
                }
            }
        }
    }

    if(show_num_rendered)
    {
        ImGui::Text("Opaque rendered: %i", num_rendered_opaque);
        ImGui::Text("Transparent rendered: %i", num_rendered_transparent);
    }

    device_context->VSSetShader(vertex_shader, 0, 0);
    device_context->PSSetShader(pixel_shader, 0, 0);
    // device_context->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);
    device_context->OMSetRenderTargets(1, &offscreen_texture_rtv, depth_stencil_view);


#if DEBUG
    event_grouper->BeginEvent(L"Clear screen");
#endif
    FLOAT color[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    // device_context->ClearRenderTargetView(render_target_view, color);
    device_context->ClearRenderTargetView(offscreen_texture_rtv, color);
    device_context->ClearDepthStencilView(depth_stencil_view, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);
#if DEBUG
    event_grouper->EndEvent();
#endif

    // light.dir = XMFLOAT3(0.0f, 1.0f, 0.0f);
    // light.ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
    // light.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    light.pos = XMFLOAT3(0.0f, 10.0f, 0.0f);
    light.range = 500.0f;
    light.att = XMFLOAT3(0.2f, 0.5f, 0.5f);
    light.ambient = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
    light.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);


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

    CB_Per_Object cb_per_object = {};
    XMMATRIX *cbpo_index = (XMMATRIX *)&cb_per_object.cube1;

    int k = 0;
    for(int32 i = 0; i < num_rendered_opaque; ++i)
    {
            *cbpo_index = XMMatrixTranspose(opaques[i].world);
            cbpo_index += 4;
            opaques[i].cbuffer_offset = ((sizeof(cb_per_object.cube1)*4) / 16) * k;
            ++k;
    }

    for(int32 i = 0; i < num_rendered_transparent; ++i)
    {
            *cbpo_index = XMMatrixTranspose(transparents[i].world);
            cbpo_index += 4;
            transparents[i].cbuffer_offset = ((sizeof(cb_per_object.cube1)*4) / 16) * k;
    }

    device_context->UpdateSubresource(cb_per_object_buffer, 0, 0, &cb_per_object, 0, 0);

    for(int i = 0; i < num_rendered_opaque; ++i)
    {
        opaques[i].dist_from_cam = find_dist_from_cam(opaques[i].world);
    }
    sort_opaque_dist(opaques, num_rendered_opaque);

    for(int i = 0; i < num_rendered_transparent; ++i)
    {
        transparents[i].dist_from_cam = find_dist_from_cam(transparents[i].world);
    }
    sort_transparent_dist(transparents, num_rendered_transparent);

    device_context->OMSetBlendState(0, 0, 0xFFFFFFFF);

#if DEBUG
    event_grouper->BeginEvent(L"Render opaque objects");
#endif

    UINT size = (sizeof(cb_per_object.cube1)*4) / 16;

    for(int32 i = 0; i < num_rendered_opaque; ++i)
    {
        if(opaques[i].mesh == PLANE)
        {
            #if DEBUG
                event_grouper->BeginEvent(L"Render ground");
            #endif
                load_ground_mesh();
                device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &opaques[i].cbuffer_offset, &size);
                device_context->PSSetShaderResources(0, 1, &opaques[i].texture_info.shader_resource_view);
                device_context->PSSetSamplers(0, 1, &opaques[i].texture_info.sampler_state);
                device_context->RSSetState(ccw_cull);
                device_context->DrawIndexed(6, 0, 0);
            #if DEBUG
                event_grouper->EndEvent();
            #endif
        }

        if(opaques[i].mesh == SKYBOX)
        {
            #if DEBUG
                event_grouper->BeginEvent(L"Render sky");
            #endif
                load_cube_mesh();
                device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &opaques[i].cbuffer_offset, &size);
                device_context->PSSetShaderResources(0, 1, &opaques[i].texture_info.shader_resource_view);
                device_context->PSSetSamplers(0, 1, &opaques[i].texture_info.sampler_state);
                device_context->VSSetShader(skymap_vs, 0, 0);
                device_context->PSSetShader(skymap_ps, 0, 0);
                device_context->OMSetDepthStencilState(ds_less_equal, 0);
                device_context->RSSetState(cull_none);
                device_context->DrawIndexed(36, 0, 0);

                // Reset state
                device_context->VSSetShader(vertex_shader, 0, 0);
                device_context->PSSetShader(pixel_shader, 0, 0);
                device_context->OMSetDepthStencilState(0, 0); 
            #if DEBUG
                event_grouper->EndEvent();
            #endif
        }

        if(opaques[i].mesh == CUBE)
        {
            load_cube_mesh();
            device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &opaques[i].cbuffer_offset, &size);
            device_context->PSSetShaderResources(0, 1, &opaques[i].texture_info.shader_resource_view);
            device_context->PSSetSamplers(0, 1, &opaques[i].texture_info.sampler_state);
            device_context->RSSetState(cw_cull);
            device_context->DrawIndexed(36, 0, 0);
        }
        else if(opaques[i].mesh == SPHERE)
        {
            load_sphere_mesh(&sphere);
            device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &opaques[i].cbuffer_offset, &size);
            device_context->PSSetShaderResources(0, 1, &opaques[i].texture_info.shader_resource_view);
            device_context->PSSetSamplers(0, 1, &opaques[i].texture_info.sampler_state);
            device_context->RSSetState(cw_cull);
            device_context->DrawIndexed(360, 0, 0);
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

    for(int32 i = 0; i < num_rendered_transparent; ++i)
    {
        if(transparents[i].mesh == CUBE)
        {
            #if DEBUG
            event_grouper->BeginEvent(L"Render transparent cube");
            #endif
            load_cube_mesh();
            device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &transparents[i].cbuffer_offset, &size);
            device_context->PSSetShaderResources(0, 1, &transparents[i].texture_info.shader_resource_view);
            device_context->PSSetSamplers(0, 1, &transparents[i].texture_info.sampler_state);
            device_context->RSSetState(ccw_cull);
            device_context->DrawIndexed(36, 0, 0);

            ++k;
            #if DEBUG
            event_grouper->EndEvent();
            #endif
        }
        else if(transparents[i].mesh == SPHERE)
        {
            #if DEBUG
            event_grouper->BeginEvent(L"Render transparent sphere");
            #endif
            load_sphere_mesh(&sphere);
            device_context->VSSetConstantBuffers1(0, 1, &cb_per_object_buffer, &transparents[i].cbuffer_offset, &size);
            device_context->PSSetShaderResources(0, 1, &transparents[i].texture_info.shader_resource_view);
            device_context->PSSetSamplers(0, 1, &transparents[i].texture_info.sampler_state);
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

    device_context->ClearState();
    device_context->CSSetShader(postprocess_shader, NULL, 0);    // set compute shader
    device_context->CSSetShaderResources(4, 1, &offscreen_texture_srv);             // set input texture
    device_context->CSSetUnorderedAccessViews(4, 1, &backbuffer_uav, NULL); // set output texture
    device_context->Dispatch((WIDTH+15)/16, (HEIGHT+15)/16, 1);          // run it
    device_context->ClearState();

    // TODO: Make imgui render to the backbuffer itself
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    swap_chain->Present(0, 0);

    VirtualFree(opaques, 0, MEM_RELEASE);
    VirtualFree(transparents, 0, MEM_RELEASE);
}

LRESULT CALLBACK win32_main_window_callback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    if (ImGui_ImplWin32_WndProcHandler(window, msg, wParam, lParam))
    {
        return true;
    }

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

/*
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
*/

int WINAPI WinMain(HINSTANCE instance,
    HINSTANCE prev_instance, 
    LPSTR cmd_line,
    int show_cmd)
{
    HWND window = window_init(instance, show_cmd, WIDTH, HEIGHT, true);
    d3d11_init(instance, window);
    init_direct_input(instance, window);
    scene_init();

    srand((unsigned int)time(0));
#if 0
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
#endif

    Game_Memory game_memory = {};
    game_memory.permanent_storage_size = megabytes(64);
    game_memory.transient_storage_size = gigabytes((uint64_t)3);
    game_memory.total_storage_size = game_memory.permanent_storage_size + game_memory.transient_storage_size;
    game_memory.game_memory_block = VirtualAlloc(0, (size_t)game_memory.total_storage_size, 
                                                 MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    game_memory.permanent_storage = game_memory.game_memory_block;
    game_memory.transient_storage = ((uint8_t *)game_memory.permanent_storage + 
                                     game_memory.permanent_storage_size);


    Game_Controller_Input user_old_input = {};
    Game_Controller_Input user_new_input = {};
    Mouse_State mouse = {};

    // TODO: Reinstate the random generation of object numbers. Will need to either move
    // setting transparency out of the init function or leave it in there and remove above code.
    // May need to debug transparents not being rendered as well.

    const int32 num_shapes = 8;
    int32 num_cube = 4;
    int32 num_sphere = 4;

    Shape random_objects[num_shapes];
    init_random_objects(random_objects, num_shapes, num_cube, num_sphere);

    Texture_Info texture_info[12];
    load_textures(texture_info);
    load_sky_texture(&texture_info[10]);
    load_ground_texture(&texture_info[11]);

    for(int i = 0; i < num_shapes; ++i)
    {
        random_objects[i].texture_info = texture_info[i];
    }

    Shape skybox = {};
    skybox.shape_type = SKYBOX;
    skybox.texture_info = texture_info[10];
    skybox.transparent = false;

    Shape ground = {};
    ground.shape_type = PLANE;
    ground.texture_info = texture_info[11];
    ground.transparent = false;

    const int num_objects = 10;
    
    Shape game_objects[num_objects];
    game_objects[0] = skybox;
    game_objects[1] = ground;

    for(int i = 2; i < num_objects; ++i)
    {
        game_objects[i] = random_objects[i];
    }

    // TODO: Load game music and a sound effect and connect the sound effect to some button press
    WasapiAudio audio;
    WA_Start(&audio, 48000, 2, SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
    size_t sampleRate = audio.bufferFormat->nSamplesPerSec;
    size_t bytesPerSample = audio.bufferFormat->nBlockAlign;

    // background "music" that will be looping
    Sound background = S_Load(L"C:/Gamedev/render/build/sound/music/midnightforest.mp3", sampleRate);
    background.loop = true;

    // simple sound effect, won't be looping
    Sound effect = S_Load(L"C:/Gamedev/render/build/sound/effects/sword_swing.wav", sampleRate);

    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);

    running = true;
    while(running)
    {
        mouse = {};
        user_new_input = {};
        for(int button_index = 0; 
            button_index < array_count(user_new_input.buttons); 
            ++button_index)
        {
            user_new_input.buttons[button_index].ended_down =
            user_old_input.buttons[button_index].ended_down;
        }
        process_pending_messages(window, &user_new_input, &mouse);

        vec2 dd_player = {};
        if(user_new_input.move_left.ended_down)
        {
            dd_player.x = -1.0f;
        }
        if(user_new_input.move_right.ended_down)
        {
            dd_player.x = 1.0f;
        }
        if(user_new_input.move_fwd.ended_down)
        {
            dd_player.y = 1.0f;
        }
        if(user_new_input.move_back.ended_down)
        {
            dd_player.y = -1.0f;
        }
        if(mouse.rotated)
        {
            cam_yaw += mouse.rot_x * 0.001f;
            cam_pitch += mouse.rot_y * 0.001f;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

		bool escPressed = false;
		bool spacePressed = false;
		bool delayPressed = false;

        // TODO: Hook this up to regular input code
		while (WaitForSingleObject(input, 0) == WAIT_OBJECT_0)
		{
			INPUT_RECORD record;
			DWORD read;
			if (ReadConsoleInputW(input, &record, 1, &read)
				&& read == 1
				&& record.EventType == KEY_EVENT
				&& record.Event.KeyEvent.bKeyDown)
			{
				switch (record.Event.KeyEvent.wVirtualKeyCode)
				{
				case VK_ESCAPE: escPressed = true; break;
				case VK_SPACE: spacePressed = true; break;
				case 'D': delayPressed = true; break;
				}
			}
		}

		if (escPressed)
		{
			printf("stop!\n");
			break;
		}

		if (spacePressed)
		{
			printf("tada!\n");
			effect.pos = 0;
		}

		{
			WA_LockBuffer(&audio);

			// write at least 100msec of samples into buffer (or whatever space available, whichever is smaller)
			// this is max amount of time you expect code will take until the next iteration of loop
			// if code will take more time then you'll hear discontinuity as buffer will be filled with silence
			size_t writeCount = min(sampleRate/10, audio.sampleCount);

			// alternatively you can write as much as "audio.sampleCount" to fully fill the buffer (~1 second)
			// then you can try to increase delay below to 900+ msec, it still should sound fine
			//writeCount = audio.sampleCount;

			// advance sound playback positions
			size_t playCount = audio.playCount;
			S_Update(&background, playCount);
			S_Update(&effect, playCount);

			// initialize output with 0.0f
			float* output = (float *)audio.sampleBuffer;
			memset(output, 0, writeCount * bytesPerSample);

			// mix sounds into output
			S_Mix(output, writeCount, 0.3f, &background);
			S_Mix(output, writeCount, 0.8f, &effect);

			WA_UnlockBuffer(&audio, writeCount);
		}

        frame_count++;
        if(get_time() > 1.0f)
        {
            fps = frame_count;
            frame_count = 0;
            start_timer();
        }
        frame_time = get_frame_time();
        // detect_input(frame_time, window);
        update_player(frame_time, dd_player, &mouse);
        update_and_render(&game_memory, game_objects, num_objects, texture_info, frame_time);
        
        Game_Controller_Input temp = user_new_input;
        user_new_input = user_old_input;
        user_old_input = temp;
    }
    WA_Stop(&audio);
    clean_up();
    
    return(0);
}
