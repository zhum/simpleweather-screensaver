#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <sysexits.h>
#include <cairo.h>
#include <string.h>
#include "gs-theme-window.h"
#include "simpleclock.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

//#define DEBUG

#include <pthread.h>

#define SLEEP_TIME (60*15)

#define WEATHER_LINE_SIZE 0

#define WEATHER_IMAGE_CHECKERS_COLOR  0
#define MAX_TIME_STR 256
#define UNKNOWN_TEXT "?"

#define max(x,y) ((x>y)?(x):(y))

#define unknown_string "Unknown..."


static int def_weather_height=1024;
static int def_weather_width=768;
static char      *NULL_STR="";

//double FONT_R=0.85;
//double FONT_G=0.85;
//double FONT_B=0.85;
double WEATHER_FONT_SIZE=25.0;
double WEATHER_BIG_FONT_SIZE=35.0;
double WEATHER_CLOCK_FONT_SIZE=50.0;
int WEATHER_IMAGE_ALPHA=100;
int WEATHER_IMAGE_NOALPHA=255;
char FONT_FACE[MAX_BUF]="Sans";
char FONT_FACE_CLOCK[MAX_BUF]="Courier";
char WEATHER_AREA[MAX_BUF]="moscow,russia";

int np_speed=6;

int MIN_TINT_HOUR=6;
int MAX_TINT_HOUR=22;

void fill_weather(weather_t *);
int  load_weather(weather_t *);
void prepare_weather_image(state *st);
char *get_time_str(void);
int update_images(weather_t *);
void init_cache_path(void);
void create_unknown_image(GdkPixbuf *image);
void count_clock_height(state *st);
char *read_now_playing(void);

pthread_t weather_thread;
pthread_mutex_t WEATHER_LOCK;

int WEATHER_IMAGE_H=120;
int WEATHER_IMAGE_W=120;
struct tm *time_now;

int get_image_alpha(void){

return (time_now->tm_hour<=MIN_TINT_HOUR || time_now->tm_hour>=MAX_TINT_HOUR)?
        WEATHER_IMAGE_ALPHA:WEATHER_IMAGE_NOALPHA;
}


#ifdef DEBUG

void mylog(char *s, ...){
  va_list ap;
  FILE *f;
  char time_str[1024];

  f=fopen("/tmp/simple.log","a+");
  if(f==NULL)
    return;

  strftime(time_str, 1024, "%a %d %b %T ", time_now);
  fprintf(f,"%s",time_str);
  va_start(ap, s);
  vfprintf(f, s, ap);
  va_end(ap);
  fclose(f);
}
#else
void mylog(char *s, ...){}
#endif


struct weather_image_t weather_images_cache[LAST_CACHE_POS];

/*
 * Set the colour
 */
void set_colour(cairo_t *cr, GdkColor colour, double alpha)
{
    double red, green, blue;

    red = (double)colour.red / 0xFFFF;
    green = (double)colour.green / 0xFFFF;
    blue = (double)colour.blue / 0xFFFF;
    cairo_set_source_rgba(cr, red, green, blue, alpha);
}

void mysleep(long time){
  int i;
  /* sleep ... */
  for(i=0;i<time; ++i){
    pthread_testcancel();
    sleep(1);
    pthread_testcancel();
  }
}

