
// EGLDlg.h: 头文件
//

#pragma once

#include "MEGL.h"
#include "MOpenGL.h"

typedef struct {
	int x;
	int y;
	unsigned width;
	unsigned height;
} place_t;

// CEGLDlg 对话框
class CEGLDlg : public CDialogEx
{
	// 构造
public:
	CEGLDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_EGL_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


public:
	void EventThread(void);

	void EGLThread(void);

	BOOL OnWindowProc(HWND hwnd, UINT message,
		WPARAM wParam, LPARAM lParam);

private:
	bool _InitEGL(HWND hwnd);

	bool _InitOpenGL(enDirection dir);

	struct {
		EGLDisplay dpy;
		EGLSurface sf;
		EGLContext ctx;
	} _egl;

	struct {
		GLuint program;
		GLuint texture[3];
		GLuint vertex;
		GLuint coordinate;
		GLuint ebo;
	} _opengl;

	CRITICAL_SECTION cs;

	struct {
		HWND hparent;	//set in
		int i_window_style;

		HWND hwnd;
		HWND hfswnd;
		HWND hvideownd;

		bool b_ready;
		bool b_first_display;

		RECT rect_parent;
		RECT rect_window;

		int x, y, w, h;

		place_t place;

		HDC hdc;
	} _vout;

	void UpdateWindowPosition(
		bool *pb_moved, bool *pb_resized,
		int x, int y, int w, int h);
// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
	CStatic m_idcView;
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
