/*This source code copyrighted by Lazy Foo' Productions (2004-2013)
and may not be redistributed without written permission.*/

#include "ParticleCL.h"
#include "MainWindow.h"
#include <ctime>



//Screen dimension constants
const int SCREEN_WIDTH = WIDTH;
const int SCREEN_HEIGHT = HEIGHT;
bool mouseMoved = false;
int mouseLocationX;
int mouseLocationY;
bool fsChanged = false;
int fsSelected = 0;

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
cl_mem bufposC_pixelPointer;
cl_mem bufTestVar;
cl_mem bufMousePos;
cl_mem bufFieldStrength;

// Kernel I/O Vars
cl_float2 posC[NUM_PARTICLES];			// Array for current position of Particle[i]
cl_float2 posP[NUM_PARTICLES];			// Array for the position of Particle[i] one frame back
cl_uint posC_pixelPointer[NUM_PARTICLES];// (int)posC[i]
cl_float2 kernelTestVal[5];				// Dummy values for debugging kernel
cl_int2 mousePos[1];					// MousePosition for each particle
cl_int2 windowSize = {WIDTH, HEIGHT};	// Window size
cl_float deadZone;
cl_float fieldStrength[10];
cl_float velDamper_input;
int velDamper_power = 3;

// SDL Vars
SDL_Window* gWindow = NULL;						//The window we'll be rendering to
SDL_Renderer* gRenderer = NULL;					//The window renderer
SDL_Surface* newFrameBuffer;
SDL_Texture* Background_Tx;
Uint32 pixData[4*HEIGHT*WIDTH];

using namespace std;

