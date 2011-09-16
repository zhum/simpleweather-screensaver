#ifndef SIMPLECLOCK_H
#define SIMPLECLOCK_H

typedef int               boolean;
typedef unsigned short    ushort;
typedef unsigned long     ulong;

typedef struct {
  int x, y;
} XY;

#define LAST_CACHE_POS 128
#define RSS_RETRY_TIME 15
#define IMAGE_RETRY_TIME 5
#define NORMAL_RETRY_TIME 3600

#define RSS_RETRY 1
#define IMAGE_RETRY 2
#define NO_RETRY 3

#define MAX_BUF 512

struct  weather_image_t {
    GdkPixbuf       *image;
    char            *name;
};


typedef struct {
    char *now_weather_text;
    char *today_forecast_text;
    char *tomorrow_forecast_text;

    char *now_weather_text1;
    char *today_forecast_text1;
    char *tomorrow_forecast_text1;
    char *now_weather_text2;
    char *today_forecast_text2;
    char *tomorrow_forecast_text2;

    int  now_image_index;
    int  today_image_index;
    int  tomorrow_image_index;

    int  now_celsium;
    int  today_celsium_high;
    int  today_celsium_low;
    int  tomorrow_celsium_high;
    int  tomorrow_celsium_low;

    char *now_weather_icon;
    char *today_forecast_icon;
    char *tomorrow_forecast_icon;

    char *area;

} weather_t;

typedef struct {
    GtkWidget *window;
    GtkWidget *drawing_area;
    GdkPixmap *pixmap;
    GdkColor   bg_colour, face_colour, marks_colour, fill_colour, line_colour;
    cairo_t   *cr;
    int        width, height;
    /*double     radius;
    XY         centre;*/
    int        x;
    int        y;
    XY         orient;
    int        timer_id;
    weather_t  weather;
    /*int        weather_x;
    int        weather_y;
    */
    int        weather_width;
    int        weather_height;

    /* commandline options */
    double     brightness;
    boolean    grayscale;
    boolean    mono;
    boolean    pin;
    double     size;
    boolean    wireframe;
    boolean    hide_index;
    boolean    hide_mark;
    boolean    hide_second;
    cairo_surface_t *weather_image;

} state;


#endif

