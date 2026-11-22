#include "../../jni/unityplayer.h"
#include <linux/input.h>
#include "../granny_touch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int action, n, ids[2];
    float x[2], y[2];
} recorded;
static recorded events[16];
static int event_count;

int up_inject_touch(int action, int n, const int *ids, const float *x, const float *y)
{
    assert(event_count < 16);
    recorded *r = &events[event_count++];
    r->action = action; r->n = n;
    for (int i = 0; i < n && i < 2; i++) {
        r->ids[i] = ids[i]; r->x[i] = x[i]; r->y[i] = y[i];
    }
    return 1;
}

static void feed(int type, int code, int value)
{
    granny_touch_feed_event(type, code, value);
}

int main(void)
{
    feed(EV_ABS, ABS_MT_SLOT, 0);
    feed(EV_ABS, ABS_MT_TRACKING_ID, 10);
    feed(EV_ABS, ABS_MT_POSITION_X, 100);
    feed(EV_ABS, ABS_MT_POSITION_Y, 200);
    feed(EV_SYN, SYN_REPORT, 0);
    assert(event_count == 1 && events[0].action == UP_ACTION_DOWN && events[0].n == 1);
    assert(events[0].ids[0] == 0 && events[0].x[0] == 100 && events[0].y[0] == 200);

    feed(EV_ABS, ABS_MT_SLOT, 1);
    feed(EV_ABS, ABS_MT_TRACKING_ID, 11);
    feed(EV_ABS, ABS_MT_POSITION_X, 500);
    feed(EV_ABS, ABS_MT_POSITION_Y, 300);
    feed(EV_SYN, SYN_REPORT, 0);
    assert(event_count == 2 && events[1].action == (UP_ACTION_POINTER_DOWN | (1 << 8)) && events[1].n == 2);

    feed(EV_ABS, ABS_MT_SLOT, 0);
    feed(EV_ABS, ABS_MT_POSITION_X, 110);
    feed(EV_ABS, ABS_MT_POSITION_Y, 210);
    feed(EV_ABS, ABS_MT_SLOT, 1);
    feed(EV_ABS, ABS_MT_POSITION_X, 490);
    feed(EV_ABS, ABS_MT_POSITION_Y, 310);
    feed(EV_SYN, SYN_REPORT, 0);
    assert(event_count == 3 && events[2].action == UP_ACTION_MOVE && events[2].n == 2);
    assert(events[2].x[0] == 110 && events[2].y[0] == 210);

    feed(EV_ABS, ABS_MT_SLOT, 1);
    feed(EV_ABS, ABS_MT_TRACKING_ID, -1);
    feed(EV_SYN, SYN_REPORT, 0);
    assert(event_count == 4 && events[3].action == (UP_ACTION_POINTER_UP | (1 << 8)) && events[3].n == 2);

    feed(EV_ABS, ABS_MT_SLOT, 0);
    feed(EV_ABS, ABS_MT_TRACKING_ID, -1);
    feed(EV_SYN, SYN_REPORT, 0);
    assert(event_count == 5 && events[4].action == UP_ACTION_UP && events[4].n == 1);

    puts("Granny evdev multitouch: DOWN, POINTER_DOWN, MOVE, POINTER_UP, UP passed");
    return 0;
}
