#pragma once

struct vector2
{
    float X, Y;
};

inline vector2 Vector2(float x, float y)
{
    vector2 Result{ x, y };

    return Result;
}

inline vector2 operator*(vector2 a, float b)
{
    vector2 Result;
    Result.X = a.X * b;
    Result.Y = a.Y * b;

    return Result;

}

inline vector2 operator+(vector2 a, vector2 b)
{
    vector2 Result;
    Result.X = a.X + b.X;
    Result.Y = a.Y + b.Y;

    return Result;
}

