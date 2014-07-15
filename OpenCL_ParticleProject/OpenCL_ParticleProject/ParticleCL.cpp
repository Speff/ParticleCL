#include "ParticleCL.h"

// OpenCL Vars
extern char* kernelSource;
extern cl_uint numDevices;
extern cl_device_id *devices;
extern cl_program program;
extern cl_context context;
extern cl_command_queue cmdQueue;
extern cl_kernel kernel;
extern cl_mem bufPosC;
extern cl_mem bufPosP;
extern cl_mem bufPos_int;
extern cl_mem bufTestVar;

// Kernel I/O Vars
extern cl_float2 posC[NUM_PARTICLES];			// Array for current position of Particle[i]
extern cl_float2 posP[NUM_PARTICLES];			// Array for the position of Particle[i] one from back
extern cl_int2 pos_int[NUM_PARTICLES];			// (int)posC[i]
extern cl_float2 kernelTestVal[5];				// Dummy values for debugging kernel
extern cl_float2 accel;							// Acceleration for for each particle
extern cl_int2 windowSize;						// Window size

void initCL(){
	printf("Initializing...\n");

	boilerplateCode();
	compileKernel();
	setMemMappings();

	initializeArrays();
	writeToBuffers();

	//printf("%f, %f", posP[0].s[0], posP[0].s[0]);

	// runSim();
	// readBuffer();

	// cl_closeAll();
}

bool readBuffer(){
	cl_int status;

	// Read the device output buffer to the host output array
	status = clEnqueueReadBuffer(cmdQueue, bufPos_int, CL_FALSE, 0, NUM_PARTICLES * sizeof(cl_int2), pos_int, 0, NULL, NULL);
	//checkErrorCode("Reading bufPos...\t", status);
	status = clEnqueueReadBuffer(cmdQueue, bufTestVar, CL_TRUE, 0, 5 * sizeof(cl_float2), kernelTestVal, 0, NULL, NULL);
	//checkErrorCode("Reading bufTestVal...\t", status);

	return true;
}

bool cl_closeAll(){
	// Free OpenCL resources
	clReleaseKernel(kernel);
	clReleaseProgram(program);
	clReleaseCommandQueue(cmdQueue);
	clReleaseMemObject(bufPosC);
	clReleaseMemObject(bufPosP);
	clReleaseMemObject(bufPos_int);
	clReleaseMemObject(bufTestVar);
	clReleaseContext(context);
	free(devices);

	printf("Closed CL context\n");
	return true;
}

bool runSim(){
	// Define an index space (global work size) of work items for execution
	// A workgroup size (local work size) is not required, but can be used.
	size_t globalWorkSize[1];
	cl_int status;

	// There are 'NUM_PARTICLES'-number of work-items
	globalWorkSize[0] = NUM_PARTICLES;

	// Execute the kernel for execution
	status = clEnqueueNDRangeKernel(cmdQueue, kernel, 1, NULL, globalWorkSize, NULL, 0, NULL, NULL);
	//checkErrorCode("Running Sim...\t\t", status);

	return true;
}

bool writeToBuffers(){
	cl_int status;
	windowSize.s[0] = WIDTH;
	windowSize.s[1] = HEIGHT;

	// Write arrays to the device buffers. bufPos is unnecessary
	status = clEnqueueWriteBuffer(cmdQueue, bufPosC, CL_FALSE, 0, NUM_PARTICLES * sizeof(cl_float2), posP, 0, NULL, NULL);
	checkErrorCode("Writing bufPosC...\t", status);
	status = clEnqueueWriteBuffer(cmdQueue, bufPosP, CL_FALSE, 0, NUM_PARTICLES * sizeof(cl_float2), posC, 0, NULL, NULL);
	checkErrorCode("Writing bufPosP...\t", status);
	status = clEnqueueWriteBuffer(cmdQueue, bufPos_int, CL_FALSE, 0, NUM_PARTICLES * sizeof(cl_int2), pos_int, 0, NULL, NULL);
	checkErrorCode("Writing bufPos...\t", status);
	status = clEnqueueWriteBuffer(cmdQueue, bufTestVar, CL_TRUE, 0, 5 * sizeof(cl_float2), kernelTestVal, 0, NULL, NULL);
	checkErrorCode("Writing testVar...\t", status);

	return true;
}

