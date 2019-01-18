#include "stdafx.h"
#include "GLWin32.h"
#include <windows.h>
#include <GL/glew.h>
#include <stdio.h>

#define IDM_TOGGLE_ON_TOP WM_USER + 1

DECLARE_HANDLE(HGPUNV);
typedef struct _GPU_DEVICE {
	DWORD cb;
	CHAR DeviceName[32];
	CHAR DeviceString[128];
	DWORD Flags;
	RECT rcVirtualScreen;
} GPU_DEVICE, *PGPU_DEVICE;

typedef HDC(WINAPI * PFNWGLCREATEAFFINITYDCNVPROC) (const HGPUNV *phGpuList);
typedef BOOL(WINAPI * PFNWGLDELETEDCNVPROC) (HDC hdc);
typedef BOOL(WINAPI * PFNWGLENUMGPUDEVICESNVPROC) (HGPUNV hGpu, UINT iDeviceIndex, PGPU_DEVICE lpGpuDevice);
typedef BOOL(WINAPI * PFNWGLENUMGPUSFROMAFFINITYDCNVPROC) (HDC hAffinityDC, UINT iGpuIndex, HGPUNV *hGpu);
typedef BOOL(WINAPI * PFNWGLENUMGPUSNVPROC) (UINT iGpuIndex, HGPUNV *phGpu);
typedef int (WINAPI * PFNWGLGETSWAPINTERVALEXTPROC) (void);
typedef BOOL(WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);

namespace {
	static inline bool HasExtension(const char *apis, const char *api)
	{
		size_t apilen = strlen(api);
		while (apis) {
			while (*apis == ' ')
				apis++;
			if (!strncmp(apis, api, apilen) && memchr(" ", apis[apilen], 2))
				return true;
			apis = strchr(apis, ' ');
		}
		return false;
	}

	static HCURSOR EmptyCursor(HINSTANCE instance)
	{
		const int cw = GetSystemMetrics(SM_CXCURSOR);
		const int ch = GetSystemMetrics(SM_CYCURSOR);

		HCURSOR cursor = NULL;
		uint8_t *and = (uint8_t *)malloc(cw * ch);
		uint8_t *xor = (uint8_t *)malloc(cw * ch);
		if (and && xor)
		{
			memset(and, 0xff, cw * ch);
			memset(xor, 0x00, cw * ch);
			cursor = CreateCursor(instance, 0, 0, cw, ch, and, xor);
		}
		free(and);
		free(xor);

		return cursor;
	}

	static long FAR PASCAL WinEventProc(HWND hwnd, UINT message,
		WPARAM wParam, LPARAM lParam)
	{
		GLEvent * p_event;
		if (message == WM_CREATE) {
			/* Store vd for future use */
			p_event = (GLEvent *)((CREATESTRUCT *)lParam)->lpCreateParams;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)p_event);
			return TRUE;
		} else {
			LONG_PTR p_user_data = GetWindowLongPtr(hwnd, GWLP_USERDATA);
			p_event = (GLEvent *)p_user_data;
			if (!p_event) {
				/* Hmmm mozilla does manage somehow to save the pointer to our
				 * windowproc and still calls it after the vout has been closed. */
				return DefWindowProc(hwnd, message, wParam, lParam);
			}
		}

		long ret = p_event->HWNDEventProc(hwnd, message, wParam, lParam);
		if (ret >= 0) {
			return ret;
		}

		/* Let windows handle the message */
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
}

unsigned __stdcall EventThread(void * arg)
{
	GLEvent * glEvent = (GLEvent *)arg;
	glEvent->Thread();
	return 0;
}

GLEvent::GLEvent(void)
{
	_sntprintf(class_main, sizeof(class_main) / sizeof(*class_main),
		_T("video main %p"), this);
	_sntprintf(class_video, sizeof(class_video) / sizeof(*class_video),
		_T("video output %p"), this);

	InitializeCriticalSection(&cs);

	memset(&event, 0, sizeof(event));
}

GLEvent::~GLEvent(void)
{
	DeleteCriticalSection(&cs);
}

int GLEvent::Init(display_t * sys)
{
	_display = sys;
	b_ready = b_done = b_error = false;
	event.hparent = sys->hparent;
	event.width = sys->width;
	event.height = sys->height;

	unsigned int id = 0;
	thread = (HWND)_beginthreadex(NULL, 0, 
		&EventThread, (void *)this, 0, &id);

	while (!b_ready) {
		Sleep(100);
	}

	_display->hwnd = event.hwnd;
	_display->hfswnd = event.hfswnd;
	_display->hvideownd = event.hvideownd;

	return 0;
}

void GLEvent::Thread(void)
{
	MSG msg;

	EnterCriticalSection(&cs);
	if (!_Win32CreateWindow()) {
		b_error = true;
	}
	b_ready = true;
	LeaveCriticalSection(&cs);

	SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED | ES_CONTINUOUS);

	/* Main loop */
   /* GetMessage will sleep if there's no message in the queue */
	for (;; ) {
		place_t place;
		video_format_t source;

		if (!GetMessage(&msg, 0, 0, 0)) {
			EnterCriticalSection(&cs);
			b_done = true;
			LeaveCriticalSection(&cs);
			break;
		}

		EnterCriticalSection(&cs);
		bool done = b_done;
		LeaveCriticalSection(&cs);

		if (done) {
			break;
		}

		switch (msg.message) {
		case WM_MOUSEMOVE:
			//printf("WM_MOUSEMOVE\n");
			break;
		default:
			/* Messages we don't handle directly are dispatched to the
			 * window procedure */
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			break;
		}
	}

	/* Check for WM_QUIT if we created the window */
	if (!event.hparent && msg.message == WM_QUIT) {
		printf("WM_QUIT... should not happen!!\n");
		event.hwnd = NULL; /* Window already destroyed */
	}
	printf("exit event thread\n");
}

