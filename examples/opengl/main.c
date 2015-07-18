/**
 * Example program for libsixel-OpenGL integration
 *
 * Hayaki Saito <user@zuse.jp>
 *
 * I declared this program is in Public Domain (CC0 - "No Rights Reserved"),
 * This file is offered AS-IS, without any warranty.
 *
 */

#include "config.h"

#if HAVE_OSMESA
# define USE_OSMESA 1
#elif HAVE_CGL
# define USE_CGL 1
#elif HAVE_X11
# define USE_GLX 1
#else
#endif

#if USE_OSMESA
# include <GL/osmesa.h>
#elif USE_CGL
# include <OpenGL/gl.h>
# include <OpenGL/OpenGL.h>
#elif USE_GLX
# include <X11/Xlib.h>
# include <GL/glx.h>
# include <GL/gl.h>
#endif

#include <sys/signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <math.h>
#if HAVE_TERMIOS_H
# include <termios.h>
#endif
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif


#ifndef M_PI
# define M_PI 3.1415926535897932386
#endif

#include <sixel.h>  /* libsixel */

#if USE_OSMESA
static unsigned char *pbuffer;
static OSMesaContext context;
#elif USE_CGL
static CGLPBufferObj pbuffer;
static CGLContextObj context;
#elif USE_GLX && (defined(GLX_VERSION_1_3) || defined(GLX_VERSION_1_4))
static Display *display = NULL;
static GLXPbuffer pbuffer = 0;
static GLXContext context;
#endif
static volatile int signaled = 0;

static void sighandler(int sig)
{
    signaled = sig;
}

