#ifndef FLUXEFFECTS_H
#define FLUXEFFECTS_H

#include "fast_teapot.c"

using namespace std;

extern fbo<2, true, GL_RGB, true> gFBO;
extern class CgState gCgState;
extern class fluxEffectWindowContainer gEffectWindows;

extern int gScreenWidth, gScreenHeight;
extern float gZoom;
extern double gTime, gStartTime;
extern int gIsMesa;
extern int gNeedDepthClear;

GLuint loadTexture(const char *filename, bool mipmapped= true);


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
			cgGLSetDebugMode(CG_FALSE);
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
									profile,        			/* Profile */
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
};



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

		bool loadFragmentProgram(const char *filename, const char *entryFunc= "main")
		{
			fragProgram= gCgState.loadFragmentProgramFromFile(filename, entryFunc);
			cgCompileProgram(fragProgram);
			cgGLBindProgram(fragProgram);
			return (programLoaded= gCgState.checkForCgError("binding fragment program"));
		}

		bool loadVertexProgram(const char *filename, const char *entryFunc= "main")
		{
			vertProgram= gCgState.loadVertexProgramFromFile(filename, entryFunc);
			cgCompileProgram(vertProgram);
			cgGLBindProgram(vertProgram);
			return (programLoaded= gCgState.checkForCgError("binding vertex program"));
		}

		bool isToplevelWindow()
		{
			return !fluxHandleWasAttached;
		}

	protected:
		CGprogram fragProgram;
		CGprogram vertProgram;
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
			effect->loadFragmentProgram(cgFilename, entryFunc);
			effectWindows.push_back(effect);
			return *effect;
		}

		void addFxWindow(fluxCgEffect *effect, const char *title= 0)
		{
			if(!title) { title= typeid(*effect).name(); while(*title>='0' && *title<='9' && *title) title++; }
			if(effect->isToplevelWindow())
			{
				wnd_setprop(clone_frame("titleframe", effect->getFluxHandle()), "title", (prop_t)title);
				wnd_setisize(effect->getFluxHandle(), wnd_getw(effect->getFluxHandle()), wnd_geth(effect->getFluxHandle()));
			}
			effectWindows.push_back(effect);
		}

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

};


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
		CGparameter cgIntensity;

		bool setup()
		{
			float blob[textureSize*textureSize];
			float dispPixels[textureSize*textureSize*2];

			if(!loadFragmentProgram("cg/displace.cg")) return false;

			// create displacement texture
			makeSmoothBlob(textureSize, blob);
			makeDisplacementMapFromHeight(textureSize, blob, dispPixels, .75);
			glGenTextures(1, &displacementTexture);
			glBindTexture(GL_TEXTURE_2D, displacementTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE16_ALPHA16, textureSize, textureSize, 0,
						 GL_LUMINANCE_ALPHA, GL_FLOAT, dispPixels);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			cgIntensity= cgGetNamedParameter(fragProgram, "intensity");

			return checkglerror();
		}

		virtual void paint(primitive *self, rect *absPos, const rectlist *dirtyRects)
		{
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, displacementTexture);
			glActiveTexture(GL_TEXTURE0);
			cgGLBindProgram(fragProgram);
			cgGLSetParameter1f(cgIntensity, 1.0 * sqrt((absPos->rgt-absPos->x)*(absPos->btm-absPos->y))/sqrt(200*180));
			cgUpdateProgramParameters(fragProgram);
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
			loadFragmentProgram("cg/plasma.cg");
			cgTime= cgGetNamedParameter(fragProgram, "time");
			cgPlasmaCoord= cgGetNamedParameter(fragProgram, "plasmaCoord");
		}

		virtual void paint(primitive *self, rect *absPos, const rectlist *dirtyRects)
		{
			if(!programLoaded) return;

			cgGLEnableProfile(gCgState.getFragmentProfile());
			cgGLBindProgram(fragProgram);
			double time= gTime-gStartTime;
			cgGLSetParameter1f(cgTime, time);
			cgGLSetParameter2f(cgPlasmaCoord, sin(time*0.23)*2, sin(time*0.258)*2);
			cgUpdateProgramParameters(fragProgram);

			glColor3f(sin(time*.33)*.7+.5, sin(time*.45)*.7+.5, sin(time*.92)*.7+.5);

			fluxCgEffect::paint(self, absPos, dirtyRects);
		}
};

