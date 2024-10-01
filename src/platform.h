#pragma once

// 
// COMPILERS
// 

#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#endif

#define KEY_MESSAGE_WAS_DOWN_BIT (1 << 30)
#define KEY_MESSAGE_IS_DOWN_BIT (1 << 31)
#define ALT_KEY_DOWN_BIT (1 << 29)

#define GET_X_LPARAM(lp)    ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)    ((int)(short)HIWORD(lp))

#define kilobytes(value) ((value) * 1024)
#define megabytes(value) (kilobytes(value) * 1024)
#define gigabytes(value) (megabytes(value) * 1024)
#define terabytes(value) (gigabytes(value) * 1024)

#define array_count(array) (sizeof(array) / sizeof((array)[0]))

struct Window_Dimensions
{
    int width;
    int height;
};

struct Game_Button_State
{
    int half_transistion_count;
    bool32 ended_down;

};

struct Mouse_State
{
    bool32 rotated;
    int rot_x;
    int rot_y;
};

struct Game_Controller_Input
{
    bool32 is_connected;

    union 
    {
        Game_Button_State buttons[10];
        struct 
        {
            Game_Button_State move_fwd;
            Game_Button_State move_back;
            Game_Button_State move_left;
            Game_Button_State move_right;

            Game_Button_State attack;
            Game_Button_State block;
            Game_Button_State inventory;
            Game_Button_State journal;
            Game_Button_State use;
            Game_Button_State jump;

            // NOTE: All buttons must be added above this line for bounds checking
            Game_Button_State terminator;
        };
        
    };
    
};

struct Game_Memory
{
    bool32 is_initialized;

    uint64 total_storage_size;
    void *game_memory_block;

    uint64 permanent_storage_size;
    void *permanent_storage; // NOTE: Since this is allocated with VitualAlloc, it is automatically
                             //       cleared to zero at startup
    uint64 transient_storage_size;
    void *transient_storage;
};

struct Game_State
{

};
