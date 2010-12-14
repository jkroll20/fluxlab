#include <iostream>
#include <flux.h>
#include <SDL/SDL.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glu.h>
#include <Cg/cg.h>
#include <Cg/cgGL.h>
#include "fbo.h"

using namespace std;


fbo<2, true, GL_RGB, false> gFBO;
int gScreenWidth= 1024, gScreenHeight= 600;
float gZoom= 1.0;

class CgState
{
	public:
		CgState(): initialized(false)
		{ }

		bool initialize()
		{
			if(initialized) return true;

			myCgContext= cgCreateContext();
			if(!checkForCgError("creating context")) return false;
			cgGLSetDebugMode(CG_TRUE);
			cgSetParameterSettingMode(myCgContext, CG_DEFERRED_PARAMETER_SETTING);

			myCgVertexProfile= cgGLGetLatestProfile(CG_GL_VERTEX);
			cgGLSetOptimalOptions(myCgVertexProfile);
			if(!checkForCgError("selecting vertex profile")) return false;

			myCgFragmentProfile= cgGLGetLatestProfile(CG_GL_FRAGMENT);
			cgGLSetOptimalOptions(myCgFragmentProfile);
			if(!checkForCgError("selecting fragment profile")) return false;

			return (initialized= true);
		}

		CGprogram loadFragmentProgramFromFile(const char *filename, const char *entryFunc= "main")
		{ return loadProgramFromFile(filename, entryFunc, myCgFragmentProfile); }

		CGprogram loadVertexProgramFromFile(const char *filename, const char *entryFunc= "main")
		{ return loadProgramFromFile(filename, entryFunc, myCgVertexProfile); }

		bool checkForCgError(const char *situation)
		{
			CGerror error;
			const char *string= cgGetLastErrorString(&error);

			if(error!=CG_NO_ERROR)
			{
				printf("%s: %s\n", situation, string);
				if (error == CG_COMPILER_ERROR)
					printf("%s\n", cgGetLastListing(myCgContext));
				return false;
			}
			return true;
		}

		const CGprofile getVertexProfile()
		{ return myCgVertexProfile; }

		const CGprofile getFragmentProfile()
		{ return myCgFragmentProfile; }

	private:
		CGprogram loadProgramFromFile(const char *filename, const char *entryFunc, CGprofile profile)
		{
			if(!initialized) return 0;
			CGprogram program= cgCreateProgramFromFile(
									myCgContext,                /* Cg runtime context */
									CG_SOURCE,                  /* Program in human-readable form */
									filename,  					/* Name of file containing program */
									profile,        			/* Profile: OpenGL ARB vertex program */
									entryFunc,      			/* Entry function name */
									NULL);                      /* No extra compiler options */
			if(!checkForCgError("creating program from file")) return false;
			cgGLLoadProgram(program);
			if(!checkForCgError("loading program")) return false;
			printf("CG program %s loaded.\n", filename);
			return program;
		}

		bool initialized;
		CGcontext	myCgContext;
		CGprofile   myCgVertexProfile,
					myCgFragmentProfile;
} gCgState;	// could be a singleton, but i can't be bothered for this demo


class fluxCgWindow
{
	public:
		fluxCgWindow(dword fluxHandleToAttachTo)
			: programLoaded(false)
		{
			wnd_set_paint_callback(fluxHandleToAttachTo, paintCB, (prop_t)this);
			fluxHandle= fluxHandleToAttachTo;
		}

		fluxCgWindow(int x, int y, int width, int height, dword parent= NOPARENT, int alignment= ALIGN_LEFT|ALIGN_TOP)
			: programLoaded(false)
		{
			fluxHandle= create_rect(parent, x,y, width,height, TRANSL_2, alignment);
			wnd_set_paint_callback(fluxHandle, paintCB, (prop_t)this);
		}

		dword getFluxHandle()
		{
			return fluxHandle;
		}

		bool loadProgram(const char *filename)
		{
			fragProgram= gCgState.loadFragmentProgramFromFile("cg/test.cg");
			cgGLBindProgram(fragProgram);
			return (programLoaded= gCgState.checkForCgError("binding fragment program"));
		}

	private:
		CGprogram fragProgram;
		CGprogram vertProgram;
		bool programLoaded;
		dword fluxHandle;

		void drawRect(rect &r)
		{
			double u0= double(r.x)/gScreenWidth, v0= double(r.y)/gScreenHeight,
				   u1= double(r.rgt)/gScreenWidth, v1= double(r.btm)/gScreenHeight;
			glTexCoord2f(u0, v0);	glVertex2i(r.x, r.y);
			glTexCoord2f(u1, v0);	glVertex2i(r.rgt, r.y);
			glTexCoord2f(u1, v1);	glVertex2i(r.rgt, r.btm);
			glTexCoord2f(u0, v1);	glVertex2i(r.x, r.btm);
		}