static int setup(int width, int height)
{
#if USE_OSMESA
    const size_t size = width * height * 4;
    pbuffer = malloc(size);
    memset(pbuffer, 0x21, size);
    context = OSMesaCreateContextExt(GL_RGBA, 24, 0, 0, 0);
    OSMesaMakeCurrent(context, (void *)pbuffer, GL_UNSIGNED_BYTE, width, height);
    return 0;
#elif USE_CGL
    /* OpenGL PBuffer initialization: OSX specific */
    CGLPixelFormatAttribute pfattr[] = {
        kCGLPFAPBuffer,
        (CGLPixelFormatAttribute)0
    };
    CGLPixelFormatObj pixformat;
    GLint npixels;
    int e;

    e = CGLChoosePixelFormat(pfattr, &pixformat, &npixels);
    if (e != kCGLNoError) {
       fprintf(stderr, "CGLChoosePixelFormat failed, err %d\n", e);
       return e;
    }
    e = CGLCreateContext(pixformat, 0, &context);
    if (e != kCGLNoError) {
       fprintf(stderr, "CGLChoosePixelFormat failed, err %d\n", e);
       return e;
    }
    e = CGLDestroyPixelFormat(pixformat);
    if (e != kCGLNoError) {
       fprintf(stderr, "CGLDestroyPixelFormat failed, err %d\n", e);
       return e;
    }
    e = CGLSetCurrentContext(context);
    if (e != kCGLNoError) {
       fprintf(stderr, "CGLSetCurrentContext failed, err %d\n", e);
       return e;
    }
    e = CGLCreatePBuffer(width, height, GL_TEXTURE_2D, GL_RGB, 0, &pbuffer);
    if (e != kCGLNoError) {
       fprintf(stderr, "CGLCreatePBuffer failed, err %d\n", e);
       return e;
    }
    e = CGLSetPBuffer(context, pbuffer, 0, 0, 0);
    if (e != kCGLNoError) {
       fprintf(stderr, "CGLSetPBuffer failed, err %d\n", e);
       return e;
    }
    return kCGLNoError;
#elif USE_GLX
    /* Open the X display */
    display = XOpenDisplay(NULL);
    if (!display) {
       printf("Error: couldn't open default X display.\n");
       return (-1);
    }

    /* Get default screen */
    int screen = DefaultScreen(display);

    char *glxversion;
 
    glxversion = (char *) glXGetClientString(display, GLX_VERSION);
    if (!(strstr(glxversion, "1.3") || strstr(glxversion, "1.4"))) {
       XCloseDisplay(display);
       return (-1);
    }

    glxversion = (char *) glXQueryServerString(display, screen, GLX_VERSION);
    if (!(strstr(glxversion, "1.3") || strstr(glxversion, "1.4"))) {
       XCloseDisplay(display);
       return (-1);
    }

    /* Create Pbuffer */
    GLXFBConfig *fbConfigs;
    GLXFBConfig chosenFBConfig;
    GLXFBConfig fbconfig = 0;
    GLXPbuffer pbuffer = None;

    int nConfigs;
    int fbconfigid;

    int fbAttribs[] = {
       GLX_RENDER_TYPE, GLX_RGBA_BIT,
       GLX_DEPTH_SIZE, 1,
       GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT | GLX_PBUFFER_BIT,
       None
    };

    int pbAttribs[] = {
       GLX_PBUFFER_WIDTH, 0,
       GLX_PBUFFER_HEIGHT, 0,
       GLX_LARGEST_PBUFFER, False,
       GLX_PRESERVED_CONTENTS, False,
       None
    };

    pbAttribs[1] = width;
    pbAttribs[3] = height;

    fbConfigs = glXChooseFBConfig(display, screen, fbAttribs, &nConfigs);

    if (0 == nConfigs || !fbConfigs) {
       printf("Error: glxChooseFBConfig failed\n");
       XFree(fbConfigs);
       XCloseDisplay(display);
       return (-1);
    }

    chosenFBConfig = fbConfigs[0];

    glXGetFBConfigAttrib(display, chosenFBConfig, GLX_FBCONFIG_ID, &fbconfigid);
    printf("Chose 0x%x as fbconfigid\n", fbconfigid);

    /* Create the pbuffer using first fbConfig in the list that works. */
    pbuffer = glXCreatePbuffer(display, chosenFBConfig, pbAttribs);

    if (pbuffer) {
       fbconfig = chosenFBConfig;
    }

    XFree(fbConfigs);

    if (pbuffer == None) {
       printf("Error: couldn't create pbuffer\n");
       XCloseDisplay(display);
       return (-1);
    }

    /* Create GLX context */
    context = glXCreateNewContext(display, fbconfig, GLX_RGBA_TYPE, NULL, True);
    if (context) {
       if (!glXIsDirect(display, context)) {
          printf("Warning: using indirect GLXContext\n");
       }
    }
    else {
       printf("Error: Couldn't create GLXContext\n");
       XCloseDisplay(display);
       return (-1);
    }

    /* Bind context to pbuffer */
    if (!glXMakeCurrent(display, pbuffer, context)) {
       printf("Error: glXMakeCurrent failed\n");
       XCloseDisplay(display);
       return (-1);
    }
    return 0;
#else
    /* TODO: pbuffer initialization */
    return 0;
#endif
}

static int
cleanup(void)
{
#if USE_OSMESA
    OSMesaDestroyContext(context);
    free(pbuffer);
#elif USE_CGL
    (void)CGLDestroyContext(context);
    (void)CGLDestroyPBuffer(pbuffer);
#elif USE_GLX
    display = XOpenDisplay(NULL);
    glXDestroyPbuffer(display, pbuffer);
    XCloseDisplay(display);
#else
    /* TODO: cleanup pbuffer and OpenGL context */
#endif
    return 0;
}


