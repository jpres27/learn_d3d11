void start_timer()
{
    LARGE_INTEGER frequency_count;
    QueryPerformanceFrequency(&frequency_count);

    counts_per_second = double(frequency_count.QuadPart);

    QueryPerformanceCounter(&frequency_count);
    counter_start = frequency_count.QuadPart;
}

real64 get_time()
{
    LARGE_INTEGER current_time;
    QueryPerformanceCounter(&current_time);
    return real64(current_time.QuadPart - counter_start)/counts_per_second;
}

real64 get_frame_time()
{
    LARGE_INTEGER current_time;
    int64 tick_count;
    QueryPerformanceCounter(&current_time);

    tick_count = current_time.QuadPart - frame_time_old;
    frame_time_old = current_time.QuadPart;

    if(tick_count < 0)
        tick_count = 0;

    return real32(tick_count)/counts_per_second;
}