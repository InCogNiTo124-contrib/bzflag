/* bzflag
 * Copyright (c) 1993-2025 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

// Own include
#include "SDL2Window.h"

// Common includes
#include "OpenGLGState.h"
#include "TimeKeeper.h"

#ifdef _WIN32
HWND SDLWindow::hwnd = NULL;
#endif

SDLWindow::SDLWindow(const SDLDisplay* _display, SDLVisual*)
    : BzfWindow(_display),
      windowId(NULL), glContext(NULL), canGrabMouse(true), fullScreen(false),
      vsync(false), base_width(640), base_height(480), min_width(-1), min_height(-1)
{
}

SDLWindow::~SDLWindow()
{
    if (windowId != NULL)
        SDL_DestroyWindow(windowId);
}

void SDLWindow::setTitle(const char *_title)
{
    title = _title;
    if (windowId)
        SDL_SetWindowTitle(windowId, title.c_str());
}

void SDLWindow::setFullscreen(bool on)
{
    fullScreen = on;
}

void SDLWindow::iconify(void)
{
    SDL_MinimizeWindow(windowId);
}


void SDLWindow::disableConfineToMotionbox()
{
#ifndef _WIN32
    SDL_SetWindowMouseGrab(windowId, false);
#else
    ClipCursor(NULL);
#endif
}


void SDLWindow::confineToMotionbox(int x1, int y1, int x2, int y2)
{
#ifdef __APPLE__
    // Work around an issue in macOS that caused mouse confinement to persist
    // after minimizing. It seems that mouse/keyboard events are sent to the
    // game even when minimized, until another other application is clicked.
    // Moving the mouse during the 5 second pause countdown would trigger a
    // call to confineToMotionbox and would continue to trap the mouse.
    if (SDL_GetWindowFlags(windowId) & SDL_WINDOW_MINIMIZED)
        return;
#endif

#ifndef _WIN32
    if (!SDL_GetWindowMouseGrab(windowId))
        SDL_SetWindowMouseGrab(windowId, true);

    BzfWindow::confineToMotionbox(x1, y1, x2, y2);
#else
    int posx, posy;
    SDL_GetWindowPosition(windowId, &posx, &posy);

    // Store the boundary positions as rectangle
    RECT rect;
    rect.top = y1 + posy;
    rect.left = x1 + posx;
    rect.bottom = y2 + posy;
    rect.right = x2 + posx;

    // Restrict cursor to that rectangle
    ClipCursor(&rect);
#endif
}


void SDLWindow::warpMouse(int _x, int _y)
{
    SDL_WarpMouseInWindow(windowId, (float)_x, (float)_y);
}

void SDLWindow::getMouse(int& _x, int& _y) const
{
    float mx = 0.0f, my = 0.0f;
    SDL_GetMouseState(&mx, &my);
    _x = (int)mx;
    _y = (int)my;
}

void SDLWindow::setSize(int _width, int _height)
{
    // workaround for two issues on Linux, where resizing by dragging the window corner causes glitching, and where
    // iconifying or switching applications while using a scaled fullscreen resolution causes the non-fullscreen
    // window resolution to assume the fullscreen resolution
#ifdef __linux__
    if(!fullScreen)
    {
        base_width  = _width;
        base_height = _height;
    }
#else
    base_width  = _width;
    base_height = _height;
    if (!fullScreen && windowId)
        SDL_SetWindowSize(windowId, base_width, base_height);
#endif // __linux__
}

void SDLWindow::getSize(int& width, int& height) const
{
    if (fullScreen)
        const_cast<SDLDisplay *>(static_cast<const SDLDisplay *>(getDisplay()))->getWindowSize(width, height);
    else
    {
        width  = base_width;
        height = base_height;
    }
}

void SDLWindow::swapBuffers()
{
    SDL_GL_SwapWindow(windowId);

    // workaround for SDL 2 bug on mac where an application window obstructed
    // by another window will not honor a vsync restriction
    // bug report: https://bugzilla.libsdl.org/show_bug.cgi?id=2998
    // TODO: Remove this workaround when/if SDL2 includes their own workaround.
#ifdef __APPLE__
    int interval = 0;
    if (!SDL_GL_GetSwapInterval(&interval) || interval == 0)
        return;

    int maxRunawayFPS = 65;
    const SDL_DisplayMode *desktopDisplayMode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (desktopDisplayMode && desktopDisplayMode->refresh_rate > 0.0f)
        maxRunawayFPS = (int)desktopDisplayMode->refresh_rate + 5;

    static TimeKeeper lastFrame = TimeKeeper::getSunGenesisTime();
    const TimeKeeper now = TimeKeeper::getCurrent();

    const double remaining = 1.0 / (double) maxRunawayFPS - (now - lastFrame);

    // this doesn't create our exact desired FPS, since our handling is
    // frame-to-frame and some frames will be late already and will not be
    // delayed, but it's close enough for the purposes of this workaround
    if (remaining > 0.0)
        TimeKeeper::sleep(remaining);

    lastFrame = now;
#endif //__APPLE__
}

// For some reason, when creating a new fullscreen window on Linux with a different resolution than before, SDL throws
// a resize event with the old window resolution, which is not what we want. Additionally, on macOS, sometime after
// version 2.0.14 of SDL we started getting bogus SDL_WINDOWEVENT_RESIZED events during the first toggle from fullscreen
// to windowed mode. This function is called to filter SDL window resize events in either of these scenarios to either
// adjust the resolution to the correct one (in the Linux scenario) or discard the event (in the macOS scenario).
#if defined(__linux__) || defined(__APPLE__)
bool SDLCALL SDLWindowEventFilter(void *resolution, SDL_Event *event)
{
    if (!resolution)
        return false; // drop the event

    if (event->type == SDL_EVENT_WINDOW_RESIZED ||
        event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
    {
        // adjust the window resolution to match the values passed to us
        event->window.data1 = static_cast<int *>(resolution)[0];
        event->window.data2 = static_cast<int *>(resolution)[1];
    }

    return true; // allow the event
}
#endif // __linux__ || __APPLE__

bool SDLWindow::create(void)
{
    int targetWidth, targetHeight;
    getSize(targetWidth, targetHeight);
    bool windowWasGrabbed = false;
    if (windowId != NULL)
        windowWasGrabbed = SDL_GetWindowMouseGrab(windowId);

    // if we have an existing identical window, go no further
    if (windowId != NULL)
    {
        int currentWidth, currentHeight;
        SDL_GetWindowSize(windowId, &currentWidth, &currentHeight);

        SDL_WindowFlags priorWindowFlags = SDL_GetWindowFlags(windowId);
        if (fullScreen == ((priorWindowFlags & SDL_WINDOW_FULLSCREEN) != 0) &&
                targetWidth == currentWidth && targetHeight == currentHeight)
            return true;
    }

    // destroy the pre-existing window if it exists
    if (windowId != NULL)
    {
        if (glContext)
            SDL_GL_DestroyContext(glContext);
        glContext = NULL;

        SDL_DestroyWindow(windowId);
    }

    // Apply filters due to bogus resize event issue on macOS (see the explanation above for SDLWindowEventFilter())
#ifdef __APPLE__
    SDL_PumpEvents();
    SDL_FilterEvents(&SDLWindowEventFilter, nullptr);
#endif // __APPLE__

    // (re)create the window

    // workaround for an SDL 2 bug on Linux with the GNOME Window List extension enabled, where attempting to create a
    // fullscreen window on a lower-resolution primary display while a higher-resolution secondary display is plugged in
    // causes an infinite loop of window creation on the secondary display
    // bug report: https://bugzilla.libsdl.org/show_bug.cgi?id=4990
#ifdef __linux__
    int numDisplays = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&numDisplays);
    if (displays)
        SDL_free(displays);
    if (!fullScreen || numDisplays < 2) // create the window with the standard logic
    {
#endif // __linux__
        windowId = SDL_CreateWindow(
                       title.c_str(),
                       targetWidth,
                       targetHeight,
                       SDL_WINDOW_OPENGL |
                       (fullScreen ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_RESIZABLE) |
                       (windowWasGrabbed ? SDL_WINDOW_MOUSE_GRABBED : 0));
        if (windowId && fullScreen)
        {
            SDL_DisplayMode closestMode;
            if (SDL_GetClosestFullscreenDisplayMode(SDL_GetPrimaryDisplay(), targetWidth, targetHeight, 0.0f, true, &closestMode))
                SDL_SetWindowFullscreenMode(windowId, &closestMode);
        }

        // continuation of above workaround
#ifdef __linux__
    }
    else // create the window in windowed mode first and then switch to fullscreen
    {
        windowId = SDL_CreateWindow(
                       title.c_str(),
                       base_width,
                       base_height,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | (windowWasGrabbed ? SDL_WINDOW_MOUSE_GRABBED : 0));
        if (!windowId)
        {
            printf("Could not set Video Mode: %s.\n", SDL_GetError());
            return false;
        }

        SDL_DisplayMode closestMode;
        if (!SDL_GetClosestFullscreenDisplayMode(SDL_GetPrimaryDisplay(), targetWidth, targetHeight, 0.0f, true, &closestMode))
        {
            printf("Unable to get fullscreen display mode: %s\n", SDL_GetError());
            SDL_DestroyWindow(windowId);
            windowId = NULL;
            return false;
        }
        if (!SDL_SetWindowFullscreenMode(windowId, &closestMode))
        {
            printf("Unable to set display mode: %s\n", SDL_GetError());
            SDL_DestroyWindow(windowId);
            windowId = NULL;
            return false;
        }
        if (!SDL_SetWindowFullscreen(windowId, true))
        {
            printf("Unable to set window to fullscreen mode: %s\n", SDL_GetError());
            SDL_DestroyWindow(windowId);
            windowId = NULL;
            return false;
        }
    }

    // Depending on the distribution (or possibly the window manager), SDL will not recognize the new window resolution
    // and will keep returning the previous resolution when SDL_GetWindowSize() is called, until a period of time has
    // passed and events have been pumped. Wait up to two seconds for the correct resolution to start being returned,
    // checking every quarter second, to avoid repeating the window destruction/re-creation process based on bad data.
    int currentWidth, currentHeight, resCheckLoops = 0;

    do
    {
        SDL_PumpEvents();
        SDL_GetWindowSize(windowId, &currentWidth, &currentHeight);

        if(currentWidth == targetWidth && currentHeight == targetHeight)
            break;

        TimeKeeper::sleep(0.25f);
    }
    while (resCheckLoops++ < 8);
#endif // __linux__

    // Apply filters due to resize event issues on Linux (see the explanation above for SDLWindowEventFilter())
#ifdef __linux__
    SDL_PumpEvents();
    int windowResolution[] = { targetWidth, targetHeight };
    SDL_FilterEvents(&SDLWindowEventFilter, windowResolution);
#endif // __linux__

    // Work around an issue with SDL on macOS where a window that gets resized by the operating system for various
    // reasons (e.g., creating a window that doesn't fit between the dock and menu bar, or switching from a maximized
    // window to native fullscreen then back to windowed mode) doesn't always correctly throw a resize event
#ifdef __APPLE__
    SDL_PumpEvents();

    int currentWidth, currentHeight;
    SDL_GetWindowSize(windowId, &currentWidth, &currentHeight);

    if(! fullScreen && (currentWidth != targetWidth || currentHeight != targetHeight))
    {
        SDL_Event fakeResizeEvent;
        SDL_zero(fakeResizeEvent);

        fakeResizeEvent.type = SDL_EVENT_WINDOW_RESIZED;
        fakeResizeEvent.window.windowID = 0; // deliberately not matching SDL_GetWindowID() so SDL doesn't purge event
        fakeResizeEvent.window.data1 = currentWidth;
        fakeResizeEvent.window.data2 = currentHeight;

        SDL_PushEvent(&fakeResizeEvent);
    }
#endif // __APPLE__

#ifdef _WIN32
    hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(windowId), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#endif

    if (!windowId)
    {
        printf("Could not set Video Mode: %s.\n", SDL_GetError());
        return false;
    }

    SDL_StartTextInput(windowId);

    if (min_width >= 0)
        setMinSize(min_width, min_height);

    makeContext();
    makeCurrent();

    if (!SDL_GL_SetSwapInterval(vsync ? -1 : 0) && vsync)
        // no adaptive vsync; set regular vsync
        SDL_GL_SetSwapInterval(1);

    // init opengl context
    OpenGLGState::initContext();

    return true;
}

void SDLWindow::enableGrabMouse(bool on)
{
    canGrabMouse = on;
    SDL_SetWindowMouseGrab(windowId, canGrabMouse);
}

void SDLWindow::makeContext()
{
    glContext = SDL_GL_CreateContext(windowId);
    if (!glContext)
        printf("Could not Create GL Context: %s.\n", SDL_GetError());
}

void SDLWindow::setVerticalSync(bool setting)
{
    vsync = setting;

    if (windowId != NULL)
        if (glContext != NULL)
            if (!SDL_GL_SetSwapInterval(vsync ? -1 : 0) && vsync)
                // no adaptive vsync; set regular vsync
                SDL_GL_SetSwapInterval(1);
}

void SDLWindow::setMinSize(int width, int height)
{
    min_width  = width;
    min_height = height;
    if (!windowId)
        return;
    SDL_SetWindowMinimumSize (windowId, width, height);
}

void SDLWindow::makeCurrent()
{
    if (!windowId)
        return;
    if (!glContext)
        return;
    if (!SDL_GL_MakeCurrent(windowId, glContext))
    {
        printf("Could not Make GL Context Current: %s.\n", SDL_GetError());
        abort();
    }
}

void SDLWindow::freeContext()
{
    SDL_GL_DestroyContext(glContext);
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
