#include <math.h> // sqrt
#include <stdio.h> // snprintf
#include <stdlib.h> // exit
#include <string.h> // memcpy

#include <GLUT/glut.h>
#include <OpenGL/gl.h>

#include "window.h"

static void pick_gamemode_resolution(int const window_wh[2],
                                     int const screen_wh[2])
{
        char buffer[64];
        int resolutions[][2] = {
                { window_wh[0], window_wh[1]},
                { screen_wh[0], screen_wh[1]},
                { 1920, 1200 },
                { 1920, 1080 },
                { 1680, 1050 },
                { 1600, 1200 },
                { 1440, 900 },
                { 1400, 1050 },
                { 1368, 768 },
                { 1280, 1024 },
                { 1280, 800 },
                { 800, 600 },
                { 640, 480 },
        };
        int resolutions_n = sizeof resolutions_n / sizeof resolutions[0];
        int i;

        for (i = 0; i < resolutions_n; i++) {
                snprintf (buffer, sizeof(buffer) - 1, "%dx%d:%d", resolutions[i][0],
                          resolutions[i][1], 32);
                glutGameModeString(buffer);
                if (glutGameModeGet (GLUT_GAME_MODE_POSSIBLE)) {
                        break;
                }
        }
}

#define DISPLAY_TIMER (0)
#define FRAME_MS 15.0
static int main_window_wh[] = { 640, 480, };

static void timer_callback (int id)
{
        /* handle and restart timer to force redisplay */
        if (DISPLAY_TIMER == id) {
                glutPostRedisplay ();
                glutTimerFunc ((unsigned int) FRAME_MS / 3.0, timer_callback, id);
        }
}

static inline void constraint_to_ratio (int box[2], int pixel_ratio[2])
{
        int tentative_width = box[0];
        int tentative_height = box[0] * pixel_ratio[1] / pixel_ratio[0];

        if (tentative_height > box[1]) {
                tentative_height = box[1];
                tentative_width = tentative_height * pixel_ratio[0] / pixel_ratio[1];
        }

        box[0] = tentative_width;
        box[1] = tentative_height;
}

static void reshape_callback (int width, int height)
{
        int dim[2] = { width, height };
        int correction[2] = { 0, 0 };

        if (glutGameModeGet (GLUT_GAME_MODE_ACTIVE)) {
                constraint_to_ratio (dim, main_window_wh);

                correction[0] = (width - dim[0]) / 2;
                correction[1] = (height - dim[1]) / 2;
        }

        glViewport(correction[0], correction[1], dim[0], dim[1]);
}

static void display_callback()
{
        float const argb[4] = { 0.0f, 0.31f, 0.27f, 0.29f };
        glClearColor (argb[1], argb[2], argb[3], argb[0]);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glutSwapBuffers();
}

static void do_keyboard (unsigned char key, int x, int y)
{
        unsigned char const KEY_ESCAPE = 0x1b;

        if (key == KEY_ESCAPE) {
                exit (0);
        }
}

static void do_mouse(int button, int state, int x, int y)
{
        if (button == 1 && state) {
                exit(0);
        }
}

void open_window(char const * title, bool const prefers_fullscreen)
{
        char const* argv[] = { "title" };
        int argc = sizeof argv / sizeof argv[0];

        glutInit(&argc, (char**) argv);

        int const screen_wh[] = {
                glutGet(GLUT_SCREEN_WIDTH),
                glutGet(GLUT_SCREEN_HEIGHT),
        };

        int window_wh[] = {
                screen_wh[0],
                screen_wh[1],
        };

        if (!prefers_fullscreen) {
                window_wh[0] *= 1.0 / sqrt(2.0);
                window_wh[1] *= 1.0 / sqrt(2.0);
        }

        memcpy(main_window_wh, window_wh, sizeof main_window_wh);

        glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_MULTISAMPLE);
        glutInitWindowPosition(100, 100);
        glutInitWindowSize(window_wh[0], window_wh[1]);

        if (prefers_fullscreen && glutGameModeGet(GLUT_GAME_MODE_POSSIBLE)) {
                pick_gamemode_resolution(window_wh, screen_wh);
                glutEnterGameMode();
                if (glutGetWindow()) {
                        glutSetCursor(GLUT_CURSOR_NONE);
                }
        } else {
                glutCreateWindow(title);
        }

        glutSpecialFunc (NULL);
        glutReshapeFunc (reshape_callback);
        glutDisplayFunc (display_callback);
        glutIdleFunc (NULL);

        if (glutDeviceGet (GLUT_HAS_KEYBOARD)) {
                glutKeyboardFunc (do_keyboard);
        }

        if (glutDeviceGet (GLUT_HAS_MOUSE)) {
                glutMouseFunc (do_mouse);
        }

        timer_callback (DISPLAY_TIMER);

        glutMainLoop();
}

void close_window()
{
        if (glutGameModeGet(GLUT_GAME_MODE_ACTIVE)) {
                glutLeaveGameMode();
        }
        glutSetCursor(GLUT_CURSOR_INHERIT);
}
