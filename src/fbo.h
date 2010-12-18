#ifndef FBO_H_INCLUDED
#define FBO_H_INCLUDED

inline bool checkglerror(bool fatal= false)
{
    int err= glGetError();
    if(err)
    {
    	printf("GL Error: %s\n", gluErrorString(err));
        if(fatal) abort();
        else return false;
    }
    return true;
}



#define UPLOAD_TEXIMAGE		0	// whether to use glTexSubImage/glGetTexImage for upload (to graphics card)
#define DOWNLOAD_TEXIMAGE	0	// and download (from graphics card). Note glGetTexImage always transfers the whole texture
template<int NTEXTURES= 1,
         bool USE_COLORBUF= true, int COLOR_FORMAT= GL_RGBA32F, bool USE_DEPTHBUF= true, bool USE_STENCIL= false,
         bool USE_DEPTHTEXTURES= false>
    struct fbo
{
    int width, height;
    GLuint fb;
    GLuint depth_renderbuffer, stencil_renderbuffer;
    GLuint color_textures[NTEXTURES];
    GLuint depth_textures[NTEXTURES];
    bool using_packed_depth_stencilbuf;
    bool initialized;

    fbo(): width(0), height(0), using_packed_depth_stencilbuf(false), initialized(false)
    { }

    void enable()
    {
        if(!initialized) return;
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);
        checkglerror();
    }

    void set_ortho_mode()
    {
		if(!initialized) return;
		// viewport for 1:1 pixel=texel=geometry mapping
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluOrtho2D(0.0, width, 0.0, height);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glViewport(0, 0, width, height);
    }

    static void disable()
    {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    }

    // select texture for drawing
    void select_framebuffer_texture(int index)
    {
		if(!initialized) return;
        if(index<0 || index>=NTEXTURES)
        {
            printf("index %d out of range\n", index);
            return;
        }
        if(USE_COLORBUF)
		{
			glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
										GL_TEXTURE_2D, color_textures[index], 0);

//			this does not work reliably on mesa for intel gma
//            glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT+index);
//            glReadBuffer(GL_COLOR_ATTACHMENT0_EXT+index);
		}
        if(USE_DEPTHBUF && USE_DEPTHTEXTURES)
            // attach depth texture to fbo's depth buffer
            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT,
                                      GL_TEXTURE_2D, depth_textures[index], 0);
		checkglerror();
    }

    void write_to_texture(int texIndex, int posX, int posY, int _width, int _height, const float *data)
    {
		if(!initialized) return;
#if UPLOAD_TEXIMAGE
		glBindTexture(GL_TEXTURE_2D, color_textures[texIndex]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, posX, posY, _width, _height,
						COLOR_FORMAT, GL_FLOAT, data);
#else
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + texIndex);
		glRasterPos2i(posX, posY);
		GLboolean texturing= glIsEnabled(GL_TEXTURE_2D);
		if(texturing) glDisable(GL_TEXTURE_2D);
		glDrawPixels(_width, _height, COLOR_FORMAT, GL_FLOAT, data);
		if(texturing) glEnable(GL_TEXTURE_2D);
#endif
	}

	void read_from_texture(int texIndex, int posX, int posY, int _width, int _height, float *data)
	{
		if(!initialized) return;
#if DOWNLOAD_TEXIMAGE
		if(posX!=0 || posY!=0 || _width!=width || _height!=height)
		{
			printf("glGetTexImage: must transfer whole texture\n");
			abort();
		}
		glBindTexture(GL_TEXTURE_2D, color_textures[texIndex]);
		glGetTexImage(GL_TEXTURE_2D, 0, COLOR_FORMAT, GL_FLOAT, data);
#else
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT + texIndex);
		checkglerror();
		// todo: check for more color formats
		glReadPixels(posX, posY, _width, _height,
					 (COLOR_FORMAT==GL_LUMINANCE32F_ARB? GL_LUMINANCE:
					  COLOR_FORMAT==GL_ALPHA32F_ARB? GL_ALPHA:
					  GL_RGBA32F),
					 GL_FLOAT, data);
		checkglerror();
