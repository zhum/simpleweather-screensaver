#include <stdlib.h>
#include "gs-theme-window.h"

#include "simpleclock.h"
#include <curl/curl.h>
#include <string.h>
#include <iconv.h>
#include <sys/stat.h>
#include <ctype.h>

#define BUFSIZE (1024*10)

#define DEBUG 1

#define BASE_URL "http://www.google.com"
#define BASE_LEN 22
//#define MAX_BUF 1024


char cache_path[MAX_BUF]="/tmp/";

char wbuffer[BUFSIZE];
int last_cache_index=1;

extern struct weather_image_t weather_images_cache[];
extern char   charset[];

static size_t write_weather(void *, size_t, size_t, void *);
int  parse_weather_google(weather_t *w, char *buffer);
void print_weather(weather_t *w);
FILE *load_icon_by_name(char *name);
char *image_url(char *name);
int get_cached_icon_index(char *name);
int cache_icon_by_name(char *name);
int append_to_image_cache_by_path_and_name(char *path, char *name);
int get_icon_index(char *name);
void mylog(char *,...);
size_t header_parse( void *ptr, size_t size, size_t nmemb, void *userdata);

int load_weather(weather_t *weather){
  CURL *curl;
  CURLcode res;
  struct curl_slist *slist=NULL;
  static char url[1024]="http://www.google.com/ig/api?weather=";
  static char charset_h[1024]="";
  char *lang,*lang2;
  int i;

  if(charset_h[0]=='\0'){
    strcpy(charset_h,"Accept-Language: ");
    lang=getenv("LANG");
    lang2=lang;
    if(lang){
      for(i=0;lang[i]!='\0';++i){
        lang[i]=tolower(lang[i]);
        if(lang[i]=='_'){
          lang[i]='-';
          lang2=lang+i+1;
        }
        if(lang[i]=='.'){
          lang[i]='\0';
          break;
        }
      }
//      charset_h[17]=lang2[0];
//      charset_h[18]=lang2[1];
//      charset_h[19]=',';
      strcat(charset_h,lang);
      strcat(charset_h,",");
      strcat(charset_h,lang2);
    }
    else{
      strcpy(charset_h+16,"en");
    }
  }

  /* create url for weather request */
  strncpy(url+37,weather->area,1024-37);

  curl = curl_easy_init();
  if(curl) {
    wbuffer[0]='\0';
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_weather);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, weather);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &header_parse);

//    slist = curl_slist_append(slist, "Accept-Language: ru-ru,ru");
    slist = curl_slist_append(slist, charset_h);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);

    mylog("HEADER: %s\n",charset_h);

    res = curl_easy_perform(curl);
    /* always cleanup */
    curl_slist_free_all(slist); /* free the list again */
    curl_easy_cleanup(curl);

    if(res==0){ // all ok
      return parse_weather_google(weather,wbuffer);
    }
  }
  return RSS_RETRY;
}

int update_images(weather_t *w){
  return parse_weather_google(w,wbuffer);
}

static size_t write_weather(void *buffer, size_t size, size_t nmemb,
                            void *weather){
    int wlen=strlen(wbuffer);
    int newlen=wlen+size*nmemb;
    if(newlen>=BUFSIZE-1){
        newlen=BUFSIZE-1;
    }
    strncpy(wbuffer+wlen, buffer, newlen-wlen);
    wbuffer[newlen]='\0';
    return size*nmemb;
}

static size_t write_icon(void *buffer, size_t size, size_t nmemb, void *f){

  int written = fwrite(buffer,size, nmemb, (FILE *)f);
  return written;
}

