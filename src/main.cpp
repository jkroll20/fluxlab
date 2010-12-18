#include <iostream>
#include <vector>
#include <algorithm>
#include <typeinfo>
#include <cmath>
#include <sys/time.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <Cg/cg.h>
#include <Cg/cgGL.h>
#include <flux.h>
#include "fbo.h"
#include "fbo.h"
#include "fast_teapot.c"
#include "fluxeffects.h"

using namespace std;


fbo<2, true, GL_RGB, true> gFBO;
CgState gCgState;
fluxEffectWindowContainer gEffectWindows;

int gScreenWidth= 1024, gScreenHeight= 600;
float gZoom= 1.0;
double gTime, gStartTime;
bool gIsMesa= false;


double getTime()
{
	timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec*0.000001;
}

void fluxCgEffect::onClose()
{
	gEffectWindows.erase(this);
}


GLuint loadTexture(const char *filename, bool mipmapped)
{
    SDL_Surface *surface= IMG_Load(filename);
    if(!surface) return 0;

    GLuint name;
    glGenTextures(1, &name);
    glBindTexture(GL_TEXTURE_2D, name);
    int bpp= surface->format->BitsPerPixel;
    int format= (bpp==8? GL_LUMINANCE: bpp==16? GL_LUMINANCE_ALPHA: bpp==24? GL_RGB: GL_RGBA);
    int internalformat= (bpp==32? 4: bpp==8? 1: 3);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmapped? GL_LINEAR_MIPMAP_LINEAR: GL_LINEAR);
    if(mipmapped)
        gluBuild2DMipmaps(GL_TEXTURE_2D, internalformat, surface->w,surface->h, format, GL_UNSIGNED_BYTE, surface->pixels);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, surface->w,surface->h, 0,
             format, GL_UNSIGNED_BYTE, surface->pixels);
    return name;
}



void setVideoMode(int w, int h)
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 0);
	SDL_SetVideoMode(w, h, 0, SDL_OPENGL|SDL_RESIZABLE); //|SDL_FULLSCREEN);
	int haveDoubleBuf, depthSize, swapControl;
	SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &haveDoubleBuf);
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depthSize);
	SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL, &swapControl);
	printf("Double Buffer: %s Depth Size: %d swap control: %d\n", haveDoubleBuf? "true": "false", depthSize, swapControl);
	SDL_ShowCursor(false);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1000, 1000);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gFBO.free_resources();
	gFBO.create(w, h);
	glDisable(GL_DITHER);
	gFBO.disable();

	if(gCgState.initialize())
		puts("Cg initialized.");
	else
		puts("Couldn't initialize Cg."), exit(1);

	flux_screenresize(w, h);
}


void flux_tick()
{
	static float x0, y0, x1, y1;
	static double lastUpdate;
	static double accumTime;
	static ulong accumFrames;
	static double avgFPS;

	aq_exec();
	run_timers();

	gFBO.enable();
	gFBO.set_ortho_mode();
	gFBO.select_framebuffer_texture(0);

	rect rc= viewport;
	rc.x/= gZoom; rc.rgt/= gZoom;
	rc.y/= gZoom; rc.btm/= gZoom;
	int w= rc.rgt-rc.x, h= rc.btm-rc.y;
	rc.x-= w/2-mouse.x; rc.rgt-= w/2-mouse.x;
	rc.y-= h/2-mouse.y; rc.btm-= h/2-mouse.y;
	if(rc.x<0) rc.x= 0, rc.rgt= rc.x+w;
	if(rc.y<0) rc.y= 0, rc.btm= rc.y+h;
	if(rc.rgt>gScreenWidth) rc.rgt= gScreenWidth, rc.x= rc.rgt-w;
	if(rc.btm>gScreenHeight) rc.btm= gScreenHeight, rc.y= rc.btm-h;

	double f= (gTime-lastUpdate)*5;
	if(x0==x1) x0= 0, x1= gScreenWidth, y0= 0, y1= gScreenHeight;
	x0+= (rc.x-x0)*f; x1+= (rc.rgt-x1)*f;
	y0+= (rc.y-y0)*f; y1+= (rc.btm-y1)*f;

	glScissor(x0, y0, x1, y1);
	glEnable(GL_SCISSOR_TEST);
	redraw_rect(&viewport);
	redraw_cursor();
	gFBO.disable();
	glDisable(GL_SCISSOR_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, gScreenWidth, gScreenHeight, 0, -1000, 1000);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gFBO.color_textures[0]);

	glDisable(GL_BLEND);
	glColor3f(1,1,1);
	glBegin(GL_QUADS);
	double u0= double(x0)/gScreenWidth, v0= double(y0)/gScreenHeight,
		   u1= double(x1)/gScreenWidth, v1= double(y1)/gScreenHeight;
	glTexCoord2f(u0,v0); glVertex2f(0, 0);
	glTexCoord2f(u1,v0); glVertex2f(viewport.rgt, 0);
	glTexCoord2f(u1,v1); glVertex2f(viewport.rgt, viewport.btm);
	glTexCoord2f(u0,v1); glVertex2f(0, viewport.btm);
	glEnd();

	char ch[32]; snprintf(ch, 32, "%.2f", avgFPS);
	draw_text(_font_getloc(FONT_DEFAULT), ch, gScreenWidth-45,gScreenHeight-14, viewport, 0xFFFFFF);

	glDisable(GL_TEXTURE_2D);

	checkglerror();

	accumFrames++;
	accumTime+= gTime-lastUpdate;
	if(accumTime>=1.0) { avgFPS= accumFrames/accumTime; accumFrames= 0; accumTime= 0; }
	lastUpdate= gTime;
}

