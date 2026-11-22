#ifndef UA_GRANNY_TOUCH_H
#define UA_GRANNY_TOUCH_H

/* Optional bridge for the Granny DualTouch mapper's named Linux evdev device. */
void granny_touch_init(int width, int height);
void granny_touch_poll(int enabled);

/* Exposed for the small host-side state-machine test. */
void granny_touch_feed_event(int type, int code, int value);

#endif
