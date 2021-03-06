
// EGLDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "EGL.h"
#include "EGLDlg.h"
#include "afxdialogex.h"

#include "MEGL.h"
#include "MOpenGL.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

const int pic_w = 320; 
const int pic_h = 180;
int view_w = 0, view_h = 0;

void PlacePicture(place_t * place)
{
	unsigned display_width;
	unsigned display_height;

	display_width = view_w;
	display_height = view_h;

	const unsigned width = pic_w;
	const unsigned height = pic_h;

	/* Compute the height if we use the width to fill up display_width */
	const int scaled_height = (int)height * display_width / width;;
	/* And the same but switching width/height */
	const int scaled_width = (int)width  * display_height / height;

	/* We keep the solution that avoid filling outside the display */
	if (scaled_width <= view_w) {
		place->width = scaled_width;
		place->height = display_height;
	}
	else {
		place->width = display_width;
		place->height = scaled_height;
	}

	place->x = ((int)view_w - (int)place->width) / 2;
	place->y = ((int)view_h - (int)place->height) / 2;
}

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CEGLDlg 对话框



CEGLDlg::CEGLDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_EGL_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	AllocConsole();
	freopen("CONOUT$", "w", stdout);

	InitializeCriticalSection(&cs);

	memset(&_vout, 0x00, sizeof(_vout));
	_vout.b_first_display = true;
}

void CEGLDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_OPENGL, m_idcView);
}

BEGIN_MESSAGE_MAP(CEGLDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDOK, &CEGLDlg::OnBnClickedOk)
	ON_WM_SIZE()
	ON_WM_CTLCOLOR()
END_MESSAGE_MAP()


// CEGLDlg 消息处理程序

BOOL CEGLDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CEGLDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CEGLDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CEGLDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

unsigned __stdcall EGLThreadFunc(void * arg)
{
	CEGLDlg * pArg = static_cast<CEGLDlg *>(arg);
	pArg->EGLThread();

	return 0;
}

unsigned __stdcall EventThreadFunc(void * arg)
{
	CEGLDlg * pArg = static_cast<CEGLDlg *>(arg);
	pArg->EventThread();

	return 0;
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

static long FAR PASCAL WinVoutEventProc(HWND hwnd, UINT message,
	WPARAM wParam, LPARAM lParam)
{
	if (message == WM_CREATE) {
		printf("WM_CREATE hwnd:%p\n", hwnd);
		CEGLDlg * pthis = (CEGLDlg *)((CREATESTRUCT *)lParam)->lpCreateParams;
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pthis);
		return TRUE;
	}

	LONG_PTR p_user_data = GetWindowLongPtr(hwnd, GWLP_USERDATA);
	CEGLDlg * pthis = (CEGLDlg *)p_user_data;
	if (!pthis) {
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	if (pthis->OnWindowProc(hwnd, message, wParam, lParam)) {
		return TRUE;
	}

	/* Let windows handle the message */
	return DefWindowProc(hwnd, message, wParam, lParam);
}

BOOL CEGLDlg::OnWindowProc(HWND hwnd, UINT message,
	WPARAM wParam, LPARAM lParam)
{
	if (hwnd == _vout.hvideownd) {
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
			::ValidateRect(hwnd, NULL);
			// fall through
		default:
			return FALSE;
		}
	}

	switch (message) {
	case WM_WINDOWPOSCHANGED:
		printf("WM_WINDOWPOSCHANGED\n");
		return TRUE;
		/* the user wants to close the window */
	case WM_CLOSE:
		printf("WM_CLOSE\n");
		return TRUE;
		/* the window has been closed so shut down everything now */
	case WM_DESTROY:
		printf("WM_DESTROY\n");
		/* just destroy the window */
		PostQuitMessage(0);
		return TRUE;
	}
}

