#ifndef __GL_WIN32_H__
#define __GL_WIN32_H__

#include <stdint.h>
#include <GL/glew.h>
	
enum enImageFormat
{
	enImageUnknown = 0,
	enImageYUV420P,
	enImageYV12,
	enImageNV21,
	enImageNV12,
	enImageYUYV,
	enImageRGB24,
	enImageRGB32,
	enImageBGRA,
	enImageRGB565,
	enImageRGB555,
	enImageiOSPixel,
	enImageEncodeSurface,
	enImageRenderSurface,
	enImageMaxFormat,
};

enum enDirection {
	UP,
	LEFT,
	DOWN,
	RIGHT,
};

enum enRenderMode {
	enRenderAuto,
	enRenderFull,
	enRender_16_9,
	enRedner_4_3,
};

typedef struct SPoint {
	int x, y;
} SPoint;

typedef struct SSize {
	int w, h;
} SSize;

typedef struct SRect {
	SPoint point;
	SSize size;
} SRect;

enum enShaderMode {
	enUnknown,
	enI420,
	enYV12,
	enNV12,
	enNV21,
	enRGB,
	enRGBA,
	enBLACK,
};

enum enUniform {
	U_TYPE,
	U_PLANE_Y,
	U_PLANE_U,
	U_PLANE_V,
	U_MAX,
};

typedef struct {
	HDC affinityHDC; // DC for the selected GPU
	HDC hGLDC;
	HGLRC hGLRC;

	/* */
	HWND hwnd;                  /* Handle of the main window */
	HWND hvideownd;        /* Handle of the video sub-window */
	HWND hparent;             /* Handle of the parent window */
	HWND hfswnd;          /* Handle of the fullscreen window *

	/* size of the display */
	RECT rect_display;

	/* size of the overall window (including black bands) */
	RECT rect_parent;

	unsigned changes;        /* changes made to the video display */

	/* Misc */
	bool is_first_display;
	bool is_on_top;

	/* screensaver system settings to be restored when vout is closed */
	UINT i_spi_screensaveactive;

	/* Coordinates of src and dest images (used when blitting to display) */
	RECT rect_dest;

	int width;
	int height;
} display_t;

/**
 * Structure used to store the result of a vout_display_PlacePicture.
 */
typedef struct {
	int x;
	int y;
	unsigned width;
	unsigned height;
} place_t;

/**
 * video format description
 */
typedef struct video_format_t
{
	unsigned int i_width;                                 /**< picture width */
	unsigned int i_height;                               /**< picture height */
	unsigned int i_x_offset;               /**< start offset of visible area */
	unsigned int i_y_offset;               /**< start offset of visible area */
	unsigned int i_visible_width;                 /**< width of visible area */
	unsigned int i_visible_height;               /**< height of visible area */

	unsigned int i_bits_per_pixel;             /**< number of bits per pixel */

	unsigned int i_sar_num;                   /**< sample/pixel aspect ratio */
	unsigned int i_sar_den;
} video_format_t;

typedef struct {

	/* Shader variables commands*/
	PFNGLGETUNIFORMLOCATIONPROC      GetUniformLocation;
	PFNGLGETATTRIBLOCATIONPROC       GetAttribLocation;
	PFNGLVERTEXATTRIBPOINTERPROC     VertexAttribPointer;
	PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;

	PFNGLUNIFORMMATRIX4FVPROC   UniformMatrix4fv;
	PFNGLUNIFORM4FVPROC         Uniform4fv;
	PFNGLUNIFORM4FPROC          Uniform4f;
	PFNGLUNIFORM1IPROC          Uniform1i;

	/* Shader command */
	PFNGLCREATESHADERPROC CreateShader;
	PFNGLSHADERSOURCEPROC ShaderSource;
	PFNGLCOMPILESHADERPROC CompileShader;
	PFNGLDELETESHADERPROC   DeleteShader;

	PFNGLCREATEPROGRAMPROC CreateProgram;
	PFNGLLINKPROGRAMPROC   LinkProgram;
	PFNGLUSEPROGRAMPROC    UseProgram;
	PFNGLDELETEPROGRAMPROC DeleteProgram;

	PFNGLATTACHSHADERPROC  AttachShader;

	/* Shader log commands */
	PFNGLGETPROGRAMIVPROC  GetProgramiv;
	PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
	PFNGLGETSHADERIVPROC   GetShaderiv;
	PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;

	PFNGLGENBUFFERSPROC    GenBuffers;
	PFNGLBINDBUFFERPROC    BindBuffer;
	PFNGLBUFFERDATAPROC    BufferData;
	PFNGLBUFFERSUBDATAPROC BufferSubData;
	PFNGLDELETEBUFFERSPROC DeleteBuffers;

	PFNGLACTIVETEXTUREPROC  ActiveTexture;
	PFNGLCLIENTACTIVETEXTUREPROC  ClientActiveTexture;

	PFNGLBINDATTRIBLOCATIONPROC BindAttribLocation;
} opengl_t;

