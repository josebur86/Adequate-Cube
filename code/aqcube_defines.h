#pragma once

#include <stdint.h>

typedef float r32;

typedef int8_t s8;
typedef uint8_t u8;

typedef int16_t s16;
typedef uint16_t u16;

typedef int32_t s32;
typedef uint32_t u32;

typedef int64_t s64;
typedef uint64_t u64;

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) ((Kilobytes(Value)) * 1024)
#define Gigabytes(Value) ((Megabytes(Value)) * 1024)

#define Assert(Condition) if (!(Condition)) { *(int *)0 = 0; }
#define ArrayCount(Array) sizeof((Array)) / sizeof((Array[0]))

