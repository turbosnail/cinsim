#ifndef _OUTSIMPACK_H_
#define _OUTSIMPACK_H_
//////////////////////


// NOTES:

// Types:

// int			: 4-byte integer
// unsigned		: unsigned integer
// float		: 4-byte float
// Vec			: 3 ints fixed point (1m = 65536)
// Vector		: 3 floats x, y, z

// Axes:

// X - right
// Y - forward
// Z - up


// Bits for "OutSim Opts" integer in cfg.txt
// OutSim Opts is hexadecimal - for all fields set OutSim Opts to ff
// The resulting UDP packet size is 272 (the full OutSim2 pack below)

#define OSO_HEADER		1
#define OSO_ID			2
#define OSO_TIME		4
#define OSO_MAIN		8
#define OSO_INPUTS		16
#define OSO_DRIVE		32
#define OSO_DISTANCE	64
#define OSO_WHEELS		128


// structure for main data - like old OutSimPack but without Time (first element) or ID (last element)

struct OutSimMain
{
	Vector		AngVel;		// 3 floats, angular velocity vector
	float		Heading;	// anticlockwise from above (Z)
	float		Pitch;		// anticlockwise from right (X)
	float		Roll;		// anticlockwise from front (Y)
	Vector		Accel;		// 3 floats X, Y, Z
	Vector		Vel;		// 3 floats X, Y, Z
	Vec			Pos;		// 3 ints   X, Y, Z (1m = 65536)
};

struct OutSimInputs
{
	float		Throttle;		// 0 to 1
	float		Brake;			// 0 to 1
	float		InputSteer;		// radians
	float		Clutch;			// 0 to 1
	float		Handbrake;		// 0 to 1
};

struct OutSimWheel // 10 ints
{
	float		SuspDeflect;		// compression from unloaded
	float		Steer;				// including Ackermann and toe
	float		XForce;				// force right
	float		YForce;				// force forward
	float		VerticalLoad;		// perpendicular to surface
	float		AngVel;				// radians/s
	float		LeanRelToRoad;		// radians a-c viewed from rear

	byte		AirTemp;			// degrees C
	byte		SlipFraction;		// (0 to 255 - see below)
	byte		Touching;			// touching ground
	byte		Sp3;

	float		SlipRatio;			// slip ratio
	float		TanSlipAngle;		// tangent of slip angle
};

struct OutSimPack2 // size depends on OutSim Opts
{
	// if (OSOpts & OSO_HEADER)

	char L;
	char F;
	char S;
	char T;

	// if (OSOpts & OSO_ID)

	int			ID;					// OutSim ID from cfg.txt

  	// if (OSOpts & OSO_TIME)

	unsigned	Time;				// time in milliseconds (to check order)

  	// if (OSOpts & OSO_MAIN)

	OutSimMain		OSMain;			// struct - see above

  	// if (OSOpts & OSO_INPUTS)

	OutSimInputs	OSInputs;		// struct - see above

	// if (OSOpts & OSO_DRIVE)

	byte		Gear;				// 0=R, 1=N, 2=first gear
	byte		Sp1;				// spare
	byte		Sp2;
	byte		Sp3;

	float		EngineAngVel;		// radians/s
	float		MaxTorqueAtVel;		// Nm : output torque for throttle 1.0

	// if (OSOpts & OSO_DISTANCE)

	float		CurrentLapDist;		// m - travelled by car
	float		IndexedDistance;	// m - track ruler measurement

	// if (OSOpts & OSO_WHEELS)

	OutSimWheel		OSWheels[4];	// array of structs - see above
};


//////
#endif