#include <math.h>
typedef struct rect
{
    u32 x;
    u32 y;
    u32 w;
    u32 h;
} rect;

static inline u32 saturating_sub(u32 sub_from, u32 sub_amount)
{
    u32 result = (sub_amount > sub_from) ? 0 : (sub_from - sub_amount);
    return result;
}

static inline u32 clamped_add(u32 add_to, u32 add_amount, u32 max)
{
    u32 result = add_to + add_amount;
    if (result > max)
    {
        result = max;
    }
    return result;
}

static const f32 PI = 3.14159265358979323846f;

f32 radians(f32 degrees)
{ 
    f32 result = (acos(-1.0f) / 180) * degrees;
    return result;
}

typedef struct v2f
{
    f32 x;
    f32 y;
} v2f;

typedef struct v2i
{
    i32 x;
    i32 y;
} v2i;


typedef struct rect_i
{
    i32 x;
    i32 y;
    i32 w;
    i32 h;
} rect_i;


typedef struct
{
    v2f top_left;
    v2f top_right;
    v2f bot_left;
    v2f bot_right;
} quadf;

typedef struct
{
    union
    {
        struct
        {
            f32 x;
            f32 y;
            f32 z;
        };

        struct
        {
            f32 v[3];
        };
    };
} v3f;

f32 len_sq(v3f v)
{
    f32 result = v.x * v.x + v.y * v.y + v.z * v.z;
    return result;
}

f32 len(v3f v)
{
    f32 result = sqrt(len_sq(v));
    return result;
}

v2f clamp_v2f(v2f a, v2f b)
{
    v2f result = a;
    if (a.x < 0)
    {
        if (a.x < - b.x)
        {
            result.x = -b.x;
        }
    }
    else if (a.x > b.x)
    {
        result.x = b.x;
    }

    if (a.y < 0)
    {
        if (a.y < - b.y)
        {
            result.y = -b.y;
        }
    }
    else if (a.y > b.y)
    {
        result.y = b.y;
    }

    return result;


}

v2f add_v2f(v2f a, v2f b)
{
    v2f result = {
        .x = a.x + b.x,
        .y = a.y + b.y,
    };
    return result;
};

v2f sub_v2f(v2f a, v2f b)
{
    v2f result = {
        .x = a.x - b.x,
        .y = a.y - b.y,
    };
    return result;
};

v2f scalar_mulv2f(f32 s, v2f a)
{
    v2f result = {
        .x = a.x * s,
        .y = a.y * s,
    };
    return result;
}

v3f subv3f(v3f a, v3f b)
{
    v3f result = {
        .x = a.x - b.x,
        .y = a.y - b.y,
        .z = a.y - b.z,
    };
    return result;
};

v3f normalize0(v3f v)
{
    f32 norm_inv = 1.0f / len(v);
    v3f result = { 
        .x = norm_inv * v.x,
        .y = norm_inv * v.y,
        .z = norm_inv * v.z
    };
    return result;
}

typedef struct
{
    union
    {
        struct
        {
            f32 x;
            f32 y;
            f32 z;
            f32 w;
        };
        struct
        {
            f32 r;
            f32 g;
            f32 b;
            f32 a;
        };
        struct
        {
            f32 v[4];

        };
    };
} v4f;



// row major;
typedef struct
{
    f32 mat[16];
} mat4f;

mat4f identity()
{
    mat4f result = { .mat =  
        {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        }
    };
    return result;
}


mat4f mul(mat4f a, mat4f b)
{
    mat4f result = {};
    for (u32 i = 0; i < 4; ++i)
    {
        for (u32 j = 0; j < 4; ++j)
        {
            for (u32 k = 0; k < 4; ++k)
            {
                result.mat[i * 4 + j] += a.mat[i * 4 + k] * b.mat[k * 4 + j];
            }
        }
    }
    return result;
}

