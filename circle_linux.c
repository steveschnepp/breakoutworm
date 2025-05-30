#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#define CIRCLE_POINTS 64
#define PI 3.14159265f

static float pos_x = 0.0f;
static float vel_x = 0.01f;

Display *dpy;
Window win;
GLXContext ctx;

void draw_circle(float x, float y, float r, float width, float r_col, float g_col, float b_col, float alpha) {
    float r0 = r;
    float r1 = r + width;

    glColor4f(r_col, g_col, b_col, alpha);

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= CIRCLE_POINTS; i++) {
        float a = 2.0f * PI * i / CIRCLE_POINTS;
        float cx = cosf(a);
        float cy = sinf(a);

        glVertex2f(x + cx * r0, y + cy * r0);
        glVertex2f(x + cx * r1, y + cy * r1);
    }
    glEnd();
}

void draw_text(float x, float y, const char *text) {
    glColor4f(1, 1, 1, 1);
    glRasterPos2f(x, y);
    glPushAttrib(GL_LIST_BIT);
    glListBase(1000);
    glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);
    glPopAttrib();
}

int main(void) {
    dpy = XOpenDisplay(NULL);
    int screen = DefaultScreen(dpy);

    GLint attribs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 24, None };
    XVisualInfo *vi = glXChooseVisual(dpy, screen, attribs);
    Colormap cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);

    XSetWindowAttributes swa = {
        .colormap = cmap,
        .event_mask = ExposureMask | KeyPressMask | StructureNotifyMask
    };

    win = XCreateWindow(dpy, RootWindow(dpy, vi->screen),
        0, 0, 800, 600, 0, vi->depth, InputOutput,
        vi->visual, CWColormap | CWEventMask, &swa);

    XMapWindow(dpy, win);
    ctx = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, ctx);

    // enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // load font and make GL display lists
    XFontStruct *font = XLoadQueryFont(dpy, "fixed");
    if (!font) return 1;
    glXUseXFont(font->fid, 0, 256, 1000);

    while (1) {
        while (XPending(dpy)) {
            XEvent e;
            XNextEvent(dpy, &e);
            if (e.type == KeyPress) return 0;
        }

        pos_x += vel_x;
        if (pos_x > 0.9f || pos_x < -0.9f) vel_x = -vel_x;

        glViewport(0, 0, 800, 600);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-1, 1, -1, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        draw_circle(pos_x, 0.0f, 0.1f, 0.02f, 1, 0, 0, 0.5f);
        draw_text(-0.9f, 0.8f, "hello world");

        glXSwapBuffers(dpy, win);
        usleep(16000);
    }

    return 0;
}