/* weather update thread function */
static void * weather_update_func(void *vst){
    int wcode=NO_RETRY;
    state *st = (state *)vst;
    weather_t tmp_weather;
    char *sp;

    init_cache_path();
    for(;;){
      switch(wcode){

      case 0:
        mysleep(NORMAL_RETRY_TIME-RSS_RETRY_TIME);
      case RSS_RETRY:
        mysleep(RSS_RETRY_TIME);
      case NO_RETRY:

        /*get weather*/
        /* !!!!!!!!!!!!!!!!!!!!!! */
        tmp_weather.area = WEATHER_AREA;
        fill_weather(&tmp_weather);
        wcode=load_weather(&tmp_weather);
        mylog("UPDATE_WEATHER: %d\n",wcode);
        if(wcode==RSS_RETRY)
          break;

        pthread_mutex_lock(&WEATHER_LOCK);

        /* write new weather data */
        st->weather.now_weather_text=tmp_weather.now_weather_text;
        st->weather.today_forecast_text=tmp_weather.today_forecast_text;
        st->weather.tomorrow_forecast_text=tmp_weather.tomorrow_forecast_text;

        st->weather.now_image_index=tmp_weather.now_image_index;
        st->weather.today_image_index=tmp_weather.today_image_index;
        st->weather.tomorrow_image_index=tmp_weather.tomorrow_image_index;

        st->weather.now_celsium=tmp_weather.now_celsium;
        st->weather.today_celsium_high=tmp_weather.today_celsium_high;
        st->weather.tomorrow_celsium_high=tmp_weather.tomorrow_celsium_high;
        st->weather.today_celsium_low=tmp_weather.today_celsium_low;
        st->weather.tomorrow_celsium_low=tmp_weather.tomorrow_celsium_low;

        st->weather.now_weather_icon=tmp_weather.now_weather_icon;
        st->weather.today_forecast_icon=tmp_weather.today_forecast_icon;
        st->weather.tomorrow_forecast_icon=tmp_weather.tomorrow_forecast_icon;

        /* split lines */
        if(st->weather.now_weather_text1)
          free(st->weather.now_weather_text1);
        if(st->weather.today_forecast_text1)
          free(st->weather.today_forecast_text1);
        if(st->weather.tomorrow_forecast_text1)
          free(st->weather.tomorrow_forecast_text1);
        st->weather.now_weather_text1=strdup(tmp_weather.now_weather_text);
        st->weather.today_forecast_text1=strdup(tmp_weather.today_forecast_text);
        st->weather.tomorrow_forecast_text1=strdup(tmp_weather.tomorrow_forecast_text);
        sp=strstr(st->weather.now_weather_text1," ");
        if(sp){
          *sp='\0';
          st->weather.now_weather_text2=sp+1;
        }
        else{
          st->weather.now_weather_text2=NULL_STR;
        }
        sp=strstr(st->weather.today_forecast_text1," ");
        if(sp){
          *sp='\0';
          st->weather.today_forecast_text2=sp+1;
        }
        else{
          st->weather.today_forecast_text2=NULL_STR;
        }
        sp=strstr(st->weather.tomorrow_forecast_text1," ");
        if(sp){
          *sp='\0';
          st->weather.tomorrow_forecast_text2=sp+1;
        }
        else{
          st->weather.tomorrow_forecast_text2=NULL_STR;
        }

        prepare_weather_image(st);
        pthread_mutex_unlock(&WEATHER_LOCK);
        break;
      case IMAGE_RETRY:
        mysleep(IMAGE_RETRY_TIME);
        wcode=update_images(&tmp_weather);
        mylog("UPDATE_IMAGES: %d\n",wcode);
        break;
      }
    }
    return NULL;
}

char *new_str(char *s){
  char *ret=malloc(strlen(s)+1);
  strcpy(ret,s);
  return ret;
}

void fill_weather(weather_t *w){

  w->now_celsium=0;
  w->today_celsium_high=w->today_celsium_low=0;
  w->tomorrow_celsium_high=w->tomorrow_celsium_low=0;

  w->now_image_index=0;
  w->today_image_index=0;
  w->tomorrow_image_index=0;

  w->now_weather_icon=new_str("");
  w->today_forecast_icon=new_str("");
  w->tomorrow_forecast_icon=new_str("");

  w->now_weather_text=strdup(unknown_string);
  w->today_forecast_text=strdup(unknown_string);
  w->tomorrow_forecast_text=strdup(unknown_string);

  w->now_weather_text1=strdup(unknown_string);
  w->today_forecast_text1=strdup(unknown_string);
  w->tomorrow_forecast_text1=strdup(unknown_string);
  w->now_weather_text2=NULL_STR;
  w->today_forecast_text2=NULL_STR;
  w->tomorrow_forecast_text2=NULL_STR;
}