long GLEvent::HWNDEventProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	event_t * p_event = &event;
	if (message == WM_CAPTURECHANGED) {
		for (int button = 0; p_event->button_pressed; button++) {
			unsigned m = 1 << button;
			if (p_event->button_pressed & m)
				//vout_display_SendEventMouseReleased(p_event->vd, button);
			p_event->button_pressed &= ~m;
		}
		p_event->button_pressed = 0;
		return 0;
	}

	if (hwnd == p_event->hvideownd) {
		switch (message) {
		/*
        ** For OpenGL and Direct3D, vout will update the whole
        ** window at regular interval, therefore dirty region
        ** can be ignored to minimize repaint.
        */
		case WM_ERASEBKGND:
			/* nothing to erase */
			return 1;
		case WM_PAINT:
			/* nothing to repaint */
			ValidateRect(hwnd, NULL);
			// fall through
		default:
			return -1;
		}
	}

	switch (message)
	{
	case WM_WINDOWPOSCHANGED:
		return 0;
		/* the user wants to close the window */
	case WM_CLOSE:
		//vout_display_SendEventClose(vd);
		return 0;
		/* the window has been closed so shut down everything now */
	case WM_DESTROY:
		printf("WinProc WM_DESTROY\n");
		/* just destroy the window */
		PostQuitMessage(0);
		return 0;
	case WM_SYSCOMMAND:
		switch (wParam) {
		case IDM_TOGGLE_ON_TOP: {           /* toggle the "on top" status */
			printf("WinProc WM_SYSCOMMAND: IDM_TOGGLE_ON_TOP\n");
			//HMENU hMenu = GetSystemMenu(vd->sys->hwnd, FALSE);
			//vout_display_SendWindowState(vd, (GetMenuState(hMenu, 
			//	IDM_TOGGLE_ON_TOP, MF_BYCOMMAND) & MF_CHECKED) ?
			//	VOUT_WINDOW_STATE_NORMAL : VOUT_WINDOW_STATE_ABOVE);
			return 0;
		}
		default:
			break;
		}
		break;
	case WM_PAINT:
	case WM_NCPAINT:
	case WM_ERASEBKGND:
		return -1;
	case WM_KILLFOCUS:
	case WM_SETFOCUS:
		return 0;
	default:
		//msg_Dbg( vd, "WinProc WM Default %i", message );
		break;
	}

	return -1;
}

