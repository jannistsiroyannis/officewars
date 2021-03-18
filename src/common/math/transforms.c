#include "transforms.h"

#define MATRIX_STACK_DEPTH 8

/*******************************************************************************
 * Local variables
 *
 ******************************************************************************/
static Mat4 modelMatrix;
static Mat4 viewMatrix;
static Mat4 projectionMatrix;

static Mat4 modelMatrixStack[MATRIX_STACK_DEPTH];
static int modelMatrixStackCounter = -1;

static Mat4 viewMatrixStack[MATRIX_STACK_DEPTH];
static int viewMatrixStackCounter = -1;

/*******************************************************************************
 * Global functions
 *
 ******************************************************************************/
void pushModelMatrix()
{
	assert( modelMatrixStackCounter < MATRIX_STACK_DEPTH - 1 );
	modelMatrixStack[++modelMatrixStackCounter] = modelMatrix;
}

void popModelMatrix()
{
	assert( modelMatrixStackCounter > -1 );
	modelMatrix = modelMatrixStack[modelMatrixStackCounter--];
}

void pushViewMatrix()
{
	assert( viewMatrixStackCounter < MATRIX_STACK_DEPTH - 1 );
	viewMatrixStack[++viewMatrixStackCounter] = viewMatrix;
}

void popViewMatrix()
{
	assert( viewMatrixStackCounter > -1 );
	viewMatrix = viewMatrixStack[viewMatrixStackCounter--];
}


void setViewIdentity()
{
	viewMatrix = M4GetIdentity();
}

void setModelIdentity()
{
	modelMatrix = M4GetIdentity();
}

void setProjectionIdentity()
{
	projectionMatrix = M4GetIdentity();
}

Mat4 getModelMatrix()
{
	return modelMatrix;
}

Mat4 getViewMatrix()
{
	return viewMatrix;
}

Mat4 getProjectionMatrix()
{
	return projectionMatrix;
}

void translate(Vec3 direction)
{
	Mat4 translationMatrix = {{
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		direction.x, direction.y, direction.z, 1.0f }};
	
	modelMatrix = M4multiply(&translationMatrix, &modelMatrix);
}

void scale(Vec3 dimensions)
{
	Mat4 scalingMatrix = {{
		dimensions.x, 0.0, 0.0, 0.0,
		0.0, dimensions.y, 0.0, 0.0,
		0.0, 0.0, dimensions.z, 0.0,
		0.0, 0.0, 0.0, 1.0 }};
	
	modelMatrix = M4multiply(&scalingMatrix, &modelMatrix);
}

void rotate(float angle, Vec3 direction)
{
	/*
	x^2(1-c)+c     xy(1-c)-zs     xz(1-c)+ys     0;
	yx(1-c)+zs     y^2(1-c)+c     yz(1-c)-xs     0;
	xz(1-c)-ys     yz(1-c)+xs     z^2(1-c)+c     0;
	0              0               0        1;*/
	
	Vec3 normalizedDirection = V3Normalized(direction);
	float x = normalizedDirection.x;
	float y = normalizedDirection.y;
	float z = normalizedDirection.z;
	float c = cos(angle);
	float s = sin(angle);
	Mat4 rotationMatrix;
 
	rotationMatrix.data[0] = x*x*(1-c)+c;
	rotationMatrix.data[1] = x*y*(1-c)-z*s;
	rotationMatrix.data[2] = x*z*(1-c)+y*s;
	rotationMatrix.data[3] = 0.0;
	rotationMatrix.data[4] = y*x*(1-c)+z*s;
	rotationMatrix.data[5] = y*y*(1-c)+c;
	rotationMatrix.data[6] = y*z*(1-c)-x*s;
	rotationMatrix.data[7] = 0.0;
	rotationMatrix.data[8] = x*z*(1-c)-y*s;
	rotationMatrix.data[9] = y*z*(1-c)+x*s;
	rotationMatrix.data[10] = z*z*(1-c)+c;
	rotationMatrix.data[11] = 0.0;
	rotationMatrix.data[12] = 0.0;
	rotationMatrix.data[13] = 0.0;
	rotationMatrix.data[14] = 0.0;
	rotationMatrix.data[15] = 1.0;

	modelMatrix = M4multiply(&rotationMatrix, &modelMatrix);
}

void applyModelTransform(Mat4 transform)
{
	modelMatrix = M4multiply(&transform, &modelMatrix);
}

