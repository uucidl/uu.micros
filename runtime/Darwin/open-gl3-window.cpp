#include <math.h> // sqrt
#include <stdio.h> // snprintf

#include <GLUT/glut.h>

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

        glutInitDisplayMode(GLUT_3_2_CORE_PROFILE
                            | GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH
                            | GLUT_MULTISAMPLE);
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

        register_window_callbacks(window_wh);

        glutMainLoop();
}

void close_window()
{
        if (glutGameModeGet(GLUT_GAME_MODE_ACTIVE)) {
                glutLeaveGameMode();
        }
        glutSetCursor(GLUT_CURSOR_INHERIT);
}