TTF_Font *font;
bool displayOn = true;
vector<string> hud;
double duration = 1;							// Time per loop for FPS calculation

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

	hud.push_back("Calculating");

	//Initialize SDL
	if( SDL_Init( SDL_INIT_VIDEO ) < 0 ){
		printf( "SDL could not initialize! SDL Error: %s\n", SDL_GetError() );
		success = false;
	}
	else{
		//Set texture filtering to linear
		//if(!SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "1" )){
		//	printf( "Warning: Linear texture filtering not enabled!" );
		//}
		//SDL_WINDOWPOS_UNDEFINED SDL_RENDERER_PRESENTVSYNC
		//Create window
		gWindow = SDL_CreateWindow( "ParticleCL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
		if( gWindow == NULL ){
			printf( "Window could not be created! SDL Error: %s\n", SDL_GetError() );
			success = false;
		}
		else{
			//Create renderer for window
			gRenderer = SDL_CreateRenderer( gWindow, -1, SDL_RENDERER_ACCELERATED || SDL_RENDERER_PRESENTVSYNC);
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

	//atexit(SDL_QUIT);
	SDL_ShowCursor( SDL_DISABLE );

	atexit(SDL_Quit);
	TTF_Init();
	font = TTF_OpenFont( "../segoeui.ttf", FONT_SIZE );
	atexit(TTF_Quit);



	return success;
}

void close(){
	//Destroy window	

	//TTF_CloseFont( font );
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
	std::clock_t start;								// Timer
	int loopCounter = 0;							// Amount of times the loop has run
	char* fps_s = (char*)malloc(sizeof(50*char()));	// Space for FPS String displayed
	double checkpoint_1, checkpoint_2 = 0;			// Timer variables to measuring sections of the loop
	Uint32* ptr;									// Pointer to blit pixels onto the frame 

	initCL();

	//Start up SDL and create window
	if(!init()){
		printf( "Failed to initialize!\n" );
	}
	else{
		//Main loop flag
		bool quit = false;

		// Create a surface to draw onto
		newFrameBuffer = SDL_CreateRGBSurface(0,WIDTH,HEIGHT,32,0, 0, 0, 0);
		//newFrameBuffer = SDL_CreateRGBSurface(0,WIDTH,HEIGHT,32,rmask,gmask,bmask, amask);
		Background_Tx = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_RGBA8888 , SDL_TEXTUREACCESS_TARGET, WIDTH, HEIGHT);

		//Event handler
		SDL_Event e;

		//While application is running
		while( !quit ){
			//Handle events on queue
			while( SDL_PollEvent( &e ) != 0 ){
				//User requests quit
				if( e.type == SDL_QUIT ){
					quit = true;
				}
				if( e.type == SDL_KEYUP){
					//quit = true;
					if(e.key.keysym.scancode == SDL_SCANCODE_DOWN){
						if(fsSelected == FS_MAX_ELEMENTS-1) fsSelected = 0;
						else fsSelected++;
					}
					if(e.key.keysym.scancode == SDL_SCANCODE_UP){
						if(fsSelected == 0) fsSelected = FS_MAX_ELEMENTS-1;
						else fsSelected--;
					}
					if(e.key.keysym.scancode == SDL_SCANCODE_BACKSPACE){
						if(velDamper_power > 0){
							velDamper_input = ((velDamper_input * pow(10,velDamper_power)) - (int)(velDamper_input * pow(10,velDamper_power))%10)/pow(10,velDamper_power);
							velDamper_power--;
						}
					}
					if(e.key.keysym.scancode >= 30 && e.key.keysym.scancode <= 38){
						velDamper_power++;
						velDamper_input = velDamper_input + (e.key.keysym.scancode-29)/pow(10,velDamper_power);
					}
					else if(e.key.keysym.scancode == SDL_SCANCODE_SPACE) quit = true;
					else if(e.key.keysym.scancode == SDL_SCANCODE_RETURN) fsChanged = true;
				}
				if( e.type == SDL_KEYDOWN){
					if(e.key.keysym.scancode == SDL_SCANCODE_LEFT){
						fieldStrength[fsSelected]--;
					}
					if(e.key.keysym.scancode == SDL_SCANCODE_RIGHT){
						fieldStrength[fsSelected]++;
					}
				}
				if (e.type == SDL_MOUSEMOTION){
					mouseMoved = true;
					if(!((int)e.motion.x == 0) && !((int)e.motion.x == 0)){
						mouseLocationX = (int) e.motion.x;
						mouseLocationY = (int) e.motion.y;
					}
				}

			}

			//velDamper_input

			if(mouseMoved){
				mouseMoved = false;
				writeMousePosToBuffers(mouseLocationX, mouseLocationY);
			}
			if(fsChanged){
				printf("Commited FS change\n");
				fsChanged = false;
				writenewFieldStrengths(fieldStrength);
			}

			// Loop start--------------------------------------------------------------------------------------------------------------

			// Start the timer
			start = std::clock();

			// Run kernel
			runSim();
			readBuffer();

			if(loopCounter%119 == 0){
				checkpoint_1 = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
				printf("Kernel runtime:\t\t %f ms\n", checkpoint_1);
			}

			//Clear screen
			SDL_SetRenderDrawColor( gRenderer, 0x00, 0x00, 0x00, 0xFF );
			SDL_RenderClear( gRenderer );

			if(loopCounter%119 == 0){
				checkpoint_2 = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
				printf("Clear screen time:\t %f ms\n", checkpoint_2-checkpoint_1);
			}

			SDL_FillRect(newFrameBuffer,&newFrameBuffer->clip_rect,SDL_MapRGB(newFrameBuffer->format,0, 0, 0));
			SDL_SetRenderDrawColor( gRenderer, 0x00, 0x00, 0x00, 0xFF ); 

			if(loopCounter%119 == 0){
				checkpoint_1 = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
				printf("Create surface time:\t %f ms\n", checkpoint_1-checkpoint_2);
			}

			// Create pointer to pixel matrix in Surface
			ptr = (Uint32*)newFrameBuffer->pixels;
			//printf("%X\n",SDL_MapRGB(newFrameBuffer->format, 0x08, 0x02, 0x08));

			// Draw Points - this is the slowest part
			for( long i = 1; i < NUM_PARTICLES; i += 1 ){
				// Get offset for pixel pointed to from kernel. Note. The variables other than posC_pixelPointer don't change during simulation
				//pixOffset = (posC_pixelPointer[i].s[1]) * (newFrameBuffer->pitch/4) + posC_pixelPointer[i].s[0] * newFrameBuffer->format->BytesPerPixel / 4;
				// Optimized version. +10fps woo. I don't think this works on big endian machines
				//pixOffset = (posC_pixelPointer[i].s[1]<<4) * (80) + posC_pixelPointer[i].s[0];

				// Alpha, Red, Green, Blue
				//if(ptr[posC_pixelPointer[i]+1] > 0xEF);
				//else ptr[posC_pixelPointer[i]] += 0x060406;
				ptr[posC_pixelPointer[i]] += 0x080608;
			}

			//if(false){
			if(displayOn){
				updateDisplayVariables();
				for(int i = 0; i < hud.size(); i++){
					SDL_Color clrFg = {200,200,200,0};  // Blue ("Fg" is foreground)
					SDL_Surface *sText = TTF_RenderText_Blended( font, hud.at(i).c_str(), clrFg );
					if(sText == NULL) printf("GUI Error\n");
					SDL_Rect rcDest = {2,i * FONT_SIZE + 3,0,0};
					if(SDL_BlitSurface( sText,NULL, newFrameBuffer,&rcDest ) != 0) printf("%s\n",SDL_GetError());
					SDL_FreeSurface( sText );
				}
			}

			if(loopCounter%119 == 0){
				checkpoint_2 = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
				printf("Draw pixels time:\t %f ms\n", checkpoint_2-checkpoint_1);
			}

			// Following line takes fucking forever. Fuck the next line
			Background_Tx = SDL_CreateTextureFromSurface(gRenderer, newFrameBuffer);
			if(Background_Tx == 0) printf("%s\n",  SDL_GetError() );

			if(SDL_RenderCopy(gRenderer, Background_Tx, NULL, NULL) != 0) printf("%s\n",SDL_GetError());


			if(loopCounter%119 == 0){
				checkpoint_1 = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
				printf("Create texture:\t\t %f ms\n", checkpoint_1-checkpoint_2);
			}


			//SDL_FreeSurface(newFrameBuffer);
			SDL_DestroyTexture(Background_Tx);

			//Update screen
			SDL_RenderPresent(gRenderer);

			if(loopCounter%119 == 0){
				checkpoint_2 = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
				printf("Paint screen time:\t %f ms\n", checkpoint_2-checkpoint_1);
			}

			if(++loopCounter%120 == 0){
				duration = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
				printf("FPS: %f\n", 1/duration);
				// WARNING. This caused a memory leak. Fix later
				//
			}
		}

	}

	cl_closeAll();
	close();

	return 0;
}

void updateDisplayVariables(){
	hud.clear();

	string hudLine = "FPS: " + to_string(1/duration);
	hud.push_back(hudLine);

	hudLine = "Vel Damper: " + to_string(velDamper_input);
	hud.push_back(hudLine);

	for(int i = 0; i < FS_MAX_ELEMENTS-1; i++){
		if(i == fsSelected) hudLine = "*FS Ring " + to_string(i) + ": " + to_string((int)fieldStrength[i]);
		else hudLine = "  FS Ring " + to_string(i) + ": " + to_string((int)fieldStrength[i]);
		hud.push_back(hudLine);
	}

	// Last element is printed slightly differently
	if(fsSelected == FS_MAX_ELEMENTS-1) hudLine = "*FS Rings " + to_string(FS_MAX_ELEMENTS-1) + "+: " + to_string((int)fieldStrength[FS_MAX_ELEMENTS-1]);
	else hudLine = "  FS Rings " + to_string(FS_MAX_ELEMENTS-1) + "+: " + to_string((int)fieldStrength[FS_MAX_ELEMENTS-1]);
	hud.push_back(hudLine);

}