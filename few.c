#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

typedef struct Key Key;
typedef void (*Events)(XEvent *event);

struct Key {
    unsigned int modifier;
    KeySym keysym;
    void (*function)(XEvent *event, char *command);
    char *command;
};

static void size(void);
static void grab(void);
static void scan(void);
static void loop(void);

static void enter(XEvent *event);
static void configure(XEvent *event);
static void key(XEvent *event);
static void map(XEvent *event);

static void focus(XEvent *event, char *command);
static void launch(XEvent *event, char *command);
static void destroy(XEvent *event, char *command);
static void refresh(XEvent *event, char *command);

static int ignore(Display *display, XErrorEvent *event);

static Display *display;
static Window root;
static int screen, width, height;
static Atom net_active;

#define ALT Mod1Mask
#define SUPER Mod4Mask

static Key keys[] = {
    { ALT, XK_Return, launch, "st" },
    { ALT, XK_space, launch, "rofi -show drun" },
    { ALT, XK_b, launch, "firefox" },
    { SUPER | ShiftMask, XK_s, launch, "shot" },
    { SUPER, XK_e, launch, "rdfm"},
    { SUPER, XK_p, launch, "bash $HOME/pkg/few/scripts/rofi/powermenu"},
    { SUPER, XK_period, launch, "bash $HOME/pkg/few/scripts/rofi/wallpaper"},
    { SUPER | ShiftMask, XK_c, launch, "px" },
    { ALT | ShiftMask, XK_q, destroy, 0 },
    { ALT | ShiftMask, XK_r, refresh, 0 },
    { SUPER, XK_Tab, launch, "rofi -show window" },
    { ALT, XK_Tab, focus, "next" },
    { ALT | ShiftMask, XK_Tab, focus, "prev" },
    { 0, XF86XK_AudioMute, launch, "pactl set-sink-mute @DEFAULT_SINK@ toggle" },
    { 0, XF86XK_AudioLowerVolume, launch, "pactl set-sink-volume @DEFAULT_SINK@ -10%" },
    { 0, XF86XK_AudioRaiseVolume, launch, "bash $HOME/pkg/few/scripts/volume_raise" },
    { 0, XF86XK_MonBrightnessDown, launch, "brightnessctl set 10%-" },
    { 0, XF86XK_MonBrightnessUp, launch, "brightnessctl set +10%" },
};

static const char *startups[] = {
    "rsxiv -B /home/himadri/pkg/few/Wallpaper/Wallpaper",
    "doid",
    "xclip",
    "greenclip daemon",
    "bash -c $HOME/pkg/few/scripts/batt",
    NULL,
};

static const Events events[LASTEvent] = {
    [ConfigureRequest] = configure,
    [EnterNotify] = enter,
    [KeyPress] = key,
    [MapRequest] = map,
};

void size(void)
{
    XWindowAttributes attributes;

    if (XGetWindowAttributes(display, root, &attributes)) {
        width = attributes.width;
        height = attributes.height;
    } else {
        width = XDisplayWidth(display, screen);
        height = XDisplayHeight(display, screen);
    }
}

void grab(void)
{
    unsigned int i;

    for (i = 0; i < sizeof(keys) / sizeof(struct Key); i++)
        XGrabKey(display, XKeysymToKeycode(display, keys[i].keysym),
                keys[i].modifier, root, True, GrabModeAsync, GrabModeAsync);
}

void scan()
{
    unsigned int i, n;
    Window r, p, *c;

    if (XQueryTree(display, root, &r, &p, &c, &n)) {
        for (i = 0; i < n; i++)
            XMoveResizeWindow(display, c[i], 0, 0, width, height);

        if (c)
            XFree(c);
    }
}

void loop(void)
{
    XEvent event;

    while (1 && !XNextEvent(display, &event))
        if (events[event.type])
            events[event.type](&event);
}

void enter(XEvent *event)
{
    Window window = event->xcrossing.window;

    XSetInputFocus(display, window, RevertToParent, CurrentTime);
    XRaiseWindow(display, window);

    XChangeProperty(display,
                    root,
                    net_active,
                    XA_WINDOW,
                    32,
                    PropModeReplace,
                    (unsigned char *)&window,
                    1);
}

void configure(XEvent *event)
{
    XConfigureRequestEvent *request = &event->xconfigurerequest;
    XWindowChanges changes = {
        .x = request->x,
        .y = request->y,
        .width = request->width,
        .height = request->height,
        .border_width = request->border_width,
        .sibling = request->above,
        .stack_mode = request->detail,
    };

    XConfigureWindow(display, request->window, request->value_mask, &changes);
}

void key(XEvent *event)
{
    unsigned int i;
    KeySym keysym = XkbKeycodeToKeysym(display, event->xkey.keycode, 0, 0);

    for (i = 0; i < sizeof(keys) / sizeof(struct Key); i++)
        if (keysym == keys[i].keysym && keys[i].modifier == event->xkey.state)
            keys[i].function(event, keys[i].command);
}

void map(XEvent *event)
{
    Window window = event->xmaprequest.window;
    XWindowChanges changes = { .border_width = 0 };

    XSelectInput(display, window, StructureNotifyMask | EnterWindowMask);
    XConfigureWindow(display, window, CWBorderWidth, &changes);
    XMoveResizeWindow(display, window, 0, 0, width, height);
    XMapWindow(display, window);
}

void focus(XEvent *event, char *command)
{
    (void)event;
    int next = command[0] == 'n';

    XCirculateSubwindows(display, root, next ? RaiseLowest : LowerHighest);
}

void launch(XEvent *event, char *command)
{
    (void)event;

    if (fork() == 0) {
        if (fork() == 0) {
            if (display)
                close(XConnectionNumber(display));

            setsid();
            execl("/bin/sh", "sh", "-c", command, 0);

            exit(1);
        }
        else {
            exit(0);
        }
    }
}

void startup(void)
{
    for (int i = 0; startups[i]; i++)
        launch(NULL, (char *)startups[i]);
}

void destroy(XEvent *event, char *command)
{
    (void)command;

    XSetCloseDownMode(display, DestroyAll);
    XKillClient(display, event->xkey.subwindow);
}

void refresh(XEvent *event, char *command)
{
    (void)event;
    (void)command;

    size();
    scan();
}

int ignore(Display *display, XErrorEvent *event)
{
    (void)display;
    (void)event;

    return 0;
}

int main(void)
{
    if (!(display = XOpenDisplay(0)))
        exit(1);

    signal(SIGCHLD, SIG_IGN);
    XSetErrorHandler(ignore);

    screen = XDefaultScreen(display);
    root = XDefaultRootWindow(display);
    net_active = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);

    startup();

    XSelectInput(display, root, SubstructureRedirectMask);
    XDefineCursor(display, root, XCreateFontCursor(display, 68));

    size();
    grab();
    scan();
    loop();
}
