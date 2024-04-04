#if !defined(RENDER_H)

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
        float u, float v,
        float nx, float ny, float nz)
        : pos(x,y,z), tex_coord(u, v), normal(nx, ny, nz){}

    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 tex_coord;
    DirectX::XMFLOAT3 normal;
};

D3D11_INPUT_ELEMENT_DESC layout[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },  
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }  
};

struct Texture_Info
{
    ID3D11Texture2D *texture;
    ID3D11ShaderResourceView *shader_resource_view;
    ID3D11SamplerState *sampler_state;
};

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

struct Light
{
    DirectX::XMFLOAT3 dir;
    real32 pad;
    DirectX::XMFLOAT4 ambient;
    DirectX::XMFLOAT4 diffuse;
};

struct CB_Per_Frame 
{
    DirectX::XMMATRIX view;
    DirectX::XMMATRIX projection;
    Light light;
};

#define RENDER_H
#endif