void CEGLDlg::EventThread(void)
{
	HINSTANCE  hInstance = GetModuleHandle(NULL);
	WNDCLASS wc;
	LPCWSTR class_main = _T("SDK VIDEO MAIN");
	LPCWSTR class_video = _T("SDK VIDEO OUTPUT");
	HCURSOR cursor_arrow = LoadCursor(NULL, IDC_ARROW);
	HCURSOR cursor_empty = EmptyCursor(hInstance);
	HICON vlc_icon = NULL;
	TCHAR vlc_path[MAX_PATH + 1];
	if (GetModuleFileName(NULL, vlc_path, MAX_PATH)) {
		vlc_icon = ExtractIcon(hInstance, vlc_path, 0);
	}
	wc.style = CS_OWNDC | CS_DBLCLKS;
	/* Fill in the window class structure */
	wc.style = CS_OWNDC | CS_DBLCLKS;          /* style: dbl click */
	wc.lpfnWndProc = (WNDPROC)WinVoutEventProc;       /* event handler */
	wc.cbClsExtra = 0;                         /* no extra class data */
	wc.cbWndExtra = 0;                        /* no extra window data */
	wc.hInstance = hInstance;				/* instance */
	wc.hIcon = vlc_icon;					/* load the vlc big icon */
	wc.hCursor = cursor_arrow;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);  /* background color */
	wc.lpszMenuName = NULL;                                  /* no menu */
	wc.lpszClassName = class_main;       /* use a special class */

	if (!RegisterClass(&wc)) {
		printf("register class main error\n");
		return;
	}

	/* Register the video sub-window class */
	wc.lpszClassName = class_video;
	wc.hIcon = 0;
	wc.hbrBackground = NULL; /* no background color */
	if (!RegisterClass(&wc)) {
		printf("register class video error\n");
		return;
	}

	RECT rect_window;
	rect_window.left = 10;
	rect_window.top = 10;
	rect_window.right = rect_window.left + pic_w;
	rect_window.bottom = rect_window.top + pic_h;

	int i_style = WS_OVERLAPPEDWINDOW | WS_SIZEBOX | WS_VISIBLE | WS_CLIPCHILDREN;
	AdjustWindowRect(&rect_window, WS_OVERLAPPEDWINDOW | WS_SIZEBOX, 0);

	if (_vout.hparent != NULL) {
		i_style = WS_VISIBLE | WS_CLIPCHILDREN | WS_CHILD;
	}
	_vout.i_window_style = i_style;

	/* Create the window */
	_vout.hwnd = CreateWindowEx(
			WS_EX_NOPARENTNOTIFY,
			class_main,             /* name of window class */
			_T(" (Video Output)"),/* window title */
			i_style,                                 /* window style */
			CW_USEDEFAULT,	/* default X coordinate */
			CW_USEDEFAULT,	/* default Y coordinate */
			rect_window.right - rect_window.left,    /* window width */
			rect_window.bottom - rect_window.top,   /* window height */
			_vout.hparent,                       /* parent window */
			NULL,                          /* no menu in this window */
			hInstance,            /* handle of this program instance */
			(LPVOID)this);           /* send vd to WM_CREATE */

	if (_vout.hwnd == NULL) {
		printf("Create video output window failed:%lu\n", GetLastError());
		return;
	}

	if (_vout.hparent)
	{
		LONG i_style;

		/* We don't want the window owner to overwrite our client area */
		i_style = GetWindowLong(_vout.hparent, GWL_STYLE);

		if (!(i_style & WS_CLIPCHILDREN))
			/* Hmmm, apparently this is a blocking call... */
			SetWindowLong(_vout.hparent, GWL_STYLE,
				i_style | WS_CLIPCHILDREN);

		/* Create our fullscreen window */
		_vout.hfswnd =
			CreateWindowEx(WS_EX_APPWINDOW, 
				class_main,
				_T(" (VLC Fullscreen Video Output)"),
				WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_SIZEBOX,
				CW_USEDEFAULT, CW_USEDEFAULT,
				CW_USEDEFAULT, CW_USEDEFAULT,
				NULL, NULL, hInstance, NULL);
	}
	else
	{
		_vout.hfswnd = NULL;
	}

	HMENU hMenu = (HMENU)::GetSystemMenu(_vout.hwnd, FALSE);
	AppendMenu(hMenu, MF_SEPARATOR, 0, _T(""));
	AppendMenu(hMenu, MF_STRING | MF_UNCHECKED,
		WM_USER + 1, _T("Always on &Top"));

	/* Create video sub-window. This sub window will always exactly match
	 * the size of the video, which allows us to use crazy overlay colorkeys
	 * without having them shown outside of the video area. */
	 /* FIXME vd->source.i_width/i_height seems wrong */
	_vout.hvideownd = CreateWindow(
			class_video,
			_T(""),   /* window class */
			WS_CHILD,                   /* window style, not visible initially */
			0, 0,
		pic_w,//rect_window.right - rect_window.left,    /* window width */
		pic_h,//rect_window.bottom - rect_window.top,   /* window height */
			_vout.hwnd,               /* parent window */
			NULL, hInstance,
			(LPVOID)this);    /* send vd to WM_CREATE */

	if (!_vout.hvideownd)
		printf("can't create video sub-window\n");
	else
		printf("created video sub-window\n");

	/* Now display the window */
	::ShowWindow(_vout.hwnd, SW_SHOW);

	EnterCriticalSection(&cs);
	_vout.b_ready = true;
	LeaveCriticalSection(&cs);

	MSG msg;
	while (true) {
		if (!GetMessage(&msg, 0, 0, 0)) {
			printf("NONE Message, break\n");
			break;
		}

		switch (msg.message) {
		default:
			/* Messages we don't handle directly are dispatched to the
			 * window procedure */
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			break;
		}
	}
}

