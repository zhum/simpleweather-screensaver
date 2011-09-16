/* simpleclock.c                                                       */
/* Copyright (C) 2010 Sebastian Brady <seb@cobwebs.id.au>              */

/* This program is free software; you can redistribute it and/or       */
/* modify it under the terms of the GNU General Public License as      */
/* published by the Free Software Foundation; either version 2 of the  */
/* License, or (at your option) any later version.                     */
/*                                                                     */
/* This program is distributed in the hope that it will be useful, but */
/* WITHOUT ANY WARRANTY; without even the implied warranty of          */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU   */
/* General Public License for more details.                            */
/*                                                                     */
/* You should have received a copy of the GNU General Public License   */
/* along with this program; if not, write to the Free Software         */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA           */
/* 02111-1307, USA.                                                    */

#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <sysexits.h>
#include <cairo.h>
#include <string.h>
#include "gs-theme-window.h"
#include "simpleclock.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>

#define MAX_STR_LEN 256

#define ALBUM_STRING "album"
#define TITLE_STRING "title"
static DBusHandlerResult
   signal_filter( DBusConnection *connection,
                 DBusMessage *message,
                 void *user_data);
static void
do_iter (DBusMessageIter *iter);

static char last_string[MAX_STR_LEN]="";
static int  varian_now=0;


/* commandline options */
static double   brightness  = 70.0;  /* [%]  */
//static char    *face_spec   = NULL;
static boolean  grayscale   = FALSE;
static boolean  mono        = FALSE;
static boolean  pin         = FALSE;
static double   size        = 45.0;  /* [%]  */
static boolean  wireframe   = FALSE;
static boolean  hide_index  = FALSE;
static boolean  hide_mark   = FALSE;
static boolean  hide_second = FALSE;
static double   speed       = 2;
static boolean  randomize   = FALSE;
static double   y_rand_factor=1;
static double   x_rand_factor=1;

char   charset[MAX_STR_LEN];

extern int WEATHER_IMAGE_H, WEATHER_IMAGE_W;
extern char *get_time_str(void);

#define SLEEP_TIME 100
#define MAX_TIME_STR 64

extern void draw_weather(state *st);
extern int MIN_TINT_HOUR;
extern int MAX_TINT_HOUR;
extern double WEATHER_FONT_SIZE;
extern double WEATHER_BIG_FONT_SIZE;
extern double WEATHER_CLOCK_FONT_SIZE;
extern int WEATHER_IMAGE_ALPHA;
extern int WEATHER_IMAGE_NOALPHA;
extern char FONT_FACE[];
extern char FONT_FACE_CLOCK[];
extern char WEATHER_AREA[];
extern char album[];
extern char title[];
extern int now_playing_changed;
extern int np_speed;

gchar *FONT_FACE_a=NULL;
gchar *FONT_FACE_CLOCK_a=NULL;
gchar *WEATHER_AREA_a=NULL;
gchar *face_spec_a=NULL;
gchar *charset_opt_a=NULL;


void mylog(char *s, ...);
int finalize_weather();