int GLEvent::_Win32CreateWindow(void)
{
	HINSTANCE  hInstance;
	HMENU      hMenu;
	RECT       rect_window;
	WNDCLASS   wc;                            /* window class components */
	TCHAR      path[MAX_PATH + 1];
	int        i_style, i_stylex;
	printf("_Win32CreateWindow\n");

	/* Get this module's instance */
	hInstance = GetModuleHandle(NULL);

	event_t * p_event = &event;

	p_event->cursor_arrow = LoadCursor(NULL, IDC_ARROW);
	p_event->cursor_empty = EmptyCursor(hInstance);
	p_event->icon = NULL;
	if (GetModuleFileName(NULL, path, MAX_PATH)) {
		p_event->icon = ExtractIcon(hInstance, path, 0);
	}

	/* Fill in the window class structure */
	wc.style = CS_OWNDC | CS_DBLCLKS;          /* style: dbl click */
	wc.lpfnWndProc = (WNDPROC)WinEventProc;       /* event handler */
	wc.cbClsExtra = 0;                         /* no extra class data */
	wc.cbWndExtra = 0;                        /* no extra window data */
	wc.hInstance = hInstance;                            /* instance */
	wc.hIcon = p_event->icon;       /* load the vlc big icon */
	wc.hCursor = p_event->is_cursor_hidden ? p_event->cursor_empty :
		p_event->cursor_arrow;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);  /* background color */
	wc.lpszMenuName = NULL;                                  /* no menu */
	wc.lpszClassName = class_main;       /* use a special class */

	/* Register the window class */
	if (!RegisterClass(&wc)) {
		if (p_event->icon)
			DestroyIcon(p_event->icon);

		printf("Win32VoutCreateWindow RegisterClass FAILED (err=%lu)\n", GetLastError());
		return -1;
	}

	/* Register the video sub-window class */
	wc.lpszClassName = class_video;
	wc.hIcon = 0;
	wc.hbrBackground = NULL; /* no background color */
	if (!RegisterClass(&wc)) {
		printf("Win32VoutCreateWindow RegisterClass FAILED (err=%lu)\n", GetLastError());
		return -1;
	}

	/* When you create a window you give the dimensions you wish it to
	 * have. Unfortunatly these dimensions will include the borders and
	 * titlebar. We use the following function to find out the size of
	 * the window corresponding to the useable surface we want */
	rect_window.left = 10;
	rect_window.top = 10;
	rect_window.right = rect_window.left + p_event->width;
	rect_window.bottom = rect_window.top + p_event->height;

	/* Open with window decoration */
	AdjustWindowRect(&rect_window, WS_OVERLAPPEDWINDOW | WS_SIZEBOX, 0);
	i_style = WS_OVERLAPPEDWINDOW | WS_SIZEBOX | WS_VISIBLE | WS_CLIPCHILDREN;
	i_stylex = 0;

	if (p_event->hparent) {
		i_style = WS_VISIBLE | WS_CLIPCHILDREN | WS_CHILD;
		i_stylex = 0;

		/* allow user to regain control over input events if requested */
		bool b_mouse_support = true;// var_InheritBool(vd, "mouse-events");
		bool b_key_support = true;// var_InheritBool(vd, "keyboard-events");
		if (!b_mouse_support && !b_key_support)
			i_style |= WS_DISABLED;
	}

	p_event->i_window_style = i_style;
	/* Create the window */
	p_event->hwnd =
		CreateWindowEx(WS_EX_NOPARENTNOTIFY | i_stylex,
			class_main,             /* name of window class */
			_T(" (Video Output)"),  /* window title */
			i_style,                                 /* window style */
			(UINT)CW_USEDEFAULT,  /* default X coordinate */
			(UINT)CW_USEDEFAULT,   /* default Y coordinate */
			rect_window.right - rect_window.left,    /* window width */
			rect_window.bottom - rect_window.top,   /* window height */
			p_event->hparent,                       /* parent window */
			NULL,                          /* no menu in this window */
			hInstance,            /* handle of this program instance */
			(LPVOID)p_event);           /* send vd to WM_CREATE */
	if (!p_event->hwnd) {
		printf("Win32VoutCreateWindow create window FAILED (err=%lu)\n", GetLastError());
		return -1;
	}

	if (p_event->hparent)
	{
		LONG i_style;

		/* We don't want the window owner to overwrite our client area */
		i_style = GetWindowLong(p_event->hparent, GWL_STYLE);

		if (!(i_style & WS_CLIPCHILDREN))
			/* Hmmm, apparently this is a blocking call... */
			SetWindowLong(p_event->hparent, GWL_STYLE,
				i_style | WS_CLIPCHILDREN);

		/* Create our fullscreen window */
		p_event->hfswnd =
			CreateWindowEx(WS_EX_APPWINDOW, class_main,
				_T(" (Fullscreen Video Output)"),
				WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_SIZEBOX,
				CW_USEDEFAULT, CW_USEDEFAULT,
				CW_USEDEFAULT, CW_USEDEFAULT,
				NULL, NULL, hInstance, NULL);
	}
	else
	{
		p_event->hfswnd = NULL;
	}

	/* Append a "Always On Top" entry in the system menu */
	hMenu = GetSystemMenu(p_event->hwnd, FALSE);
	AppendMenu(hMenu, MF_SEPARATOR, 0, _T(""));
	AppendMenu(hMenu, MF_STRING | MF_UNCHECKED,
		IDM_TOGGLE_ON_TOP, _T("Always on &Top"));

	/* Create video sub-window. This sub window will always exactly match
	 * the size of the video, which allows us to use crazy overlay colorkeys
	 * without having them shown outside of the video area. */
	 /* FIXME vd->source.i_width/i_height seems wrong */
	p_event->hvideownd = CreateWindow(
			class_video, _T(""),   /* window class */
			WS_CHILD,                   /* window style, not visible initially */
			0, 0,
			p_event->width,          /* default width */
			p_event->height,        /* default height */
			p_event->hwnd,               /* parent window */
			NULL, hInstance,
			(LPVOID)p_event);    /* send vd to WM_CREATE */

	if (!p_event->hvideownd)
		printf("can't create video sub-window\n");
	else
		printf("created video sub-window\n");

	/* Now display the window */
	ShowWindow(p_event->hwnd, SW_SHOW);
	return 0;
}

GLWin32::GLWin32(void)
{
	memset(&_display, 0, sizeof(_display));
	memset(&gl_env, 0, sizeof(gl_env));
}

GLWin32::~GLWin32(void)
{
}

void GLWin32::Open(HWND hwnd)
{
	_display.hparent = hwnd;
	_display.is_first_display = true;
	_display.width = 320;
	_display.height = 180;

	_event.Init(&_display);

	//_CreateGPUAffinityDC(1);

	display_t * sys = &_display;
	sys->hGLDC = GetDC(sys->hvideownd);

	/* Set the pixel format for the DC */
	PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd, 0, sizeof(pfd));
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;
	SetPixelFormat(sys->hGLDC,
		ChoosePixelFormat(sys->hGLDC, &pfd), &pfd);

	/*
	 * Create and enable the render context
	 * For GPU affinity, attach the window DC
	 * to the GPU affinity DC
	 */
	sys->hGLRC = wglCreateContext((sys->affinityHDC != NULL) ? sys->affinityHDC : sys->hGLDC);
	wglMakeCurrent(sys->hGLDC, sys->hGLRC);

	const char *extensions = (const char*)glGetString(GL_EXTENSIONS);
	if (HasExtension(extensions, "WGL_EXT_swap_control")) {
		PFNWGLSWAPINTERVALEXTPROC SwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
		if (SwapIntervalEXT)
			SwapIntervalEXT(1);
	}

	_InitOpenGLEnvironment();

	_InitOpenGL();
}