const egl::api_t GL_API = {
			"OpenGL_ES",
			EGL_OPENGL_ES_API,
			3,
			EGL_OPENGL_ES2_BIT,
			{
					EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE,
			}
};

const EGLint GL_ATTR[] = {
		EGL_BUFFER_SIZE, 32,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_RENDERABLE_TYPE, GL_API.render_bit,
		EGL_NONE,
};

bool CEGLDlg::_InitEGL(HWND hwnd)
{
	EGLint major, minor, cfgc;
	const char * ext = NULL;
	EGLContext context = EGL_NO_CONTEXT;
	EGLSurface surface = EGL_NO_SURFACE;
	EGLDisplay display = egl::getDisplay(EGL_DEFAULT_DISPLAY);
	EGLNativeWindowType * native = &hwnd;
	EGLConfig cfgv[1];

	if (display == EGL_NO_DISPLAY) {
		// Unable to open connection to local windowing system
		printf("eglGetDisplay failed\n");
		goto error;
	}

	if (egl::initialize(display, &major, &minor) != EGL_TRUE) {
		// Unable to initialize EGL. Handle and recover
		printf("eglGetDisplay Unable to initialize EGL\n");
		goto error;
	}
	printf("egl version:%d:%d\n", major, minor);
	printf("egl version:%s:%s\n",
		egl::queryString(display, EGL_VERSION),
		egl::queryString(display, EGL_VENDOR));

	ext = egl::queryString(display, EGL_EXTENSIONS);
	if (ext != NULL) {
		printf("extension:%s\n", ext);
	}

	if (major != 1 || minor < GL_API.min_minor ||
		!egl::checkAPI(display, GL_API.name)) {
		printf("cannot select %s API\n", GL_API.name);
		goto error;
	}

	if (egl::chooseConfig(display, GL_ATTR, cfgv, 1, &cfgc) != EGL_TRUE) {
		// Something didn't work … handle error situation
		printf("cannot choose EGL configuration\n");
		goto error;
	}

	if (egl::bindAPI(GL_API.api) != EGL_TRUE) {
		printf("cannot bind EGL api\n");
		goto error;
	}

	surface = egl::createWindowSurface(display, cfgv[0], *native, NULL);
	if (surface == EGL_NO_SURFACE) {
		printf("cannot create EGL window surface\n");
		goto error;
	}

	context = egl::createContext(display, cfgv[0], EGL_NO_CONTEXT,
		GL_API.attr);
	if (context == EGL_NO_CONTEXT) {
		printf("cannot create EGL context\n");
		goto error;
	}

	printf("initialize EGL success, dpy:%p, ctx:%p, sf:%p\n",
		display, context, surface);
	_egl = { display, surface, context };
	return true;
error:
	printf("init EGL failed\n");
	egl::destroy(display, surface, context);
	return false;
}

