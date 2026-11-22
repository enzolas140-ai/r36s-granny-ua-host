// Optional evdev multitouch bridge for Granny's two-contact mapper.
// The mapper exposes a type-B Linux touchscreen named "Granny Dual Touchscreen".
// This module forwards its slots to Unity's Android MotionEvent queue.
#include "granny_touch.h"
#include "../jni/unityplayer.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define GT_SLOTS 2

typedef struct {
    int active;
    int tracking;
    int x, y;
} gt_slot;

static int gt_fd = -1;
static int gt_width = 640, gt_height = 480;
static int gt_min_x = 0, gt_max_x = 639, gt_min_y = 0, gt_max_y = 479;
static int gt_slot;
static int gt_was_enabled;
static double gt_next_scan;
static char gt_device_name[128];
static gt_slot gt_now[GT_SLOTS], gt_sent[GT_SLOTS];

static double gt_clock(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static int gt_count(const gt_slot *s)
{
    int n = 0;
    for (int i = 0; i < GT_SLOTS; i++) n += !!s[i].active;
    return n;
}

static int gt_index(const gt_slot *s, int slot)
{
    int n = 0;
    for (int i = 0; i < slot; i++) n += !!s[i].active;
    return n;
}

static float gt_x(int x)
{
    int range = gt_max_x - gt_min_x;
    if (range <= 0) range = gt_width - 1;
    if (x < gt_min_x) x = gt_min_x;
    if (x > gt_max_x) x = gt_max_x;
    return (float)(x - gt_min_x) * (float)(gt_width - 1) / (float)range;
}

static float gt_y(int y)
{
    int range = gt_max_y - gt_min_y;
    if (range <= 0) range = gt_height - 1;
    if (y < gt_min_y) y = gt_min_y;
    if (y > gt_max_y) y = gt_max_y;
    return (float)(y - gt_min_y) * (float)(gt_height - 1) / (float)range;
}

static void gt_send(int action, const gt_slot *s)
{
    int ids[GT_SLOTS], n = 0;
    float x[GT_SLOTS], y[GT_SLOTS];
    for (int i = 0; i < GT_SLOTS; i++) {
        if (!s[i].active) continue;
        ids[n] = i;                 // stable Android pointer IDs: slot 0 and slot 1
        x[n] = gt_x(s[i].x);
        y[n] = gt_y(s[i].y);
        n++;
    }
    if (n) up_inject_touch(action, n, ids, x, y);
}

static int gt_same(const gt_slot *a, const gt_slot *b, int i)
{
    return a[i].active && b[i].active && a[i].tracking == b[i].tracking;
}

static void gt_flush(int enabled)
{
    gt_slot work[GT_SLOTS];
    memcpy(work, gt_sent, sizeof work);
    if (!enabled) {
        memcpy(gt_sent, gt_now, sizeof gt_sent);
        return;
    }

    int transition = 0;

    // Release old contacts first. The lifted pointer remains in the event carrying POINTER_UP.
    for (int i = 0; i < GT_SLOTS; i++) {
        if (work[i].active && !gt_same(gt_sent, gt_now, i)) {
            for (int j = 0; j < GT_SLOTS; j++)
                if (j != i && gt_same(gt_sent, gt_now, j)) {
                    work[j].x = gt_now[j].x;
                    work[j].y = gt_now[j].y;
                }
            int n = gt_count(work), index = gt_index(work, i);
            if (n == 1) gt_send(UP_ACTION_UP, work);
            else if (n > 1) gt_send(UP_ACTION_POINTER_UP | (index << 8), work);
            work[i].active = 0;
            transition = 1;
        }
    }

    // Add new contacts. A second contact is an Android POINTER_DOWN with its slot index.
    for (int i = 0; i < GT_SLOTS; i++) {
        if (gt_now[i].active && !gt_same(gt_sent, gt_now, i)) {
            work[i] = gt_now[i];
            for (int j = 0; j < GT_SLOTS; j++)
                if (j != i && gt_same(gt_sent, gt_now, j)) {
                    work[j].x = gt_now[j].x;
                    work[j].y = gt_now[j].y;
                }
            int n = gt_count(work), index = gt_index(work, i);
            if (n == 1) gt_send(UP_ACTION_DOWN, work);
            else gt_send(UP_ACTION_POINTER_DOWN | (index << 8), work);
            transition = 1;
        }
    }

    if (!transition) {
        int moved = 0;
        for (int i = 0; i < GT_SLOTS; i++)
            if (gt_same(gt_sent, gt_now, i) &&
                (gt_sent[i].x != gt_now[i].x || gt_sent[i].y != gt_now[i].y))
                moved = 1;
        if (moved) gt_send(UP_ACTION_MOVE, gt_now);
    }

    memcpy(gt_sent, gt_now, sizeof gt_sent);
}

void granny_touch_feed_event(int type, int code, int value)
{
    if (type == EV_ABS) {
        switch (code) {
        case ABS_MT_SLOT:
            if (value >= 0 && value < GT_SLOTS) gt_slot = value;
            break;
        case ABS_MT_TRACKING_ID:
            gt_now[gt_slot].active = value >= 0;
            gt_now[gt_slot].tracking = value;
            break;
        case ABS_MT_POSITION_X:
            gt_now[gt_slot].x = value;
            break;
        case ABS_MT_POSITION_Y:
            gt_now[gt_slot].y = value;
            break;
        }
    } else if (type == EV_SYN && code == SYN_REPORT) {
        gt_flush(1);
    } else if (type == EV_SYN && code == SYN_DROPPED) {
        memset(gt_now, 0, sizeof gt_now);
        gt_flush(1);
    }
}

static void gt_scan(void)
{
    DIR *d = opendir("/dev/input");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "event", 5)) continue;
        char path[256], name[256] = {0};
        snprintf(path, sizeof path, "/dev/input/%s", e->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        if (ioctl(fd, EVIOCGNAME(sizeof name), name) < 0 ||
            strcmp(name, gt_device_name) != 0) {
            close(fd);
            continue;
        }
        struct input_absinfo ax, ay;
        if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &ax) == 0) {
            gt_min_x = ax.minimum; gt_max_x = ax.maximum;
        }
        if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &ay) == 0) {
            gt_min_y = ay.minimum; gt_max_y = ay.maximum;
        }
        gt_fd = fd;
        fprintf(stderr, "[input] Granny evdev multitouch: %s (%s), %d..%d x %d..%d\n",
                path, name, gt_min_x, gt_max_x, gt_min_y, gt_max_y);
        break;
    }
    closedir(d);
}

