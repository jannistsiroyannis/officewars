#ifndef VECTOR_H
#define VECTOR_H

/*******************************************************************************
 * Global types
 *
 ******************************************************************************/

typedef struct
{
	float x;
	float y;
	float z;
	float w;
} Vec4;

typedef struct
{
	float x;
	float y;
	float z;
} Vec3;

typedef struct
{
	float x;
	float y;
} Vec2;

/*******************************************************************************
 * Global functions
 *
 ******************************************************************************/
static inline Vec4 V3toV4(Vec3 v3) { Vec4 v4 = {v3.x, v3.y, v3.z, 1.0}; return v4; };
static inline Vec3 V4toV3(Vec4 v4) { Vec3 v3 = {v4.x, v4.y, v4.z}; return v3; };

static inline Vec3 V2toV3(Vec2 v2) { Vec3 v3 = {v2.x, v2.y, 0.0}; return v3; };
static inline Vec2 V3toV2(Vec3 v3) { Vec2 v2 = {v3.x, v3.y}; return v2; };

Vec3 V3Add(Vec3, Vec3);
Vec3 V3Subt(Vec3, Vec3);
Vec3 V3ScalarMult(float, Vec3);
Vec3 V3CrossMult(Vec3, Vec3);
float V3DotMult(Vec3, Vec3);
Vec3 V3RotateAroundV3(Vec3 vector, float angle, Vec3 around);
Vec3 V3Normalized(Vec3 vector);
float V3SqrDist(Vec3 a, Vec3 b);
float V3Length(Vec3 vector);

Vec2 V2Add(Vec2, Vec2);
Vec2 V2Subt(Vec2, Vec2);
Vec2 V2ScalarMult(float, Vec2);
Vec2 V2CrossMult(Vec2, Vec2);
float V2DotMult(Vec2, Vec2);
Vec2 V2Normalized(Vec2 vector);
float V2SqrDist(Vec2 a, Vec2 b);
float V2Length(Vec2 vector);

static inline Vec3 V3Project(Vec3 u, Vec3 a)
{
	// Project u on a
	float t = V3DotMult(u, a);
	float la = V3Length(a);
	float n = la*la;
	Vec3 projection = V3ScalarMult(t/n, a);
	if (t < 0) // correct for sign
		projection = V3ScalarMult(-1.0, projection);
	return projection;
}

static inline unsigned int
V3PackSinglePrecision(Vec3 v, void* area)
{
	*((float*) (area + sizeof(float) * 0)) = (float) v.x;
	*((float*) (area + sizeof(float) * 1)) = (float) v.y;
	*((float*) (area + sizeof(float) * 2)) = (float) v.z;
	return sizeof(float)*3;
}

static inline Vec3
V3UnpackSinglePrecision(void* area)
{
	Vec3 v =
	{
		*((float*)(area + sizeof(float) * 0)),
		*((float*)(area + sizeof(float) * 1)),
		*((float*)(area + sizeof(float) * 2)),
	};
	return v;
}

static inline unsigned int
V2PackSinglePrecision(Vec2 v, void* area)
{
	*((float*) (area + sizeof(float) * 0)) = (float) v.x;
	*((float*) (area + sizeof(float) * 1)) = (float) v.y;
	return sizeof(float)*2;
}

static inline Vec2
V2UnpackSinglePrecision(void* area)
{
	Vec2 v =
	{
		*((float*)(area + sizeof(float) * 0)),
		*((float*)(area + sizeof(float) * 1)),
	};
	return v;
}

static inline float
floatmin(float a, float b)
{
	return (a < b) ? a : b;
}

static inline float
floatmax(float a, float b)
{
	return (a < b) ? b : a;
}

static inline double
doublemin(double a, double b)
{
	return (a < b) ? a : b;
}

static inline double
doublemax(double a, double b)
{
	return (a < b) ? b : a;
}

static inline float
floatabs(float d)
{
	return (d >= 0.0) ? d : -d;
}


#endif
