#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <math.h>

#define CIRCLE_POINTS 64
#define PI 3.14159265f

static float pos_x = 0.0f;
static float vel_x = 0.01f;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CLOSE || msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void draw_circle(float x, float y, float r) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int i = 0; i <= CIRCLE_POINTS; i++) {
        float a = 2.0f * PI * i / CIRCLE_POINTS;
        glVertex2f(x + cosf(a) * r, y + sinf(a) * r);
    }
    glEnd();
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    WNDCLASS wc = {
        .style = CS_OWNDC,
        .lpfnWndProc = WndProc,
        .hInstance = hInst,
        .lpszClassName = "CircleWindow",
    };

    RegisterClass(&wc);

    HWND hWnd = CreateWindowEx(0, wc.lpszClassName, "Moving Circle",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600, NULL, NULL, hInst, NULL);

    HDC hDC = GetDC(hWnd);

    PIXELFORMATDESCRIPTOR pfd = {
        .nSize = sizeof(pfd),
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 32,
        .cDepthBits = 24,
        .iLayerType = PFD_MAIN_PLANE,
    };

    int pf = ChoosePixelFormat(hDC, &pfd);
    SetPixelFormat(hDC, pf, &pfd);
    HGLRC rc = wglCreateContext(hDC);
    wglMakeCurrent(hDC, rc);

    ShowWindow(hWnd, SW_SHOW);

    MSG msg;
    while (1) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                goto exit;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        pos_x += vel_x;
        if (pos_x > 0.9f || pos_x < -0.9f) vel_x = -vel_x;

        glClearColor(0.1f, 0.1f, 0.1f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        glLoadIdentity();
        draw_circle(pos_x, 0.0f, 0.1f);

        SwapBuffers(hDC);
        Sleep(16);
    }

exit:
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc);
    ReleaseDC(hWnd, hDC);
    DestroyWindow(hWnd);
    return 0;
}
