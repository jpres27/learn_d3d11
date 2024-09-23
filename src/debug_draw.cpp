void draw_debug_lines(std::vector<Vertex> line_vertices)
{
    HRESULT hr;

    D3D11_BUFFER_DESC vert_buffer_desc = {};
    vert_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    vert_buffer_desc.ByteWidth = (UINT)(sizeof(Vertex)*line_vertices.size());
    vert_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vert_buffer_desc.CPUAccessFlags = 0;
    vert_buffer_desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA vert_buffer_data = {};
    vert_buffer_data.pSysMem = line_vertices.data();

    hr = device->CreateBuffer(&vert_buffer_desc, &vert_buffer_data, &debug_vert_buffer);
    AssertHR(hr);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    device_context->IASetVertexBuffers(0, 1, &debug_vert_buffer, &stride, &offset);

    hr = device->CreateInputLayout(layout, ARRAYSIZE(layout), d3d11_vshader, sizeof(d3d11_vshader), &vertex_layout);
    AssertHR(hr);

    device_context->IASetInputLayout(vertex_layout); 
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    device_context->Draw((UINT)line_vertices.size(), 0);
}