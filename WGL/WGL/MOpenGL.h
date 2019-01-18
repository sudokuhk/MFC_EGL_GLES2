#ifndef __SDK_OPENGL_H__
#define __SDK_OPENGL_H__

#include <GLES2/gl2.h>

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

typedef unsigned char uint8_t;

namespace opengl {

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

	SRect fixSize(enRenderMode mode, int pic_w, int pic_h,
		int view_w, int view_h, enDirection direction);

	GLuint genAndBindVAO(void);

	GLuint genAndBindVBO(void);

	GLuint genCoordinate(void);

	void updateCoordinate(GLuint gl, enDirection dir);

	void printGLString(const char *name, GLenum s);
}

#endif