int SDLMouseButtonToFluxMouseButton(int button)
{
	if(button>3) return button-1;
	else return (button==1? 0: button==3? 1: 2);
}


void makeShapeWindow(int x, int y)
{
    dword wnd= create_rect(NOPARENT, x,y, 260,160, COL_WINDOW, ALIGN_LEFT|ALIGN_TOP);
    dword frame= clone_frame("titleframe", wnd);
    wnd_setprop(frame, "title", (prop_t)"Shapes");

    dword ellipse= create_ellipse(wnd, 0,0, 5000,10000, COL_ITEMHI, ALIGN_LEFT|ALIGN_BOTTOM|WREL|HREL, true,
				  0.5,0.5, 0.5,0.5);

    dblpos verts1[]= { {0.0,0.0}, {1.0,0.25}, {0.5,1.0} };
    dword poly1= create_poly(wnd, 5000,0, 5000,100, COL_ITEMHI, ALIGN_LEFT|ALIGN_BOTTOM|WREL|XREL, true,
			     3, verts1);

    dblpos verts2[]= { {0.25,0.0}, {0.75,0.5}, {0.25,0.75}, {0.0,0.5} };
    dword poly2= create_poly(wnd, 5000,0, 5000,100, COL_TITLE, ALIGN_LEFT|ALIGN_BOTTOM|WREL|XREL, true,
			     4, verts2);
}


class fluxEffectButton
{
	public:
		fluxEffectButton(int x, int y, int width, int height, const char *title, int type,
						 ulong alignment= ALIGN_RIGHT|ALIGN_TOP, ulong parent= 0)
		{
			this->type= type;
			fluxHandle= clone_group("button", parent, x,y, width,height, alignment);
			wnd_setprop(fluxHandle, "text", (prop_t)title);
			wnd_setprop(fluxHandle, "on_click", (prop_t)clickCB);
			wnd_addprop(fluxHandle, "this", (prop_t)this, PROP_DWORD);
		}

		void onClick()
		{
			int width= 200, height= 180;
			int x, y;
			randPos(width, height, x, y);
			switch(type)
			{
				case 0:
					gEffectWindows.createGenericFxWindow(x,y, width,height, "Farbton Invertieren", "cg/colors.cg", "invertHue");
					break;
				case 1:
					gEffectWindows.createGenericFxWindow(x,y, width,height, "Helligkeit Invertieren", "cg/colors.cg", "invertLightness");
					break;
				case 2:
					gEffectWindows.createGenericFxWindow(x,y, width,height, "Negativ", "cg/colors.cg", "negate");
					break;
				case 3:
					gEffectWindows.addFxWindow(new fluxMagnifyEffect(x,y, width,height));
					break;
				case 4:
					gEffectWindows.addFxWindow(new fluxPlasmaEffect(x,y, width,height));
					break;
				case 5:
					width= 280; height= 240;
					randPos(width, height, x, y);
					gEffectWindows.addFxWindow(new fluxTeapot(x,y, width,height));
					break;
				case 6:
					gEffectWindows.addFxWindow(new fluxDisplacementEffect(x,y, width,height));
					break;
				case 7:
					makeShapeWindow(x, y);
			}
		}

	private:
		ulong fluxHandle;
		int type;

