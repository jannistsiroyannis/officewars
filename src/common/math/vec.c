#include <math.h>

#include "vec.h"

Vec3 V3Add(Vec3 a, Vec3 b)
{
	Vec3 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	return result;
}

Vec3 V3Subt(Vec3 a, Vec3 b)
{
	Vec3 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	return result;
}

Vec3 V3ScalarMult(float scalar, Vec3 vector)
{
	Vec3 result;
	result.x = vector.x * scalar;
	result.y = vector.y * scalar;
	result.z = vector.z * scalar;
	return result;
}

Vec3 V3RotateAroundV3(Vec3 vector, float angle, Vec3 around)
{
	/*
	    x^2(1-c)+c     xy(1-c)-zs     xz(1-c)+ys     0;
	    yx(1-c)+zs     y^2(1-c)+c     yz(1-c)-xs     0;
	    xz(1-c)-ys     yz(1-c)+xs     z^2(1-c)+c     0;
	    0              0               0        1;*/

	float x = around.x;
	float y = around.y;
	float z = around.z;

	float norm = sqrt(x*x + y*y + z*z);
	x /= norm;
	y /= norm;
	z /= norm;

	float c = cos(angle);
	float s = sin(angle);
	float rotationMatrix[16];

	rotationMatrix[0] = x*x*(1-c)+c;
	rotationMatrix[1] = x*y*(1-c)-z*s;
	rotationMatrix[2] = x*z*(1-c)+y*s;
	rotationMatrix[3] = 0.0f;
	rotationMatrix[4] = y*x*(1-c)+z*s;
	rotationMatrix[5] = y*y*(1-c)+c;
	rotationMatrix[6] = y*z*(1-c)-x*s;
	rotationMatrix[7] = 0.0f;
	rotationMatrix[8] = x*z*(1-c)-y*s;
	rotationMatrix[9] = y*z*(1-c)+x*s;
	rotationMatrix[10] = z*z*(1-c)+c;
	rotationMatrix[11] = 0.0f;
	rotationMatrix[12] = 0.0f;
	rotationMatrix[13] = 0.0f;
	rotationMatrix[14] = 0.0f;
	rotationMatrix[15] = 1.0f;

	Vec3 result;
	result.x = vector.x * rotationMatrix[0] + vector.y * rotationMatrix[1] + vector.z * rotationMatrix[2] + rotationMatrix[3];
	result.y = vector.x * rotationMatrix[4] + vector.y * rotationMatrix[5] + vector.z * rotationMatrix[6] + rotationMatrix[7];
	result.z = vector.x * rotationMatrix[8] + vector.y * rotationMatrix[9] + vector.z * rotationMatrix[10] + rotationMatrix[11];
	return result;
}

Vec3 V3CrossMult(Vec3 a, Vec3 b)
{
	Vec3 result;
	result.x = a.y * b.z - a.z * b.y;
	result.y = a.z * b.x - a.x * b.z;
	result.z = a.x * b.y - a.y * b.x;
	return result;
}

float V3DotMult(Vec3 a, Vec3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 V3Normalized(Vec3 vector)
{
	Vec3 result;
	float norm = sqrt( vector.x * vector.x +
			vector.y * vector.y +
			vector.z * vector.z );
	if (norm != 0.0)
	{
		result.x = vector.x / norm;
		result.y = vector.y / norm;
		result.z = vector.z / norm;
	}
	else
		return vector;
	return result;
}

float V3SqrDist(Vec3 a, Vec3 b)
{
	return (a.x - b.x) * (a.x - b.x) +
			(a.y - b.y) * (a.y - b.y) +
			(a.z - b.z) * (a.z - b.z);
}

float V3Length(Vec3 vector)
{
	return sqrt( vector.x * vector.x +
			vector.y * vector.y +
			vector.z * vector.z );
}

Vec2 V2Add(Vec2 a, Vec2 b)
{
	Vec2 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	return result;
}

Vec2 V2Subt(Vec2 a, Vec2 b)
{
	Vec2 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

Vec2 V2ScalarMult(float scalar, Vec2 vector)
{
	Vec2 result;
	result.x = vector.x * scalar;
	result.y = vector.y * scalar;
	return result;
}

float V2DotMult(Vec2 a, Vec2 b)
{
	return a.x * b.x + a.y * b.y;
}

Vec2 V2Normalized(Vec2 vector)
{
	Vec2 result;
	float norm = sqrt( vector.x * vector.x +
			vector.y * vector.y);
	if (norm != 0.0)
	{
		result.x = vector.x / norm;
		result.y = vector.y / norm;
	}
	else
		return vector;
	return result;
}

float V2SqrDist(Vec2 a, Vec2 b)
{
	return (a.x - b.x) * (a.x - b.x) +
			(a.y - b.y) * (a.y - b.y);
}

float V2Length(Vec2 vector)
{
	return sqrt( vector.x * vector.x +
			vector.y * vector.y);
}
