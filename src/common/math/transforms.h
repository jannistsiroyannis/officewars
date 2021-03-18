#ifndef TRANSFORMS_H
#define TRANSFORMS_H

#include <math.h>
#include <assert.h>

#include "matrix.h"

#define MATRIX_STACK_DEPTH 8

void pushModelMatrix();
void popModelMatrix();
void pushViewMatrix();
void popViewMatrix();
void setViewIdentity();
void setModelIdentity();
void setProjectionIdentity();
Mat4 getModelMatrix();
Mat4 getViewMatrix();
Mat4 getProjectionMatrix();
void translate(Vec3 direction);
void scale(Vec3 dimensions);
void rotate(float angle, Vec3 direction);
void applyModelTransform(Mat4 transform);
void setModelTransform(Mat4 transform);
void setViewTransform(Mat4 transform);
void setPerspectiveProjection(float fovy, float aspect, float zNear, float zFar);
void setOrthogonalProjection(
	float left,
	float right,
	float bottom,
	float top,
	float zNear,
	float zFar);
void lookAt(Vec3 eye, Vec3 center, Vec3 up);
Vec3 viewportTransform(Vec4 v, float screenWidth, float screenHeight, float nearClip, float farClip);

#endif