int init_weather(state *st){

    weather_images_cache[0].image=
      gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 64, 64);

    create_unknown_image(weather_images_cache[0].image);

    weather_images_cache[0].name = "unknown";

    /* start update thread */
    pthread_mutex_init(&WEATHER_LOCK,NULL);

    fill_weather(&(st->weather));

    st->x=0;
    st->y=0;
    st->weather_width=10;
    st->weather_height=WEATHER_IMAGE_H*5/2;

//    count_clock_height(st);
    st->weather_image=NULL;
    st->weather.area = WEATHER_AREA;

    prepare_weather_image(st);

//    execute_monitor();

    if (pthread_create(&weather_thread, NULL, weather_update_func, st) != 0)
    {
        return EXIT_FAILURE;
    }

    return 0;
}

int finalize_weather(){

    if (pthread_cancel(weather_thread) != 0)
    {
        return EXIT_FAILURE;
    }

    pthread_mutex_destroy(&WEATHER_LOCK);
    return 0;
}

void draw_weather(state *st){
    cairo_text_extents_t extents;
    char *time_str;
    char np[MAX_BUF],*np_read;
    int len;
    static int pos=0;
    static struct timeval tv_old={0,0},tv_now;

    pthread_mutex_lock(&WEATHER_LOCK);

    if(st->weather_image!=NULL){
      cairo_set_source_surface(st->cr, st->weather_image, st->x, st->y);
      cairo_paint(st->cr);
    }
    /* CLOCK */
    cairo_set_line_width (st->cr, WEATHER_LINE_SIZE);
    cairo_select_font_face (st->cr, FONT_FACE_CLOCK, CAIRO_FONT_SLANT_NORMAL,
                            CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (st->cr, WEATHER_CLOCK_FONT_SIZE);
    time_str=get_time_str();
    cairo_text_extents (st->cr, time_str, &extents);

    cairo_move_to (st->cr, st->x+(st->weather_width-extents.width)/2,
                           st->y+WEATHER_IMAGE_H*5/2);
    cairo_text_path (st->cr, time_str);

    /* now playing */
    gettimeofday(&tv_now,NULL);

    np_read=read_now_playing();
    strncpy(np,np_read,MAX_BUF-1);
    len=strlen(np);
    np[len]='|';
    strncpy(np+len+1,np_read,MAX_BUF-len-2);
    np[MAX_BUF-1]='\0';

    pos=(tv_now.tv_sec*10+(tv_now.tv_usec/100000))/np_speed % (len>0?len:1);

//    if(pos>=len)
//      pos=0;
    np[pos+len]='\0';

    cairo_move_to (st->cr, st->x+(st->weather_width-extents.width)/2,
                           st->y+WEATHER_IMAGE_H*5/2+extents.height);
    cairo_text_path (st->cr, np+pos);
/*
    if((tv_now.tv_sec>tv_old.tv_sec)){
      pos+=1;
      tv_old.tv_usec=tv_now.tv_usec;
      tv_old.tv_sec=tv_now.tv_sec;
    }
*/

    //cairo_set_source_rgba (st->cr, FONT_R, FONT_G, FONT_B, get_image_alpha());
    set_colour(st->cr, st->face_colour, (double)get_image_alpha()/255);
    cairo_fill_preserve (st->cr);
//    cairo_set_source_rgba (st->cr, LINE_RGB, FONT_ALPHA);
    cairo_stroke(st->cr);

    pthread_mutex_unlock(&WEATHER_LOCK);
}

void prepare_weather_image(state *st){
    int              w, h, x, y;
    GdkPixbuf        *image, *image1, *image2;
    GdkPixbuf        *scaled_image;
    cairo_text_extents_t extents;
    char w_now[MAX_BUF],w_today[MAX_BUF],w_tomorrow[MAX_BUF];
    cairo_surface_t  *sf0;
    cairo_t          *cr;
    int              big_text_h, small_text_h;

    sf0 = cairo_image_surface_create( CAIRO_FORMAT_ARGB32,
                                     def_weather_width, def_weather_height);

    sprintf(w_now,"%+3d %s", st->weather.now_celsium,
                                     st->weather.now_weather_text1);
    sprintf(w_today,"%+3d..%+3d %s", st->weather.today_celsium_low,
                                     st->weather.today_celsium_high,
                                     st->weather.today_forecast_text1);
    sprintf(w_tomorrow,"%+3d..%+3d %s", st->weather.tomorrow_celsium_low,
                                     st->weather.tomorrow_celsium_high,
                                     st->weather.tomorrow_forecast_text1);
/* image */

    image = weather_images_cache[st->weather.now_image_index].image;
    image1 = weather_images_cache[st->weather.today_image_index].image;
    image2 = weather_images_cache[st->weather.tomorrow_image_index].image;

    w = gdk_pixbuf_get_width(image);
    h = gdk_pixbuf_get_height(image);

    cr = cairo_create(sf0);

    scaled_image=gdk_pixbuf_composite_color_simple(image,
      WEATHER_IMAGE_W, WEATHER_IMAGE_H,
      GDK_INTERP_BILINEAR,
      get_image_alpha(),
      WEATHER_IMAGE_W, /* checker size */
      WEATHER_IMAGE_CHECKERS_COLOR, WEATHER_IMAGE_CHECKERS_COLOR);

    gdk_cairo_set_source_pixbuf (cr, scaled_image, 0, 0);

    cairo_paint (cr);

    cairo_destroy(cr);
//    cairo_restore(cr);
//    cairo_save(cr);

    x=0;
    y=WEATHER_IMAGE_H;

    cr = cairo_create(sf0);
//    cairo_scale(cr,x_scale/2,y_scale/2);
/*    cairo_set_line_width(cr, 2);*/
/*    cairo_set_source_rgb(cr,1,0,0);*/
/*    cairo_rectangle(cr,0,0,w,h);*/
/*    cairo_stroke(cr);*/
    scaled_image=gdk_pixbuf_composite_color_simple(image1,
      WEATHER_IMAGE_W/2, WEATHER_IMAGE_H/2,
      GDK_INTERP_BILINEAR,
      get_image_alpha(),
      WEATHER_IMAGE_W, /* checker size */
      WEATHER_IMAGE_CHECKERS_COLOR, WEATHER_IMAGE_CHECKERS_COLOR);

    gdk_cairo_set_source_pixbuf (cr, scaled_image, x, y);

//    gdk_cairo_set_source_pixbuf (cr, image1, 0,y);

    cairo_paint (cr);

    cairo_destroy(cr);
    cr = cairo_create(sf0);

    y=WEATHER_IMAGE_H*3/2;
    scaled_image=gdk_pixbuf_composite_color_simple(image2,
      WEATHER_IMAGE_W/2, WEATHER_IMAGE_H/2,
      GDK_INTERP_BILINEAR,
      get_image_alpha(),
      WEATHER_IMAGE_W, /* checker size */
      WEATHER_IMAGE_CHECKERS_COLOR, WEATHER_IMAGE_CHECKERS_COLOR);

    gdk_cairo_set_source_pixbuf (cr, scaled_image, x, y);

    cairo_paint (cr);

    cairo_destroy(cr);
    cr = cairo_create(sf0);

/* text */

    cairo_set_line_width (cr, WEATHER_LINE_SIZE);
    cairo_select_font_face (cr, FONT_FACE, CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, WEATHER_BIG_FONT_SIZE);

    /* get max width */
    cairo_text_extents (cr, w_now, &extents);
    st->weather_width=extents.width;
    big_text_h = extents.height;
    cairo_text_extents (cr, st->weather.now_weather_text2, &extents);
    st->weather_width=max(st->weather_width,extents.width);

//    x = w/2-(extents.width/2 + extents.x_bearing);
//    y = h/2-(extents.height/2 + extents.y_bearing);


    cairo_move_to (cr, WEATHER_IMAGE_W, WEATHER_IMAGE_H/3);
    cairo_text_path (cr, w_now);
    cairo_move_to (cr, WEATHER_IMAGE_W, WEATHER_IMAGE_H/3+big_text_h);
    cairo_text_path (cr, st->weather.now_weather_text2);

    cairo_set_font_size (cr, WEATHER_FONT_SIZE);
    cairo_text_extents (cr, w_today, &extents);
    st->weather_width=max(st->weather_width,extents.width);
    small_text_h = extents.height;
    cairo_text_extents (cr, st->weather.today_forecast_text2, &extents);
    st->weather_width=max(st->weather_width,extents.width);
    cairo_text_extents (cr, w_tomorrow, &extents);
    st->weather_width=max(st->weather_width,extents.width);
    cairo_text_extents (cr, st->weather.tomorrow_forecast_text2, &extents);
    st->weather_width=max(st->weather_width,extents.width);

    cairo_move_to (cr, WEATHER_IMAGE_W, WEATHER_IMAGE_H*5/4);
    cairo_text_path (cr, w_today);
    cairo_move_to (cr, WEATHER_IMAGE_W, WEATHER_IMAGE_H*5/4+small_text_h);
    cairo_text_path (cr, st->weather.today_forecast_text2);

    cairo_move_to (cr, WEATHER_IMAGE_W, WEATHER_IMAGE_H*7/4);
    cairo_text_path (cr, w_tomorrow);
    cairo_move_to (cr, WEATHER_IMAGE_W, WEATHER_IMAGE_H*7/4+small_text_h);
    cairo_text_path (cr, st->weather.tomorrow_forecast_text2);

    //cairo_set_source_rgba (cr, FONT_R, FONT_G, FONT_B, get_image_alpha());
    set_colour(cr, st->face_colour, (double)get_image_alpha()/255);
    cairo_fill_preserve (cr);

    cairo_stroke (cr);

    cairo_destroy(cr);

    if(st->weather_image!=NULL)
      cairo_surface_destroy(st->weather_image);

    st->weather_image=sf0;

    st->weather_width+=WEATHER_IMAGE_W;

}

void count_clock_height(state *st){
    cairo_text_extents_t extents;
    cairo_surface_t  *sf0;
    cairo_t          *cr;

    sf0 = cairo_image_surface_create( CAIRO_FORMAT_ARGB32,
                                     def_weather_width, def_weather_height);
    cr=cairo_create(sf0);
    cairo_set_font_size (cr, WEATHER_CLOCK_FONT_SIZE);
    cairo_text_extents (cr, get_time_str(), &extents);
    st->weather_height=WEATHER_IMAGE_H*2+extents.height;
    cairo_destroy(cr);
}

char *get_time_str(void)
{
  static char timestring[MAX_TIME_STR];
  time_t t;

  t = time(NULL);
  time_now = localtime(&t);
  if (time_now == NULL) {
    perror("localtime");
    exit(1);
  }

  if(t%2==0){
    strftime(timestring, MAX_TIME_STR, "%a %d %b %H:%M",time_now);
  }
  else{
    strftime(timestring, MAX_TIME_STR, "%a %d %b %H %M",time_now);
  }
  return timestring;
}

static const cairo_user_data_key_t pixbuf_key;

/**
* pixbuf_cairo_create:
* @pixbuf: GdkPixbuf that you wish to wrap with cairo context
*
* This function will initialize new cairo context with contents of @pixbuf. You
* can then draw using returned context. When finished drawing, you must call
* pixbuf_cairo_destroy() or your pixbuf will not be updated with new contents!
*
* Return value: New cairo_t context. When you're done with it, call
* pixbuf_cairo_destroy() to update your pixbuf and free memory.
*/
static cairo_t *
pixbuf_cairo_create( GdkPixbuf *pixbuf )
{
   gint             width,        /* Width of both pixbuf and surface */
                height,       /* Height of both pixbuf and surface */
                p_stride,     /* Pixbuf stride value */
                p_n_channels, /* RGB -> 3, RGBA -> 4 */
                s_stride;     /* Surface stride value */
   guchar          *p_pixels,     /* Pixbuf's pixel data */
               *s_pixels;     /* Surface's pixel data */
   cairo_surface_t *surface;      /* Temporary image surface */
   cairo_t         *cr;           /* Final context */

   g_object_ref( G_OBJECT( pixbuf ) );

   /* Inspect input pixbuf and create compatible cairo surface */
   g_object_get( G_OBJECT( pixbuf ), "width",           &width,
                             "height",          &height,
                             "rowstride",       &p_stride,
                             "n-channels",      &p_n_channels,
                             "pixels",          &p_pixels,
                             NULL );
   surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, width, height );
   s_stride = cairo_image_surface_get_stride( surface );
   s_pixels = cairo_image_surface_get_data( surface );

   /* Copy pixel data from pixbuf to surface */
   while( height-- )
   {
      gint    i;
      guchar *p_iter = p_pixels,
            *s_iter = s_pixels;

      for( i = 0; i < width; i++ )
      {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
         /* Pixbuf:  RGB(A)
          * Surface: BGRA */
         if( p_n_channels == 3 )
         {
            s_iter[0] = p_iter[2];
            s_iter[1] = p_iter[1];
            s_iter[2] = p_iter[0];
            s_iter[3] = 0xff;
         }
         else /* p_n_channels == 4 */
         {
            gdouble alpha_factor = p_iter[3] / (gdouble)0xff;

            s_iter[0] = (guchar)( p_iter[2] * alpha_factor + .5 );
            s_iter[1] = (guchar)( p_iter[1] * alpha_factor + .5 );
            s_iter[2] = (guchar)( p_iter[0] * alpha_factor + .5 );
            s_iter[3] =           p_iter[3];
         }
#elif G_BYTE_ORDER == G_BIG_ENDIAN
         /* Pixbuf:  RGB(A)
          * Surface: ARGB */
         if( p_n_channels == 3 )
         {
            s_iter[3] = p_iter[2];
            s_iter[2] = p_iter[1];
            s_iter[1] = p_iter[0];
            s_iter[0] = 0xff;
         }
         else /* p_n_channels == 4 */
         {
            gdouble alpha_factor = p_iter[3] / (gdouble)0xff;

            s_iter[3] = (guchar)( p_iter[2] * alpha_factor + .5 );
            s_iter[2] = (guchar)( p_iter[1] * alpha_factor + .5 );
            s_iter[1] = (guchar)( p_iter[0] * alpha_factor + .5 );
            s_iter[0] =           p_iter[3];
         }
#else /* PDP endianness */
         /* Pixbuf:  RGB(A)
          * Surface: RABG */
         if( p_n_channels == 3 )
         {
            s_iter[0] = p_iter[0];
            s_iter[1] = 0xff;
            s_iter[2] = p_iter[2];
            s_iter[3] = p_iter[1];
         }
         else /* p_n_channels == 4 */
         {
            gdouble alpha_factor = p_iter[3] / (gdouble)0xff;

            s_iter[0] = (guchar)( p_iter[0] * alpha_factor + .5 );
            s_iter[1] =           p_iter[3];
            s_iter[1] = (guchar)( p_iter[2] * alpha_factor + .5 );
            s_iter[2] = (guchar)( p_iter[1] * alpha_factor + .5 );
         }
#endif
         s_iter += 4;
         p_iter += p_n_channels;
      }
      s_pixels += s_stride;
      p_pixels += p_stride;
   }

   /* Create context and set user data */
   cr = cairo_create( surface );
   cairo_surface_destroy( surface );
   cairo_set_user_data( cr, &pixbuf_key, pixbuf, g_object_unref );

   /* Return context */
   return( cr );
}