/* search charset... */
size_t header_parse( void *ptr, size_t size, size_t nmemb, void *userdata){
  char *p;
  if(strncasecmp(ptr,"content-type:",size*nmemb)==0){
    if((p=strcasestr(ptr,";charset="))!=NULL){
      p+=9;
      strncpy(charset,p,size*nmemb-(p-(char *)ptr));
      charset[size*nmemb-(p-(char *)ptr)]='\0';
      mylog("CHARSET: %s\n",charset);
    }
  }
  return size*nmemb;
}
/*
    char *now_weather_text;
    char *today_forecast_text;
    char *tomorrow_forecast_text;

    char *now_weather_icon;
    char *today_forecast_icon;
    char *tomorrow_forecast_icon;

    int  now_image_index;
    int  today_image_index;
    int  tomorrow_image_index;

    int  now_celsium;
    int  today_celsium_high;
    int  tomorrow_celsium_high;
*/
int parse_weather_google(weather_t *w, char *buffer_orig){
  const char *section, *p,*p2;
  iconv_t cd;
  size_t insize,outsize;
  char buffer[MAX_BUF*4]="",*b1,*b2;

  cd = iconv_open ("UTF-8",charset);
  if( cd == (iconv_t)-1){
    /* failed to convert! */
    return RSS_RETRY; /*!!!!!!!!!!!!!!!!!!!!!!!!*/
  }

  /* covert! */
  insize=strlen(buffer_orig);
  outsize=MAX_BUF*4;
  b1=buffer_orig;
  b2=buffer;
  iconv(cd, NULL,NULL,
            &b2,&outsize);
  iconv(cd, &b1, &insize,
            &b2,&outsize);
  iconv_close(cd);
  mylog("GOOGLE: %s\n\n",buffer);

  if(NULL!=(section=strstr(buffer,"<current_conditions>"))){
    /* found conditions section */
    if(NULL!=(p=strstr(section,"<condition "))){

        p=strstr(p,"\"")+1;
        p2=strstr(p,"\"");
        if((p!=NULL) &&(p2!=NULL)){
          if(w->now_weather_text==NULL)
            {w->now_weather_text=malloc(1);w->now_weather_text[0]='\0';}
          if(strncmp(w->now_weather_text,p,p2-p)){
            if(w->now_weather_text)
                free(w->now_weather_text);
            w->now_weather_text=malloc(p2-p+1);
            strncpy(w->now_weather_text,p,p2-p);
            w->now_weather_text[p2-p]='\0';
          }
        }
        else{ return RSS_RETRY;}
    }
    else{ return RSS_RETRY;}

    if(NULL!=(p=strstr(section,"<icon "))){

        p=strstr(p,"\"")+1;
        p2=strstr(p,"\"");
        if((p!=NULL) &&(p2!=NULL)){
          if(w->now_weather_icon==NULL)
            {w->now_weather_icon=malloc(1);w->now_weather_icon[0]='\0';}
          if(strncmp(w->now_weather_icon,p,p2-p)){
            if(w->now_weather_icon)
              free(w->now_weather_icon);
            w->now_weather_icon=malloc(p2-p+1);
            strncpy(w->now_weather_icon,p,p2-p);
            w->now_weather_icon[p2-p]='\0';
            w->now_image_index=get_icon_index(w->now_weather_icon);
          }
        }
        else{ return RSS_RETRY;}
   }
   else{ return RSS_RETRY;}

    if(NULL!=(p=strstr(section,"<temp_c"))){

        p=strstr(p,"\"")+1;
        p2=strstr(p,"\"");
        sscanf(p,"%d",&(w->now_celsium));
    }
    else{ return RSS_RETRY;}
  } /* current cond */

  if(NULL!=(section=strstr(buffer,"<forecast_conditions>"))){
    /* found first forecast section */
    if(NULL!=(p=strstr(section,"<condition data"))){

      p=strstr(p,"\"")+1;
      p2=strstr(p,"\"");
      if((p!=NULL) &&(p2!=NULL)){
        if(w->today_forecast_text==NULL)
          {w->today_forecast_text=malloc(1);w->today_forecast_text[0]='\0';}
        if(strncmp(w->today_forecast_text,p,p2-p)){
          if(w->today_forecast_text)
              free(w->today_forecast_text);
          w->today_forecast_text=malloc(p2-p+1);
          strncpy(w->today_forecast_text,p,p2-p);
          w->today_forecast_text[p2-p]='\0';
        }
      }
      else{ return RSS_RETRY;}
    }
    else{ return RSS_RETRY;}

    if(NULL!=(p=strstr(section,"<icon "))){

      p=strstr(p,"\"")+1;
      p2=strstr(p,"\"");
      if((p!=NULL) &&(p2!=NULL)){
        if(w->today_forecast_icon==NULL)
          {w->today_forecast_icon=malloc(1);w->today_forecast_icon[0]='\0';}
        if(strncmp(w->today_forecast_icon,p,p2-p)){
          if(w->today_forecast_icon)
              free(w->today_forecast_icon);
          w->today_forecast_icon=malloc(p2-p+1);
          strncpy(w->today_forecast_icon,p,p2-p);
          w->today_forecast_icon[p2-p]='\0';
          w->today_image_index=get_icon_index(w->today_forecast_icon);
        }
      }
      else{ return RSS_RETRY;}
    }
    else{ return RSS_RETRY;}

    if(NULL!=(p=strstr(section,"<high "))){

        p=strstr(p,"\"")+1;
        sscanf(p,"%d",&(w->today_celsium_high));
    }
    else{ return RSS_RETRY;}
    if(NULL!=(p=strstr(section,"<low "))){

        p=strstr(p,"\"")+1;
        sscanf(p,"%d",&(w->today_celsium_low));
    }
    else{ return RSS_RETRY;}
  } /* today forecast */

  if(NULL!=(section=strstr(p+1,"<day_of_week"))){
    /* found second forecast section */
    if(NULL!=(p=strstr(section,"<condition data"))){

        p=strstr(p,"\"")+1;
        p2=strstr(p,"\"");
      if((p!=NULL) &&(p2!=NULL)){
        if(w->tomorrow_forecast_text==NULL){
          w->tomorrow_forecast_text=malloc(1);
          w->tomorrow_forecast_text[0]='\0';
        }
        if(strncmp(w->tomorrow_forecast_text,p,p2-p)){
          if(w->tomorrow_forecast_text)
              free(w->tomorrow_forecast_text);
          w->tomorrow_forecast_text=malloc(p2-p+1);
          strncpy(w->tomorrow_forecast_text,p,p2-p);
          w->tomorrow_forecast_text[p2-p]='\0';
        }
      }
      else{ return RSS_RETRY;}
    }
    else{ return RSS_RETRY;}

    if(NULL!=(p=strstr(section,"<icon "))){

        p=strstr(p,"\"")+1;
        p2=strstr(p,"\"");
      if((p!=NULL) &&(p2!=NULL)){
        if(w->tomorrow_forecast_icon==NULL){
          w->tomorrow_forecast_icon=malloc(1);
          w->tomorrow_forecast_icon[0]='\0';
        }
        if(strncmp(w->tomorrow_forecast_icon,p,p2-p)){
          if(w->tomorrow_forecast_icon)
              free(w->tomorrow_forecast_icon);
          w->tomorrow_forecast_icon=malloc(p2-p+1);
          strncpy(w->tomorrow_forecast_icon,p,p2-p);
          w->tomorrow_forecast_icon[p2-p]='\0';
          w->tomorrow_image_index=get_icon_index(w->tomorrow_forecast_icon);
        }
      }
      else{ return RSS_RETRY;}
    }
    else{ return RSS_RETRY;}

    if(NULL!=(p=strstr(section,"<high "))){

        p=strstr(p,"\"");
        sscanf(p+1,"%d",&(w->tomorrow_celsium_high));
    }
    else{ return RSS_RETRY;}

    if(NULL!=(p=strstr(section,"<low "))){

        p=strstr(p,"\"");
        sscanf(p+1,"%d",&(w->tomorrow_celsium_low));
    }
    else{ return RSS_RETRY;}
  } /* tomorrow forecast */

  if(w->now_image_index==-1 ||
     w->today_image_index == -1 ||
     w->tomorrow_image_index ==-1){
    return IMAGE_RETRY;
  }
  #ifdef DEBUG
    print_weather(w);
  #endif
  return 0;
}