bool initializeArrays(){
	for(int i = 0; i < NUM_PARTICLES; i++){
		posC[i].s[0] = WIDTH/2;
		posC[i].s[1] = HEIGHT/2;

		// Old Position (One frame back) = Current position - Velocity vector/Framerate
		posP[i].s[0] = posC[i].s[0] - (PIXEL_VELOCITY/FRAMERATE) * cos(2 * PI * ((float)i/NUM_PARTICLES));
		posP[i].s[1] = posC[i].s[1] - (PIXEL_VELOCITY/FRAMERATE) * sin(2 * PI * ((float)i/NUM_PARTICLES));
	}

	return true;
}

bool setMemMappings(){
	cl_int status;

	// Create a buffer object
	bufPosC = clCreateBuffer(context, CL_MEM_READ_WRITE, NUM_PARTICLES * sizeof(cl_float2), NULL, &status);
	checkErrorCode("Creating buf (PosC)...\t", status);
	bufPosP = clCreateBuffer(context, CL_MEM_READ_WRITE, NUM_PARTICLES * sizeof(cl_float2), NULL, &status);
	checkErrorCode("Creating buf (PosP)...\t", status);
	bufPos_int = clCreateBuffer(context, CL_MEM_WRITE_ONLY, NUM_PARTICLES * sizeof(cl_int2), NULL, &status);
	checkErrorCode("Creating buf (Pos)...\t", status);
	bufTestVar = clCreateBuffer(context, CL_MEM_READ_WRITE, 5 * sizeof(cl_float2), NULL, &status);
	checkErrorCode("Creating buf (T1)...\t", status);

	// Associate the input and output buffers & variables with the kernel
	status = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufPosC); checkErrorCode("Setting KernelArg(1)...\t", status);
	status = clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufPosP); checkErrorCode("Setting KernelArg(2)...\t", status);
	status = clSetKernelArg(kernel, 2, sizeof(cl_mem), &bufPos_int); checkErrorCode("Setting KernelArg(3)...\t", status);
	status = clSetKernelArg(kernel, 3, sizeof(cl_float2), &accel); checkErrorCode("Setting KernelArg(4)...\t", status);
	status = clSetKernelArg(kernel, 4, sizeof(cl_int2), &windowSize); checkErrorCode("Setting KernelArg(5)...\t", status);
	status = clSetKernelArg(kernel, 5, sizeof(cl_mem), &bufTestVar); checkErrorCode("Setting KernelArg(6)...\t", status);

	return true;
}

bool compileKernel(){
	FILE *fp;
	char fileLocation[] = "C:/kernels/kernel.txt";
	size_t sourceSize;
	cl_int status;

	fp = fopen(fileLocation, "r");
	if(!fp){
		printf("Can't read kernel source\n");
		return false;
	}

	fseek(fp, 0, SEEK_END);
	sourceSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	kernelSource = (char*)calloc(1, sourceSize + 1);
	fread(kernelSource, 1, sourceSize, fp);
	fclose(fp);
	//printf("%s", kernelSource);
	if(VERBOSE) printf("Kernel source read\n");

	// Create a program using the source code
	program = clCreateProgramWithSource(context, 1, (const char**)&kernelSource, NULL, &status);
	checkErrorCode("Creating program...\t", status);

	// Compile the program
	status = clBuildProgram(program, numDevices, devices, NULL, NULL, NULL);
	checkErrorCode("Compiling program...\t", status);

	char* buildLog;
	size_t buildLogSize;
	clGetProgramBuildInfo(program,*devices,CL_PROGRAM_BUILD_LOG, NULL, NULL, &buildLogSize);
	buildLog = (char*)malloc(buildLogSize);
	clGetProgramBuildInfo(program,*devices,CL_PROGRAM_BUILD_LOG, buildLogSize, buildLog, NULL);
	if(buildLogSize > 2) printf("%s\n",buildLog);
	free(buildLog);

	// Create the vector addition kernel
	kernel = clCreateKernel(program, "updateParticle", &status);
	checkErrorCode("Creating kernel...\t", status);

	return true;
}

