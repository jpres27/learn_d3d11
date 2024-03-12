#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "d3d11.lib")


#include <stdint.h>
#include <windows.h>
#include <d3d11.h>
#include <DirectXMath.h>

#define internal static
#define local_persist static
#define global_variable static

#define assert(expression) if(expression == false) {*(int *)0 = 0;}

typedef int32_t int32;
typedef int64_t int64;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int32_t bool32;
typedef float real32;
typedef double real64;
typedef real32 RGBA[4];

struct vector3 { real32 x, y, z; };
struct matrix { real32 m[4][4]; };

matrix operator*(const matrix& m1, const matrix& m2);

struct Vertex
{
    Vertex(){}
    Vertex(float x, float y, float z,
        float cr, float cg, float cb, float ca)
        : pos(x,y,z), color(cr, cg, cb, ca){}

    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 color;
};

D3D11_INPUT_ELEMENT_DESC layout[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },  
    { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },  
};
UINT numElements = ARRAYSIZE(layout);

IDXGISwapChain* swap_chain;
ID3D11Device* device;
ID3D11DeviceContext* device_context;
ID3D11RenderTargetView* render_target_view;

ID3D11Buffer* sq_index_buffer;
ID3D11Buffer* sq_vert_buffer;
ID3D11DepthStencilView* depth_stencil_view;
ID3D11Texture2D* depth_stencil_buffer;

ID3D11VertexShader* vertex_shader;
ID3D11PixelShader* pixel_shader;
ID3D11InputLayout* vertex_layout;

ID3D11Buffer* cb_per_object_buffer;


real32 red = 0.0f;
real32 green = 0.0f;
real32 blue = 0.0f;
int colormodr = 1;
int colormodg = 1;
int colormodb = 1;

LPCTSTR WndClassName = "firstwindow";
HWND window = NULL;

const int WIDTH  = 300;
const int HEIGHT = 300;

DirectX::XMMATRIX wvp;
DirectX::XMMATRIX world;
DirectX::XMMATRIX cam_view;
DirectX::XMMATRIX cam_projection;
DirectX::XMVECTOR cam_position;
DirectX::XMVECTOR cam_target;
DirectX::XMVECTOR cam_up;

struct CB_Per_Object
{
    DirectX::XMMATRIX wvp;
};

CB_Per_Object cb_per_object;

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

bool32 window_init(HINSTANCE hInstance,
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
        return 1;
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
        return 1;
    }

    ShowWindow(window, ShowWnd);
    UpdateWindow(window);

    return true;
}

bool32 d3d11_init(HINSTANCE hInstance)
{
    //Describe our Buffer
    DXGI_MODE_DESC buffer_desc;

    ZeroMemory(&buffer_desc, sizeof(DXGI_MODE_DESC));

    buffer_desc.Width = WIDTH;
    buffer_desc.Height = HEIGHT;
    buffer_desc.RefreshRate.Numerator = 60;
    buffer_desc.RefreshRate.Denominator = 1;
    buffer_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    buffer_desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    buffer_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    
    //Describe our SwapChain
    DXGI_SWAP_CHAIN_DESC swap_chain_desc; 
        
    ZeroMemory(&swap_chain_desc, sizeof(DXGI_SWAP_CHAIN_DESC));

    swap_chain_desc.BufferDesc = buffer_desc;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 1;
    swap_chain_desc.OutputWindow = window; 
    swap_chain_desc.Windowed = TRUE; 
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    HRESULT hr;

    hr = D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, levels, ARRAYSIZE(levels),
        D3D11_SDK_VERSION, &swap_chain_desc, &swap_chain, &device, 0, &device_context);
    assert(SUCCEEDED(hr));

    ID3D11Texture2D* backbuffer;
    swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer);

    device->CreateRenderTargetView(backbuffer, NULL, &render_target_view);
    backbuffer->Release();

    D3D11_TEXTURE2D_DESC depth_stencil_desc = {};
    depth_stencil_desc.Width = WIDTH;
    depth_stencil_desc.Height = HEIGHT;
    depth_stencil_desc.MipLevels = 1;
    depth_stencil_desc.ArraySize = 1;
    depth_stencil_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_stencil_desc.SampleDesc.Count = 1;
    depth_stencil_desc.SampleDesc.Quality = 0;
    depth_stencil_desc.Usage = D3D11_USAGE_DEFAULT;
    depth_stencil_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    device->CreateTexture2D(&depth_stencil_desc, 0, &depth_stencil_buffer);
    device->CreateDepthStencilView(depth_stencil_buffer, 0, &depth_stencil_view);
    device_context->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);

    return true;
}