#endif
	}

	void free_resources()
	{
		if(!initialized) return;
		disable();
        if(USE_DEPTHBUF && USE_DEPTHTEXTURES)
            glDeleteRenderbuffersEXT(1, &depth_renderbuffer);
        if(USE_COLORBUF)
			glDeleteTextures(NTEXTURES, color_textures);
		if(USE_DEPTHTEXTURES)
			glDeleteTextures(NTEXTURES, depth_textures);
		glDeleteFramebuffersEXT(1, &fb);
		width= height= 0;
		initialized= false;
	}

    bool create(int width, int height)
    {
        #define estr(x) { x, #x }
        struct { unsigned error; const char *name; } errlookup[]=
        {
            estr(GL_FRAMEBUFFER_COMPLETE),
            estr(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            estr(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
            estr(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT),
            estr(GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT),
            estr(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT),
            estr(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT),
            estr(GL_FRAMEBUFFER_UNSUPPORTED),
        };
        #undef estr

        GLint maxbuffers;
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxbuffers);
		printf("max. color attachments: %d\n", maxbuffers);

        this->width= width;
        this->height= height;

        checkglerror(true);

        // create objects
        glGenFramebuffersEXT(1, &fb);           // frame buffer
        if(USE_DEPTHBUF) // && USE_DEPTHTEXTURES)
            glGenRenderbuffersEXT(1, &depth_renderbuffer);    // depth render buffers
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);
        checkglerror(true);

        if(USE_COLORBUF)
        {
            // generate color textures
            glGenTextures(NTEXTURES, color_textures);
            for(int i= 0; i<NTEXTURES; i++)
            {
                // initialize color texture
                glBindTexture(GL_TEXTURE_2D, color_textures[i]);

                glTexImage2D(GL_TEXTURE_2D, 0, COLOR_FORMAT, width, height, 0,
                             GL_RGB, GL_UNSIGNED_BYTE, NULL);
                // set texture parameters
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                // attach color texture to framebuffer's color buffer
                glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT+i,
                                          GL_TEXTURE_2D, color_textures[i], 0);
                checkglerror(true);
            }
        }
        else
        {
            // no color buffer, turn off read & write buffers to make fbo complete
            glReadBuffer(GL_NONE);
            glDrawBuffer(GL_NONE);
        }

        if(USE_DEPTHBUF || USE_STENCIL)
        {
            if(USE_STENCIL)
            {
                // try to initialize packed depth-stencil renderbuffer
                glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth_renderbuffer);
                glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_STENCIL_EXT, width, height);
                glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depth_renderbuffer);
                glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depth_renderbuffer);

                if(!glGetError())
                {
                    using_packed_depth_stencilbuf= true;
                    printf("using packed stencil/depth buffer\n");
                }
                else
                {
                    // try to initialize separate stencil renderbuffer
                    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, stencil_renderbuffer);
                    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_STENCIL_INDEX, width, height);
                    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT,
                                                 GL_RENDERBUFFER_EXT, stencil_renderbuffer);
                    int e= glGetError();
                    if(e)
                    {
                        // fail!
                        printf("failed to initialize FBO stencil buffer:\n%s\n", gluErrorString(e));
                        // continue anyway
                    }
                    else printf("using separate stencil buffer\n");
                }
            }
            else
            {
                // initialize depth renderbuffer
                glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth_renderbuffer);
                glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT16, width, height);
            }

            if(USE_DEPTHTEXTURES)
            {
                // generate depth textures
                glGenTextures(NTEXTURES, depth_textures);
                for(int i= 0; i<NTEXTURES; i++)
                {
                    // initialize depth texture
                    glBindTexture(GL_TEXTURE_2D, color_textures[i]);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT/*todo: use DEPTH24_STENCIL8_EXT if necessary*/,
                                 width, height, 0, GL_LUMINANCE/* -GL_UNSIGNED_INT_24_8_EXT*/, GL_FLOAT, NULL);
                    // set texture parameters
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    checkglerror(true);
                }

                // attach depth texture 0 to fbo's depth buffer
                glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT,
                                          GL_TEXTURE_2D, depth_textures[0], 0);
            }
            else
            {
                // attach renderbuffer to framebuffer's depth buffer
                glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                                             GL_RENDERBUFFER_EXT, depth_renderbuffer);
            }
        }
        else
            glDisable(GL_DEPTH_TEST);

        checkglerror();

        GLenum status= glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
        if(status!=GL_FRAMEBUFFER_COMPLETE)
        {
            for(unsigned i= 0; i<sizeof(errlookup)/sizeof(errlookup[0]); i++)
                if(errlookup[i].error==status)
                {
                    puts(errlookup[i].name);
                    break;
                }
            return false;
        }
        checkglerror();

        int depth_bits= 0, stencil_bits= 0;
        if(USE_DEPTHBUF || USE_STENCIL)
		{
			glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_DEPTH_SIZE_EXT, &depth_bits);
			if(USE_STENCIL && !using_packed_depth_stencilbuf) glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, stencil_renderbuffer);
			glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_STENCIL_SIZE_EXT, &stencil_bits);
		}
        printf("fbo initialized, size %dx%d, depth bits: %d, stencil bits: %d\n", width, height, depth_bits, stencil_bits);
        checkglerror();

        return (initialized= true);
    }
};

#endif // FBO_H_INCLUDED