void GLWin32::UpdateRects(void)
{
	display_t * sys = &_display;
	RECT  rect;
	POINT point;

	/* Retrieve the window size */
	GetClientRect(sys->hwnd, &rect);

	/* Retrieve the window position */
	point.x = point.y = 0;
	ClientToScreen(sys->hwnd, &point);

	place_t place = {0, 0, rect.right, rect.bottom};
	sys->rect_dest = { 0, 0, rect.right, rect.bottom };

	if (sys->hvideownd)
		SetWindowPos(sys->hvideownd, 0,
			place.x, place.y, place.width, place.height,
			SWP_NOCOPYBITS | SWP_NOZORDER | SWP_ASYNCWINDOWPOS);
}

void GLWin32::Manager(void)
{
	display_t * sys = &_display;

	/* We used to call the Win32 PeekMessage function here to read the window
	 * messages. But since window can stay blocked into this function for a
	 * long time (for example when you move your window on the screen), I
	 * decided to isolate PeekMessage in another thread. */

	 /* If we do not control our window, we check for geometry changes
	  * ourselves because the parent might not send us its events. */
	if (sys->hparent) {
		RECT rect_parent;
		POINT point;

		/* Check if the parent window has resized or moved */
		GetClientRect(sys->hparent, &rect_parent);
		point.x = point.y = 0;
		ClientToScreen(sys->hparent, &point);
		OffsetRect(&rect_parent, point.x, point.y);

		if (!EqualRect(&rect_parent, &sys->rect_parent)) {
			sys->rect_parent = rect_parent;

			/* This code deals with both resize and move
			 *
			 * For most drivers(direct3d, gdi, opengl), move is never
			 * an issue. The surface automatically gets moved together
			 * with the associated window (hvideownd)
			 *
			 * For directx, it is still important to call UpdateRects
			 * on a move of the parent window, even if no resize occurred
			 */
			SetWindowPos(sys->hwnd, 0, 0, 0,
				rect_parent.right - rect_parent.left,
				rect_parent.bottom - rect_parent.top,
				SWP_NOZORDER);

			UpdateRects();
		}
	}
}

void GLWin32::Draw(uint8_t * pic, int w, int h)
{
	display_t * sys = &_display; 
	
	if (_display.is_first_display) {
		/* Video window is initially hidden, show it now since we got a
		* picture to show.
		*/
		SetWindowPos(_display.hvideownd, 0, 0, 0, 0, 0,
			SWP_ASYNCWINDOWPOS |
			SWP_FRAMECHANGED |
			SWP_SHOWWINDOW |
			SWP_NOMOVE |
			SWP_NOSIZE |
			SWP_NOZORDER);
		_display.is_first_display = false;
	}

	wglMakeCurrent(sys->hGLDC, sys->hGLRC);
	setShaderMode(ctx.program, enImageYUV420P);
	bindYUV420(pic, w, h);
	draw(enRenderAuto, w, h, _display.rect_dest.right,
		_display.rect_dest.bottom, UP);

	//drawBlack();

	SwapBuffers(_display.hGLDC);
	//printf("draw\n");

	Manager();
}

void GLWin32::Close(void)
{
}

/* Create an GPU Affinity DC */
void GLWin32::_CreateGPUAffinityDC(UINT nVidiaAffinity) 
{
	PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd, 0, sizeof(pfd));
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;

	/* create a temporary GL context */
	HDC winDC = GetDC(_display.hvideownd);
	::SetPixelFormat(winDC, ChoosePixelFormat(winDC, &pfd), &pfd);
	HGLRC hGLRC = wglCreateContext(winDC);
	wglMakeCurrent(winDC, hGLRC);

	/* Initialize the neccessary function pointers */
	PFNWGLENUMGPUSNVPROC fncEnumGpusNV = (PFNWGLENUMGPUSNVPROC)wglGetProcAddress("wglEnumGpusNV");
	PFNWGLCREATEAFFINITYDCNVPROC fncCreateAffinityDCNV = (PFNWGLCREATEAFFINITYDCNVPROC)wglGetProcAddress("wglCreateAffinityDCNV");

	/* delete the temporary GL context */
	wglDeleteContext(hGLRC);

	/* see if we have the extensions */
	if (!fncEnumGpusNV || !fncCreateAffinityDCNV) {
		printf("donot support GPU Affinity\n");
		return;
	}

	/* find the graphics card */
	HGPUNV GpuMask[2];
	GpuMask[0] = NULL;
	GpuMask[1] = NULL;
	HGPUNV hGPU;
	if (!fncEnumGpusNV(nVidiaAffinity, &hGPU)) return;

	/* make the affinity DC */
	GpuMask[0] = hGPU;
	_display.affinityHDC = fncCreateAffinityDCNV(GpuMask);
	if (_display.affinityHDC == NULL) {
		printf("donot support GPU Affinity\n");
		return;
	}
	SetPixelFormat(_display.affinityHDC,
		ChoosePixelFormat(_display.affinityHDC, &pfd), &pfd);

	printf("GPU affinity set to adapter: %d", nVidiaAffinity);
}