static int
draw_scene(void)
{
    static GLfloat rot1, rot2;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushMatrix();
    glTranslatef(-1.5, 0, -6);
    glRotatef(rot1 += 1.5, 0, 1, 0);
    glBegin(GL_TRIANGLES);
      glColor3f(1,  0,  0); glVertex3f(0,  1,  0);
      glColor3f(0,  1,  0); glVertex3f(-1, -1,  1);
      glColor3f(0,  0,  1); glVertex3f(1, -1,  1);

      glColor3f(1,  0,  0); glVertex3f(0,  1,  0);
      glColor3f(0,  0,  1); glVertex3f(1, -1,  1);
      glColor3f(0,  1,  0); glVertex3f(1, -1, -1);

      glColor3f(1,  0,  0); glVertex3f(0,  1,  0);
      glColor3f(0,  1,  0); glVertex3f(1, -1, -1);
      glColor3f(0,  0,  1); glVertex3f(-1, -1, -1);

      glColor3f(1,  0,  0); glVertex3f(0,  1,  0);
      glColor3f(0,  0,  1); glVertex3f(-1, -1, -1);
      glColor3f(0,  1,  0); glVertex3f(-1, -1,  1);
    glEnd();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(1.5, 0, -6);
    glRotatef(rot2 += 1.8, 1, 0, 0);
    glBegin(GL_QUADS);
      glColor3f(0, 0, 0); glVertex3f(-1, -1, -1);
      glColor3f(0, 0, 1); glVertex3f(-1, -1,  1);
      glColor3f(0, 1, 1); glVertex3f(-1,  1,  1);
      glColor3f(0, 1, 0); glVertex3f(-1,  1, -1);

      glColor3f(1, 0, 0); glVertex3f( 1, -1, -1);
      glColor3f(1, 0, 1); glVertex3f( 1, -1,  1);
      glColor3f(1, 1, 1); glVertex3f( 1,  1,  1);
      glColor3f(1, 1, 0); glVertex3f( 1,  1, -1);

      glColor3f(0, 0, 0); glVertex3f(-1, -1, -1);
      glColor3f(0, 0, 1); glVertex3f(-1, -1,  1);
      glColor3f(1, 0, 1); glVertex3f( 1, -1,  1);
      glColor3f(1, 0, 0); glVertex3f( 1, -1, -1);

      glColor3f(0, 1, 0); glVertex3f(-1,  1, -1);
      glColor3f(0, 1, 1); glVertex3f(-1,  1,  1);
      glColor3f(1, 1, 1); glVertex3f( 1,  1,  1);
      glColor3f(1, 1, 0); glVertex3f( 1,  1, -1);

      glColor3f(0, 0, 0); glVertex3f(-1, -1, -1);
      glColor3f(0, 1, 0); glVertex3f(-1,  1, -1);
      glColor3f(1, 1, 0); glVertex3f( 1,  1, -1);
      glColor3f(1, 0, 0); glVertex3f( 1, -1, -1);

      glColor3f(0, 0, 1); glVertex3f(-1, -1,  1);
      glColor3f(0, 1, 1); glVertex3f(-1,  1,  1);
      glColor3f(1, 1, 1); glVertex3f( 1,  1,  1);
      glColor3f(1, 0, 1); glVertex3f( 1, -1,  1);
    glEnd();
    glPopMatrix();

    return 0;
}


static int
sixel_write(char *data, int size, void *priv)
{
    return fwrite(data, 1, size, (FILE *)priv);
}


static SIXELSTATUS
output_sixel(unsigned char *pixbuf, int width, int height,
             int ncolors, int pixelformat)
{
    sixel_output_t *context;
    sixel_dither_t *dither;
    SIXELSTATUS status;

#if USE_OSMESA
    pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
#endif
    context = sixel_output_create(sixel_write, stdout);
    dither = sixel_dither_create(ncolors);
    status = sixel_dither_initialize(dither, pixbuf,
                                     width, height,
                                     pixelformat,
                                     SIXEL_LARGE_AUTO,
                                     SIXEL_REP_AUTO,
                                     SIXEL_QUALITY_AUTO);
    if (SIXEL_FAILED(status))
        return status;
    status = sixel_encode(pixbuf, width, height,
                          pixelformat, dither, context);
    if (SIXEL_FAILED(status))
        return status;
    sixel_output_unref(context);
    sixel_dither_unref(dither);

    return status;
}

