#include <stdbool.h>
#include <string.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <signal.h>

Display* display;
Window window;

bool clipboard_watching = false;
bool toggle_key_watching = false;

bool GetSelection(Display *display, Window window, const char *bufname, const char *fmtname) {
        char *result;
        unsigned long ressize, restail;
        int resbits;
        Atom bufid = XInternAtom(display, bufname, False),
             fmtid = XInternAtom(display, fmtname, False),
             propid = XInternAtom(display, "XSEL_DATA", False),
             incrid = XInternAtom(display, "INCR", False);
        XEvent event;

        XConvertSelection(display, bufid, fmtid, propid, window, CurrentTime);
        do {
                XNextEvent(display, & event);
        } while (event.type != SelectionNotify || event.xselection.selection != bufid);

        if (event.xselection.property) {
                XGetWindowProperty(display, window, propid, 0, LONG_MAX/4, False, AnyPropertyType, &bufid, &resbits, &ressize, &restail, (unsigned char**)&result);
                if (fmtid == incrid) {
                        printf("Buffer is too large");
                } else {
                        printf("%s", result);
                }

                XFree(result);
                return true;
        } else {
                return false;
        }

}

int WatchEnablerKey() {
        unsigned int modifiers = ControlMask | ShiftMask;
        int keycode = XKeysymToKeycode(display, XK_Y);
        Window grab_window = DefaultRootWindow(display);
        Bool owner_events = False;
        int pointer_mode = GrabModeAsync;
        int keyboard_mode = GrabModeAsync;

        XGrabKey(display, keycode, modifiers, grab_window, owner_events, pointer_mode, keyboard_mode);
        XSelectInput(display, grab_window, KeyPressMask);
        
        while (true) {
                bool should_quit = false;
                XEvent event;
                XNextEvent(display, &event);
                switch (event.type) {
                        case KeyPress:
                                printf("Key pressed\n");
                                should_quit = true;
                                break;
                        // default:
                                // break;
                }
                if (should_quit) {
                        break;
                }
        }
        return 0;
}

bool WatchClipboard() {
        const char* bufname = "CLIPBOARD";

        int event_base, error_base;
        XEvent event;
        Atom bufid = XInternAtom(display, bufname, False);

        if (!XFixesQueryExtension(display, &event_base, &error_base)) {
                return false;
        }

        XFixesSelectSelectionInput(display, DefaultRootWindow(display), bufid, XFixesSetSelectionOwnerNotifyMask);

        clipboard_watching = true;
        while (clipboard_watching) {
                XNextEvent(display, &event);

                if (event.type == event_base + XFixesSelectionNotify && ((XFixesSelectionNotifyEvent*)&event)->selection == bufid) {
                        if (!GetSelection(display, window, bufname, "UTF8_STRING")) {
                                GetSelection(display, window, bufname, "STRING");
                        } else {
                                printf("Clipboard is empty\n");
                        }
                }
        }

        return true;
}

void StopWatchingClipboard() {
        if (!clipboard_watching || !display) {
                return; 
        }

        XClientMessageEvent dummyEvent;
        memset(&dummyEvent, 0, sizeof(dummyEvent));
        dummyEvent.type = ClientMessage;
        dummyEvent.display = display;
        dummyEvent.window = window;
        dummyEvent.message_type = XInternAtom(display, "CLIPBOARD", False);
        dummyEvent.format = 32;
        dummyEvent.data.l[0] = 0;
        dummyEvent.data.l[1] = 0;
        dummyEvent.data.l[2] = 0;
        dummyEvent.data.l[3] = 0;
        dummyEvent.data.l[4] = 0;
        XSendEvent(display, window, False, NoEventMask, (XEvent*)&dummyEvent);
        XFlush(display);
        clipboard_watching = false;
}


void SignalCallbackHandler(int signum) {
        printf("Caught signal %d\n", signum);
        StopWatchingClipboard();
        exit(signum);
}

int main() {
        signal(SIGINT, SignalCallbackHandler);

        display = XOpenDisplay(NULL);
        unsigned long color = BlackPixel(display, DefaultScreen(display));
        window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0,0, 1,1, 0, color, color);

               while (true) {
               WatchEnablerKey();
               WatchClipboard();
               }

        XDestroyWindow(display, window);
        XCloseDisplay(display);

        return EXIT_SUCCESS;
}