void GLWin32::_InitOpenGLEnvironment()
{
	gl_env.CreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
	gl_env.ShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
	gl_env.CompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
	gl_env.AttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");

	gl_env.GetProgramiv = (PFNGLGETPROGRAMIVPROC)wglGetProcAddress("glGetProgramiv");
	gl_env.GetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");
	gl_env.GetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog");
	gl_env.GetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");

	gl_env.DeleteShader = (PFNGLDELETESHADERPROC)wglGetProcAddress("glDeleteShader");

	gl_env.GetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
	gl_env.GetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)wglGetProcAddress("glGetAttribLocation");
	gl_env.VertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)wglGetProcAddress("glVertexAttribPointer");
	gl_env.EnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress("glEnableVertexAttribArray");
	gl_env.UniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)wglGetProcAddress("glUniformMatrix4fv");
	gl_env.Uniform4fv = (PFNGLUNIFORM4FVPROC)wglGetProcAddress("glUniform4fv");
	gl_env.Uniform4f = (PFNGLUNIFORM4FPROC)wglGetProcAddress("glUniform4f");
	gl_env.Uniform1i = (PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i");

	gl_env.CreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
	gl_env.LinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
	gl_env.UseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
	gl_env.DeleteProgram = (PFNGLDELETEPROGRAMPROC)wglGetProcAddress("glDeleteProgram");

	gl_env.GenBuffers = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
	gl_env.BindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
	gl_env.BufferData = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
	gl_env.DeleteBuffers = (PFNGLDELETEBUFFERSPROC)wglGetProcAddress("glDeleteBuffers");
	gl_env.BufferSubData = (PFNGLBUFFERSUBDATAPROC)wglGetProcAddress("glBufferSubData");

	if (!gl_env.CreateShader || !gl_env.ShaderSource || !gl_env.CreateProgram) {
		printf("do not support sharder\n");
	}

	gl_env.ActiveTexture = (PFNGLACTIVETEXTUREPROC)wglGetProcAddress("glActiveTexture");
	gl_env.ClientActiveTexture = (PFNGLCLIENTACTIVETEXTUREPROC)wglGetProcAddress("glClientActiveTexture");
	gl_env.BindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC)wglGetProcAddress("glBindAttribLocation");
	printf("initialize OpenGL environment\n");
	printGLString("Version", GL_VERSION);
	printGLString("Vendor", GL_VENDOR);
	printGLString("Renderer", GL_RENDERER);
	//printGLString("Extensions", GL_EXTENSIONS);

#undef glBeginQuery                                            
#undef glBindBuffer                                            
#undef glBufferData                                            
#undef glBufferSubData                                         
#undef glDeleteBuffers                                         
#undef glDeleteQueries                                         
#undef glEndQuery                                              
#undef glGenBuffers                                            
#undef glGenQueries                                            
#undef glGetBufferParameteriv                              
#undef glGetBufferPointerv                                     
#undef glGetBufferSubData                                      
#undef glGetQueryObjectiv                                      
#undef glGetQueryObjectuiv                                     
#undef glGetQueryiv                                            
#undef glIsBuffer                                              
#undef glIsQuery                                               
#undef glMapBuffer                                             
#undef glUnmapBuffer     
#undef glLinkProgram
#undef glUseProgram
#undef glUniform1i
#undef glVertexAttribPointer
#undef glShaderSource
#undef glGetUniformLocation
#undef glGetShaderiv
#undef glGetProgramiv
#undef glGetShaderInfoLog
#undef glGetProgramInfoLog
#undef glEnableVertexAttribArray
#undef glDeleteShader
#undef glActiveTexture
#undef glDeleteProgram
#undef glCreateShader
#undef glAttachShader
#undef glCompileShader
#undef glCreateProgram
#undef glBindAttribLocation
#undef glAttachShader
#undef glGetAttribLocation

#define glGetAttribLocation			gl_env.GetAttribLocation
#define glCompileShader				gl_env.CompileShader     
#define glDeleteProgram				gl_env.DeleteProgram     
#define glCreateShader				gl_env.CreateShader      
#define glCreateProgram				gl_env.CreateProgram     
#define glBindAttribLocation		gl_env.BindAttribLocation
#define glAttachShader				gl_env.AttachShader      
#define glActiveTexture				gl_env.ActiveTexture    
#define glGetProgramiv				gl_env.GetProgramiv
#define glGetShaderInfoLog			gl_env.GetShaderInfoLog
#define glGetProgramInfoLog			gl_env.GetProgramInfoLog
#define glEnableVertexAttribArray	gl_env.EnableVertexAttribArray
#define glDeleteShader				gl_env.DeleteShader
#define glUniform1i					gl_env.Uniform1i
#define glVertexAttribPointer		gl_env.VertexAttribPointer
#define glShaderSource				gl_env.ShaderSource
#define glGetUniformLocation		gl_env.GetUniformLocation
#define glGetShaderiv				gl_env.GetShaderiv
#define glBindBuffer				gl_env.BindBuffer
#define glBufferData				gl_env.BufferData
#define glBufferSubData				gl_env.BufferSubData
#define glDeleteBuffers				gl_env.DeleteBuffers
#define glGenBuffers				gl_env.GenBuffers
#define glUseProgram				gl_env.UseProgram
#define glLinkProgram				gl_env.LinkProgram
}

