#pragma once

#include "aqcube_defines.h"
#include <math.h>
#include <float.h>

#define PI32 3.14159265359f
#define DEG_TO_RAD (PI32) / (180.0f)
#define RAD_TO_DEG (180.0f) / (PI32)

#define EPSILON 1.192092896e-07f
#define REAL32MAX FLT_MAX

struct vector2
{
    r32 X;
    r32 Y;
};

union vector3
{
    struct
    {
        r32 X, Y, Z;
    };
    struct
    {
        r32 R, G, B;
    };
};

union vector4 
{
    struct
    {
        r32 X, Y, Z, W;
    };
    struct
    {
        r32 C0, C1, C2, C3;
    };
    struct
    {
        r32 R, G, B, A;
    };
};

union rect
{
    struct
    {
        vector2 TopLeft;
        vector2 BottomRight;
    };
    struct
    {
        r32 Left;
        r32 Top;
        r32 Right;
        r32 Bottom;
    };
};

/* NOTE(joe):
 * R0C0 R0C1 R0C2 R0C3
 * R1C0 R1C1 R1C2 R1C3
 * R2C0 R2C1 R2C2 R2C3
 * R3C0 R3C1 R3C2 R3C3
 */
struct matrix44
{
    vector4 R0;
    vector4 R1;
    vector4 R2;
    vector4 R3;
};

//
// Misc.
//

inline r32 Cos(r32 Degrees)
{
    r32 Result = cosf(Degrees * DEG_TO_RAD);
    return Result;
}

inline r32 Sin(r32 Degrees)
{
    r32 Result = sinf(Degrees * DEG_TO_RAD);
    return Result;
}

inline r32 Tan(r32 Degrees)
{
    r32 Result = tanf(Degrees * DEG_TO_RAD);

    return Result;
}

inline r32 ATan(r32 V)
{
    r32 Result = atanf(V) * RAD_TO_DEG;

    return Result;
}

inline r32 SquareRoot(r32 V) { return (r32)sqrt(V); }

inline r32 Square(float a)
{
    float Result = a*a;

    return Result;
}

inline r32 Pow(r32 V, r32 E)
{
    return powf(V, E);
}

inline s32 Abs(s32 V)
{
    s32 Result = (V < 0) ? -V : V;
    return Result;
}

//
// Vector2 operations
//

inline vector2 V2(r32 X, r32 Y)
{
    vector2 Result;
    Result.X = X;
    Result.Y = Y;

    return Result;
}

inline vector2 V2(s32 X, s32 Y) { return V2((r32)X, (r32)Y); }

inline vector2 operator*(vector2 A, r32 B)
{
    vector2 Result { A.X * B, A.Y * B };

    return Result;
}

inline vector2 operator*(r32 A, vector2 B)
{
    vector2 Result { A * B.X, A * B.Y };

    return Result;
}

inline vector2 & operator*=(vector2 &A, vector2 B)
{
    A.X *= B.X;
    A.Y *= B.Y;

    return A;
}

inline vector2 & operator*=(vector2 &A, r32 B)
{
    A.X *= B;
    A.Y *= B;

    return A;
}

inline vector2 operator+(vector2 A, vector2 B)
{
    vector2 Result { A.X + B.X, A.Y + B.Y };

    return Result;
}

inline vector2 & operator+=(vector2 &A, vector2 B)
{
    A.X += B.X;
    A.Y += B.Y;

    return A;
}

inline vector2 Normalize(vector2 V)
{
    r32 Length = SquareRoot(V.X * V.X + V.Y * V.Y);

    vector2 Result;
    Result.X = V.X / Length;
    Result.Y = V.Y / Length;

    return Result;
}
//
// Vector3 operations
//

inline vector3 V3(r32 X, r32 Y, r32 Z)
{
    vector3 Result;
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;

    return Result;
}

inline vector3 V3(s32 X, s32 Y, s32 Z) { return V3((r32)X, (r32)Y, (r32)Z); }

inline vector3 V3(vector4 V)
{
    vector3 Result;
    Result.X = V.X;
    Result.Y = V.Y;
    Result.Z = V.Z;

    return Result;
}

inline vector3 operator-(vector3 A)
{
    vector3 Result = {};
    Result.X = -A.X;
    Result.Y = -A.Y;
    Result.Z = -A.Z;

    return Result;
}

inline vector3 operator+(vector3 A, vector3 B)
{
    vector3 Result = {};

    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    Result.Z = A.Z + B.Z;

    return Result;
}

inline vector3 operator+(r32 S, vector3 A)
{
    vector3 Result = {};

    Result.X = A.X + S;
    Result.Y = A.Y + S;
    Result.Z = A.Z + S;

    return Result;
}
inline vector3 operator+(vector3 A, r32 S)
{
    return operator+(S, A);
}

inline vector3 operator-(vector3 A, vector3 B)
{
    vector3 Result = {};

    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    Result.Z = A.Z - B.Z;

    return Result;
}