bool boilerplateCode(){
	// Use this to check the output of each API call
	cl_int status;

	// Retrieve the number of platforms
	cl_uint numPlatforms = 0;
	status = clGetPlatformIDs(0, NULL, &numPlatforms);
	checkErrorCode("Getting platforms...\t", status);

	// Allocate enough space for each platform
	cl_platform_id *platforms = NULL;
	platforms = (cl_platform_id*)malloc(numPlatforms * sizeof(cl_platform_id));

	// Fill in the platforms
	status = clGetPlatformIDs(numPlatforms, platforms, NULL);
	checkErrorCode("Filling platforms...\t", status);

	// Retrieve the number of devices
	status = clGetDeviceIDs(platforms[1], CL_DEVICE_TYPE_ALL, 0, NULL, &numDevices);

	// Allocate space for each device
	devices = (cl_device_id*)malloc(numDevices * sizeof(cl_device_id));

	// Fill in the devices
	status = clGetDeviceIDs(platforms[1], CL_DEVICE_TYPE_ALL, numDevices, devices, NULL);
	checkErrorCode("Filling devices...\t", status);

	// Create a contect and associate it with the devices
	context = clCreateContext(NULL, numDevices, devices, NULL, NULL, &status);
	checkErrorCode("Creating context...\t", status);

	// Create a command queue and associate it with the device
	cmdQueue = clCreateCommandQueue(context, devices[0], 0, &status);
	checkErrorCode("Creating cmd queue...\t", status);

	char* devName;
	size_t nameSize;
	clGetDeviceInfo(devices[0], CL_DEVICE_NAME, NULL, NULL, &nameSize);
	devName = (char*)malloc(nameSize);
	clGetDeviceInfo(devices[0], CL_DEVICE_NAME, nameSize, devName, NULL);
	if(status == CL_SUCCESS && VERBOSE) printf("Using device:\t\t%s\n", devName); 

	free(platforms);
	free(devName);
	return true;
}

