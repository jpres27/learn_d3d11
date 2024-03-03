#pragma comment(lib, "user32.lib") 
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "D3DCompiler.lib")

#include <stdint.h>
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
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
    Vertex(float x, float y, float z)
        : pos(x,y,z){}

    DirectX::XMFLOAT3 pos;
};

D3D11_INPUT_ELEMENT_DESC layout[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },  
};
UINT numElements = ARRAYSIZE(layout);

IDXGISwapChain* swap_chain;
ID3D11Device* device;
ID3D11DeviceContext* device_context;
ID3D11RenderTargetView* render_target_view;

ID3D11Buffer* triangle_vertex_buffer;
ID3D11VertexShader* vertex_shader;
ID3D11PixelShader* pixel_shader;
ID3D10Blob* vertex_shader_buffer;
ID3D10Blob* pixel_shader_buffer;
ID3D11InputLayout* vertex_layout;


real32 red = 0.0f;
real32 green = 0.0f;
real32 blue = 0.0f;
int colormodr = 1;
int colormodg = 1;
int colormodb = 1;

LPCTSTR WndClassName = "firstwindow";
HWND window = NULL;

const int Width  = 300;
const int Height = 300;

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

    buffer_desc.Width = Width;
    buffer_desc.Height = Height;
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


    //Create our SwapChain
    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, NULL, NULL,
        D3D11_SDK_VERSION, &swap_chain_desc, &swap_chain, &device, NULL, &device_context);

    //Create our backbuffer
    ID3D11Texture2D* backbuffer;
    swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer);

    //Create our Render Target
    device->CreateRenderTargetView(backbuffer, NULL, &render_target_view);
    backbuffer->Release();

    //Set our Render Target
    device_context->OMSetRenderTargets(1, &render_target_view, NULL);

    return true;
}

void release_objects()
{
    //Release the COM Objects we created
    swap_chain->Release();
    device->Release();
    device_context->Release();
}
bool32 scene_init()
{
    HRESULT hr;

    hr = D3DCompileFromFile((const wchar_t*)"Effects.fx", 0, 0, "VS", "vertex_shader_5_0", 0, 0, &vertex_shader_buffer, 0);
    assert(hr == S_OK);
    hr = D3DCompileFromFile((const wchar_t*)"Effects.fx", 0, 0, "PS", "pixel_shader_5_0", 0, 0, &pixel_shader_buffer, 0);
    assert(hr == S_OK);

    hr = device->CreateVertexShader(vertex_shader_buffer->GetBufferPointer(), 
        vertex_shader_buffer->GetBufferSize(), 0, &vertex_shader);
    hr = device->CreatePixelShader(pixel_shader_buffer->GetBufferPointer(), 
        pixel_shader_buffer->GetBufferSize(), 0, &pixel_shader);

    device_context->VSSetShader(vertex_shader, 0, 0);
    device_context->PSSetShader(pixel_shader, 0, 0);

    Vertex v[] =
    {
        Vertex(0.0f, 0.5f, 0.5f),
        Vertex(0.5f, -0.5f, 0.5f),
        Vertex(-0.5f, -0.5f, 0.5f),
    };

    D3D11_BUFFER_DESC vertexBufferDesc = {};

    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(Vertex) * 3;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = 0;
    vertexBufferDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA vertexBufferData ={};

    vertexBufferData.pSysMem = v;
    hr = device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &triangle_vertex_buffer);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    device_context->IASetVertexBuffers(0, 1, &triangle_vertex_buffer, &stride, &offset);

    hr = device->CreateInputLayout(layout, numElements, vertex_shader_buffer->GetBufferPointer(), 
        vertex_shader_buffer->GetBufferSize(), &vertex_layout);

    device_context->IASetInputLayout(vertex_layout);

    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D11_VIEWPORT viewport = {};

    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = Width;
    viewport.Height = Height;

    device_context->RSSetViewports(1, &viewport);

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
    device_context->Draw(3, 0);
    swap_chain->Present(0, 0);
}

int messageloop(){
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
            scene_update
        ();
            scene_render
        ();
            }
    }
    return msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance,    //Main windows function
    HINSTANCE hPrevInstance, 
    LPSTR lpCmdLine,
    int nShowCmd)
{

    if(!window_init(hInstance, nShowCmd, Width, Height, true))
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

    release_objects();
    
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
