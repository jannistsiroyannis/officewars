#ifndef MATRIX_H
#define MATRIX_H

#include "vec.h"

/*******************************************************************************
 * Global types
 *
 ******************************************************************************/

typedef struct
{
	float data[16];
} Mat4;

typedef struct
{
	float data[9];
} Mat3;

/*******************************************************************************
 * Global functions
 *
 ******************************************************************************/

static inline Mat3 M4toM3(Mat4 m4)
{
	Mat3 m3 =
	{{
		m4.data[0], m4.data[1], m4.data[2],
		m4.data[4], m4.data[5], m4.data[6],
		m4.data[8], m4.data[9], m4.data[10]
	}};
	return m3;
}

static inline Mat4 M4GetIdentity() { Mat4 m = {{	1.0, 0.0, 0.0, 0.0,
													0.0, 1.0, 0.0, 0.0,
													0.0, 0.0, 1.0, 0.0,
													0.0, 0.0, 0.0, 1.0 }};
													return m; }

Mat4 M4multiply(const Mat4* first, const Mat4* second);

void M4print(const char* description, const Mat4* matrix);

Mat4 M4invert(const Mat4* matrix);

Mat4 M4transpose(const Mat4* matrix);

Vec4 M4V4Multiply(const Mat4* mat, Vec4 vec);

Vec3 M3V3Multiply(const Mat3* mat, Vec3 vec);

#endif