/*This source code copyrighted by Lazy Foo' Productions (2004-2013)
and may not be redistributed without written permission.*/

#include "ParticleCL.h"
#include "SDL_Test.h"
#include <ctime>


//Screen dimension constants
const int SCREEN_WIDTH = WIDTH;
const int SCREEN_HEIGHT = HEIGHT;
bool mouseMoved = false;
int mouseLocationX;
int mouseLocationY;

// OpenCL Vars
char* kernelSource;
cl_uint numDevices;
cl_device_id *devices;
cl_program program;
cl_context context;
cl_command_queue cmdQueue;
cl_kernel kernel;
cl_mem bufPosC;
cl_mem bufPosP;
cl_mem bufPos_int;
cl_mem bufTestVar;
cl_mem bufMousePos;

// Kernel I/O Vars
cl_float2 posC[NUM_PARTICLES];			// Array for current position of Particle[i]
cl_float2 posP[NUM_PARTICLES];			// Array for the position of Particle[i] one frame back
cl_int2 pos_int[NUM_PARTICLES];			// (int)posC[i]
cl_float2 kernelTestVal[5];				// Dummy values for debugging kernel
cl_int2 mousePos[1];					// MousePosition for each particle
cl_int2 windowSize = {WIDTH, HEIGHT};	// Window size

// SDL Vars
SDL_Window* gWindow = NULL;						//The window we'll be rendering to
SDL_Renderer* gRenderer = NULL;					//The window renderer
SDL_Surface* newFrameBuffer;
SDL_Texture* Background_Tx;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
uint32 rmask = 0xff000000;
uint32 gmask = 0x00ff0000;
uint32 bmask = 0x0000ff00;
uint32 amask = 0x000000ff;
#else
uint32_t rmask = 0x000000ff;
uint32_t gmask = 0x0000ff00;
uint32_t bmask = 0x00ff0000;
uint32_t amask = 0xff000000;
#endif

bool init(){
	//Initialization flag
	bool success = true;

	//Initialize SDL
	if( SDL_Init( SDL_INIT_VIDEO ) < 0 ){
		printf( "SDL could not initialize! SDL Error: %s\n", SDL_GetError() );
		success = false;
	}
	else{
		//Set texture filtering to linear
		if(!SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "1" )){
			printf( "Warning: Linear texture filtering not enabled!" );
		}
		//SDL_WINDOWPOS_UNDEFINED
		//Create window
		gWindow = SDL_CreateWindow( "Particle Sim", 400, 200, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
		if( gWindow == NULL ){
			printf( "Window could not be created! SDL Error: %s\n", SDL_GetError() );
			success = false;
		}
		else{
			//Create renderer for window
			gRenderer = SDL_CreateRenderer( gWindow, -1, SDL_RENDERER_ACCELERATED );
			if( gRenderer == NULL ){
				printf( "Renderer could not be created! SDL Error: %s\n", SDL_GetError() );
				success = false;
			}
			else{
				//Initialize renderer color
				SDL_SetRenderDrawColor( gRenderer, 0xFF, 0xFF, 0xFF, 0xFF );
			}
		}
	}

	return success;
}

void close(){
	//Destroy window	
	SDL_DestroyRenderer(gRenderer);
	SDL_DestroyWindow(gWindow);
	gWindow = NULL;
	gRenderer = NULL;

	//Quit SDL subsystems
	SDL_Quit();

	printf("Closing SDL\n");
}

int main( int argc, char* args[] )
{
	std::clock_t start;
	int loopCounter = 0;
	double duration = 0;
	Uint32 *ptr;

	initCL();

	//Start up SDL and create window
	if(!init()){
		printf( "Failed to initialize!\n" );
	}
	else{
		//Main loop flag
		bool quit = false;

		//Event handler
		SDL_Event e;

		//While application is running
		while( !quit ){
			//Handle events on queue
			while( SDL_PollEvent( &e ) != 0 ){
				//User requests quit
				if( e.type == SDL_QUIT ){
					quit = true;
					cl_closeAll();
					close();
					return 0;
				}
				if (e.type == SDL_MOUSEMOTION){
					mouseMoved = true;
					if(!((int)e.motion.x == 0) && !((int)e.motion.x == 0)){
						mouseLocationX = (int) e.motion.x;
						mouseLocationY = (int) e.motion.y;
						//printf("%i,%i\n", mouseLocationX, mouseLocationY);
					}
				}
			}

			if(mouseMoved){
				mouseMoved = false;
				writeMousePosToBuffers(mouseLocationX, mouseLocationY);
			}

			// Start the timer
			start = std::clock();

			// Run kernel
			runSim();
			readBuffer();

			// Draw output

			//Clear screen
			SDL_SetRenderDrawColor( gRenderer, 0xFF, 0xFF, 0xFF, 0xFF );
			SDL_RenderClear( gRenderer );

			// Create a surface to draw onto
			newFrameBuffer = SDL_CreateRGBSurface(0,WIDTH,HEIGHT,32,0, 0, 0, amask);
			//newFrameBuffer = SDL_CreateRGBSurface(0,WIDTH,HEIGHT,32,rmask,gmask,bmask, amask);


			SDL_LockSurface(newFrameBuffer);
			// Create pointer to pixel matrix in Surface
			ptr = (Uint32*)newFrameBuffer->pixels;
			// Draw Points
			for( long i = 0; i < NUM_PARTICLES; i += 1 ){
				// Get offset for pixel pointed to from kernel
				long pixOffset = (pos_int[i].s[1]) * (newFrameBuffer->pitch/4) + pos_int[i].s[0] * newFrameBuffer->format->BytesPerPixel / 4;
				//printf("%i,%i\n%li. Particle %li\n", pos_int[i].s[0],pos_int[i].s[1],pixOffset, i);
				//ptr[pixOffset] += SDL_MapRGBA(newFrameBuffer->format, 0x05, 0x01, 0x05, 0xFF);
				//printf("%#X\n",SDL_MapRGBA(newFrameBuffer->format, 0xAB, 0xCD, 0xEF, 0xAC));
				// Set the color at offset. Alpha, R, G, B
				//ptr[pixOffset] += 0xFF100010;
				// Alpha, Red, Green, Blue
				ptr[pixOffset] += 0xFF100010;
				//ptr[pixOffset] = 0xFF;
				//ptr[pixOffset+32] += 0x80;
				//ptr[pixOffset+32+32+32] += 0x80;
			}
			SDL_UnlockSurface(newFrameBuffer);
			//SDL_RenderDrawPoints(gRenderer, pointsToDraw, NUM_PARTICLES);

			Background_Tx = SDL_CreateTextureFromSurface(gRenderer, newFrameBuffer);
			SDL_FreeSurface(newFrameBuffer);
			SDL_RenderCopy(gRenderer, Background_Tx, NULL, NULL);
			SDL_DestroyTexture(Background_Tx);

			//Update screen
			SDL_RenderPresent( gRenderer );

			if(++loopCounter%120 == 0){

				printf("%i, %i\n", pos_int[0].s[0], pos_int[0].s[0]);
				duration = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
				printf("FPS: %f\n", 1/duration);
			}
		}

	}

	close();

	return 0;
}