static GOptionEntry options[] = {
  {"start-hour",         'l', 0, G_OPTION_ARG_INT, &MIN_TINT_HOUR,
   "Hour, when light time is started (def 8)", NULL},
  {"end-hour",           'e', 0, G_OPTION_ARG_INT, &MAX_TINT_HOUR,
   "Hour, when light time is ended (def 22)",  NULL},
  {"now-font-size",      'F', 0, G_OPTION_ARG_DOUBLE,   &WEATHER_BIG_FONT_SIZE,
   "Now weather font size (def 35)",           NULL},
  {"forecast-font-size", 'f', 0, G_OPTION_ARG_DOUBLE,   &WEATHER_FONT_SIZE,
   "Weather forecast font size (def 25)",      NULL},
  {"clock-font-size",    'c', 0, G_OPTION_ARG_DOUBLE,   &WEATHER_CLOCK_FONT_SIZE,
   "Clock font size (def 50)",                 NULL},
  {"alpha",              'a', 0, G_OPTION_ARG_INT,   &WEATHER_IMAGE_ALPHA,
   "Tint alpha 0-255 (def 100)",               NULL},
  {"no-alpha",           'n', 0, G_OPTION_ARG_INT,   &WEATHER_IMAGE_NOALPHA,
   "No tint alpha 0-255 (def 255)",            NULL},
  {"font-face",          's', 0, G_OPTION_ARG_STRING,   &FONT_FACE_a,
   "Font face for weather and forecast (def Sans)",       NULL},
  {"clock-face",         'k', 0, G_OPTION_ARG_STRING,   &FONT_FACE_CLOCK_a,
   "Font face for clock (def Courier)",        NULL},
  {"area",               'a', 0, G_OPTION_ARG_STRING,   &WEATHER_AREA_a,
   "Weather area (def moscow,russia)",         NULL},
  {"speed",              'p', 0, G_OPTION_ARG_DOUBLE, &speed,
   "Moving speed (def 2)",                     NULL},
  {"randomize",          'z', 0, G_OPTION_ARG_NONE,   &randomize,
   "Randomize speed",                          NULL},
  {"face-color",         'r', 0, G_OPTION_ARG_STRING, &face_spec_a,
   "Color to use for text (default gray)",    "SPEC"},
  {"now-playing-speed",  'P', 0, G_OPTION_ARG_INT, &np_speed,
   "Marquee 'now playing' speed 1-10 (default 5)",    NULL},/*
  {"charset",            't', 0, G_OPTION_ARG_STRING,   &charset_opt_a,
   "Charset for weather service (def windows-1251)",       NULL},

  {"hide-index",  'I', 0, G_OPTION_ARG_NONE,   &hide_index,
   "Hide indexes",                                NULL},
  {"hide-mark",   'M', 0, G_OPTION_ARG_NONE,   &hide_mark,
   "Hide second marks",                           NULL},
  {"hide-second", 'S', 0, G_OPTION_ARG_NONE,   &hide_second,
   "Hide second hand",                            NULL},
  {"background",  'c', 0, G_OPTION_ARG_STRING, &bg_spec,
   "Color to use for background (default black)", "SPEC"},
*/
  {NULL}
};

static char *parameter_string = "";

/* type definitions */
enum HAND_TYPE {htSECOND, htMINUTE, htHOUR, htMAX};
enum HAND_VERTEX_TYPE {hvAPEX, hvRIGHT, hvBOTTOM, hvLEFT, hvMAX};

/* parameter settings */

/* ratio definitions [%] */
static const double mark_outer_ratio    = 100;
static const double mark_inner_ratio    =  96;
static const double index_outer_ratio   = 100;
static const double index_inner_ratio   =  90;
static const double hand_radius_ratios[htMAX][hvMAX] = {
  {95, 29, 30, 29}, /* Sec  */
  {92,  3, 15,  3}, /* Min  */
  {65,  4, 15,  4}  /* Hour */
};
static const double centre_circle_ratio  =  2;

/* angle definitions [degree] */
static const double index_arc_angle = 0.6;
static const double hand_vertex_angles[htMAX][hvMAX] = {
  {0, 178, 180, 182}, /* Sec  */
  {0,  54, 180, 306}, /* Min  */
  {0,  54, 180, 306}  /* Hour */
};

/* color range settings */
//static ushort high_color_default = 0xF000;
//static ushort mid_color_default  = 0xE000;
//static ushort low_color_default  = 0xD000;
//static ushort color_step_default = 0x0010;

/* function macros */
#define i_random(i) (g_random_int_range(0,(i)))
#define i_round(x)  ((int)(((double)x)+0.5))
#define d_sin(x)    (sin(2*M_PI*(x)/360))
#define d_cos(x)    (cos(2*M_PI*(x)/360))
#define min(x,y)    (((x)<(y))?(x):(y))

void draw_clock(state*);
void draw_weather(state*);
int init_weather(state *st);

