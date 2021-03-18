#include <stdio.h>

#include "matrix.h"

Mat4 M4multiply(const Mat4* first, const Mat4* second)
{
	Mat4 result;
	
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			result.data[row*4+column] = 0.0f;
			for (int i = 0; i < 4; ++i)
			{
				result.data[row*4+column] += first->data[row * 4 + i] * second->data[column + i * 4];
			}
		}
	}
	
	return result;
}

void M4print(const char* description, const Mat4* matrix)
{
	printf("%s=\n"
	 "%f\t%f\t%f\t%f\n"
	 "%f\t%f\t%f\t%f\n"
	 "%f\t%f\t%f\t%f\n"
	 "%f\t%f\t%f\t%f\n",
	 description,
	 matrix->data[0],
	 matrix->data[1],
	 matrix->data[2],
	 matrix->data[3],
	 matrix->data[4],
	 matrix->data[5],
	 matrix->data[6],
	 matrix->data[7],
	 matrix->data[8],
	 matrix->data[9],
	 matrix->data[10],
	 matrix->data[11],
	 matrix->data[12],
	 matrix->data[13],
	 matrix->data[14],
	 matrix->data[15]);
}

Mat4 M4invert(const Mat4* matrix)
{
	const float* m = matrix->data;
	Mat4 result;
    float determinant;

    // first column
	result.data[0] =   m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
	+ m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
	result.data[1] =  -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15]
	- m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
	result.data[2] =   m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15]
	+ m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
	result.data[3] =  -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11]
	- m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];

    // second column
	result.data[4] =  -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15]
	- m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
	result.data[5] =   m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15]
	+ m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
	result.data[6] =  -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15]
	- m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
	result.data[7] =   m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11]
	+ m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];

    // third column
	result.data[8] =   m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15]
	+ m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
	result.data[9] =  -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15]
	- m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
	result.data[10] =  m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15]
	+ m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
	result.data[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11]
	- m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];

    // fourth column
	result.data[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14]
	- m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
	result.data[13] =  m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14]
	+ m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
	result.data[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14]
	- m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
	result.data[15] =  m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10]
	+ m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

	determinant = m[0]*result.data[0] + m[1]*result.data[4] + m[2]*result.data[8] + m[3]*result.data[12];
	if (determinant == 0)
	{
		printf("ERROR: Attempted to invert matrix with determinant of 0.");
		return *matrix;
	}

	determinant = 1.0 / determinant;

	for (int i = 0; i < 16; i++)
		result.data[i] *= determinant;

	return result;
}

Mat4 M4transpose(const Mat4* matrix)
{
	Mat4 transposed =
	{
		{
			matrix->data[0], matrix->data[4], matrix->data[8], matrix->data[12],
			matrix->data[1], matrix->data[5], matrix->data[9], matrix->data[13],
			matrix->data[2], matrix->data[6], matrix->data[10], matrix->data[14],
			matrix->data[3], matrix->data[7], matrix->data[11], matrix->data[15],
		}
	};
	return transposed;
}

Vec4 M4V4Multiply(const Mat4* mat, Vec4 v)
{
	Vec4 result;
	const float* m = mat->data;

    result.x = m[0]*v.x + m[4]*v.y + m[8]*v.z + m[12]*v.w;
    result.y = m[1]*v.x + m[5]*v.y + m[9]*v.z + m[13]*v.w;
    result.z = m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w;
    result.w = m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w;

    return result;
}

Vec3 M3V3Multiply(const Mat3* mat, Vec3 v)
{
	Vec3 result;
	const float* m = mat->data;

    result.x = m[0]*v.x + m[3]*v.y + m[6]*v.z;
    result.y = m[1]*v.x + m[4]*v.y + m[7]*v.z;
    result.z = m[2]*v.x + m[5]*v.y + m[8]*v.z;

    return result;
}
