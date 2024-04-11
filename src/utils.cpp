void concat_strings(size_t string_a_size, char *string_a,
                    size_t string_b_size, char* string_b,
                    size_t dest_size, char *dest)
{
    // TODO: Bounds check dest 
    for(int i = 0; i < string_a_size; ++i)
    {
        *dest++ = *string_a++;
    }
    for(int i = 0; i < string_b_size; ++i)
    {
        *dest++ = *string_b++;
    }
    *dest++ = 0;
}

int string_length(char *string)
{
    int count = 0;
    while(*string++)
    {
        ++count;
    }
    return(count);
}

void swap_shape(Shape *x, Shape *y)
{
    Shape temp = *x;
    *x = *y;
    *y = temp;
}

void sort_transparent_dist(Shape *shapes, int size)
{
    int max;

    for(int i = 0; i < size - 1; ++i)
    {
        max = i;
        for(int j = i + 1; j < size; ++j)
        {
            if(shapes[j].dist_from_cam > shapes[max].dist_from_cam)
            {
                max = j;
            }
            swap_shape(&shapes[max], &shapes[i]);
        }
    }
}

void sort_opaque_dist(Shape *shapes, int size)
{
    int min;

    for(int i = 0; i < size - 1; ++i)
    {
        min = i;
        for(int j = i + 1; j < size; ++j)
        {
            if(shapes[j].dist_from_cam < shapes[min].dist_from_cam)
            {
                min = j;
            }
            swap_shape(&shapes[min], &shapes[i]);
        }
    }
}

void shuffle()
{
    #if 0 
    bool32 shuffled_objects[10];
    bool32 visited[10];
    for(int i = 0; i < 10; ++i)
    {
        visited[i] = false;
    }

    for (int i = 0; i < 10; ++i)
    {
        int j = rand() % 10;
        while(visited[j])
        {
            j = rand() % 10;
        }
        shuffled_objects[i] = objects[j];
        visited[j] = true;
    }  
#endif
}