void print_weather(weather_t *w){

  mylog("now: %d grad, %d %s %s\n", w->now_celsium, w->now_image_index,
          w->now_weather_text, w->now_weather_icon);
  mylog("today: %d grad, %d %s %s\n", w->today_celsium_high,
          w->today_image_index,w->today_forecast_text,w->today_forecast_icon);
  mylog("tomorrow: %d grad, %d %s %s\n", w->tomorrow_celsium_high,
          w->tomorrow_image_index, w->tomorrow_forecast_text,
          w->tomorrow_forecast_icon);
}

/* -1 if image not loaded yet... */
int get_icon_index(char *name){

  char *lastname=name;
  int index;

  index=get_cached_icon_index(lastname);
  if(index>0)
    return index;

  return cache_icon_by_name(name);
}

int get_cached_icon_index(char *name){

  int i;
  for(i=0;i<last_cache_index;++i){

    if(strcmp(weather_images_cache[i].name,name))
      continue;
    return i;
  }
  return 0;
}

/* try to load cached or not yet cached image from disk */
/* -1 if image was not loaded and cached */
int cache_icon_by_name(char *name){

  int i;
  FILE *f;
  char *lastname;
  char *filename;

  if(name[0]=='\0')
    return 0;

  for(i=0;name[i]!='\0';++i){

    if(name[i]=='/')
      lastname=name+i+1;
  }

  filename=malloc(strlen(cache_path)+strlen(lastname)+2);
  if(filename==NULL){return 0;}

  /* path to cached image */
  strcpy(filename,cache_path);
  filename[strlen(cache_path)]='/';
  strcpy(filename+strlen(cache_path)+1,lastname);

  f=fopen(filename,"r");
  if(f==NULL){
    /* file is not downloaded yet... */
    f=load_icon_by_name(name);
    if(f==NULL){
      free(filename);
      mylog("Failed load %s\n",name);
      return -1;
    }
  }
  fclose(f);

  i=append_to_image_cache_by_path_and_name(filename,lastname);
  free(filename);
  return i;

}

