
#include "stdafx.h"
#include "MEGL.h"

#define LOGV	
#define LOGI
#define LOGE
#define SDK_UNUSED

namespace egl {
	const char *TAG = "egl";

	bool makeCurrent(EGLDisplay dpy, EGLSurface sf, EGLContext ctx) {
		EGLBoolean res;
		res = eglMakeCurrent(dpy, sf, sf, ctx);

		return res == EGL_TRUE;
	}

	bool releaseCurrent(EGLDisplay dpy) {
		EGLBoolean res;
		res = eglMakeCurrent(dpy, EGL_NO_SURFACE,
			EGL_NO_SURFACE, EGL_NO_CONTEXT);

		return res == EGL_TRUE;
	}

	EGLBoolean swapBuffers(EGLDisplay dpy, EGLSurface sf) {
		return eglSwapBuffers(dpy, sf);
	}

	void *getSymbol(const char *procname) {
		return (void *)eglGetProcAddress(procname);
	}

	const char *queryString(EGLDisplay dpy, int name) {
		return eglQueryString(dpy, name);
	}

	bool checkToken(const char *haystack, const char *needle) {
		size_t len = strlen(needle);
		while (haystack != NULL) {
			while (*haystack == ' ') {
				haystack++;
			}

			if (!strncmp(haystack, needle, len) &&
				memchr(" ", haystack[len], 2) != NULL) {
				return true;
			}

			haystack = strchr(haystack, ' ');
		}
		return false;
	}

	bool checkAPI(EGLDisplay dpy, const char *api) {
		const char *apis = eglQueryString(dpy, EGL_CLIENT_APIS);
		if (apis != NULL) {
			LOGV(TAG, "apis:%s\n", apis);
			return checkToken(apis, api);
		}
		return false;
	}

	bool checkClientExt(const char *name) {
		const char *exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
		return checkToken(exts, name);
	}

	void destroy(EGLDisplay dpy, EGLSurface sf, EGLContext ctx) {
		EGLBoolean res;
		if (dpy != EGL_NO_DISPLAY) {
			if (ctx != EGL_NO_CONTEXT) {
				res = eglDestroyContext(dpy, ctx);
				LOGV(TAG, "eglDestroyContext:%p res:%d", ctx, res);
			}
			if (sf != EGL_NO_SURFACE) {
				res = eglDestroySurface(dpy, sf);
				LOGV(TAG, "eglDestroySurface:%p res:%d", sf, res);
			}
			res = eglTerminate(dpy);
			LOGV(TAG, "eglTerminate:%p res:%d", dpy, res);
		}
	}

	EGLDisplay getDisplay(EGLNativeDisplayType display_id) {
		return eglGetDisplay(display_id);
	}

	EGLBoolean initialize(EGLDisplay dpy, EGLint *major, EGLint *minor) {
		return eglInitialize(dpy, major, minor);
	}

	EGLBoolean chooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
		EGLConfig *configs, EGLint config_size,
		EGLint *num_config) {
		return eglChooseConfig(dpy, attrib_list, configs,
			config_size, num_config);
	}

	EGLBoolean bindAPI(EGLenum api) {
		return eglBindAPI(api);
	}

	EGLSurface createWindowSurface(EGLDisplay dpy, EGLConfig config,
		EGLNativeWindowType win,
		const EGLint *attrib_list) {
		return eglCreateWindowSurface(dpy, config, win, attrib_list);
	}

	EGLContext createContext(EGLDisplay dpy, EGLConfig config,
		EGLContext share_context,
		const EGLint *attrib_list) {
		return eglCreateContext(dpy, config, share_context, attrib_list);
	}
}
