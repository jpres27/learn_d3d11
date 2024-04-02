#if !defined(STRINGS_H)

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

#define STRINGS_H
#endif