#if 0
/*
 * Draw the minutes marks on the clock face.
 */
static void draw_minutes(state *st)
{
    int    i, j;
    double outer_radius, inner_radius;
    double theta;
    double sin_theta, cos_theta;
    int    x1, y1, x2, y2, x0, y0, rel_x1, rel_x2, rel_y1, rel_y2;

    outer_radius = st->radius * mark_outer_ratio / 100;
    inner_radius = st->radius * mark_inner_ratio / 100;

    // for (minute = 0; minute < 60; minute++) {
    for (i = 0; i < 90; i = i + 30) {
        for (j = 1; j < 5; j++) {
            theta = j * 6 + i;
            sin_theta = d_sin(theta);
            cos_theta = d_cos(theta);
            rel_x1 = outer_radius * sin_theta;
            rel_y1 = outer_radius * cos_theta;
            rel_x2 = inner_radius * sin_theta;
            rel_y2 = inner_radius * cos_theta;
            x1 = i_round(st->centre.x + outer_radius * sin_theta);
            y1 = i_round(st->centre.y - outer_radius * cos_theta);
            x2 = i_round(st->centre.x + inner_radius * sin_theta);
            y2 = i_round(st->centre.y - inner_radius * cos_theta);
            x0 = st->centre.x;
            y0 = st->centre.y;

            cairo_move_to(st->cr, i_round(x0 + rel_x1), i_round(y0 - rel_y1));
            cairo_line_to(st->cr, i_round(x0 + rel_x2), i_round(y0 - rel_y2));
            cairo_move_to(st->cr, i_round(x0 - rel_x1), i_round(y0 - rel_y1));
            cairo_line_to(st->cr, i_round(x0 - rel_x2), i_round(y0 - rel_y2));
            cairo_move_to(st->cr, i_round(x0 + rel_x1), i_round(y0 + rel_y1));
            cairo_line_to(st->cr, i_round(x0 + rel_x2), i_round(y0 + rel_y2));
            cairo_move_to(st->cr, i_round(x0 - rel_x1), i_round(y0 + rel_y1));
            cairo_line_to(st->cr, i_round(x0 - rel_x2), i_round(y0 + rel_y2));

            cairo_stroke(st->cr);
        }
    }
}


/*
 * Draw the hour marks on the clock face.
 */
static void draw_hours(state *st)
{
    int      hour;
    double   outer_radius, inner_radius, height, width, offset;
    double   rad;

    outer_radius = st->radius * index_outer_ratio / 100;
    inner_radius = st->radius * index_inner_ratio / 100;
    height = outer_radius - inner_radius;
    width = height / 4;
    offset = width / 2 * -1;
    cairo_save(st->cr);
    cairo_set_line_width(st->cr, 1);

    cairo_translate(st->cr, st->centre.x, st->centre.y);
    rad = ((2 * M_PI) / 12.0);
    for (hour = 0; hour < 3; hour++) {

        cairo_rotate(st->cr, rad);
        cairo_rectangle(st->cr, offset, outer_radius * -1, width, height);
        cairo_rectangle(st->cr, outer_radius, offset, height * -1, width);
        cairo_rectangle(st->cr, offset, outer_radius, width, height * -1);
        cairo_rectangle(st->cr, outer_radius * -1, offset, height, width);

        if (!st->wireframe) {

            cairo_fill_preserve(st->cr);
        }

        cairo_stroke(st->cr);
    }

    cairo_restore(st->cr);
}


/*
 * Draw a clock hand.
 */
