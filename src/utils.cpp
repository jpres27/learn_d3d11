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

void swap_real32(real32 *x, real32 *y)
{
    real32 temp = *x;
    *x = *y;
    *y = temp;
}

void swap_bool32(bool32 *x, bool32 *y)
{
    bool32 temp = *x;
    *x = *y;
    *y = temp;
}

void swap_matrix(DirectX::XMMATRIX *x, DirectX::XMMATRIX *y)
{
    DirectX::XMMATRIX temp = *x;
    *x = *y;
    *y = temp;
}

void sort_object_dist(real32 distances[], DirectX::XMMATRIX objects[], bool32 shuffled[], int size)
{
    int max;

    for(int i = 0; i < size - 1; ++i)
    {
        max = i;
        for(int j = i + 1; j < size; ++j)
        {
            if(distances[j] > distances[max])
            {
                max = j;
            }
            swap_real32(&distances[max], &distances[i]);
            swap_bool32(&shuffled[max], &shuffled[i]);
            swap_matrix(&objects[max], &objects[i]);
        }
    }
}