void granny_touch_init(int width, int height)
{
    gt_width = width > 0 ? width : 640;
    gt_height = height > 0 ? height : 480;
    const char *name = getenv("UA_EVDEV_TOUCH_NAME");
    if (!name || !*name) return;
    snprintf(gt_device_name, sizeof gt_device_name, "%s", name);
    gt_scan();
    gt_next_scan = gt_clock() + 0.5;
    if (gt_fd < 0)
        fprintf(stderr, "[input] waiting for evdev touchscreen named %s\n", gt_device_name);
}

void granny_touch_poll(int enabled)
{
    if (!gt_device_name[0]) return;
    if (enabled && !gt_was_enabled) memset(gt_sent, 0, sizeof gt_sent);
    gt_was_enabled = enabled;

    if (gt_fd < 0) {
        if (gt_clock() >= gt_next_scan) {
            gt_next_scan = gt_clock() + 0.5;
            gt_scan();
        }
        return;
    }

    struct input_event ev[32];
    for (;;) {
        ssize_t n = read(gt_fd, ev, sizeof ev);
        if (n > 0) {
            int count = (int)(n / (ssize_t)sizeof ev[0]);
            for (int i = 0; i < count; i++) {
                if (ev[i].type == EV_ABS) {
                    switch (ev[i].code) {
                    case ABS_MT_SLOT:
                        if (ev[i].value >= 0 && ev[i].value < GT_SLOTS) gt_slot = ev[i].value;
                        break;
                    case ABS_MT_TRACKING_ID:
                        gt_now[gt_slot].active = ev[i].value >= 0;
                        gt_now[gt_slot].tracking = ev[i].value;
                        break;
                    case ABS_MT_POSITION_X: gt_now[gt_slot].x = ev[i].value; break;
                    case ABS_MT_POSITION_Y: gt_now[gt_slot].y = ev[i].value; break;
                    }
                } else if (ev[i].type == EV_SYN && ev[i].code == SYN_REPORT) {
                    gt_flush(enabled);
                } else if (ev[i].type == EV_SYN && ev[i].code == SYN_DROPPED) {
                    memset(gt_now, 0, sizeof gt_now);
                    gt_flush(enabled);
                }
            }
            continue;
        }
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            if (gt_count(gt_sent) && enabled) gt_send(UP_ACTION_CANCEL, gt_sent);
            close(gt_fd);
            gt_fd = -1;
            memset(gt_now, 0, sizeof gt_now);
            memset(gt_sent, 0, sizeof gt_sent);
            gt_next_scan = gt_clock() + 0.5;
        }
        break;
    }
}