static void draw_hand(state *st, enum HAND_TYPE ht, double theta)
/* theta [degree] */
{
    enum HAND_VERTEX_TYPE hv;
    double   hand_radius;
    GdkPoint hand_vertexes[hvMAX + 1];

    for (hv = 0; hv < hvMAX; hv++) {
        hand_radius = st->radius * hand_radius_ratios[ht][hv] / 100;
        hand_vertexes[hv].x =
            i_round (st->centre.x +
                     hand_radius *
                     d_sin (theta + hand_vertex_angles[ht][hv]));
        hand_vertexes[hv].y =
            i_round (st->centre.y -
                     hand_radius *
                     d_cos (theta + hand_vertex_angles[ht][hv]));
        cairo_line_to(st->cr, hand_vertexes[hv].x, hand_vertexes[hv].y );
    }

    hand_vertexes[hvMAX].x = hand_vertexes[0].x;
    hand_vertexes[hvMAX].y = hand_vertexes[0].y;
    cairo_line_to(st->cr, hand_vertexes[0].x, hand_vertexes[0].y);

    if (!st->wireframe) {

        set_colour(st, st->fill_colour);
        cairo_fill_preserve(st->cr);
        cairo_save(st->cr);
        set_colour(st, st->line_colour);
        cairo_set_line_width(st->cr, 1);
        cairo_stroke(st->cr);
        cairo_restore(st->cr);

    } else {

        cairo_stroke(st->cr);
    }
}




/*
 * Draws the centre circle in the clock face.
 */
static void draw_centre_circle (state *st)
{

    double circle_radius;
    int    x, y, w, h, r;

    circle_radius = st->radius * centre_circle_ratio / 100;
    x = i_round(st->centre.x);
    y = i_round(st->centre.y);
    w = h = i_round (circle_radius * 2);
    r = w / 2;

    cairo_arc(st->cr, x, y, circle_radius, 0, 2 * M_PI);
    if (!st->wireframe) {

        set_colour(st, st->fill_colour);
        cairo_fill_preserve (st->cr);
        set_colour(st, st->line_colour);
        cairo_stroke (st->cr);

    } else {

        set_colour(st, st->fill_colour);
        cairo_fill_preserve (st->cr);
        set_colour(st, st->line_colour);
        cairo_stroke (st->cr);
    }
}
#endif

static void draw_all(state *st){

    gtk_widget_set_size_request(st->drawing_area, st->width, st->height);
    st->cr = gdk_cairo_create(st->drawing_area->window);

    //draw_clock(st);
    draw_weather(st);
    cairo_destroy(st->cr);
}

/*
 * Draw the clock.
 */
/*
#define WEATHER_BIG_FONT_SIZE 35.0
void draw_clock(state *st)
{
  char timestring[MAX_TIME_STR];
  time_t t;
  struct tm *tmp;

  t = time(NULL);
  tmp = localtime(&t);
  if (tmp == NULL) {
    perror("localtime");
    exit(1);
  }

  strftime(timestring, MAX_TIME_STR, "%R",tmp);

  cairo_select_font_face (st->cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                          CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (st->cr, WEATHER_BIG_FONT_SIZE);
  cairo_set_source_rgb(st->cr, 1, 0, 0);
  cairo_move_to(st->cr, WEATHER_IMAGE_W*4, WEATHER_IMAGE_H*6);
  cairo_text_path(st->cr,timestring);
  cairo_stroke(st->cr);
}
*/
static void new_random_factors(void){
  if(randomize==FALSE)
    return;
  x_rand_factor=(double)rand()/RAND_MAX;
  y_rand_factor=(double)rand()/RAND_MAX;
}

static void move_clock (state *st)
{
  if (st->x <= 0){
    st->orient.x =  1;
    new_random_factors();
  }
  if (st->width <= st->x + st->weather_width){
    st->orient.x = -1;
    new_random_factors();
  }
  if (st->y <= 0){
    st->orient.y =  1;
    new_random_factors();
  }
  if (st->height <= st->y + st->weather_height){
    st->orient.y = -1;
    new_random_factors();
  }
  st->x += st->orient.x*speed*x_rand_factor;
  st->y += st->orient.y*speed*y_rand_factor;
}


/* Set the cairo context in state struct.
static void create_cairo_context (state *st)
{
    if (st->cr!=NULL)
        cairo_destroy(st->cr);

    st->cr = gdk_cairo_create(st->drawing_area->window);
    cairo_set_source_rgb(st->cr, 1, 0, 0);

    cairo_set_line_width (st->cr, 1);
}
*/

