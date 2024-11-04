#ifndef CONFIG_H
#define CONFIG_H

#define NOTIFICATION_WIDTH NOTIFICATION_MIN_WIDTH
#define NOTIFICATION_HEIGHT NOTIFICATION_MIN_HEIGHT

/* appearance */
#define BORDER_WIDTH 2
#define PADDING 15
#define BACKGROUND_COLOR "#222222"        /* background color in hex */
#define FOREGROUND_COLOR "#bbbbbb"        /* text color in hex */
#define BORDER_COLOR "#005577"            /* border color in hex */
#define FONT "Liberation Mono 10"               /* font name and size */

/* behavior */
#define DURATION 3000                     /* notification display duration in ms */
#define FADE_TIME 200                     /* fade animation duration in ms */
#define MAX_NOTIFICATIONS 5               /* maximum number of notifications shown */
#define SPACING 10                        /* space between notifications */
#define NOTIFICATION_MIN_WIDTH 300        /* minimum width */
#define NOTIFICATION_MIN_HEIGHT 50        /* minimum height */  
#define NOTIFICATION_MAX_WIDTH 600        /* prevent notifications from getting too wide */

/* position (0 = top, 1 = bottom) */
#define POSITION 0
/* alignment (0 = left, 1 = center, 2 = right) */
#define ALIGNMENT 2

#endif /* CONFIG_H */