inline vector3 operator*(r32 S, vector3 V)
{
    vector3 Result = {};

    Result.X = S * V.X;
    Result.Y = S * V.Y;
    Result.Z = S * V.Z;

    return Result;
}
inline vector3 operator*(vector3 V, r32 S)
{
    return operator*(S, V);
}

inline r32 Length(vector3 V)
{
    r32 Result = SquareRoot(V.X * V.X + V.Y * V.Y + V.Z * V.Z);
    return Result;
}

inline vector3 Normalize(vector3 V)
{
    r32 Length = SquareRoot(V.X * V.X + V.Y * V.Y + V.Z * V.Z);

    vector3 Result;
    Result.X = V.X / Length;
    Result.Y = V.Y / Length;
    Result.Z = V.Z / Length;

    return Result;
}

inline vector3 Hadamard(vector3 A, vector3 B)
{
    vector3 Result;
    Result.X = A.X * B.X;
    Result.Y = A.Y * B.Y;
    Result.Z = A.Z * B.Z;

    return Result;

}

inline r32 Dot(vector3 A, vector3 B)
{
    r32 Result = (A.X * B.X) + (A.Y * B.Y) + (A.Z * B.Z);
    return Result;
}

// NOTE(joe): The formula for the cross product can be produced by finding the determinant of the
// following matrix:
// \  i   j   k \
// \ Ax  Ay  Az \
// \ Bx  By  Bz \
//
// where i = <1, 0, 0>, j = <0, 1, 0>, k = <0, 0, 1>.
inline vector3 Cross(vector3 A, vector3 B)
{
    vector3 Result = {};

    Result.X = A.Y * B.Z - A.Z * B.Y;
    Result.Y = -(A.X * B.Z - A.Z * B.X);
    Result.Z = A.X * B.Y - A.Y * B.X;

    return Result;
}

inline vector3 Reflect(vector3 V, vector3 N)
{
    vector3 Result;
    Result = V - (2 * Dot(V, N) * N);

    return Result;
}

//
// Vector4 Operations
//

inline vector4 V4(r32 X, r32 Y, r32 Z, r32 W)
{
    vector4 Result;
    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;

    return Result;
}

inline vector4 V4(vector3 V, r32 W)
{
    vector4 Result = {};
    Result.X       = V.X;
    Result.Y       = V.Y;
    Result.Z       = V.Z;
    Result.W       = W;

    return Result;
}

inline vector4 operator+(vector4 A, vector4 B)
{
    vector4 Result;
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    Result.Z = A.Z + B.Z;
    Result.W = A.W + B.W;

    return Result;
}

inline vector4 operator*(r32 S, vector4 V)
{
    vector4 Result;
    Result.X = S * V.X;
    Result.Y = S * V.Y;
    Result.Z = S * V.Z;
    Result.W = S * V.W;

    return Result;
}

inline vector4 Hadamard(vector4 A, vector4 B)
{
    vector4 Result;
    Result.X = A.X * B.X;
    Result.Y = A.Y * B.Y;
    Result.Z = A.Z * B.Z;
    Result.W = A.W * B.W;

    return Result;
}

inline r32 Dot(vector4 A, vector4 B)
{
    r32 Result = (A.X * B.X) + (A.Y * B.Y) + (A.Z * B.Z) + (A.W * B.W);
    return Result;
}

//
// Matrix44 Operations
//

inline matrix44 Mat4()
{
    matrix44 Result = {};

    Result.R0.C0 = 1.0f;
    Result.R1.C1 = 1.0f;
    Result.R2.C2 = 1.0f;
    Result.R3.C3 = 1.0f;

    return Result;
}