		void randPos(int width, int height, int &x, int &y)
		{
			x= rand()%(gScreenWidth-width-4);
			y= rand()%(gScreenHeight-height-(font_height(FONT_DEFAULT)+8+4));
		}

//typedef void (*btn_cbclick)(int id, bool btndown, int whichbtn);
		static void clickCB(int id, bool btndown, int whichbtn)
		{
			if(!btndown) reinterpret_cast<fluxEffectButton*>(wnd_getprop(id, "this"))->onClick();
		}
};


int main()
{
	SDL_Event events[32];
	bool doQuit= false;
	bool isPaused= false;
	double time;

	gStartTime= getTime();
	setVideoMode(gScreenWidth, gScreenHeight);

	const unsigned char *glRenderer= glGetString(GL_RENDERER);
	if(strstr((const char*)glRenderer, "Mesa"))
	{
		gIsMesa= true;
		// workaround for off-by-one error in mesa renderer.
		extern long outline_pixel_offset_x;
		outline_pixel_offset_x= -1<<16;
	}

	for(int i= 0; i<0; i++)
	{
		ulong w= create_rect(NOPARENT, rand()%(gScreenWidth-200),rand()%(gScreenHeight-150), 200,150, (rand()&0xFFFFFF)|TRANSL_2);
		ulong t= clone_frame("titleframe", w);
		wnd_setprop(t, "title", (prop_t)"Halbtransparent");
	}

	gEffectWindows.addFxWindow(new fluxBackgroundImage(0));

	int buttonW= 140,buttonH= 22, buttonX= 10,buttonY= 10-buttonH;
	fluxEffectButton buttons[]= {
		fluxEffectButton(buttonX, buttonY+=buttonH*1.5, buttonW, buttonH, "Farbton Invertieren", 0),
		fluxEffectButton(buttonX, buttonY+=buttonH*1.5, buttonW, buttonH, "Helligkeit Invertieren", 1),
		fluxEffectButton(buttonX, buttonY+=buttonH*1.5, buttonW, buttonH, "Negativ", 2),
		fluxEffectButton(buttonX, buttonY+=buttonH*1.5, buttonW, buttonH, "Lupe", 3),
		fluxEffectButton(buttonX, buttonY+=buttonH*1.5, buttonW, buttonH, "Plasma", 4),
		fluxEffectButton(buttonX, buttonY+=buttonH*1.5, buttonW, buttonH, "Teapot", 5),
		fluxEffectButton(buttonX, buttonY+=buttonH*1.5, buttonW, buttonH, "Displacement", 6),
		fluxEffectButton(buttonX, buttonY+=buttonH*1.5, buttonW, buttonH, "Shapes", 7),
	};

	while(!doQuit)
	{
		time= getTime();
		if(!isPaused)
			gTime= time;

		SDL_PumpEvents();
		int numEvents= SDL_PeepEvents(events, 32, SDL_GETEVENT, SDL_ALLEVENTS);
		for(int i= 0; i<numEvents; i++)
		{
			SDL_Event &ev= events[i];
			switch(ev.type)
			{
				case SDL_KEYDOWN:
					if(ev.key.keysym.sym==SDLK_ESCAPE)
						doQuit= true;
					else if(ev.key.keysym.sym==' ')
						isPaused= !isPaused;
					break;
				case SDL_QUIT:
					doQuit= true;
					break;
				case SDL_MOUSEBUTTONDOWN:
					flux_mouse_button_event(SDLMouseButtonToFluxMouseButton(ev.button.button), ev.button.state);
					if(ev.button.button==4) gZoom*= 1.1;
					else if(ev.button.button==5) { gZoom*= 0.9; if(gZoom<1.15) gZoom= 1.0; }
					else if(ev.button.button==2) gZoom= 1.0;
					break;
				case SDL_MOUSEBUTTONUP:
					flux_mouse_button_event(SDLMouseButtonToFluxMouseButton(ev.button.button), ev.button.state);
					break;
				case SDL_MOUSEMOTION:
					flux_mouse_move_event(ev.motion.xrel, ev.motion.yrel);
					break;
				case SDL_VIDEORESIZE:
					printf("window resized\n");
					setVideoMode(ev.resize.w, ev.resize.h);
					gScreenWidth= ev.resize.w; gScreenHeight= ev.resize.h;
					break;
			}
		}

		flux_tick();
		SDL_GL_SwapBuffers();

		double frametime= getTime() - time;
		double delay= (1.0/100) - frametime;
		if(delay<0.001) delay= 0.001;
		usleep(useconds_t(delay*1000000));
	}

	SDL_Quit();
    return 0;
}
