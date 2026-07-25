#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
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
static void expose(XEvent *event);

static void focus(XEvent *event, char *command);
static void launch(XEvent *event, char *command);
static void destroy(XEvent *event, char *command);
static void refresh(XEvent *event, char *command);

static void barinit(void);
static void bar(void);
static char *run(const char *command);
static void join(char *dest, size_t size, const char **commands);
static void windowlist(char *dest, size_t size);

static int ignore(Display *display, XErrorEvent *event);

static Display *display;
static Window root, barwin, active;
static GC gc;
static XFontStruct *font;
static int screen, width, height;
static Atom net_active;

#define ALT Mod1Mask
#define SUPER Mod4Mask
#define BARHEIGHT 20

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

/* bar modules: each section is just a list of shell commands, same idea
 * as startups[] above. first line of each command's stdout is used.
 * the left section is handled separately -- see windowlist(). */
static const char *center[] = {
    "hostname",
    NULL,
};

static const char *right[] = {
    "bash $HOME/pkg/few/scripts/barbatt",
    "date +%H:%M",
    NULL,
};

static const Events events[LASTEvent] = {
    [ConfigureRequest] = configure,
    [EnterNotify] = enter,
    [Expose] = expose,
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

    height -= BARHEIGHT;
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
            if (c[i] != barwin)
                XMoveResizeWindow(display, c[i], 0, BARHEIGHT, width, height);

        if (c)
            XFree(c);
    }
}

void loop(void)
{
    XEvent event;
    struct timeval tv;
    fd_set fds;
    int xfd = ConnectionNumber(display);

    while (1) {
        if (XPending(display)) {
            XNextEvent(display, &event);

            if (events[event.type])
                events[event.type](&event);

            continue;
        }

        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        if (select(xfd + 1, &fds, 0, 0, &tv) == 0)
            bar();
    }
}

void enter(XEvent *event)
{
    Window window = event->xcrossing.window;

    active = window;

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
    XMoveResizeWindow(display, window, 0, BARHEIGHT, width, height);
    XMapWindow(display, window);
}

void expose(XEvent *event)
{
    if (event->xexpose.window == barwin && event->xexpose.count == 0)
        bar();
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
    XMoveResizeWindow(display, barwin, 0, 0, width, BARHEIGHT);
    bar();
}

/* runs a command, returns its first line of stdout (trimmed).
 * uses a static buffer -- caller must use the result before
 * calling run() again. */
char *run(const char *command)
{
    static char buffer[256];
    FILE *pipe;

    buffer[0] = 0;

    if ((pipe = popen(command, "r"))) {
        if (fgets(buffer, sizeof buffer, pipe))
            buffer[strcspn(buffer, "\n")] = 0;

        pclose(pipe);
    }

    return buffer;
}

/* joins a module list's output into dest, separated by " | " */
void join(char *dest, size_t size, const char **commands)
{
    char *result;
    int i;

    dest[0] = 0;

    for (i = 0; commands[i]; i++) {
        result = run(commands[i]);

        if (!result[0])
            continue;

        if (dest[0] && strlen(dest) + strlen(" | ") < size)
            strcat(dest, " | ");

        if (strlen(dest) + strlen(result) < size)
            strcat(dest, result);
    }
}

/* builds "[Firefox] Discord rsxiv" -- every mapped top-level window,
 * space separated, with the currently focused one wrapped in []. */
void windowlist(char *dest, size_t size)
{
    Window r, p, *c;
    XWindowAttributes wattr;
    XTextProperty prop;
    unsigned int i, n;
    char *name;
    int first = 1;

    dest[0] = 0;

    if (!XQueryTree(display, root, &r, &p, &c, &n))
        return;

    for (i = 0; i < n; i++) {
        if (c[i] == barwin)
            continue;

        if (!XGetWindowAttributes(display, c[i], &wattr) ||
                wattr.map_state != IsViewable)
            continue;

        name = NULL;

        if (XGetWMName(display, c[i], &prop) && prop.value)
            name = (char *)prop.value;

        if (!name)
            name = "?";

        if (!first && strlen(dest) + 1 < size)
            strcat(dest, " ");

        first = 0;

        if (strlen(dest) + strlen(name) + 3 < size) {
            if (c[i] == active)
                strcat(dest, "[");

            strcat(dest, name);

            if (c[i] == active)
                strcat(dest, "]");
        }

        if (name != NULL && strcmp(name, "?") != 0)
            XFree(prop.value);
    }

    if (c)
        XFree(c);
}

void barinit(void)
{
    XSetWindowAttributes attributes = {
        .override_redirect = True,
        .background_pixel = BlackPixel(display, screen),
        .event_mask = ExposureMask,
    };

    font = XLoadQueryFont(display, "fixed");

    barwin = XCreateWindow(display, root, 0, 0, width, BARHEIGHT, 0,
            DefaultDepth(display, screen), CopyFromParent,
            DefaultVisual(display, screen),
            CWOverrideRedirect | CWBackPixel | CWEventMask, &attributes);

    gc = XCreateGC(display, barwin, 0, 0);
    XSetForeground(display, gc, WhitePixel(display, screen));

    if (font)
        XSetFont(display, gc, font->fid);

    XMapWindow(display, barwin);
    XRaiseWindow(display, barwin);
}

void bar(void)
{
    char l[256], c[256], r[256];
    int lw, cw, rw;
    int baseline = font ? font->ascent + 2 : 14;

    windowlist(l, sizeof l);
    join(c, sizeof c, center);
    join(r, sizeof r, right);

    XClearWindow(display, barwin);

    lw = font ? XTextWidth(font, l, strlen(l)) : 0;
    cw = font ? XTextWidth(font, c, strlen(c)) : 0;
    rw = font ? XTextWidth(font, r, strlen(r)) : 0;
    (void)lw;

    XDrawString(display, barwin, gc, 4, baseline, l, strlen(l));
    XDrawString(display, barwin, gc, (width - cw) / 2, baseline, c, strlen(c));
    XDrawString(display, barwin, gc, width - rw - 4, baseline, r, strlen(r));
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
    barinit();
    bar();
    grab();
    scan();
    loop();
}