static void option_warning (char *opt, char *val, char *msg, char *newval)
{
    g_warning ("%s %s is %s: using %s.\n",
               opt, val, msg, newval);
}


static void correct_options (state *st)
{
  char    val[256], newval[256];
  char   *range_msg      = "out of range";
  char   *opt_brightness = "brightness";
  char   *opt_size       = "size";
  char   *fmt_brightness = "%.1f";
  char   *fmt_size       = "%.1f";
  double  min_brightness =  10.0;
  double  max_brightness = 100.0;
  double  min_size       =  10.0;
  double  max_size       =  99.0;

  sprintf (val, fmt_brightness, st->brightness);
  if (st->brightness < min_brightness) {
    sprintf (newval, fmt_brightness, min_brightness);
    option_warning (opt_brightness, val, range_msg, newval);
    st->brightness = min_brightness;
  }
  if (max_brightness < st->brightness) {
    sprintf (newval, fmt_brightness, max_brightness);
    option_warning (opt_brightness, val, range_msg, newval);
    st->brightness = max_brightness;
  }

  sprintf (val, fmt_size, st->size);
  if (st->size < min_size) {
    sprintf (newval, fmt_size, min_size);
    option_warning (opt_size, val, range_msg, newval);
    st->size = min_size;
  }
  if (max_size < st->size) {
    sprintf (newval, fmt_size, max_size);
    option_warning (opt_size, val, range_msg, newval);
    st->size = max_size;
  }
  if(np_speed<1 || np_speed>10)
    np_speed=6;
  np_speed = 11-np_speed;
}


static void *clock_init (GtkWidget *window, GtkWidget *drawing_area)
{
    char  *env;
    state *st = (state *) calloc (1, sizeof (*st));

    st->window = window;
    st->drawing_area = drawing_area;
    st->width  = st->window->allocation.width;
    st->height = st->window->allocation.height;

    st->brightness  = brightness;
    st->grayscale   = grayscale;
    st->mono        = mono;
    st->pin         = pin;
    st->size        = size;
    st->wireframe   = wireframe;
    st->hide_index  = hide_index;
    st->hide_mark   = hide_mark;
    st->hide_second = hide_second;
    correct_options(st);

    st->bg_colour.red   = 0;
    st->bg_colour.green = 0;
    st->bg_colour.blue  = 0;

    st->face_colour.red    = (guint16)(0xffff*0.85);
    st->face_colour.green  = (guint16)(0xffff*0.85);
    st->face_colour.blue   = (guint16)(0xffff*0.85);
    if (face_spec_a)
        if (!gdk_color_parse(face_spec_a, &st->face_colour))
            g_warning("Can't parse color %s: using black.", (char *)face_spec_a);

    if(charset_opt_a){
      strncpy(charset, (char *)charset_opt_a, MAX_BUF);
    }
    else{
      strcpy(charset, "windows-1251");
    }

    if(FONT_FACE_a!=NULL){
      strncpy(FONT_FACE,FONT_FACE_a,MAX_BUF);
    }
    if(FONT_FACE_CLOCK_a!=NULL){
      strncpy(FONT_FACE_CLOCK,FONT_FACE_CLOCK_a,MAX_BUF);
    }
    if(WEATHER_AREA_a!=NULL){
      strncpy(WEATHER_AREA,WEATHER_AREA_a,MAX_BUF);
    }
    else{
      env=getenv("SIMPLEWEATHER_LOCATION");
      if(env!=NULL){
        strncpy(WEATHER_AREA,env,MAX_BUF);
      }
    }

    st->marks_colour.red   = 0xffff;
    st->marks_colour.green = 0xffff;
    st->marks_colour.blue  = 0xffff;

    st->line_colour.red    = 0x0000;
    st->line_colour.green  = 0x0000;
    st->line_colour.blue   = 0x0000;

    st->fill_colour.red    = 0xFFFF;
    st->fill_colour.green  = 0xFFFF;
    st->fill_colour.blue   = 0xFFFF;
#if 0
    st->radius = min(st->width, st->height) / 2.0 * st->size / 100;
    if (st->pin) {
        st->centre.x = i_round(st->width  / 2.0);
        st->centre.y = i_round(st->height / 2.0);
    } else {
        st->centre.x = i_round(st->radius) +
            i_random (i_round(st->width  - st->radius * 2));
        st->centre.y = i_round(st->radius) +
            i_random (i_round(st->height - st->radius * 2));
    }
#endif
    st->orient.x = i_random(2) * 2 - 1;
    st->orient.y = i_random(2) * 2 - 1;

    return st;
}

