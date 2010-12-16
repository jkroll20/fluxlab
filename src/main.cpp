#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <flux.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <Cg/cg.h>
#include <Cg/cgGL.h>
#include "fbo.h"

using namespace std;


fbo<2, true, GL_RGB, true> gFBO;
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
} gCgState;


class fluxCgEffect
{
	public:
		fluxCgEffect(dword fluxHandleToAttachTo)
			: programLoaded(false), fluxHandleWasAttached(true)
		{
			wnd_set_paint_callback(fluxHandleToAttachTo, paintCB, (prop_t)this);
			fluxHandle= fluxHandleToAttachTo;
			wnd_set_status_callback(fluxHandle, statusCB, (prop_t)this);
		}

		fluxCgEffect(int x, int y, int width, int height, dword parent= NOPARENT, int alignment= ALIGN_LEFT|ALIGN_TOP)
			: programLoaded(false), fluxHandleWasAttached(false)
		{
			fluxHandle= create_rect(parent, x,y, width,height, TRANSL_2, alignment);
			wnd_set_paint_callback(fluxHandle, paintCB, (prop_t)this);
			wnd_set_status_callback(fluxHandle, statusCB, (prop_t)this);
			wnd_setisize(fluxHandle, width, height);
		}

		virtual ~fluxCgEffect()
		{
			if(fluxHandleWasAttached) wnd_set_paint_callback(fluxHandle, 0, 0);
			else wnd_close(fluxHandle);
			if(programLoaded) cgDestroyProgram(fragProgram);
		}

		dword getFluxHandle()
		{
			return fluxHandle;
		}

		bool loadProgram(const char *filename, const char *entryFunc= "main")
		{
			fragProgram= gCgState.loadFragmentProgramFromFile(filename, entryFunc);
			cgCompileProgram(fragProgram);
			cgGLBindProgram(fragProgram);
			return (programLoaded= gCgState.checkForCgError("binding fragment program"));
		}

	protected:
		CGprogram fragProgram;
		bool programLoaded;
		dword fluxHandle;
		bool fluxHandleWasAttached;

		void drawRect(rect &r)
		{
			double u0= double(r.x)/gScreenWidth, v0= double(r.y)/gScreenHeight,
				   u1= double(r.rgt)/gScreenWidth, v1= double(r.btm)/gScreenHeight;
			glTexCoord2f(u0, v0);	glVertex2i(r.x, r.y);
			glTexCoord2f(u1, v0);	glVertex2i(r.rgt, r.y);
			glTexCoord2f(u1, v1);	glVertex2i(r.rgt, r.btm);
			glTexCoord2f(u0, v1);	glVertex2i(r.x, r.btm);
		}

		void drawRectWithTexCoords(rect &r, rect &abspos)
		{
			double u00= double(r.x)/gScreenWidth, v00= double(r.y)/gScreenHeight,
				   u01= double(r.rgt)/gScreenWidth, v01= double(r.btm)/gScreenHeight;
			int w= abspos.rgt-abspos.x, h= abspos.btm-abspos.y;
			double u10= double(r.x-abspos.x)/w, v10= double(r.y-abspos.y)/h,
				   u11= double(r.rgt-abspos.x)/w, v11= double(r.btm-abspos.y)/h;
			glMultiTexCoord2d(GL_TEXTURE1, u10, v10); glMultiTexCoord2d(GL_TEXTURE0, u00, v00); glVertex2i(r.x, r.y);
			glMultiTexCoord2d(GL_TEXTURE1, u11, v10); glMultiTexCoord2d(GL_TEXTURE0, u01, v00); glVertex2i(r.rgt, r.y);
			glMultiTexCoord2d(GL_TEXTURE1, u11, v11); glMultiTexCoord2d(GL_TEXTURE0, u01, v01); glVertex2i(r.rgt, r.btm);
			glMultiTexCoord2d(GL_TEXTURE1, u10, v11); glMultiTexCoord2d(GL_TEXTURE0, u00, v01); glVertex2i(r.x, r.btm);
		}

		virtual void onClose();

