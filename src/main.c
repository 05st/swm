#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

int max(int a, int b) { return (a > b ? a : b); }
int min(int a, int b) { return (a > b ? b : a); }

bool running = true;

static Window wins[4096];
static int win_count = 0;

static Display* _display;
static Window _root;

typedef struct { int x, y; } Position;
typedef struct { int w, h; } Size;
static Position d_start;
static Position df_start;
static Size df_size;

static Atom WM_PROTOCOLS, WM_DELETE_WINDOW;

static bool wm_detected = false;

void onConfigureRequest(XEvent*);
void onMapRequest(XEvent*);
void onUnmapNotify(XEvent*);
void onButtonPress(XEvent*);
void onMotionNotify(XEvent*);
void onKeyPress(XEvent*);

static void (*handler[]) (XEvent*) = {
    [ConfigureRequest] = onConfigureRequest,
    [MapRequest] = onMapRequest,
    [UnmapNotify] = onUnmapNotify,
    [ButtonPress] = onButtonPress,
    [MotionNotify] = onMotionNotify,
    [KeyPress] = onKeyPress,
};

void onConfigureRequest(XEvent* _e) {
    XConfigureRequestEvent* e = &_e->xconfigurerequest;
    
    XWindowChanges changes;
    changes.x = e->x;
    changes.y = e->y;
    changes.width = e->width;
    changes.height = e->height;
    changes.border_width = e->border_width;
    changes.sibling = e->above;
    changes.stack_mode = e->detail;
    
    XConfigureWindow(_display, e->window, e->value_mask, &changes);
}