/* Set up the widget for drawing. */
static void configure_event (GtkWidget *widget, GdkEventConfigure *event,
                             gpointer data)
{
    state *st = (state *) data;

    st->width  = widget->allocation.width;
    st->height = widget->allocation.height;
//    st->radius = min (st->width, st->height) / 2.0 * st->size / 100;

    if (st->pin) {
        st->x = i_round(st->width  / 2.0);
        st->y = i_round(st->height / 2.0);
    } else {
      st->x=st->y=0;
      /*
        if (st->x - st->radius < 0)
            st->x = i_round (st->radius);
        if (st->width < st->centre.x + st->radius)
            st->centre.x = st->width - i_round (st->radius);
        if (st->centre.y - st->radius < 0)
            st->centre.y = i_round (st->radius);
        if (st->height < st->centre.y + st->radius)
            st->centre.y = st->height - i_round (st->radius);
      */
    }
}


static void expose_event (GtkWidget *widget, GdkEventExpose *event,
                          gpointer data)
{
    state *st = (state *) data;

    if (!st->pin)
      move_clock (st);

    draw_all(st);
}


static int clock_timer (gpointer data)
{
    state *st = (state *) data;
    static boolean drawing = FALSE;
    struct timeval tv;

    if (!drawing) {
        drawing = TRUE;
        gtk_widget_draw(st->drawing_area, NULL);
        drawing = FALSE;
    }

    gettimeofday(&tv, NULL);

    st->timer_id = g_timeout_add(SLEEP_TIME, clock_timer, st);

    return FALSE;
}


static void clock_free (state *st)
{
    g_source_remove(st->timer_id);

    free(st);
}

static    GMainLoop *loop;
static    DBusConnection *bus;

static void quit_app(GtkWidget *widget, gpointer data)
{
  finalize_weather();
//  dbus_connection_close(bus);
  g_main_loop_quit(loop);
}

int main (int argc, char **argv)
{
    state     *st;
    GtkWidget *window;
    GtkWidget *drawing_area;

    GError    *error = NULL;

/*
    GMainLoop *loop;
    DBusConnection *bus;
*/
    DBusError dbus_error;

    gtk_set_locale();

    gtk_init_with_args(&argc, &argv,
                       parameter_string,
                       options,
                       "gnome-screensaver",
                       &error);

    if (error != NULL) {
        g_printerr ("%s. See --help for usage information.\n",
                  error->message);
        g_error_free (error);
        return EX_SOFTWARE;
    }

    get_time_str();
    window = gs_theme_window_new();
    drawing_area = gtk_drawing_area_new();
    st = clock_init(window, drawing_area);
    init_weather(st);

    gtk_widget_show(drawing_area);
    gtk_container_add(GTK_CONTAINER (window), drawing_area);

    gtk_widget_show(window);

/*
    g_signal_connect(G_OBJECT (window), "delete-event",
                     G_CALLBACK (gtk_main_quit), NULL);
*/
   loop = g_main_loop_new (NULL, FALSE);

    dbus_error_init (&dbus_error);
    bus = dbus_bus_get (DBUS_BUS_SESSION, &dbus_error);
    if (!bus) {
      g_warning ("Failed to connect to the D-BUS daemon: %s", dbus_error.message);
      dbus_error_free (&dbus_error);
      return 1;
    }
    dbus_connection_setup_with_g_main (bus, NULL);

    /* listening to messages from all objects as no path is specified */
    dbus_bus_add_match (bus, "type='signal',interface='org.freedesktop.MediaPlayer'", &dbus_error);
    dbus_connection_add_filter (bus, signal_filter, loop, NULL);


    g_signal_connect(G_OBJECT (window), "delete-event",
                     GTK_SIGNAL_FUNC(quit_app), loop);
    g_signal_connect(GTK_OBJECT (drawing_area), "configure_event",
                     GTK_SIGNAL_FUNC (configure_event), st);
    g_signal_connect(GTK_OBJECT (drawing_area), "expose_event",
                     GTK_SIGNAL_FUNC (expose_event), st);

    g_random_set_seed(time (NULL));

    st->timer_id = g_timeout_add_seconds(1, clock_timer, st);

    //gtk_main ();
    g_main_loop_run (loop);
//    dbus_connection_close(bus);

    clock_free (st);

    return EX_OK;
}