		virtual void paint(primitive *self, rect *absPos, const rectlist *dirtyRects)
		{
			if(!programLoaded) return;

			glEnable(GL_DITHER);
			gFBO.select_framebuffer_texture(1);
			glDisable(GL_DEPTH_TEST);
			cgGLEnableProfile(gCgState.getFragmentProfile());
			cgGLBindProgram(fragProgram);
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, gFBO.color_textures[0]);
			glDisable(GL_BLEND);
			glBegin(GL_QUADS);
			const rectlist *r;
			for(r= dirtyRects; r; r= r->next)
				drawRectWithTexCoords(*r->self, *absPos);
			glEnd();
			cgGLDisableProfile(gCgState.getFragmentProfile());
			glDisable(GL_DITHER);

			gFBO.select_framebuffer_texture(0);
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
			glColor3f(1,1,1);
			reinterpret_cast<fluxCgEffect*>(arg)->paint(self, abspos, dirty_rects);
		}

		static int statusCB(prop_t arg, struct primitive *self, int type)
		{
			if(type==STAT_DESTROY)
				reinterpret_cast<fluxCgEffect*>(arg)->onClose();
			return 1;
		}
};

class fluxEffectWindowContainer
{
	public:
		~fluxEffectWindowContainer()
		{
			clear();
		}

		fluxCgEffect &createGenericFxWindow(int x, int y, int width, int height, const char *title,
											const char *cgFilename, const char *entryFunc= "main")
		{
			fluxCgEffect *effect= new fluxCgEffect(x,y, width,height);
			wnd_setprop(clone_frame("titleframe", effect->getFluxHandle()), "title", (prop_t)title);
			effect->loadProgram(cgFilename, entryFunc);
			effectWindows.push_back(effect);
			return *effect;
		}

		class fluxMagnifyEffect &createMagniFxWindow(int x, int y, int width, int height, const char *title);

		class fluxPlasmaEffect &createPlasmaWindow(int x, int y, int width, int height, const char *title);

		class fluxTeapot &createTeapot(int x, int y, int width, int height, const char *title);
		class fluxDisplacementEffect &createDisplacementWindow(int x, int y, int width, int height, const char *title);

		void erase(fluxCgEffect *x)
		{
			for(effectWindowList::iterator i= effectWindows.begin(); i!=effectWindows.end(); i++)
				if(*i==x)
				{
					delete(*i);
					effectWindows.erase(i);
					break;
				}
		}

		void clear()
		{
			while(effectWindows.size()) erase(effectWindows[0]);
		}

	private:
		typedef vector<fluxCgEffect*> effectWindowList;
		effectWindowList effectWindows;

} gEffectWindows;


void fluxCgEffect::onClose()
{
	gEffectWindows.erase(this);
}


class fluxMagnifyEffect: public fluxCgEffect
{
	public:
		fluxMagnifyEffect(dword fluxHandleToAttachTo)
			: fluxCgEffect(fluxHandleToAttachTo)
		{
			setup();
		}

		fluxMagnifyEffect(int x, int y, int width, int height, dword parent= NOPARENT, int alignment= ALIGN_LEFT|ALIGN_TOP)
			: fluxCgEffect(x,y, width,height, parent, alignment)
		{
			setup();
		}

		~fluxMagnifyEffect()
		{
			glDeleteTextures(1, &displacementTexture);
		}

	private:
		enum { textureSize= 512 };
		GLuint displacementTexture;

		bool setup()
		{
			float blob[textureSize*textureSize];
			float dispPixels[textureSize*textureSize*2];

			if(!loadProgram("cg/displace.cg")) return false;

			// create displacement texture
			makeSmoothBlob(textureSize, blob);
			makeDisplacementMapFromHeight(textureSize, blob, dispPixels, .75);
			glGenTextures(1, &displacementTexture);
			glBindTexture(GL_TEXTURE_2D, displacementTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE16_ALPHA16, textureSize, textureSize, 0,
						 GL_LUMINANCE_ALPHA, GL_FLOAT, dispPixels);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			return checkglerror();
		}