void GLWin32::_InitOpenGL()
{
	ctx.program = buildProgram();
	linkProgram(ctx.program);
	useProgram(ctx.program);
	setUniform(ctx.program, U_PLANE_Y, 0);
	setUniform(ctx.program, U_PLANE_U, 1);
	setUniform(ctx.program, U_PLANE_V, 2);
	for (GLenum error; (error = glGetError()) != GL_NO_ERROR;) {
		printf("linkProgram:%d\n", error);
	}

	ctx.vertex = genAndBindVAO();
	ctx.vbo = genAndBindVBO();
	ctx.coorinate = genCoordinate();
	for (int i = 0; i < 3; i++) {
		ctx.texture[i] = genTexture(i);
	}
	updateCoordinate(ctx.program, UP);

	/* */
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

	const GLfloat vertices[8] = {
		-1.0f, -1.0f,
		1.0f, -1.0f,
		1.0f, 1.0f,
		-1.0f, 1.0f,
	};

	const GLubyte indices[6] = {
		0, 1, 2,
		2, 3, 0
	};

	const GLfloat coordinate[8] = {    //anticlockwise
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
	};

	const int degree[4][4] = {
		3, 2, 1, 0, //up
		0, 3, 2, 1, //left
		1, 0, 3, 2, //down
		2, 1, 0, 3, //right
	};

	const char *uniform_name[] = {
		"type",
		"plane_y",
		"plane_u",
		"plane_v",
	};

#define SHADER_STRING(text) #text
	const char *shader_vs = SHADER_STRING(
		attribute vec2 position;
		attribute vec2 coordinate;
		varying vec2 texcoord;

		void main() {
			texcoord = coordinate;
			gl_Position = vec4(position, 0.0, 1.0);
		}
	);

	const char *shader_fs = SHADER_STRING(
		varying vec2 texcoord;
		uniform sampler2D plane_y;
		uniform sampler2D plane_u;
		uniform sampler2D plane_v;
		uniform int type;
		const vec3 delta = vec3(-0.0 / 255.0, -128.0 / 255.0, -128.0 / 255.0);
		const vec3 matYUVRGB1 = vec3(1.0, 0.0, 1.402);
		const vec3 matYUVRGB2 = vec3(1.0, -0.344, -0.714);
		const vec3 matYUVRGB3 = vec3(1.0, 1.772, 0.0);

		void main() {
			if (type > 0 && type <= 4) {
				vec3 yuv;
				vec3 result;

				yuv.x = texture2D(plane_y, texcoord).r;
				if (type == 1/*I420*/ || type == 2/*YV12*/) {
					yuv.y = texture2D(plane_u, texcoord).r;
					yuv.z = texture2D(plane_v, texcoord).r;
				}
				else if (type == 3) { //NV12
					yuv.y = texture2D(plane_u, texcoord).r;
					yuv.z = texture2D(plane_u, texcoord).a;
				}
				else if (type == 4) { //NV21
					yuv.y = texture2D(plane_u, texcoord).a;
					yuv.z = texture2D(plane_u, texcoord).r;
				}

				yuv += delta;
				result.x = dot(yuv, matYUVRGB1);
				result.y = dot(yuv, matYUVRGB2);
				result.z = dot(yuv, matYUVRGB3);
				gl_FragColor = vec4(result.rgb, 1.0);
			} else if (type == 5) {
				gl_FragColor = vec4(0.0, 0.0, 1.0, 1.0);
			} else if (type == 6) {
				gl_FragColor = texture2D(plane_y, texcoord).rgba;
			} else {
				gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
			}
		}
	);
#undef SHADER_STRING

void GLWin32::deleteBuffer(GLuint &gl) {
	glDeleteBuffers(1, &gl);
}

void GLWin32::deleteTextures(GLuint &gl) {
	glDeleteTextures(1, &gl);
}

void GLWin32::deleteProgram(GLuint & gl) {
	if (gl > 0) {
		glDeleteProgram(gl);
		gl = 0;
	}
}

void GLWin32::deleteShader(GLuint &shader) {
	if (shader > 0) {
		glDeleteShader(shader);
		shader = 0;
	}
}

GLuint GLWin32::createShader(const char *sz_shader, int type) {
	GLuint shader = 0;
	GLint success = 0;

	shader = glCreateShader(type);
	glShaderSource(shader, 1, &sz_shader, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE) {
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 0) {
			char *buf = (char *)malloc(infoLen);
			if (buf != NULL) {
				glGetShaderInfoLog(shader, infoLen, NULL, buf);
				printf("error:%s\n", buf);
				free(buf);
			}
			deleteShader(shader);
		}
	}
	return shader;
}

bool GLWin32::linkProgram(GLuint program) {
	if (program == 0) {
		return false;
	}

	GLint success = 0;
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success != GL_TRUE) {
		char log[1024];
		glGetProgramInfoLog(program, 1024, NULL, log);
		printf("error:%s\n", log);
		return false;
	}
	return true;
}

bool GLWin32::useProgram(GLuint program) {
	glUseProgram(program);
	return true;
}

void GLWin32::addAttribute(GLuint program, const char *attr, GLuint index) {
	glBindAttribLocation(program, index, attr);
}

