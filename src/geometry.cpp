#include "geometry.h"
real32 PI = 3.14159265359;

internal void build_smooth_sphere(Sphere *sphere)
{
    real32 x, y, z, xy;
    real32 nx, ny, nz, length_inv = 1.0f / radius;
    real32 s, t;

    real32 sector_step = 2 * PI / sector_count;
    real32 stack_step = PI / stack_count;
    real32 sector_angle, stack_angle;

    uint32 counter = 0;
    for (int i = 0; i <= stack_count; ++i)
    {
        stack_angle = PI / 2 - i * stack_step;
        xy = radius * cosf(stack_angle);
        z = radius * sinf(stack_angle);

        for(int j = 0; j <= sector_count; ++j)
        {
            sector_angle = j * sector_step;

            x = xy * cosf(sector_angle);
            y = xy * sinf(sector_angle);

            nx = x * length_inv;
            ny = y * length_inv;
            nz = z * length_inv;

            s = (real32)j / sector_count;
            t = (real32)i / stack_count;

            assert(counter < array_count(sphere->vertices));
            sphere->vertices[counter] = Vertex(x, y, z, nx, ny, nz, s, t);
            ++counter;
        }
    }
    counter = 0;
    int32 k1, k2;
    for(int32 i = 0; i < stack_count; ++i)
    {
        k1 = (int32)(i * (sector_count + 1)); // beginning of current stack
        k2 = (int32)(k1 + sector_count + 1); // next stack

        for(int32 j = 0; j < sector_count; ++j, ++k1, ++k2)
        {
            if (i != 0) // two triangles per sector excluding first and last stacks
            {
                assert(counter < array_count(sphere->indices));
                sphere->indices[counter] = k1;
                ++counter;
                sphere->indices[counter] = k2;
                ++counter;
                sphere->indices[counter] = k1 + 1;
                ++counter;
            }

            if(i != stack_count-1)
            {
                assert(counter < array_count(sphere->indices));
                sphere->indices[counter] = k1 + 1;
                ++counter;
                sphere->indices[counter] = k2;
                ++counter;
                sphere->indices[counter] = k2 + 1;
                ++counter;
            }
        }
    }
}