class fluxTeapot: public fluxCgEffect
{
	public:
		fluxTeapot(dword fluxHandleToAttachTo)
			: fluxCgEffect(fluxHandleToAttachTo)
		{
			setup();
			gNeedDepthClear++;
		}

		fluxTeapot(int x, int y, int width, int height, dword parent= NOPARENT, int alignment= ALIGN_LEFT|ALIGN_TOP)
			: fluxCgEffect(x,y, width,height, parent, alignment)
		{
			setup();
			gNeedDepthClear++;
		}

		~fluxTeapot()
		{
			gNeedDepthClear--;
		}

	private:
		GLuint texture;
		GLUquadric *quadric;
		CGparameter cgModelViewMatrix, cgProjectionMatrix;
		CGparameter cgModelViewProjMatrix;
		CGparameter cgCamPos;
		CGparameter cgInvWindowSize;
		CGparameter cgIntensity;

		void setup()
		{
#ifdef GLUTPOT
			int argc= 1;
			char *argv[]= { (char*)"main", 0 };
			glutInit(&argc, argv);
#endif

			texture= loadTexture("data/mix.png");
			quadric= gluNewQuadric();

#define FRAGTEST1
#ifdef FRAGTEST1
			loadFragmentProgram("cg/teapot.cg", "teapotFragTest1");
			loadVertexProgram("cg/teapot.cg", "teapotVertexProgram2");
#else
			loadFragmentProgram("cg/teapot.cg", "main");
			loadVertexProgram("cg/teapot.cg", "teapotVertexProgram");
#endif

			cgModelViewMatrix= cgGetNamedParameter(vertProgram, "modelViewMatri");

			cgProjectionMatrix= cgGetNamedParameter(vertProgram, "projectionMatrix");
			if(!cgProjectionMatrix) printf("couldn't find projectionMatrix parameter.\n");

			cgModelViewProjMatrix= cgGetNamedParameter(vertProgram, "modelViewProjMatrix");
			if(!cgModelViewProjMatrix) printf("couldn't find modelViewProjMatrix parameter.\n");

			cgCamPos= cgGetNamedParameter(vertProgram, "camPos");
			if(!cgCamPos) printf("couldn't find camPos parameter.\n");

			cgInvWindowSize= cgGetNamedParameter(fragProgram, "invWindowSize");
			if(!cgInvWindowSize) printf("couldn't find invWindowSize parameter.\n");

			cgIntensity= cgGetNamedParameter(fragProgram, "intensity");
			if(!cgIntensity) printf("couldn't find intensity parameter.\n");
		}