		virtual void paint(primitive *self, rect *absPos, const rectlist *dirtyRects)
		{
			if(!programLoaded) return;

			gFBO.select_framebuffer_texture(1);
			glDisable(GL_DEPTH_TEST);
			glColor3f(1,1,1);
			cgGLEnableProfile(gCgState.getFragmentProfile());
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, gFBO.color_textures[0]);
			glDisable(GL_BLEND);
			glBegin(GL_QUADS);
			const rectlist *r;
			for(r= dirtyRects; r; r= r->next)
				drawRect(*r->self);
			glEnd();
			cgGLDisableProfile(gCgState.getFragmentProfile());

			gFBO.select_framebuffer_texture(0);
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, gFBO.color_textures[1]);
			glBegin(GL_QUADS);
			for(r= dirtyRects; r; r= r->next)
				drawRect(*r->self);
			glEnd();
			glDisable(GL_TEXTURE_2D);

			checkglerror();
		}

		static void paintCB(prop_t arg, primitive *self, rect *abspos, const rectlist *dirty_rects)
		{
			reinterpret_cast<fluxCgWindow*>(arg)->paint(self, abspos, dirty_rects);
		}
};


void setVideoMode(int w, int h)
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_SetVideoMode(w, h, 0, SDL_OPENGL|SDL_RESIZABLE); //|SDL_FULLSCREEN);
	SDL_ShowCursor(false);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1000, 1000);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gFBO.free_resources();
	gFBO.create(w, h);
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

	double t= SDL_GetTicks()*0.001;
	double f= (t-lastUpdate)*5;
	lastUpdate= t;
	if(x0==x1) x0= 0, x1= gScreenWidth, y0= 0, y1= gScreenHeight;
	x0+= (rc.x-x0)*f; x1+= (rc.rgt-x1)*f;
	y0+= (rc.y-y0)*f; y1+= (rc.btm-y1)*f;

	redraw_rect(&viewport);
	redraw_cursor();
	gFBO.disable();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gFBO.color_textures[0]);

	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glColor3f(1,1,1);
	glBegin(GL_QUADS);
	double u0= double(x0)/gScreenWidth, v0= double(y0)/gScreenHeight,
		   u1= double(x1)/gScreenWidth, v1= double(y1)/gScreenHeight;
	glTexCoord2f(u0,v1); glVertex2f(0, 0);
	glTexCoord2f(u1,v1); glVertex2f(viewport.rgt, 0);
	glTexCoord2f(u1,v0); glVertex2f(viewport.rgt, viewport.btm);
	glTexCoord2f(u0,v0); glVertex2f(0, viewport.btm);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, gFBO.color_textures[0]);

	glDisable(GL_TEXTURE_2D);

	checkglerror();
}

int SDLMouseButtonToFluxMouseButton(int button)
{
	if(button>3) return button-1;
	else return (button==1? 0: button==3? 1: 2);
}

int main()
{
	SDL_Event events[32];
	bool doQuit= false;

	setVideoMode(gScreenWidth, gScreenHeight);

		fluxCgWindow *cgWnd= new fluxCgWindow(0,0, 300,200);
		cgWnd->loadProgram("cg/test.cg");
		clone_frame("titleframe", cgWnd->getFluxHandle());

	for(int i= 0; i<3; i++)
	{
		ulong w= create_rect(NOPARENT, rand()%(gScreenWidth-200),rand()%(gScreenHeight-150), 200,150, (rand()&0xFFFFFF)|TRANSL_2);
		ulong t= clone_frame("titleframe", w);
		char ch[128]; sprintf(ch, "Test %d\n", rand());
		wnd_setprop(t, "title", (prop_t)ch);

		if(!i)
		{
			fluxCgWindow *cgWnd= new fluxCgWindow(w);
			cgWnd->loadProgram("cg/test.cg");
		}

		w= create_rect(0, rand()%(gScreenWidth-200),rand()%(gScreenHeight-150), 200,150, (rand()&0xFFFFFF)|TRANSL_2);
	}

	extern void testdlg(); testdlg();

	while(!doQuit)
	{
		ulong startTicks= SDL_GetTicks();
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

		ulong frametime= SDL_GetTicks()-startTicks;
		int delay= (1000/100) - frametime;
		if(delay<1) delay= 1;
		SDL_Delay(delay);
	}

	SDL_Quit();
    return 0;
}