void checkErrorCode(char* action, int errorCode){
	if(!VERBOSE) return;
	else{
		printf("%s\t",action);
		if(errorCode == CL_SUCCESS) printf("CL_SUCCESS\n");
		if(errorCode == CL_DEVICE_NOT_FOUND) printf("CL_DEVICE_NOT_FOUND\n");
		if(errorCode == CL_DEVICE_NOT_AVAILABLE) printf("CL_DEVICE_NOT_AVAILABLE\n");
		if(errorCode == CL_COMPILER_NOT_AVAILABLE) printf("CL_COMPILER_NOT_AVAILABLE\n");
		if(errorCode == CL_MEM_OBJECT_ALLOCATION_FAILURE) printf("CL_MEM_OBJECT_ALLOCATION_FAILURE\n");
		if(errorCode == CL_OUT_OF_RESOURCES) printf("CL_OUT_OF_RESOURCES\n");
		if(errorCode == CL_OUT_OF_HOST_MEMORY) printf("CL_OUT_OF_HOST_MEMORY\n");
		if(errorCode == CL_PROFILING_INFO_NOT_AVAILABLE) printf("CL_PROFILING_INFO_NOT_AVAILABLE\n");
		if(errorCode == CL_MEM_COPY_OVERLAP) printf("CL_MEM_COPY_OVERLAP\n");
		if(errorCode == CL_IMAGE_FORMAT_MISMATCH) printf("CL_IMAGE_FORMAT_MISMATCH\n");
		if(errorCode == CL_IMAGE_FORMAT_NOT_SUPPORTED) printf("CL_IMAGE_FORMAT_NOT_SUPPORTED\n");
		if(errorCode == CL_BUILD_PROGRAM_FAILURE) printf("CL_BUILD_PROGRAM_FAILURE\n");
		if(errorCode == CL_MAP_FAILURE) printf("CL_MAP_FAILURE\n");
		if(errorCode == CL_INVALID_VALUE) printf("CL_INVALID_VALUE\n");
		if(errorCode == CL_INVALID_DEVICE_TYPE) printf("CL_INVALID_DEVICE_TYPE\n");
		if(errorCode == CL_INVALID_PLATFORM) printf("CL_INVALID_PLATFORM\n");
		if(errorCode == CL_INVALID_DEVICE) printf("CL_INVALID_DEVICE\n");
		if(errorCode == CL_INVALID_CONTEXT) printf("CL_INVALID_CONTEXT\n");
		if(errorCode == CL_INVALID_QUEUE_PROPERTIES) printf("CL_INVALID_QUEUE_PROPERTIES\n");
		if(errorCode == CL_INVALID_COMMAND_QUEUE) printf("CL_INVALID_COMMAND_QUEUE\n");
		if(errorCode == CL_INVALID_HOST_PTR) printf("CL_INVALID_HOST_PTR\n");
		if(errorCode == CL_INVALID_MEM_OBJECT) printf("CL_INVALID_MEM_OBJECT\n");
		if(errorCode == CL_INVALID_IMAGE_FORMAT_DESCRIPTOR) printf("CL_INVALID_IMAGE_FORMAT_DESCRIPTOR\n");
		if(errorCode == CL_INVALID_IMAGE_SIZE) printf("CL_INVALID_IMAGE_SIZE\n");
		if(errorCode == CL_INVALID_SAMPLER) printf("CL_INVALID_SAMPLER\n");
		if(errorCode == CL_INVALID_BINARY) printf("CL_INVALID_BINARY\n");
		if(errorCode == CL_INVALID_BUILD_OPTIONS) printf("CL_INVALID_BUILD_OPTIONS\n");
		if(errorCode == CL_INVALID_PROGRAM) printf("CL_INVALID_PROGRAM\n");
		if(errorCode == CL_INVALID_PROGRAM_EXECUTABLE) printf("CL_INVALID_PROGRAM_EXECUTABLE\n");
		if(errorCode == CL_INVALID_KERNEL_NAME) printf("CL_INVALID_KERNEL_NAME\n");
		if(errorCode == CL_INVALID_KERNEL_DEFINITION) printf("CL_INVALID_KERNEL_DEFINITION\n");
		if(errorCode == CL_INVALID_KERNEL) printf("CL_INVALID_KERNEL\n");
		if(errorCode == CL_INVALID_ARG_INDEX) printf("CL_INVALID_ARG_INDEX\n");
		if(errorCode == CL_INVALID_ARG_VALUE) printf("CL_INVALID_ARG_VALUE\n");
		if(errorCode == CL_INVALID_ARG_SIZE) printf("CL_INVALID_ARG_SIZE\n");
		if(errorCode == CL_INVALID_KERNEL_ARGS) printf("CL_INVALID_KERNEL_ARGS\n");
		if(errorCode == CL_INVALID_WORK_DIMENSION) printf("CL_INVALID_WORK_DIMENSION\n");
		if(errorCode == CL_INVALID_WORK_GROUP_SIZE) printf("CL_INVALID_WORK_GROUP_SIZE\n");
		if(errorCode == CL_INVALID_WORK_ITEM_SIZE) printf("CL_INVALID_WORK_ITEM_SIZE\n");
		if(errorCode == CL_INVALID_GLOBAL_OFFSET) printf("CL_INVALID_GLOBAL_OFFSET\n");
		if(errorCode == CL_INVALID_EVENT_WAIT_LIST) printf("CL_INVALID_EVENT_WAIT_LIST\n");
		if(errorCode == CL_INVALID_EVENT) printf("CL_INVALID_EVENT\n");
		if(errorCode == CL_INVALID_OPERATION) printf("CL_INVALID_OPERATION\n");
		if(errorCode == CL_INVALID_GL_OBJECT) printf("CL_INVALID_GL_OBJECT\n");
		if(errorCode == CL_INVALID_BUFFER_SIZE) printf("CL_INVALID_BUFFER_SIZE\n");
		if(errorCode == CL_INVALID_MIP_LEVEL) printf("CL_INVALID_MIP_LEVEL\n");
		if(errorCode == CL_INVALID_GLOBAL_WORK_SIZE) printf("CL_INVALID_GLOBAL_WORK_SIZE\n");
	}
}