#include <CL/cl.h>
#include <stdio.h>
#include <cstdio>
#include <stdlib.h>
#include <math.h>


#ifdef cl_khr_fp64
    #pragma OPENCL EXTENSION cl_khr_fp64 : enable
#elif defined(cl_amd_fp64)
    #pragma OPENCL EXTENSION cl_amd_fp64 : enable
#else
#endif

#define NUM_PARTICLES	1048576
//#define NUM_PARTICLES	32
#define ACCEL_X			-1
#define ACCEL_Y			-1
#define WIDTH			1280
#define HEIGHT			720
#define FRAMERATE		60
#define PIXEL_VELOCITY	700
#define PI				3.14159f
#define VERBOSE			true



// Functions
void initCL();
bool boilerplateCode();
bool compileKernel();
bool setMemMappings();
bool initializeArrays();
bool writeToBuffersInit();
bool writeMousePosToBuffers(int, int);
bool runSim();
bool cl_closeAll();
bool readBuffer();
void checkErrorCode(char*, int);


// Copy these vars
// OpenCL Vars
//char* kernelSource;
//cl_uint numDevices;
//cl_device_id *devices;
//cl_program program;
//cl_context context;
//cl_command_queue cmdQueue;
//cl_kernel kernel;
//cl_mem bufPosC;
//cl_mem bufPosP;
//cl_mem bufPos_int;
//cl_mem bufTestVar;
//
//// Kernel I/O Vars
//cl_float2 posC[NUM_PARTICLES];			// Array for current position of Particle[i]
//cl_float2 posP[NUM_PARTICLES];			// Array for the position of Particle[i] one from back
//cl_int2 pos_int[NUM_PARTICLES];			// (int)posC[i]
//cl_float2 kernelTestVal[5];				// Dummy values for debugging kernel
//cl_float2 mousePos = {ACCEL_X,ACCEL_Y};	// MousePoseration for for each particle
//cl_float2 windowSize = {WIDTH, HEIGHT};	// Window size