bool32 scene_init()
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
        Vertex( -0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f ),
        Vertex( -0.5f,  0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f ),
        Vertex(  0.5f,  0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f ),
        Vertex(  0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f ),
    };

    DWORD indices[] = 
    {
        0, 1, 2,
        0, 2, 3,
    };

    D3D11_BUFFER_DESC index_buffer_desc = {};
    index_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    index_buffer_desc.ByteWidth = sizeof(DWORD)*2*3;
    index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    index_buffer_desc.CPUAccessFlags = 0;
    index_buffer_desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA index_init_data ={};
    index_init_data.pSysMem = indices;
    hr = device->CreateBuffer(&index_buffer_desc, &index_init_data, &sq_index_buffer);
    device_context->IASetIndexBuffer(sq_index_buffer, DXGI_FORMAT_R32_UINT, 0);

    D3D11_BUFFER_DESC vert_buffer_desc = {};
    vert_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    vert_buffer_desc.ByteWidth = sizeof(Vertex)*4;
    vert_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vert_buffer_desc.CPUAccessFlags = 0;
    vert_buffer_desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA vert_buffer_data = {};
    vert_buffer_data.pSysMem = v;
    hr = device->CreateBuffer( &vert_buffer_desc, &vert_buffer_data, &sq_vert_buffer);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    device_context->IASetVertexBuffers( 0, 1, &sq_vert_buffer, &stride, &offset );

    hr = device->CreateInputLayout(layout, numElements, d3d11_vshader, sizeof(d3d11_vshader), &vertex_layout);

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

    D3D11_BUFFER_DESC cb_buffer_desc = {};
    cb_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    cb_buffer_desc.ByteWidth = sizeof(CB_Per_Object);
    cb_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    hr = device->CreateBuffer(&cb_buffer_desc, 0, &cb_per_object_buffer);

    cam_position = DirectX::XMVectorSet(0.0f, 0.0f, -0.5f, 0.0f);
    cam_target = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    cam_up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    cam_view = DirectX::XMMatrixLookAtLH(cam_position, cam_target, cam_up);
    cam_projection = DirectX::XMMatrixPerspectiveFovLH(0.4f*3.14f, (real32)(WIDTH)/(real32)HEIGHT, 1.0f, 1000.0f);

    return true;
}

void scene_update()
{
    //Update the colors of our scene
    red += colormodr * 0.00005f;
    green += colormodg * 0.00002f;
    blue += colormodb * 0.00001f;

    if(red >= 1.0f || red <= 0.0f)
        colormodr *= -1;
    if(green >= 1.0f || green <= 0.0f)
        colormodg *= -1;
    if(blue >= 1.0f || blue <= 0.0f)
        colormodb *= -1;
}

void scene_render()
{
    real32 bgColor[4] = {(0.0f, 0.0f, 0.0f, 0.0f)};
    device_context->ClearRenderTargetView(render_target_view, bgColor);

    device_context->ClearDepthStencilView(depth_stencil_view, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

    world = DirectX::XMMatrixIdentity();
    wvp = world*cam_view*cam_projection;
    cb_per_object.wvp = DirectX::XMMatrixTranspose(wvp);

    device_context->UpdateSubresource(cb_per_object_buffer, 0, 0, &cb_per_object, 0, 0);
    device_context->VSSetConstantBuffers(0, 1, &cb_per_object_buffer);

    device_context->DrawIndexed(6, 0, 0);
    swap_chain->Present(0, 0);
}

int messageloop()
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
            scene_update();
            scene_render();
            }
    }
    return msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance,    //Main windows function
    HINSTANCE hPrevInstance, 
    LPSTR lpCmdLine,
    int nShowCmd)
{

    if(!window_init(hInstance, nShowCmd, WIDTH, HEIGHT, true))
    {
        MessageBox(0, "Window Initialization - Failed",
            "Error", MB_OK);
        return 0;
    }

    if(!d3d11_init(hInstance))    //Initialize Direct3D
    {
        MessageBox(0, "Direct3D Initialization - Failed",
            "Error", MB_OK);
        return 0;
    }

    if(!scene_init())    //Initialize our scene
    {
        MessageBox(0, "Scene Initialization - Failed",
            "Error", MB_OK);
        return 0;
    }
    
    messageloop();
    
    return 0;
}