/**
* pixbuf_cairo_destroy:
* @cr: Cairo context that you wish to destroy
* @create_new_pixbuf: If TRUE, new pixbuf will be created and returned. If
*                     FALSE, input pixbuf will be updated in place.
*
* This function will destroy cairo context, created with pixbuf_cairo_create().
*
* Return value: New or updated GdkPixbuf. You own a new reference on return
* value, so you need to call g_object_unref() on returned pixbuf when you don't
* need it anymore.
*/
static GdkPixbuf *
pixbuf_cairo_destroy( cairo_t  *cr,
                    gboolean  create_new_pixbuf )
{
   gint             width,        /* Width of both pixbuf and surface */
                height,       /* Height of both pixbuf and surface */
                p_stride,     /* Pixbuf stride value */
                p_n_channels, /* RGB -> 3, RGBA -> 4 */
                s_stride;     /* Surface stride value */
   guchar          *p_pixels,     /* Pixbuf's pixel data */
               *s_pixels;     /* Surface's pixel data */
   cairo_surface_t *surface;      /* Temporary image surface */
   GdkPixbuf       *pixbuf,       /* Pixbuf to be returned */
               *tmp_pix;      /* Temporary storage */

   /* Obtain pixbuf to be returned */
   tmp_pix = cairo_get_user_data( cr, &pixbuf_key );
   if( create_new_pixbuf )
      pixbuf = gdk_pixbuf_copy( tmp_pix );
   else
      pixbuf = g_object_ref( G_OBJECT( tmp_pix ) );

   /* Obtain surface from where pixel values will be copied */
   surface = cairo_get_target( cr );

   /* Inspect pixbuf and surface */
   g_object_get( G_OBJECT( pixbuf ), "width",           &width,
                             "height",          &height,
                             "rowstride",       &p_stride,
                             "n-channels",      &p_n_channels,
                             "pixels",          &p_pixels,
                             NULL );
   s_stride = cairo_image_surface_get_stride( surface );
   s_pixels = cairo_image_surface_get_data( surface );

   /* Copy pixel data from surface to pixbuf */
   while( height-- )
   {
      gint    i;
      guchar *p_iter = p_pixels,
            *s_iter = s_pixels;

      for( i = 0; i < width; i++ )
      {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
         /* Pixbuf:  RGB(A)
          * Surface: BGRA */
         gdouble alpha_factor = (gdouble)0xff / s_iter[3];

         p_iter[0] = (guchar)( s_iter[2] * alpha_factor + .5 );
         p_iter[1] = (guchar)( s_iter[1] * alpha_factor + .5 );
         p_iter[2] = (guchar)( s_iter[0] * alpha_factor + .5 );
         if( p_n_channels == 4 )
            p_iter[3] = s_iter[3];
#elif G_BYTE_ORDER == G_BIG_ENDIAN
         /* Pixbuf:  RGB(A)
          * Surface: ARGB */
         gdouble alpha_factor = (gdouble)0xff / s_iter[0];

         p_iter[0] = (guchar)( s_iter[1] * alpha_factor + .5 );
         p_iter[1] = (guchar)( s_iter[2] * alpha_factor + .5 );
         p_iter[2] = (guchar)( s_iter[3] * alpha_factor + .5 );
         if( p_n_channels == 4 )
            p_iter[3] = s_iter[0];
#else /* PDP endianness */
         /* Pixbuf:  RGB(A)
          * Surface: RABG */
         gdouble alpha_factor = (gdouble)0xff / s_iter[1];

         p_iter[0] = (guchar)( s_iter[0] * alpha_factor + .5 );
         p_iter[1] = (guchar)( s_iter[3] * alpha_factor + .5 );
         p_iter[2] = (guchar)( s_iter[2] * alpha_factor + .5 );
         if( p_n_channels == 4 )
            p_iter[3] = s_iter[1];
#endif
         s_iter += 4;
         p_iter += p_n_channels;
      }
      s_pixels += s_stride;
      p_pixels += p_stride;
   }

   /* Destroy context */
   cairo_destroy( cr );

   /* Return pixbuf */
   return( pixbuf );
}


void create_unknown_image(GdkPixbuf *image){
  cairo_t          *cr;
  cairo_text_extents_t extents;
  int w,h,x,y;

  w = gdk_pixbuf_get_width(image);
  h = gdk_pixbuf_get_height(image);

  cr = pixbuf_cairo_create(image);
  cairo_set_source_rgb (cr, 0, 0, 0);

  cairo_rectangle(cr, 0,0,w,h);

  cairo_fill(cr);

  cairo_set_source_rgb (cr, 1,1,1);
  cairo_select_font_face (cr, "Seif",
    CAIRO_FONT_SLANT_NORMAL,
    CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, max(w,h));
  cairo_text_extents (cr, UNKNOWN_TEXT, &extents);
  x = w/2-(extents.width/2 + extents.x_bearing);
  y = h/2-(extents.height/2 + extents.y_bearing);

  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_move_to (cr, x, y);
  cairo_show_text (cr, UNKNOWN_TEXT);
  image=pixbuf_cairo_destroy(cr, FALSE);

}