inline matrix44 operator*(matrix44 A, matrix44 B)
{
    matrix44 Result = {};

    Result.R0.C0 = A.R0.C0 * B.R0.C0 + A.R0.C1 * B.R1.C0 + A.R0.C2 * B.R2.C0 + A.R0.C3 * B.R3.C0;
    Result.R0.C1 = A.R0.C0 * B.R0.C1 + A.R0.C1 * B.R1.C1 + A.R0.C2 * B.R2.C1 + A.R0.C3 * B.R3.C1;
    Result.R0.C2 = A.R0.C0 * B.R0.C2 + A.R0.C1 * B.R1.C2 + A.R0.C2 * B.R2.C2 + A.R0.C3 * B.R3.C2;
    Result.R0.C3 = A.R0.C0 * B.R0.C3 + A.R0.C1 * B.R1.C3 + A.R0.C2 * B.R2.C3 + A.R0.C3 * B.R3.C3;

    Result.R1.C0 = A.R1.C0 * B.R0.C0 + A.R1.C1 * B.R1.C0 + A.R1.C2 * B.R2.C0 + A.R1.C3 * B.R3.C0;
    Result.R1.C1 = A.R1.C0 * B.R0.C1 + A.R1.C1 * B.R1.C1 + A.R1.C2 * B.R2.C1 + A.R1.C3 * B.R3.C1;
    Result.R1.C2 = A.R1.C0 * B.R0.C2 + A.R1.C1 * B.R1.C2 + A.R1.C2 * B.R2.C2 + A.R1.C3 * B.R3.C2;
    Result.R1.C3 = A.R1.C0 * B.R0.C3 + A.R1.C1 * B.R1.C3 + A.R1.C2 * B.R2.C3 + A.R1.C3 * B.R3.C3;

    Result.R2.C0 = A.R2.C0 * B.R0.C0 + A.R2.C1 * B.R1.C0 + A.R2.C2 * B.R2.C0 + A.R2.C3 * B.R3.C0;
    Result.R2.C1 = A.R2.C0 * B.R0.C1 + A.R2.C1 * B.R1.C1 + A.R2.C2 * B.R2.C1 + A.R2.C3 * B.R3.C1;
    Result.R2.C2 = A.R2.C0 * B.R0.C2 + A.R2.C1 * B.R1.C2 + A.R2.C2 * B.R2.C2 + A.R2.C3 * B.R3.C2;
    Result.R2.C3 = A.R2.C0 * B.R0.C3 + A.R2.C1 * B.R1.C3 + A.R2.C2 * B.R2.C3 + A.R2.C3 * B.R3.C3;

    Result.R3.C0 = A.R3.C0 * B.R0.C0 + A.R3.C1 * B.R1.C0 + A.R3.C2 * B.R2.C0 + A.R3.C3 * B.R3.C0;
    Result.R3.C1 = A.R3.C0 * B.R0.C1 + A.R3.C1 * B.R1.C1 + A.R3.C2 * B.R2.C1 + A.R3.C3 * B.R3.C1;
    Result.R3.C2 = A.R3.C0 * B.R0.C2 + A.R3.C1 * B.R1.C2 + A.R3.C2 * B.R2.C2 + A.R3.C3 * B.R3.C2;
    Result.R3.C3 = A.R3.C0 * B.R0.C3 + A.R3.C1 * B.R1.C3 + A.R3.C2 * B.R2.C3 + A.R3.C3 * B.R3.C3;

    return Result;
}

inline vector4 operator*(matrix44 M, vector4 V)
{
    vector4 Result;

    Result.X = (M.R0.C0 * V.X) + (M.R0.C1 * V.Y) + (M.R0.C2 * V.Z) + (M.R0.C3 * V.W);
    Result.Y = (M.R1.C0 * V.X) + (M.R1.C1 * V.Y) + (M.R1.C2 * V.Z) + (M.R1.C3 * V.W);
    Result.Z = (M.R2.C0 * V.X) + (M.R2.C1 * V.Y) + (M.R2.C2 * V.Z) + (M.R2.C3 * V.W);
    Result.W = (M.R3.C0 * V.X) + (M.R3.C1 * V.Y) + (M.R3.C2 * V.Z) + (M.R3.C3 * V.W);

    return Result;
}

inline matrix44 Translate(matrix44 M, vector3 T)
{
    matrix44 Result = M;
    Result.R3.C0    = T.X;
    Result.R3.C1    = T.Y;
    Result.R3.C2    = T.Z;

    return Result;
}

inline matrix44 Scale(matrix44 M, vector3 S)
{
    matrix44 Result = M;
    Result.R0.C0 = S.X;
    Result.R1.C1 = S.Y;
    Result.R2.C2 = S.Z;

    return Result;
}

inline matrix44 LookAt(vector3 Position, vector3 Target, vector3 Up)
{
    vector3 Direction = Normalize(Target - Position); //Normalize(Position - Target);
    vector3 Right     = Normalize(Cross(Direction, Up));

    matrix44 Result = Mat4();
    Result.R0.C0    = Right.X;
    Result.R0.C1    = Right.Y;
    Result.R0.C2    = Right.Z;

    Result.R1.C0 = Up.X;
    Result.R1.C1 = Up.Y;
    Result.R1.C2 = Up.Z;

    Result.R2.C0 = Direction.X;
    Result.R2.C1 = Direction.Y;
    Result.R2.C2 = Direction.Z;

    Result.R0.C3    = Position.X;
    Result.R1.C3    = Position.Y;
    Result.R2.C3    = Position.Z;

    return Result;
}

//
// Rectangle
//

inline rect Rect(vector2 TopLeft, vector2 BottomRight)
{
    rect Result = {TopLeft, BottomRight};
    return Result;
}

inline r32 Width(rect R)
{
    r32 Result = R.Right - R.Left;
    return Result;
}

inline r32 Height(rect R)
{
    r32 Result = R.Bottom - R.Top;
    return Result;
}

inline vector2 Center(rect R)
{
    vector2 Result = {};
    Result.X = (R.TopLeft.X + R.BottomRight.X) * 0.5f;
    Result.Y = (R.TopLeft.Y + R.BottomRight.Y) * 0.5f;

    return Result;
}