bool CEGLDlg::_InitOpenGL(enDirection dir)
{
	if (egl::makeCurrent(_egl.dpy, _egl.sf, _egl.ctx) != EGL_TRUE) {
		printf("init OpenGL error due to makeCurrent\n");
		return false;
	}

	opengl::printGLString("Version", GL_VERSION);
	opengl::printGLString("Vendor", GL_VENDOR);
	opengl::printGLString("Renderer", GL_RENDERER);
	opengl::printGLString("Extensions", GL_EXTENSIONS);

	_opengl.program = opengl::buildProgram();
	opengl::linkProgram(_opengl.program);
	opengl::useProgram(_opengl.program);
	for (int i = 0; i < 3; i++) {
		_opengl.texture[i] = opengl::genTexture(i);
	}

	_opengl.vertex = opengl::genAndBindVAO();
	_opengl.ebo = opengl::genAndBindVBO();
	_opengl.coordinate = opengl::genCoordinate();

	opengl::setUniform(_opengl.program, opengl::U_PLANE_Y, 0);
	opengl::setUniform(_opengl.program, opengl::U_PLANE_U, 1);
	opengl::setUniform(_opengl.program, opengl::U_PLANE_V, 2);

	opengl::updateCoordinate(_opengl.coordinate, dir);

	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	egl::releaseCurrent(_egl.dpy);

	printf("program:%d, vertex:%d\n", _opengl.program, _opengl.vertex);
	return true;
}

#include <io.h>
#include <fcntl.h>
#include <stdlib.h>

#pragma warning(disable:4996)