void frame(Window w, bool old) {
    XWindowAttributes attr;
    XGetWindowAttributes(_display, w, &attr);

    if (old && (attr.override_redirect || attr.map_state != IsViewable))
        return;

    XSelectInput(_display, w, SubstructureRedirectMask | SubstructureNotifyMask);
    XAddToSaveSet(_display, w);

    XGrabButton(_display, Button1, Mod1Mask, w, false, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(_display, Button3, Mod1Mask, w, false, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, None);
    
    XGrabKey(_display, XKeysymToKeycode(_display, XK_W), Mod1Mask, w, false, GrabModeAsync, GrabModeAsync);
	XGrabKey(_display, XKeysymToKeycode(_display, XK_Tab), Mod1Mask, w, false, GrabModeAsync, GrabModeAsync);
	XGrabKey(_display, XKeysymToKeycode(_display, XK_Q), Mod1Mask | ShiftMask, w, false, GrabModeAsync, GrabModeAsync);
}

void onMapRequest(XEvent* _e) {
    XMapRequestEvent* e = &_e->xmaprequest;

    frame(e->window, false);
    XMapWindow(_display, e->window);

	wins[win_count] = e->window;
    win_count++;
}

void unframe(Window w) {
    XReparentWindow(_display, w, _root, 0, 0);
	XRemoveFromSaveSet(_display, w);
	XDestroyWindow(_display, w);
}

void onUnmapNotify(XEvent* _e) {
    XUnmapEvent* e = &_e->xunmap;

	for (int i = 0; i < win_count; i++) {
		if (e->window == wins[i]) {
            printf("Before:\n");
            for (int i = 0; i < win_count; i++) {
                printf("%d ", wins[i]);
            }
            printf("\n");

            for (int j = i + 1; j < win_count; j++)
                wins[j-1] = wins[j];
            win_count--;

            printf("After:\n");
            for (int i = 0; i < win_count; i++) {
                printf("%d ", wins[i]);
            }
            printf("\n");
			break;
		}
	}

    if (e->event == _root)
        return;

	unframe(e->window);
}

void onButtonPress(XEvent* _e) {
    XButtonEvent* e = &_e->xbutton;

    d_start = (Position){.x = e->x_root, .y = e->y_root};

    Window ret_root;
    int x, y;
    unsigned w, h, bw, d;
    XGetGeometry(_display, e->window, &ret_root, &x, &y, &w, &h, &bw, &d);

    df_start = (Position){.x = x, .y = y};
    df_size = (Size){.w = w, .h = h};

    XRaiseWindow(_display, e->window);
}

void onMotionNotify(XEvent* _e) {
    XMotionEvent* e = &_e->xmotion;

    Position dpos = (Position){.x = e->x_root, .y = e->y_root};
    int dx = dpos.x - d_start.x;
    int dy = dpos.y - d_start.y;

    if (e->state & Button1Mask)
        XMoveWindow(_display, e->window, df_start.x + dx, df_start.y + dy);
    else if (e->state & Button3Mask)
        XResizeWindow(_display, e->window, max(4, df_size.w + dx), max(4, df_size.h + dy));
}

void onKeyPress(XEvent* _e) {
    XKeyEvent* e = &_e->xkey;
    
    if ((e->state & Mod1Mask) && (e->keycode == XKeysymToKeycode(_display, XK_W))) {
		Atom* ps;
		int ps_count;
		if (XGetWMProtocols(_display, e->window, &ps, &ps_count)) {
			bool has_del_p = false;
			for (int i = 0; i < ps_count; i++) {
				if (ps[i] == WM_DELETE_WINDOW) {
					has_del_p = true;
					break;
				}
			}

			if (has_del_p) {
				XEvent msg;
				msg.xclient.type = ClientMessage;
				msg.xclient.message_type = WM_PROTOCOLS;
				msg.xclient.window = e->window;
				msg.xclient.format = 32;
				msg.xclient.data.l[0] = WM_DELETE_WINDOW;
				XSendEvent(_display, e->window, false, 0, &msg);
			} else {
				XKillClient(_display, e->window);
			}
		}
    } else if ((e->state & Mod1Mask) && (e->keycode == XKeysymToKeycode(_display, XK_Tab))) {
		int next_i = 0;
		for (int i = 0; i < win_count; i++) {
			if (e->window == wins[i]) {
				next_i = (i + 1) % win_count;
				break;
			}
        }

        printf("next_i: %lld\n", next_i);

		XRaiseWindow(_display, wins[next_i]);
		XSetInputFocus(_display, wins[next_i], RevertToPointerRoot, CurrentTime);
	} else if ((e->state & (Mod1Mask | ShiftMask)) && (e->keycode == XKeysymToKeycode(_display, XK_Q))) {
        running = false;
    }
}

int onXError(Display* display, XErrorEvent* e) {
    char msg[1024];
    XGetErrorText(display, e->error_code, msg, 1024);
    printf("Error: %s\n\tReq: %d\n\tCode: %d\n\tID: %lld\n", msg, e->request_code, e->error_code, e->resourceid);
    fflush(stdout);
    return 0;
}

int onWMDetected(Display* display, XErrorEvent* e) {
    if (e->error_code == BadAccess)
        wm_detected = true;
    return 0;
}

void run() {
    XSetErrorHandler(&onWMDetected);
    XSelectInput(_display, _root, SubstructureRedirectMask | SubstructureNotifyMask);
    XSync(_display, false);

    if (wm_detected) {
        printf("Detected another window manager\n");
        fflush(stdout);
        return;
    }

    XSetErrorHandler(&onXError);
    
    XGrabServer(_display);

    Window ret_root, ret_parent;
    Window* tl_windows;
    int tl_count;
    XQueryTree(_display, _root, &ret_root, &ret_parent, &tl_windows, &tl_count);

    for (int i = 0; i < tl_count; i++)
        frame(tl_windows[i], true);
    
    XFree(tl_windows);
    XUngrabServer(_display);
    
    XEvent e;
    while (running && !XNextEvent(_display, &e))
        if (handler[e.type])
            handler[e.type](&e);
}

int main() {
	_display = XOpenDisplay(NULL);
    if (!_display) {
        printf("Failed to open display %s\n", XDisplayName(NULL));
        fflush(stdout);
        return 1;
    }
    
    WM_PROTOCOLS = XInternAtom(_display, "WM_PROTOCOLS", false);
    WM_DELETE_WINDOW = XInternAtom(_display, "WM_DELETE_WINDOW", false);

    _root = DefaultRootWindow(_display);

	system("sxhkd &");
    run();

    system("killall sxhkd");
    XCloseDisplay(_display);
	return 0;
}