static DBusHandlerResult
signal_filter (DBusConnection *connection, DBusMessage *message, void *user_data)
{
  /* User data is the event loop we are running in */
  //GMainLoop *loop = user_data;

  /* A signal from the bus saying we are about to be disconnected */
  if (dbus_message_is_signal
        (message, DBUS_INTERFACE_LOCAL, "Disconnected")) {
    /* Tell the main loop to quit */
//    g_main_loop_quit (loop);
    /* We have handled this message, don't pass it on */
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  /* A Ping signal on the com.burtonini.dbus.Signal interface */
  else if (dbus_message_is_signal (message, "org.freedesktop.MediaPlayer", "TrackChange")) {
    DBusError error;
    DBusMessageIter iter;
    dbus_error_init (&error);

    dbus_message_iter_init (message, &iter);
    last_string[0]='\0';
    varian_now=0;
    do_iter(&iter);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
do_iter (DBusMessageIter *iter)
{
	do
		{
			int type = dbus_message_iter_get_arg_type (iter);
			const char *str;

			if (type == DBUS_TYPE_INVALID)
				break;

			switch (type)
			{
				case DBUS_TYPE_STRING:
					dbus_message_iter_get_basic (iter, &str);
          if(varian_now){
            mylog(">>>>>>> %s : '%s'\n",last_string,str);
            if(strcmp(ALBUM_STRING,last_string)==0){
              strncpy(album,str,MAX_STR_LEN);
              now_playing_changed=1;
            }
            else if(strcmp(TITLE_STRING,last_string)==0){
              strncpy(title,str,MAX_STR_LEN);
              now_playing_changed=1;
            }
          }
          strncpy(last_string,str,MAX_STR_LEN);
					break;
				case DBUS_TYPE_VARIANT:
				{
					DBusMessageIter subiter;
					dbus_message_iter_recurse (iter, &subiter);
//					printf ("variant:");
          varian_now=1;
					do_iter (&subiter);
					varian_now=0;
					break;
				}
				case DBUS_TYPE_ARRAY:
				{
					int current_type;
					DBusMessageIter subiter;

					dbus_message_iter_recurse (iter, &subiter);

//					printf("[");
					while ((current_type = dbus_message_iter_get_arg_type (&subiter))
						!= DBUS_TYPE_INVALID)
					{
						do_iter (&subiter);
						dbus_message_iter_next (&subiter);
//						if (dbus_message_iter_get_arg_type (&subiter) != DBUS_TYPE_INVALID)
//							printf (",");
					}
//					printf("]");
					break;
				}
				case DBUS_TYPE_DICT_ENTRY:
				{
					DBusMessageIter subiter;
					dbus_message_iter_recurse (iter, &subiter);
//					printf("{");
					do_iter (&subiter);
					dbus_message_iter_next (&subiter);
					do_iter (&subiter);
//					printf("}");
					break;
				}
				default:
					break;
				}

	} while (dbus_message_iter_next (iter));
}

