#pragma comment(lib, "user32.lib") 
#pragma comment(lib, "d3d11.lib")

#include <stdint.h>
#include <windows.h>
#include <d3d11.h>

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

IDXGISwapChain* SwapChain;
ID3D11Device* d3d11Device;
ID3D11DeviceContext* d3d11DevCon;
ID3D11RenderTargetView* renderTargetView;

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
    DXGI_MODE_DESC bufferDesc;

    ZeroMemory(&bufferDesc, sizeof(DXGI_MODE_DESC));

    bufferDesc.Width = Width;
    bufferDesc.Height = Height;
    bufferDesc.RefreshRate.Numerator = 60;
    bufferDesc.RefreshRate.Denominator = 1;
    bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    
    //Describe our SwapChain
    DXGI_SWAP_CHAIN_DESC swapChainDesc; 
        
    ZeroMemory(&swapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));

    swapChainDesc.BufferDesc = bufferDesc;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 1;
    swapChainDesc.OutputWindow = window; 
    swapChainDesc.Windowed = TRUE; 
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;


    //Create our SwapChain
    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, NULL, NULL,
        D3D11_SDK_VERSION, &swapChainDesc, &SwapChain, &d3d11Device, NULL, &d3d11DevCon);

    //Create our BackBuffer
    ID3D11Texture2D* BackBuffer;
    SwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void**)&BackBuffer );

    //Create our Render Target
    d3d11Device->CreateRenderTargetView( BackBuffer, NULL, &renderTargetView );
    BackBuffer->Release();

    //Set our Render Target
    d3d11DevCon->OMSetRenderTargets( 1, &renderTargetView, NULL );

    return true;
}

void release_objects()
{
    //Release the COM Objects we created
    SwapChain->Release();
    d3d11Device->Release();
    d3d11DevCon->Release();
}
bool32 scene_init()
{

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
    //Clear our backbuffer to the updated color
    d3d11DevCon->ClearRenderTargetView(renderTargetView, RGBA{red, green, blue, 1.0f});

    //Present the backbuffer to the screen
    SwapChain->Present(0, 0);
}

///////////////**************new**************////////////////////

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
    
///////////////**************new**************////////////////////

    return 0;
}