void CEGLDlg::EGLThread(void)
{
	//LONG style = ::GetWindowLong(m_idc_2.GetSafeHwnd(), GWL_STYLE);
	//::SetWindowLong(m_idc_2.GetSafeHwnd(), GWL_STYLE, style | WS_CLIPCHILDREN);

	int fd = open("pic1.yuv", O_RDONLY | O_BINARY);
	unsigned char * pic = new unsigned char[pic_w * pic_h * 3];
	int npic = read(fd, pic, pic_w * pic_h * 3);
	close(fd);

	bool b_ready = false;
	while (!b_ready) {
		Sleep(10);
		EnterCriticalSection(&cs);
		b_ready = _vout.b_ready;
		LeaveCriticalSection(&cs);
	}

	printf("vout ready, init EGL/OpenGL\n");
	_InitEGL(_vout.hwnd);

	_InitOpenGL(UP);

	RECT rect_window;
	::GetWindowRect(_vout.hwnd, &rect_window);
	while (true) {

		//)
		egl::makeCurrent(_egl.dpy, _egl.sf, _egl.ctx);

		opengl::bindPicture(_opengl.program, enImageYUV420P, pic, pic_w, pic_h);
		opengl::draw(enRenderAuto, pic_w, pic_h, rect_window.right,
			rect_window.bottom, UP);

		if (_vout.b_first_display) {
			_vout.b_first_display = false;

			::SetWindowPos(_vout.hvideownd, 0, 0, 0, 0, 0,
				SWP_ASYNCWINDOWPOS |
				SWP_FRAMECHANGED |
				SWP_SHOWWINDOW |
				SWP_NOMOVE |
				SWP_NOSIZE |
				SWP_NOZORDER);
		}

		if (_vout.hparent) {
			RECT rect_parent;
			POINT point;

			::GetClientRect(_vout.hparent, &rect_parent);
			point.x = point.y = 0;
			::ClientToScreen(_vout.hparent, &point);
			::OffsetRect(&rect_parent, point.x, point.y);
/*
			printf("old:%d, %d, %d, %d\n", 
				_vout.rect_parent.left, _vout.rect_parent.top, 
				_vout.rect_parent.right, _vout.rect_parent.bottom);
			printf("new:%d, %d, %d, %d\n",
				rect_parent.left, rect_parent.top, rect_parent.right, rect_parent.bottom);*/
			if (!EqualRect(&rect_parent, &_vout.rect_parent)) {
				_vout.rect_parent = rect_parent;

				/* This code deals with both resize and move
				 *
				 * For most drivers(direct3d9, gdi, opengl), move is never
				 * an issue. The surface automatically gets moved together
				 * with the associated window (hvideownd)
				 *
				 * For directdraw, it is still important to call UpdateRects
				 * on a move of the parent window, even if no resize occurred
				 */
				::SetWindowPos(_vout.hwnd, 0, 0, 0,
					rect_parent.right - rect_parent.left,
					rect_parent.bottom - rect_parent.top,
					SWP_NOZORDER);

				{
					//UpdateRects();
					RECT rect;
					POINT point;

					point.x = point.y = 0;
					::GetClientRect(_vout.hwnd, &rect);
					::ClientToScreen(_vout.hwnd, &point);

					bool has_moved, is_resized;

					UpdateWindowPosition(&has_moved, &is_resized,
						point.x, point.y,
						rect.right, rect.bottom);

					printf("moved:%d, resize:%d\n", has_moved, is_resized);
					if (is_resized) {
						view_w = rect.right;
						view_h = rect.bottom;
					}

					place_t place;
					PlacePicture(&place);
					_vout.place = place;

					if (_vout.hvideownd)
						::SetWindowPos(_vout.hvideownd, 0,
							place.x, place.y, place.width, place.height,
							SWP_NOCOPYBITS | SWP_NOZORDER | SWP_ASYNCWINDOWPOS);
					//continue;
					glViewport(0, 0, view_w, view_h);
				}
			}

			if (egl::swapBuffers(_egl.dpy, _egl.sf) != EGL_TRUE) {
				printf("swapBuffers error\n");
			}

			egl::releaseCurrent(_egl.dpy);

		}

		Sleep(10);
	}
}



void CEGLDlg::UpdateWindowPosition(
	bool *pb_moved, bool *pb_resized,
	int x, int y, int w, int h)
{
	*pb_moved = x != _vout.x || y != _vout.y;
	*pb_resized = w != _vout.w || h != _vout.h;

	_vout.x = x;
	_vout.y = y;
	_vout.w = w;
	_vout.h = h;

	printf("Posi(%d, %d), size:%dx%d\n", x, y, w, h);
}

void CEGLDlg::OnBnClickedOk()
{
	// TODO: 在此添加控件通知处理程序代码
	unsigned id = 0;

	_vout.hparent = m_idcView.GetSafeHwnd();

	HANDLE thread1 = (HANDLE)_beginthreadex(NULL,
		0/*1024 * 1024*/, &EGLThreadFunc, (void *)this, 0, &id);

	HANDLE thread2 = (HANDLE)_beginthreadex(NULL,
		0/*1024 * 1024*/, &EventThreadFunc, (void *)this, 0, &id);

	//CDialogEx::OnOK();
}


void CEGLDlg::OnSize(UINT nType, int cx, int cy)
{
	//printf("OnSize:%dx%d\n", cx, cy);
	CDialogEx::OnSize(nType, cx, cy);
}


HBRUSH CEGLDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

	if (pWnd->GetDlgCtrlID() == IDC_OPENGL) {
		return (HBRUSH)GetStockObject(BLACK_BRUSH);
	}

	return hbr;
}