		virtual void paint(primitive *self, rect *absPos, const rectlist *dirtyRects)
		{
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, displacementTexture);
			glActiveTexture(GL_TEXTURE0);
			fluxCgEffect::paint(self, absPos, dirtyRects);
		}

		void makeSmoothBlob(int size, float *data)
		{
			float center= float(size)*.5;
			for(int y= size-1; y>=0; y--)
			{
				for(int x= size-1; x>=0; x--)
				{
					float dx= x-center, dy= y-center;
					float d= ((dx*dx)+(dy*dy)) / center;
					d= 2-d*(.025*256/textureSize);
					if(d<0) d= 0;
					data[y*size+x]= d;
				}
			}
		}

		// outputs to a 2-value array suitable for GL_LUMINANCE_ALPHA (L = x displacement, A = y displacement)
		void makeDisplacementMapFromHeight(int size, float* const src, float *dest, double scale= 1)
		{
			scale= scale * 16 * textureSize/256;
			for(int y= size-1; y>=0; y--)
			{
				for(int x= size-1; x>=0; x--)
				{
					const int d= 1;
					const double p= 1.5;
					double x0= x<d? src[y*size+x+d]: src[y*size+x];
					double x1= x<d? src[y*size+x]: src[y*size+x-d];
					double dx= pow(x1, p)-pow(x0, p);

					double y0= y<d? src[(y+d)*size+x+d]: src[y*size+x];
					double y1= y<d? src[y*size+x]: src[(y-d)*size+x];
					double dy= pow(y1, p)-pow(y0, p);

					dest[(y*size+x)*2+0]= (dx) * scale + .5;
					dest[(y*size+x)*2+1]= (dy) * scale + .5;
				}
			}
		}
};

class fluxPlasmaEffect: public fluxCgEffect
{
	public:
		fluxPlasmaEffect(dword fluxHandleToAttachTo)
			: fluxCgEffect(fluxHandleToAttachTo)
		{
			setup();
		}

		fluxPlasmaEffect(int x, int y, int width, int height, dword parent= NOPARENT, int alignment= ALIGN_LEFT|ALIGN_TOP)
			: fluxCgEffect(x,y, width,height, parent, alignment)
		{
			setup();
		}

	private:
		CGparameter cgTime;
		CGparameter cgPlasmaCoord;

		void setup()
		{
			loadProgram("cg/plasma.cg");
			cgTime= cgGetNamedParameter(fragProgram, "time");
			cgPlasmaCoord= cgGetNamedParameter(fragProgram, "plasmaCoord");
		}

		virtual void paint(primitive *self, rect *absPos, const rectlist *dirtyRects)
		{
			if(!programLoaded) return;

			cgGLEnableProfile(gCgState.getFragmentProfile());
			cgGLBindProgram(fragProgram);
			double time= SDL_GetTicks()*0.001;
			cgGLSetParameter1f(cgTime, time);
			cgGLSetParameter2f(cgPlasmaCoord, sin(time*0.23)*2, sin(time*0.258)*2);
			cgUpdateProgramParameters(fragProgram);

			glColor3f(sin(time*.33)*.7+.5, sin(time*.45)*.7+.5, sin(time*.92)*.7+.5);

			fluxCgEffect::paint(self, absPos, dirtyRects);
		}
};


GLuint loadTexture(const char *filename, bool mipmapped= true)
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

class fluxTeapot: public fluxCgEffect
{
	public:
		fluxTeapot(dword fluxHandleToAttachTo)
			: fluxCgEffect(fluxHandleToAttachTo)
		{
			setup();
		}

		fluxTeapot(int x, int y, int width, int height, dword parent= NOPARENT, int alignment= ALIGN_LEFT|ALIGN_TOP)
			: fluxCgEffect(x,y, width,height, parent, alignment)
		{
			setup();
		}

	private:
		GLuint texture;

		void setup()
		{
			int argc= 1;
			char *argv[]= { (char*)"whatever", 0 };
			glutInit(&argc, argv);
			texture= loadTexture("data/mix.png");
		}

		virtual void paint(primitive *self, rect *absPos, const rectlist *dirtyRects)
		{
			float w= absPos->rgt-absPos->x, h= absPos->btm-absPos->y;
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glLoadIdentity();
			gluPerspective(90*float(h)/w, float(w)/h, 0.01, 100);
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			glLoadIdentity();
			glViewport(absPos->x,absPos->y, w,h);

			glClear(GL_DEPTH_BUFFER_BIT);

			glTranslatef(0, 0, -9);
			glRotatef(-22.5, 1, 0, 0);
			glRotatef(SDL_GetTicks()*0.001*100, 0, 1, 0);
			glRotatef(180, 1, 0, 0);
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, texture);
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT);
			glEnable(GL_DEPTH_TEST);
			glMatrixMode(GL_TEXTURE);
			glScalef(.125, .125, 1);
			glMatrixMode(GL_MODELVIEW);
			glutSolidTeapot(4);
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);

			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glViewport(0,0, gScreenWidth, gScreenHeight);
		}
};