void setModelTransform(Mat4 transform)
{
	modelMatrix = transform;
}

void setViewTransform(Mat4 transform)
{
	viewMatrix = transform;
}

void setPerspectiveProjection(float fovy, float aspect, float zNear, float zFar)
{
	/*
            f
        ------------       0              0              0
           aspect


            0              f              0              0

                                      zFar+zNear    2*zFar*zNear
            0              0          ----------    ------------
                                      zNear-zFar     zNear-zFar

            0              0              -1             0
    */
	
	float f = 1/tan(fovy/2.0);

	projectionMatrix.data[0] = f / aspect;
	projectionMatrix.data[1] = 0.0;
	projectionMatrix.data[2] = 0.0;
	projectionMatrix.data[3] = 0.0;

	projectionMatrix.data[4] = 0.0;
	projectionMatrix.data[5] = f;
	projectionMatrix.data[6] = 0.0;
	projectionMatrix.data[7] = 0.0;

	projectionMatrix.data[8] = 0.0;
	projectionMatrix.data[9] = 0.0;
	projectionMatrix.data[10] = (zFar+zNear)/(zNear-zFar);
	projectionMatrix.data[11] = -1.0;

	projectionMatrix.data[12] = 0.0;
	projectionMatrix.data[13] = 0.0;
	projectionMatrix.data[14] = 2.0*zFar*zNear/(zNear-zFar);
	projectionMatrix.data[15] = 0.0;
}

void setOrthogonalProjection(
	float left,
	float right,
	float bottom,
	float top,
	float zNear,
	float zFar)
{
	/*
              2
        ------------       0              0              tx
        right - left

                           2
            0         ------------        0              ty
                      top - bottom


                                          -2
            0              0         ------------        tz
                                      zFar-zNear

            0              0              0              1


       where

                       tx = - (right + left) / (right - left)

                       ty = - (top + bottom) / (top - bottom)

                       tz = - (zFar + zNear) / (zFar - zNear)
    */
	float tx = - (right + left) / (right - left);
	float ty = - (top + bottom) / (top - bottom);
	float tz = - (zFar + zNear) / (zFar - zNear);


	projectionMatrix.data[0] = 2.0f / (right - left);
	projectionMatrix.data[1] = 0.0f;
	projectionMatrix.data[2] = 0.0f;
	projectionMatrix.data[3] = 0.0f;

	projectionMatrix.data[4] = 0.0f;
	projectionMatrix.data[5] = 2.0f / (top - bottom);
	projectionMatrix.data[6] = 0.0f;
	projectionMatrix.data[7] = 0.0f;

	projectionMatrix.data[8] = 0.0f;
	projectionMatrix.data[9] = 0.0f;
	projectionMatrix.data[10] = -2.0f / (zFar - zNear);
	projectionMatrix.data[11] = 0.0f;

	projectionMatrix.data[12] = tx;
	projectionMatrix.data[13] = ty;
	projectionMatrix.data[14] = tz;
	projectionMatrix.data[15] = 1.0f;
}

void lookAt(Vec3 eye, Vec3 center, Vec3 up)
{
	Vec3 yVec = V3Normalized( up );
	Vec3 zVec = V3Normalized( V3Subt(eye, center) );
	Vec3 xVec = V3CrossMult(yVec, zVec);
	yVec = V3CrossMult(zVec, xVec);
	
	Mat4 rotationMatrix = {{
		xVec.x, yVec.x, zVec.x, 0,
		xVec.y, yVec.y, zVec.y, 0,
		xVec.z, yVec.z, zVec.z, 0,
		0, 0, 0, 1
	}};
	
	Mat4 translationMatrix = {{
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		-eye.x, -eye.y, -eye.z, 1.0f }};
	
	//viewMatrix = M4multiply(&viewMatrix, &rotationMatrix); // Possibly backwards!
		
	viewMatrix = rotationMatrix;
	viewMatrix = M4multiply(&translationMatrix, &viewMatrix);
}

Vec3 viewportTransform(Vec4 v, float screenWidth, float screenHeight, float nearClip, float farClip)
{
   Vec3 sc = {
      ( screenWidth / 2.0 ) * v.x + ( screenWidth / 2.0 ),
      ( screenHeight / 2.0) * v.y + ( screenHeight / 2.0),
      ( farClip - nearClip ) * v.z / 2.0 + ( farClip + nearClip ) / 2.0
   };

   return sc;
}
