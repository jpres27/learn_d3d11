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
typedef size_t memory_index;

struct Win32_State
{
    uint64_t total_size;
    void *game_memory_block;
};

struct Memory_Arena 
{
    memory_index size;
    uint8_t *base;
    memory_index used;
};

#define push_struct(arena, type) (type *)push_size_(arena, sizeof(type))
#define push_array(arena, count, type) (type *)push_size_(arena, (count)*sizeof(type))
void * push_size_(Memory_Arena *arena, memory_index size)
{
    assert((arena->used + size) < arena->size);
    void *result = arena->base + arena->used;
    arena->used += size;
    return(result);
}

struct vec2
{
    real32 x, y;
};

enum Shape_Type
{
    CUBE,
    SPHERE,
    PLANE,
    SKYBOX
};

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
    int texture_tag;
};

struct Shape
{
    Shape_Type shape_type;
    real32 x_coord;
    real32 y_coord; 
    bool32 transparent;
    Texture_Info texture_info;
};

struct Render_Object
{
    Shape_Type mesh;
    DirectX::XMMATRIX world;
    real32 dist_from_cam;
    Texture_Info texture_info;
    UINT cbuffer_offset;
};

struct Frustum_Planes
{
    DirectX::XMFLOAT4 plane[6];
};

struct Bounding_Box 
{
    DirectX::XMFLOAT3 min_vert;
    DirectX::XMFLOAT3 max_vert;
};

struct Bounding_Sphere 
{
    DirectX::XMFLOAT3 center;
    real32 radius;
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

    DirectX::XMMATRIX cube11;
    DirectX::XMMATRIX pad31;
    DirectX::XMMATRIX pad32;
    DirectX::XMMATRIX pad33;

    DirectX::XMMATRIX cube12;
    DirectX::XMMATRIX pad34;
    DirectX::XMMATRIX pad35;
    DirectX::XMMATRIX pad36;

    DirectX::XMMATRIX cube13;
    DirectX::XMMATRIX pad37;
    DirectX::XMMATRIX pad38;
    DirectX::XMMATRIX pad39;

    DirectX::XMMATRIX cube14;
    DirectX::XMMATRIX pad40;
    DirectX::XMMATRIX pad41;
    DirectX::XMMATRIX pad42;

    DirectX::XMMATRIX cube15;
    DirectX::XMMATRIX pad43;
    DirectX::XMMATRIX pad44;
    DirectX::XMMATRIX pad45; // 3840 bytes
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