class fluxDisplacementEffect: public fluxCgEffect
{
	public:
		fluxDisplacementEffect(dword fluxHandleToAttachTo)
			: fluxCgEffect(fluxHandleToAttachTo)
		{
			setup();
		}

		fluxDisplacementEffect(int x, int y, int width, int height, dword parent= NOPARENT, int alignment= ALIGN_LEFT|ALIGN_TOP)
			: fluxCgEffect(x,y, width,height, parent, alignment)
		{
			setup();
		}

	private:
		GLuint displacementTexture;

		void setup()
		{
			displacementTexture= loadTexture("data/metal.png");
			loadProgram("cg/displace.cg", "displaceHeightmap");
		}

		virtual void paint(primitive *self, rect *absPos, const rectlist *dirtyRects)
		{
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, displacementTexture);
			glActiveTexture(GL_TEXTURE0);
			fluxCgEffect::paint(self, absPos, dirtyRects);
		}
};


fluxMagnifyEffect &fluxEffectWindowContainer::createMagniFxWindow(int x, int y, int width, int height, const char *title)
{
	fluxMagnifyEffect *effect= new fluxMagnifyEffect(x,y, width,height);
	wnd_setprop(clone_frame("titleframe", effect->getFluxHandle()), "title", (prop_t)title);
	effectWindows.push_back(effect);
	return *effect;
}

fluxPlasmaEffect &fluxEffectWindowContainer::createPlasmaWindow(int x, int y, int width, int height, const char *title)
{
	fluxPlasmaEffect *effect= new fluxPlasmaEffect(x,y, width,height);
	wnd_setprop(clone_frame("titleframe", effect->getFluxHandle()), "title", (prop_t)title);
	effectWindows.push_back(effect);
	return *effect;
}

fluxTeapot &fluxEffectWindowContainer::createTeapot(int x, int y, int width, int height, const char *title)
{
	fluxTeapot *effect= new fluxTeapot(x,y, width,height);
	wnd_setprop(clone_frame("titleframe", effect->getFluxHandle()), "title", (prop_t)title);
	effectWindows.push_back(effect);
	return *effect;
}

fluxDisplacementEffect &fluxEffectWindowContainer::createDisplacementWindow(int x, int y, int width, int height, const char *title)
{
	fluxDisplacementEffect *effect= new fluxDisplacementEffect(x,y, width,height);
	wnd_setprop(clone_frame("titleframe", effect->getFluxHandle()), "title", (prop_t)title);
	effectWindows.push_back(effect);
	return *effect;
}


void setVideoMode(int w, int h)
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
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

	for(int i= 0; i<0; i++)
	{
		ulong w= create_rect(NOPARENT, rand()%(gScreenWidth-200),rand()%(gScreenHeight-150), 200,150, (rand()&0xFFFFFF)|TRANSL_2);
		ulong t= clone_frame("titleframe", w);
		wnd_setprop(t, "title", (prop_t)"Halbtransparent");
	}

//	extern void testdlg(); testdlg();

	gEffectWindows.createMagniFxWindow(100,50, 200,180, "Verzerrung");
	gEffectWindows.createGenericFxWindow(250,50, 200,180, "Farbton Invertieren", "cg/colors.cg", "invertHue");
	gEffectWindows.createGenericFxWindow(100,300, 200,180, "Helligkeit Invertieren", "cg/colors.cg", "invertLightness");
	gEffectWindows.createGenericFxWindow(300,300, 200,180, "Negativ", "cg/colors.cg", "negate");
	gEffectWindows.createPlasmaWindow(400,300, 200,180, "Plasma");
	gEffectWindows.createTeapot(500,50, 200,180, "Teapot.");
	gEffectWindows.createDisplacementWindow(600,150, 200,180, "Displacement");

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
