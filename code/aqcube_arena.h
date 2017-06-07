#pragma once

//
// Arena
//
struct arena
{
    u64 BaseAddress;
    u64 Size;
    u64 MaxSize;
};

arena InitializeArena(u64 BaseAddress, u64 MaxSize)
{
    arena Result = {};
    Result.BaseAddress = BaseAddress;
    Result.MaxSize = MaxSize;

    return Result;
}

void *PushSize(arena *Arena, u64 Size)
{
    Assert(Arena->Size + Size <= Arena->MaxSize);

    void *Result = (void *)(Arena->BaseAddress + Arena->Size);
    
    Arena->Size += Size;

    return Result;
}

arena SubArena(arena *Arena, u64 Size)
{
    void *Memory = PushSize(Arena, Size);

    arena SubArena = {};
    SubArena.BaseAddress = (u64)Memory;
    SubArena.MaxSize = Size;

    return SubArena;
}

void ClearArena(arena *Arena)
{
    Arena->Size = 0;
}

#define PushStruct(Arena, Structure) (Structure *)PushSize(Arena, sizeof(Structure))

