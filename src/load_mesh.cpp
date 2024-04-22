// load_mesh.cpp

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