int append_to_image_cache_by_path_and_name(char *path, char *name){

  if(last_cache_index>=LAST_CACHE_POS)
    return 0;

  weather_images_cache[last_cache_index].name=malloc(strlen(name)+1);

  strcpy(weather_images_cache[last_cache_index].name,name);
  weather_images_cache[last_cache_index].image=
    gdk_pixbuf_new_from_file(path, NULL);

  if(weather_images_cache[last_cache_index].image==NULL){
    free(weather_images_cache[last_cache_index].name);
    return 0;
  }
  ++last_cache_index;
  return last_cache_index-1;
}

/* try to load image from internet */
FILE *load_icon_by_name(char *name){

  CURL *curl;
  CURLcode res;
  FILE *f;
  char *filename;
  char *lastname;
  int i;

  for(i=0;name[i]!='\0';++i){

    if(name[i]=='/')
      lastname=name+i+1;
  }

  filename=malloc(strlen(cache_path)+strlen(lastname)+2);
  if(filename==NULL){return NULL;}

  /* path to cached image */
  strcpy(filename,cache_path);
  filename[strlen(cache_path)]='/';
  strcpy(filename+strlen(cache_path)+1,lastname);

  f=fopen(filename,"w");
  if(f==NULL){
    free(filename);
    mylog("failed to get icon %s\n",name);
    return NULL;
  }

  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, image_url(name));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_icon);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    res = curl_easy_perform(curl);
    /* always cleanup */
    curl_easy_cleanup(curl);

    if(res==0){ // all ok
      free(filename);
      return f;
    }
    fclose(f);
  }
  free(filename);
  mylog("Cannot download %s (%d)\n",name,(int)res);
  return NULL;
}