static int
wait_stdin(int usec)
{
#if HAVE_SYS_SELECT_H
    fd_set rfds;
    struct timeval tv;
#endif  /* HAVE_SYS_SELECT_H */
    int ret = 0;

#if HAVE_SYS_SELECT_H
    tv.tv_sec = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    ret = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
#else
    (void) usec;
#endif  /* HAVE_SYS_SELECT_H */

    return ret;
}

static void
scroll_on_demand(int pixelheight)
{
#if HAVE_SYS_IOCTL_H
    struct winsize size = {0, 0, 0, 0};
#endif
#if HAVE_TERMIOS_H
    struct termios old_termios;
    struct termios new_termios;
#endif
    int row = 0;
    int col = 0;
    int cellheight;
    int scroll;

#if HAVE_SYS_IOCTL_H
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    if (size.ws_ypixel <= 0) {
        printf("\033[H\0337");
        return;
    }
# if HAVE_TERMIOS_H
    /* set the terminal to cbreak mode */
    tcgetattr(STDIN_FILENO, &old_termios);
    memcpy(&new_termios, &old_termios, sizeof(old_termios));
    new_termios.c_lflag &= ~(ECHO | ICANON);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);

    /* request cursor position report */
    printf("\033[6n");
    if (wait_stdin(1000 * 1000) != (-1)) { /* wait 1 sec */
        if (scanf("\033[%d;%dR", &row, &col) == 2) {
            cellheight = pixelheight * size.ws_row / size.ws_ypixel + 1;
            scroll = cellheight + row - size.ws_row;
            printf("\033[%dS\033[%dA", scroll, scroll);
            printf("\0337");
        } else {
            printf("\033[H\0337");
        }
    }

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_termios);
# else
    printf("\033[H\0337");
# endif  /* HAVE_TERMIOS_H */
#else
    printf("\033[H\0337");
#endif  /* HAVE_SYS_IOCTL_H */
}

int main(int argc, char** argv)
{
    SIXELSTATUS status;
    unsigned char *pixbuf;

    int width = 400;
    int height = 300;
    int ncolors = 16;

    (void) argc;
    (void) argv;

    if (signal(SIGINT, sighandler) == SIG_ERR)
       return (-1);
    if (setup(width, height) != 0)
       return (-1);

    scroll_on_demand(height);

    glShadeModel(GL_SMOOTH);
    glClearColor(0, 0, 0, 0);
    glClearDepth(1);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    GLfloat fovy = 45;
    GLfloat aspect = (GLfloat)width / (GLfloat)height;
    GLfloat znear = 0.1;
    GLfloat zfar = 100;
    GLfloat radian= 2 * M_PI * fovy / 360.0;
    GLfloat t = (GLfloat)(1.0 / tan(radian / 2));
    GLfloat matrix[]={
        t / aspect, 0, 0, 0,
        0, t, 0, 0,
        0, 0, (zfar + znear) / (znear - zfar), -1,
        0, 0, (2 * zfar * znear) / (znear - zfar), 0
    };
    glLoadMatrixf(matrix);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

#if !defined(USE_OSMESA)
    pixbuf = malloc(width * height * 3);
#endif

    while (!signaled) {
        glPushMatrix();
        glScalef(1, -1, 1);
        draw_scene();
        glPopMatrix();

        if (signaled)
            break;

        printf("\0338");
#if USE_OSMESA
        pixbuf = pbuffer;
#else
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixbuf);
#endif
        status = output_sixel(pixbuf, width, height, ncolors,
                              SIXEL_PIXELFORMAT_RGB888);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr, "%s\n%s\n",
                    sixel_helper_format_error(status),
                    sixel_helper_get_additional_message());
            break;
        }
    }

#if !defined(USE_OSMESA)
    free(pixbuf);
#endif

    printf("\e\\");

    if (cleanup() != 0)
       return (-1);
    return 0;
}

/* EOF */
