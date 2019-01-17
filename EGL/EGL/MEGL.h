#ifndef __SDK_EGL_H__
#define __SDK_EGL_H__

#include <EGL/egl.h>

namespace egl {
	struct api_t {
		const char * name;
		EGLenum  api;
		EGLint min_minor;
		EGLint render_bit;
		EGLint attr[3];
	};

	bool makeCurrent(EGLDisplay dpy, EGLSurface sf, EGLContext ctx);

	bool releaseCurrent(EGLDisplay dpy);

	EGLBoolean swapBuffers(EGLDisplay dpy, EGLSurface sf);

	void * getSymbol(const char * procname);

	const char * queryString(EGLDisplay dpy, int name);

	bool checkToken(const char * haystack, const char * needle);

	bool checkAPI(EGLDisplay dpy, const char * api);

	bool checkClientExt(const char * name);

	void destroy(EGLDisplay dpy, EGLSurface sf, EGLContext ctx);

	EGLDisplay getDisplay(EGLNativeDisplayType display_id);

	EGLBoolean initialize(EGLDisplay dpy, EGLint *major, EGLint *minor);

	EGLBoolean chooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
		EGLConfig *configs, EGLint config_size,
		EGLint *num_config);

	EGLBoolean bindAPI(EGLenum api);

	EGLSurface createWindowSurface(EGLDisplay dpy, EGLConfig config,
		EGLNativeWindowType win,
		const EGLint *attrib_list);

	EGLContext createContext(EGLDisplay dpy, EGLConfig config,
		EGLContext share_context,
		const EGLint *attrib_list);
}
#endif
