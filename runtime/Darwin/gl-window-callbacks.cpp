#include <string.h> // memcpy
#include <stdlib.h> // exit

#include <GLUT/glut.h>

#include <api.h>

#include "window.h"

#define DISPLAY_TIMER (0)
#define FRAME_MS 15.0
static int main_window_wh[2] = { 1980, 720 };

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
        render_next_gl(0);
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

extern void register_window_callbacks(int const window_wh[2])
{
        memcpy(main_window_wh, window_wh, sizeof main_window_wh);

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

}
