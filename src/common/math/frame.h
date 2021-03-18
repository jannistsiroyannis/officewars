#ifndef FRAME_H
#define FRAME_H

#include "vec.h"
#include "matrix.h"

/*******************************************************************************
 * Global Types
 *
 ******************************************************************************/

typedef struct
{
	Vec3 position;
	Vec3 forward;
	Vec3 up;
} Frame;


/*******************************************************************************
 * Global Functions
 *
 ******************************************************************************/

static inline Mat4 makeTransform(Frame frame)
{
	Mat4 matrix;
	Vec3 right = V3Normalized( V3CrossMult(frame.forward, frame.up) );

	// x axis in 0, 1, 2
	matrix.data[0] = right.x;
	matrix.data[1] = right.y;
	matrix.data[2] = right.z;
	matrix.data[3] = 0.0f;

	// y axis in 4, 5, 6
	matrix.data[4] = frame.up.x;
	matrix.data[5] = frame.up.y;
	matrix.data[6] = frame.up.z;
	matrix.data[7] = 0.0f;

	// z axis in 8, 9, 10
	matrix.data[8] = -frame.forward.x;
	matrix.data[9] = -frame.forward.y;
	matrix.data[10] = -frame.forward.z;
	matrix.data[11] = 0.0f;

	// position in 12, 13, 14
	matrix.data[12] = frame.position.x;
	matrix.data[13] = frame.position.y;
	matrix.data[14] = frame.position.z;
	matrix.data[15] = 1.0f;
	
	return matrix;
}

static inline void
pitchFrame(Frame* frame, float amount)
{
	Vec3 side = V3CrossMult(frame->forward, frame->up);
	frame->forward = V3RotateAroundV3(frame->forward, amount, side);
	frame->up = V3RotateAroundV3(frame->up, amount, side);
}

static inline void
yawFrame(Frame* frame, float amount)
{
	frame->forward = V3RotateAroundV3(frame->forward, amount, frame->up );
}

static inline void
rollFrame(Frame* frame, float amount)
{
	frame->up = V3RotateAroundV3(frame->up, amount, frame->forward);
}

static inline void
moveFrame(Frame* frame, Vec3 direction, float amount)
{
	frame->position = V3Add( frame->position, V3ScalarMult( amount, direction ) );
}

// in Vector, as expressed in local system (localX, localY, localZ), out same Vector in world coords
static inline Vec3
fromLocalToGlobalDirection(Frame localSystem, Vec3 vector)
{
	Vec3 localBasisY = localSystem.up;
	Vec3 localBasisZ = V3ScalarMult(-1.0, localSystem.forward);
	Vec3 localBasisX = V3CrossMult(localBasisY, localBasisZ);
	
	Vec3 xVector = V3ScalarMult(vector.x, localBasisX);
	Vec3 yVector = V3ScalarMult(vector.y, localBasisY);
	Vec3 zVector = V3ScalarMult(vector.z, localBasisZ);
	Vec3 result = V3Add(xVector, V3Add(yVector, zVector));
	return result;
}

static inline Vec3
fromGlobalToLocalDirection(Frame localSystem, Vec3 vector)
{
	Mat4 modelMatrix = makeTransform(localSystem);
	Mat4 invModelMatrix = M4invert( &modelMatrix );
	Vec3 basisX = {invModelMatrix.data[0], invModelMatrix.data[1], invModelMatrix.data[2]};
	Vec3 basisY = {invModelMatrix.data[4], invModelMatrix.data[5], invModelMatrix.data[6]};
	Vec3 basisZ = {invModelMatrix.data[8], invModelMatrix.data[9], invModelMatrix.data[10]};
	Vec3 xVector = V3ScalarMult(vector.x, basisX);
	Vec3 yVector = V3ScalarMult(vector.y, basisY);
	Vec3 zVector = V3ScalarMult(vector.z, basisZ);
	Vec3 result = V3Add(xVector, V3Add(yVector, zVector));
	return result;
}

// in Point, as expressed in local system (localX, localY, localZ), out same Point in world coords
static inline Vec3
fromLocalToGlobalPosition(Frame localSystem, Vec3 point)
{
	Vec3 direction = fromLocalToGlobalDirection(localSystem, point);
	return V3Add( localSystem.position, direction );
}

static inline Vec3
fromGlobalToLocalPosition(Frame localSystem, Vec3 point)
{
	Mat4 modelMatrix = makeTransform(localSystem);
	Mat4 invModelMatrix = M4invert( &modelMatrix );
	return V4toV3( M4V4Multiply(&invModelMatrix, V3toV4(point)) );
}

static inline Frame
fromLocalToGLobalFrame(Frame localSystem, Frame frame)
{
	Frame newFrame;
	newFrame.position = fromLocalToGlobalPosition(localSystem, frame.position);
	newFrame.forward = fromLocalToGlobalDirection(localSystem, frame.forward);
	newFrame.up = fromLocalToGlobalDirection(localSystem, frame.up);
	return newFrame;
}

static inline Vec3
fromGlobalToLocalPositionCachedInverse(Mat4 invLocalSystem, Vec3 point)
{
	return V4toV3( M4V4Multiply(&invLocalSystem, V3toV4(point)) );
}

static inline void
rejuvenateFrame(Frame* frame)
{
	Vec3 right = V3CrossMult(frame->up, frame->forward);
	frame->up = V3Normalized( V3CrossMult(frame->forward, right) );
	frame->forward = V3Normalized( frame->forward );
}

static inline Frame
getIdentityFrame()
{
	Frame frame;
	frame.position.x = 0;
	frame.position.y = 0;
	frame.position.z = 0;
	
	frame.forward.x = 0;
	frame.forward.y = 0;
	frame.forward.z = -1;
	
	frame.up.x = 0;
	frame.up.y = 1;
	frame.up.z = 0;
	
	return frame;
}

#endif