		virtual void paint(primitive *self, rect *absPos, const rectlist *dirtyRects)
		{
			float w= absPos->rgt-absPos->x, h= absPos->btm-absPos->y;

			// draw to fbo texture 1 (extra buffer)
			gFBO.select_framebuffer_texture(1);
			glEnable(GL_TEXTURE_2D);
			// read from fbo texture 0 (default draw buffer)
			glBindTexture(GL_TEXTURE_2D, gFBO.color_textures[0]);

			// copy background to fbo texture 1 (extra buffer)
			glBegin(GL_QUADS);
			drawRect(*absPos);
			glEnd();

			cgGLBindProgram(fragProgram);
			cgGLBindProgram(vertProgram);

			glEnable(GL_CULL_FACE);

			// set up modelview & projection matrices for teapot
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glLoadIdentity();
			gluPerspective(-90*float(3)/4, float(4)/3, 0.01, 100);
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			glLoadIdentity();
			glViewport(absPos->x,absPos->y, w,h);
			glEnable(GL_DEPTH_TEST);

			float camPos[3]= { 0, -1.2, -5.5 };
			glTranslatef(camPos[0], camPos[1], camPos[2]);
			glRotatef(22.5, 1, 0, 0);
			glRotatef(-90, 1, 0, 0);

			glPushMatrix();

			cgUpdateProgramParameters(vertProgram);
			cgGLDisableProfile(gCgState.getFragmentProfile());
			cgGLDisableProfile(gCgState.getVertexProfile());
			glDisable(GL_TEXTURE_2D);
			glColor3f(1,1,1);
			glCullFace(GL_BACK);
			glEnable(GL_TEXTURE_2D);
			glLoadIdentity();

			cgGLEnableProfile(gCgState.getFragmentProfile());
			cgSetParameter2f(cgInvWindowSize, 1.0/gScreenWidth, (gIsMesa? -1.0: 1.0)/gScreenHeight);
			cgSetParameter1f(cgIntensity, 0.03 * sqrt(w*h)/sqrt(200*180));
			cgUpdateProgramParameters(fragProgram);
			cgGLEnableProfile(gCgState.getVertexProfile());
			cgGLBindProgram(vertProgram);

			glPopMatrix();

			glRotatef((gTime-gStartTime)*50, 0, 0, 1);

			glCullFace(GL_FRONT);

//			cgGLSetStateMatrixParameter(cgModelViewMatrix,
//										CG_GL_MODELVIEW_MATRIX,
//										CG_GL_MATRIX_IDENTITY);

			cgGLSetStateMatrixParameter(cgProjectionMatrix,
										CG_GL_PROJECTION_MATRIX,
										CG_GL_MATRIX_IDENTITY);

			cgGLSetStateMatrixParameter(cgModelViewProjMatrix,
										CG_GL_MODELVIEW_PROJECTION_MATRIX,
										CG_GL_MATRIX_IDENTITY);

			cgGLSetParameter3fv(cgCamPos, camPos);

			cgUpdateProgramParameters(vertProgram);

#ifdef GLUTPOT
			glutSolidTeapot(4);
#else
			fastTeapot(4);
#endif

			glDisable(GL_CULL_FACE);

			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glViewport(0,0, gScreenWidth, gScreenHeight);
			glDisable(GL_DEPTH_TEST);
			cgGLDisableProfile(gCgState.getFragmentProfile());
			cgGLDisableProfile(gCgState.getVertexProfile());
			glDisable(GL_DITHER);

			glViewport(0,0, gScreenWidth,gScreenHeight);

			gFBO.select_framebuffer_texture(0);
			glBindTexture(GL_TEXTURE_2D, gFBO.color_textures[1]);
			glBegin(GL_QUADS);
			for(const rectlist *r= dirtyRects; r; r= r->next)
				drawRect(*r->self);
			glEnd();

			glDisable(GL_TEXTURE_2D);
		}
};


class fluxBackgroundImage: public fluxCgEffect
{
	public:
		fluxBackgroundImage(dword fluxHandleToAttachTo)
			: fluxCgEffect(fluxHandleToAttachTo)
		{
			setup();
		}

		fluxBackgroundImage(int x, int y, int width, int height, dword parent= NOPARENT, int alignment= ALIGN_LEFT|ALIGN_TOP)
			: fluxCgEffect(x,y, width,height, parent, alignment)
		{
			setup();
		}

	private:
		GLuint texture1;

		void setup()
		{
			texture1= loadTexture("data/moonrisemk_connelley.png");
		}

		virtual void paint(primitive *self, rect *absPos, const rectlist *dirtyRects)
		{
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, texture1);
			glBegin(GL_QUADS);
			for(const rectlist *r= dirtyRects; r; r= r->next)
				drawRect(*r->self);
			glEnd();
			glDisable(GL_TEXTURE_2D);
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
		CGparameter cgIntensity;

		void setup()
		{
			displacementTexture= loadTexture("data/metal.png");
			loadFragmentProgram("cg/displace.cg", "displaceHeightmap");
			cgIntensity= cgGetNamedParameter(fragProgram, "intensity");
		}

		virtual void paint(primitive *self, rect *absPos, const rectlist *dirtyRects)
		{
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, displacementTexture);
			glActiveTexture(GL_TEXTURE0);
			cgGLBindProgram(fragProgram);
			cgGLSetParameter1f(cgIntensity, 0.25 * sqrt((absPos->rgt-absPos->x)*(absPos->btm-absPos->y))/sqrt(200*180));
			cgUpdateProgramParameters(fragProgram);
			fluxCgEffect::paint(self, absPos, dirtyRects);
		}
};




#endif //FLUXEFFECTS_H