class GLEvent
{
public:
	GLEvent(void);

	~GLEvent(void);

	int Init(display_t* sys);

	void Thread(void);

	long HWNDEventProc(HWND hwnd, UINT message,
		WPARAM wParam, LPARAM lParam);

private:
	int _Win32CreateWindow(void);

private:
	TCHAR class_main[256];
	TCHAR class_video[256];

	HWND thread;
	bool b_ready;
	bool b_done;
	bool b_error;

	display_t* _display;

	CRITICAL_SECTION cs;

	struct event_t {
		bool is_cursor_hidden;
		HCURSOR cursor_arrow;
		HCURSOR cursor_empty;
		unsigned button_pressed;

		/* Title */
		char *psz_title;

		int i_window_style;

		HICON icon;

		HWND hparent;
		HWND hwnd;
		HWND hvideownd;
		HWND hfswnd;

		int width, height;
	} event;
};

class GLWin32
{
public:
	GLWin32(void);

	~GLWin32(void);

	void Open(HWND hwnd);

	void UpdateRects(void);

	void Manager(void);

	void Draw(uint8_t * pic, int w, int h);

	void Close(void);

private:
	void _CreateGPUAffinityDC(UINT);

	void _InitOpenGLEnvironment();

	void _InitOpenGL();

private:
	display_t _display;
	GLEvent _event;
	opengl_t gl_env;

	struct {
		GLuint program;
		GLuint vbo, coorinate, vertex;
		GLuint texture[3];
	} ctx ;

	void deleteBuffer(GLuint &gl);

	void deleteTextures(GLuint &gl);

	void deleteProgram(GLuint &program);

	void deleteFramebuffers(GLuint & fb);

	void deleteRenderbuffers(GLuint & rb);

	GLuint buildProgram(void);

	bool linkProgram(GLuint program);

	bool useProgram(GLuint program);

	//void addAttribute(GLuint program, const char *attr, GLuint index);

	void setUniform(GLuint program, enUniform uniform, int value);

	enShaderMode setShaderMode(GLuint gl, enImageFormat format);

	void draw(SRect rect);

	void draw(enRenderMode mode, int pic_w, int pic_h,
		int view_w, int view_h, enDirection direction);

	void drawBlack(void);

	GLuint genTexture(int layer);

	void bindTexture(int layer, GLenum fmt, int width, int height,
		unsigned char * data);

	void bindYUV420(unsigned char *yuv, int width, int height);

	void bindYV12(unsigned char *yuv, int width, int height);

	void bindNV12(unsigned char *yuv, int width, int height);

	void bindNV21(unsigned char *yuv, int width, int height);

	void bindPicture(GLuint gl, enImageFormat fmt, uint8_t * pic, int w, int h);

	void bindPicture(GLuint gl, enImageFormat fmt,
		uint8_t * y, uint8_t * u, uint8_t * v, int w, int h);

	void bindBGRA(GLuint gl, uint8_t * img, int w, int h);

	void bindPicture(GLuint gl, enImageFormat fmt,
		uint8_t * y, uint8_t * uv, int w, int h);

	GLuint genAndBindVAO(void);

	GLuint genAndBindVBO(void);

	GLuint genCoordinate(void);

	void updateCoordinate(GLuint gl, enDirection dir);

	void printGLString(const char *name, GLenum s);

	void addAttribute(GLuint program, const char *attr, GLuint index);

	void deleteShader(GLuint &shader);

	GLuint createShader(const char *sz_shader, int type);
};

#endif