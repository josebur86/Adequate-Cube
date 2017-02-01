#pragma once

inline float Square(float a)
{
    float Result = a*a;

    return Result;
}

struct vector2
{
    float X, Y;
};

inline vector2 Vector2(float x, float y)
{
    vector2 Result{ x, y };

    return Result;
}
inline vector2 operator-(vector2 a)
{
    vector2 Result;

    Result.X = -a.X;
    Result.Y = -a.Y;

    return Result;
}

inline vector2 operator*(vector2 a, float b)
{
    vector2 Result;
    Result.X = a.X * b;
    Result.Y = a.Y * b;

    return Result;

}
inline vector2 operator*(float a, vector2 b)
{
    vector2 Result;
    Result.X = a * b.X;
    Result.Y = a * b.Y;

    return Result;

}

inline vector2 operator+(vector2 a, vector2 b)
{
    vector2 Result;
    Result.X = a.X + b.X;
    Result.Y = a.Y + b.Y;

    return Result;
}