mat4f scale(v4f v)
{
    mat4f result =  { .mat = 
        {
            v.x, 0.0f, 0.0f, 0.0f,
            0.0f, v.y, 0.0f, 0.0f,
            0.0f, 0.0f, v.z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        }
    };
    return result;
}

v3f cross_v3f(v3f a, v3f b)
{
    v3f result = {
        .x = a.y * b.z - a.z * b.y,
        .y = a.z * b.x - a.x * b.z,
        .z = a.x * b.y - a.y * b.x,
    };
    return result;
}

f32 to_radians(f32 degrees)
{
    f32 result = degrees * PI / 180.0f;
    return result;
}

mat4f rotate(f32 o, v3f axis)
{
    mat4f result =  { .mat = 
        {
            cos(o) + axis.x * axis.x * (1 - cos(o)),
            axis.x * axis.y * (1 - cos(o)) - axis.z * sin(o),
            axis.x * axis.z * (1 - cos(o)) + axis.y * sin(o),
            0.0f,
            axis.y * axis.x * (1 - cos(o)) + axis.z * sin(o),
            cos(o) + axis.y * axis.y * (1 - cos(o)),
            axis.y * axis.z * (1 - cos(o)) - axis.x * sin(o),
            0.0f,
            axis.z * axis.x * (1 - cos(o)) - axis.y * sin(o),
            axis.z * axis.y * (1 - cos(o)) + axis.x * sin(o),
            cos(o) + axis.z * axis.z * (1 - cos(o)),
            0,
            0, 
            0,
            0,
            1,
        }
    };
    return result;
}

v4f mul_v(mat4f a, v4f b)
{
    v4f result = {};
    for (u32 i = 0; i < 4; ++i)
    {
        for (u32 k = 0; k < 4; ++k)
        {
            result.v[i] += a.mat[i * 4 + k] * b.v[k];
        }
    }
    return result;
}

mat4f orthographic(f32 l, f32 r, f32 b, f32 t, f32 n, f32 f)
{
    mat4f result = { .mat = 
        {   
            2.0f/(r-l),      0.0f,            0.0f,            0.0f,
            0.0f,            2.0f/(t-b),      0.0f,            0.0f,
            0.0f,            0,           -2.0f/(f-n),      0.0f,
            -(r+l)/(r-l),           -(t+b)/(t-b),            -(f+n)/(f-n), 1.0f
        }
    };
    return result;
}

mat4f translation(v3f trans)
{
    mat4f result = { .mat =  
        {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            trans.x, trans.y, trans.z, 1.0f
        }
    };
    return result;
}


mat4f perspective0(f32 l, f32 r, f32 b, f32 t, f32 n, f32 f)
{
    mat4f result = { .mat = 
        {   
            2*n/(r-l),  0,            (r+l)/(r-l),   0,
            0,          2*n/(t-b),    (t+b)/(t-b),   0,
            0,          0,             -(f+n)/(f-n), -(2*f*n)/(f-n),
            0,          0,            -1           , 0
        }
    };
    return result;
}

mat4f perspective(
    f32 fov_radians,
    f32 aspect,
    f32 near,
    f32 far)
{
    f32 f = 1.0f / tanf(fov_radians * 0.5f);

    mat4f result = {
        .mat =
        {
            f / aspect, 0, 0, 0,
            0,          f, 0, 0,
            0,          0, (far + near) / (near - far), (2.0f * far * near) / (near - far),
            0,          0, -1, 0
        }
    };

    return result;
}

mat4f make_frustum(f32 fovY, f32 a, f32 n, f32 f)
{
    f32 s = 1 / tan(fovY / 2);
    // params: left, right, bottom, top, near(front), far(back)
    mat4f matrix = {};
    f32 *mat = (f32 *) matrix.mat;

    mat[0]  =  s / a;
    mat[5]  =  s;
    mat[10] = (f + n) / (n - f);
    mat[11] =  (2 * f * n) / (n - f);
    mat[14] = - 1;
    return matrix;
}