GLuint GLWin32::buildProgram(void) {
	bool ok = false;
	GLuint program = 0, vs_shader = 0, fs_shader = 0;
	vs_shader = createShader(shader_vs, GL_VERTEX_SHADER);
	if (vs_shader == 0) {
		goto out;
	}
	fs_shader = createShader(shader_fs, GL_FRAGMENT_SHADER);
	if (fs_shader == 0) {
		goto out;
	}

	program = glCreateProgram();
	if (program == 0) {
		goto out;
	}
	glAttachShader(program, vs_shader);
	glAttachShader(program, fs_shader);
	addAttribute(program, "position", 0);
	addAttribute(program, "coordinate", 1);

	for (GLenum error; (error = glGetError()) != GL_NO_ERROR;) {
		printf("GLError:%d\n", error);
	}
	ok = true;
out:
	deleteShader(vs_shader);
	deleteShader(fs_shader);
	if (!ok) {
		deleteProgram(program);
	}
	return program;
}

void GLWin32::setUniform(GLuint program, enUniform uniform, int value) {
	if (program > 0 && uniform >= U_TYPE && uniform < U_MAX) {
		GLint idx = glGetUniformLocation(program, uniform_name[uniform]);
		if (idx >= 0) {
			glUniform1i(idx, (GLint)value);
		}
	}
}

enShaderMode GLWin32::setShaderMode(GLuint gl, enImageFormat format) {
	enShaderMode mode = enUnknown;
	switch (format) {
	case enImageYUV420P:
		mode = enI420;
		break;
	case enImageYV12:
		mode = enYV12;
		break;
	case enImageNV12:
		mode = enNV12;
		break;
	case enImageNV21:
		mode = enNV21;
		break;
	default:
		mode = enBLACK;
		break;
	}
	setUniform(gl, U_TYPE, (int)mode);
	return mode;
}

void GLWin32::draw(SRect rect) {
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glViewport(rect.point.x, rect.point.y,
		rect.size.w, rect.size.h);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
}

static float f_ratio(enRenderMode mode, int pic_w, int pic_h,
	int view_w, int view_h, enDirection direction) {
	float img_w = pic_w, img_h = pic_h;
	if (direction == LEFT || direction == RIGHT) {
		//std::swap(img_w, img_h);
	}

	switch (mode) {
	case enRedner_4_3:
		img_w = 4.0f;
		img_h = 3.0f;
		break;
	case enRender_16_9:
		img_w = 16.0f;
		img_h = 9.0f;
		break;
	case enRenderFull:
		break;
	case enRenderAuto:
		break;
	}
#define MEDIASDK_OPENG_ABS(n)  ((n) > 0 ? (n) : -(n))
	float ratio = (img_h * view_w) / (img_w * view_h);
	if (MEDIASDK_OPENG_ABS(ratio - 1.0f) < 0.005f) {
		ratio = 1.0f;
	}
#undef MEDIASDK_OPENG_ABS
	return ratio;
}

SRect fixSize(enRenderMode mode, int pic_w, int pic_h,
	int view_w, int view_h, enDirection direction) {
	SRect rect = { 0, 0, view_w, view_h };
	float ratio = f_ratio(mode, pic_w, pic_h, view_w, view_h, direction);
	if (ratio < 1.0) {
		rect.point.y = view_h * (1 - ratio) / 2;
		rect.size.h = view_h * ratio;
	}
	else if (ratio > 1.0) {
		rect.point.x = view_w * (1 - (1 / ratio)) / 2;
		rect.size.w = view_w * (1 / ratio);
	}

	return rect;
}


void GLWin32::draw(enRenderMode mode, int pic_w, int pic_h,
	int view_w, int view_h, enDirection direction) {
	SRect r = fixSize(mode, pic_w, pic_h,
		view_w, view_h, direction);
	//LOGV(TAG, "fix, point:(%d, %d), size:%dx%d\n", r.point.x, r.point.y, r.size.w, r.size.h);
	draw(r);
}

void GLWin32::drawBlack(void)
{
	//setUniform(_opengl.program, opengl::U_TYPE, (int)opengl::enBLACK);
	SRect rect = { 0, 0, 1, 1 };
	draw(rect);
}

GLuint GLWin32::genTexture(int layer) {
	GLuint gl;
	GLenum texture_id = GL_TEXTURE0 + layer;
	glActiveTexture(texture_id);
	glGenTextures(1, &gl);
	glBindTexture(GL_TEXTURE_2D, gl);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1.0);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	for (GLenum error; (error = glGetError()) != GL_NO_ERROR;) {
		printf("genTexture:%d\n", error);
	}

	return gl;
}

void GLWin32::bindTexture(int layer, GLenum fmt, int width, int height,
	unsigned char * data) {
	GLenum texture_id = GL_TEXTURE0 + layer;
	GLenum type = GL_UNSIGNED_BYTE;

	glActiveTexture(texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, fmt, width, height, 0, fmt, type, data);
}

void GLWin32::bindYUV420(unsigned char *yuv, int width, int height) {
	unsigned char *y = yuv;
	unsigned char *u = yuv + width * height;
	unsigned char *v = yuv + width * height * 5 / 4;

	bindTexture(0, GL_LUMINANCE, width, height, y);
	bindTexture(1, GL_LUMINANCE, width / 2, height / 2, u);
	bindTexture(2, GL_LUMINANCE, width / 2, height / 2, v);
}