char *image_url(char *name){
  static char buf[MAX_BUF];

  strcpy(buf,BASE_URL);
  strncpy(buf+strlen(buf),name,MAX_BUF-BASE_LEN);
  return buf;

}

void init_cache_path(void){
  static int done=0;

  if(done)
    return;
  done=1;
  char *home=getenv("HOME");
  struct stat dstat;
  if(home!=NULL){
    strncpy(cache_path,home,MAX_BUF);
    strncat(cache_path,"/.cache/simpleweather",MAX_BUF);
    mkdir(cache_path,0755);
    if(!lstat(cache_path,&dstat)){
      /* file or dir exists */
      if(S_ISDIR(dstat.st_mode) && (S_IWUSR&dstat.st_mode)){
        return;
      }
    }
  }
  strcpy(cache_path,"/tmp");
}

/*
    char *now_weather_text;
    char *today_forecast_text;
    char *tomorrow_forecast_text;

    char *now_weather_icon;
    char *today_forecast_icon;
    char *tomorrow_forecast_icon;

    int  now_image_index;
    int  today_image_index;
    int  tomorrow_image_index;

    int  now_celsium;
    int  today_celsium_high;
    int  tomorrow_celsium_high;
*/

/* igoogle
   http://www.google.com/ig/api?weather=...

   <xml_api_reply version="1">
   <weather module_id="0" tab_id="0" mobile_row="0" mobile_zipped="1" row="0"
           section="0">
   <forecast_information>
   <city data="moscow,russia"/>
   <postal_code data="moscow,russia"/>
   <latitude_e6 data=""/>
   <longitude_e6 data=""/>
   <forecast_date data="2011-06-13"/>
   <current_date_time data="2011-06-13 15:49:06 +0000"/>
   <unit_system data="SI"/>
   </forecast_information>
   <current_conditions>
     <condition data="Переменная облачность"/>
     <temp_f data="75"/>
     <temp_c data="24"/>
     <humidity data="Влажность: 44 %"/>
     <icon data="/ig/images/weather/partly_cloudy.gif"/>
     <wind_condition data="Ветер: С, 2 м/с"/>
   </current_conditions>
   <forecast_conditions>
   <day_of_week data="пн"/>
     <low data="13"/>
     <high data="23"/>
     <icon data="/ig/images/weather/chance_of_rain.gif"/>
     <condition data="Временами ливни"/>
   </forecast_conditions>
   <forecast_conditions>
     <day_of_week data="вт"/>
     <low data="13"/>
     <high data="22"/>
     <icon data="/ig/images/weather/rain.gif"/>
     <condition data="Дождь"/>
   </forecast_conditions>
   <forecast_conditions>
     <day_of_week data="ср"/>
     <low data="12"/>
     <high data="21"/>
     <icon data="/ig/images/weather/partly_cloudy.gif"/>
     <condition data="Переменная облачность"/>
   </forecast_conditions>
   <forecast_conditions>
     <day_of_week data="чт"/>
     <low data="12"/>
     <high data="21"/>
     <icon data="/ig/images/weather/chance_of_storm.gif"/>
     <condition data="Временами грозы"/>
   </forecast_conditions>
   </weather>
   </xml_api_reply>

   */

int now_playing_changed=0;
char album[MAX_BUF]="",title[MAX_BUF]="";

char *read_now_playing(void){
  static char now_playing[MAX_BUF]="";

  if(now_playing_changed){
    snprintf(now_playing,MAX_BUF,"%s / %s",album,title);
    now_playing_changed=0;
  }
  return now_playing;
}