matrix operator*(const matrix& m1, const matrix& m2)
{
    return
    {
        m1.m[0][0] * m2.m[0][0] + m1.m[0][1] * m2.m[1][0] + m1.m[0][2] * m2.m[2][0] + m1.m[0][3] * m2.m[3][0],
        m1.m[0][0] * m2.m[0][1] + m1.m[0][1] * m2.m[1][1] + m1.m[0][2] * m2.m[2][1] + m1.m[0][3] * m2.m[3][1],
        m1.m[0][0] * m2.m[0][2] + m1.m[0][1] * m2.m[1][2] + m1.m[0][2] * m2.m[2][2] + m1.m[0][3] * m2.m[3][2],
        m1.m[0][0] * m2.m[0][3] + m1.m[0][1] * m2.m[1][3] + m1.m[0][2] * m2.m[2][3] + m1.m[0][3] * m2.m[3][3],
        m1.m[1][0] * m2.m[0][0] + m1.m[1][1] * m2.m[1][0] + m1.m[1][2] * m2.m[2][0] + m1.m[1][3] * m2.m[3][0],
        m1.m[1][0] * m2.m[0][1] + m1.m[1][1] * m2.m[1][1] + m1.m[1][2] * m2.m[2][1] + m1.m[1][3] * m2.m[3][1],
        m1.m[1][0] * m2.m[0][2] + m1.m[1][1] * m2.m[1][2] + m1.m[1][2] * m2.m[2][2] + m1.m[1][3] * m2.m[3][2],
        m1.m[1][0] * m2.m[0][3] + m1.m[1][1] * m2.m[1][3] + m1.m[1][2] * m2.m[2][3] + m1.m[1][3] * m2.m[3][3],
        m1.m[2][0] * m2.m[0][0] + m1.m[2][1] * m2.m[1][0] + m1.m[2][2] * m2.m[2][0] + m1.m[2][3] * m2.m[3][0],
        m1.m[2][0] * m2.m[0][1] + m1.m[2][1] * m2.m[1][1] + m1.m[2][2] * m2.m[2][1] + m1.m[2][3] * m2.m[3][1],
        m1.m[2][0] * m2.m[0][2] + m1.m[2][1] * m2.m[1][2] + m1.m[2][2] * m2.m[2][2] + m1.m[2][3] * m2.m[3][2],
        m1.m[2][0] * m2.m[0][3] + m1.m[2][1] * m2.m[1][3] + m1.m[2][2] * m2.m[2][3] + m1.m[2][3] * m2.m[3][3],
        m1.m[3][0] * m2.m[0][0] + m1.m[3][1] * m2.m[1][0] + m1.m[3][2] * m2.m[2][0] + m1.m[3][3] * m2.m[3][0],
        m1.m[3][0] * m2.m[0][1] + m1.m[3][1] * m2.m[1][1] + m1.m[3][2] * m2.m[2][1] + m1.m[3][3] * m2.m[3][1],
        m1.m[3][0] * m2.m[0][2] + m1.m[3][1] * m2.m[1][2] + m1.m[3][2] * m2.m[2][2] + m1.m[3][3] * m2.m[3][2],
        m1.m[3][0] * m2.m[0][3] + m1.m[3][1] * m2.m[1][3] + m1.m[3][2] * m2.m[2][3] + m1.m[3][3] * m2.m[3][3],
    };
}
