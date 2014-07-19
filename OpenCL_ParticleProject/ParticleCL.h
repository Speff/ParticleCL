#include <CL/cl.h>
#include <stdio.h>
#include <cstdio>
#include <stdlib.h>
#include <math.h>
#include <SDL_TTF.h>


#ifdef cl_khr_fp64
    #pragma OPENCL EXTENSION cl_khr_fp64 : enable
#elif defined(cl_amd_fp64)
    #pragma OPENCL EXTENSION cl_amd_fp64 : enable
#else
#endif

#define SCREEN_FPS 60;
#define SCREEN_TICKS_PER_FRAME = 1000 / SCREEN_FPS;

#define NUM_PARTICLES	1048576
//#define NUM_PARTICLES	1024
#define WIDTH			1280	
#define HEIGHT			720
#define FRAMERATE		60
#define PIXEL_VELOCITY	700
#define PI				3.14159f
#define VERBOSE			true
#define FS_MAX_ELEMENTS	10



// Functions
void initCL();
bool boilerplateCode();
bool compileKernel();
bool setMemMappings();
bool initializeArrays();
bool writeToBuffersInit();
bool writeMousePosToBuffers(int, int);
bool writenewFieldStrengths(cl_float[]);
bool runSim();
bool cl_closeAll();
bool readBuffer();
void checkErrorCode(char*, int);