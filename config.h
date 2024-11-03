#ifndef CONFIG_H
#define CONFIG_H

/* appearance */
#define BORDER_WIDTH 2
#define PADDING 15
#define BACKGROUND_COLOR "#222222"        /* background color in hex */
#define FOREGROUND_COLOR "#bbbbbb"        /* text color in hex */
#define BORDER_COLOR "#005577"            /* border color in hex */
#define FONT "monospace 10"               /* font name and size */

/* behavior */
#define DURATION 3000                     /* notification display duration in ms */
#define FADE_TIME 200                     /* fade animation duration in ms */
#define MAX_NOTIFICATIONS 5               /* maximum number of notifications shown */
#define NOTIFICATION_WIDTH 300            /* width of notification window */
#define NOTIFICATION_HEIGHT 100            /* height of notification window */
#define SPACING 10                        /* space between notifications */

/* position (0 = top, 1 = bottom) */
#define POSITION 0
/* alignment (0 = left, 1 = center, 2 = right) */
#define ALIGNMENT 2

#endif /* CONFIG_H */