void GLWin32::bindYV12(unsigned char *yuv, int width, int height) {
	unsigned char *y = yuv;
	unsigned char *v = yuv + width * height;
	unsigned char *u = yuv + width * height * 5 / 4;

	bindTexture(0, GL_LUMINANCE, width, height, y);
	bindTexture(1, GL_LUMINANCE, width / 2, height / 2, u);
	bindTexture(2, GL_LUMINANCE, width / 2, height / 2, v);
}

void GLWin32::bindNV12(unsigned char *yuv, int width, int height) {
	unsigned char *y = yuv;
	unsigned char *uv = yuv + width * height;

	bindTexture(0, GL_LUMINANCE, width, height, y);
	bindTexture(1, GL_LUMINANCE_ALPHA, width / 2, height / 2, uv);
}

void GLWin32::bindNV21(unsigned char *yuv, int width, int height) {
	unsigned char *y = yuv;
	unsigned char *vu = yuv + width * height;

	bindTexture(0, GL_LUMINANCE, width, height, y);
	bindTexture(1, GL_LUMINANCE_ALPHA, width / 2, height / 2, vu);
}

void GLWin32::bindPicture(GLuint gl, enImageFormat fmt, uint8_t * pic, int w, int h)
{
	enShaderMode mode = setShaderMode(gl, fmt);
	switch (fmt) {
	case enImageYUV420P:
		bindYUV420(pic, w, h);
		break;
	case enImageNV12:
		bindNV12(pic, w, h);
		break;
	case enImageYV12:
		bindYV12(pic, w, h);
		break;
	case enImageNV21:
		bindNV21(pic, w, h);
		break;
	default:
		break;
	}
}

void GLWin32::bindPicture(GLuint gl, enImageFormat fmt,
	uint8_t * y, uint8_t * u, uint8_t * v, int w, int h)
{
	enShaderMode mode = setShaderMode(gl, fmt);
	switch (fmt) {
	case enImageYUV420P:
	case enImageYV12:
		bindTexture(0, GL_LUMINANCE, w, h, y);
		bindTexture(1, GL_LUMINANCE, w / 2, h / 2, u);
		bindTexture(2, GL_LUMINANCE, w / 2, h / 2, v);
		break;
	default:
		break;
	}
}

void GLWin32::bindBGRA(GLuint gl, uint8_t * img, int w, int h)
{
	setUniform(gl, U_TYPE, (int)enRGBA);
	bindTexture(0, GL_RGBA, w, h, img);
}

void GLWin32::bindPicture(GLuint gl, enImageFormat fmt,
	uint8_t * y, uint8_t * uv, int w, int h)
{
	enShaderMode mode = setShaderMode(gl, fmt);
	switch (fmt) {
	case enImageNV12:
	case enImageNV21:
		bindTexture(0, GL_LUMINANCE, w, h, y);
		bindTexture(1, GL_LUMINANCE_ALPHA, w / 2, h / 2, uv);
		break;
	default:
		break;
	}
}

GLuint GLWin32::genAndBindVAO(void) {
	const GLsizei stride = 2 * sizeof(GLfloat);
	GLuint vertex;
	glGenBuffers(1, &vertex);
	glBindBuffer(GL_ARRAY_BUFFER, vertex);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	GLuint position = 0;// glGetAttribLocation(ctx.program, "position");
	glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, stride, NULL);
	glEnableVertexAttribArray(position);
	for (GLenum error; (error = glGetError()) != GL_NO_ERROR;) {
		printf("vertex GLError:%d\n", error);
	}
	return vertex;
}

GLuint GLWin32::genAndBindVBO(void) {
	GLuint gl;

	glGenBuffers(1, &gl);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices),
		indices, GL_STATIC_DRAW);

	return gl;
}

GLuint GLWin32::genCoordinate(void) {
	const GLsizei stride = 2 * sizeof(GLfloat);
	GLuint gl;
	glGenBuffers(1, &gl);
	glBindBuffer(GL_ARRAY_BUFFER, gl);
	glBufferData(GL_ARRAY_BUFFER, sizeof(coordinate), coordinate, GL_DYNAMIC_DRAW);
	GLuint coordinate1 = 1;// glGetAttribLocation(ctx.program, "coordinate");
	glVertexAttribPointer(coordinate1, 2, GL_FLOAT, GL_FALSE, stride, NULL);
	glEnableVertexAttribArray(coordinate1);
	for (GLenum error; (error = glGetError()) != GL_NO_ERROR;) {
		printf("coordinate1 GLError:%d\n", error);
	}
	return gl;
}

void GLWin32::updateCoordinate(GLuint gl, enDirection dir) {
	GLfloat coord[8];
	for (int i = 0; i < 4; i++) {
		coord[i * 2] = coordinate[degree[dir][i] * 2];
		coord[i * 2 + 1] = coordinate[degree[dir][i] * 2 + 1];
	}

	glBindBuffer(GL_ARRAY_BUFFER, gl);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(coord), coord);

	for (GLenum error; (error = glGetError()) != GL_NO_ERROR;) {
		printf("updateCoordinate:%d\n", error);
	}
}

void GLWin32::printGLString(const char *name, GLenum s) {
	const char *v = (const char *)glGetString(s);
	printf("%s = %s\n", name, v);
}