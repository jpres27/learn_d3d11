#if !defined(GEOMETRY_H)

real32 stack_count = 6;
real32 sector_count = 12;
real32 sphere_radius = 1;

struct Sphere
{
    Vertex vertices[91];
    DWORD indices[360];
};

#define GEOMETRY_H
#endif