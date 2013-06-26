CC = gcc

EXTRAFLAGS = -DDEBUG -g -DDYNAMIC_ICON_LOAD
#EXTRAFLAGS = 
GIFS=iconsbest.com-icons
#GIFS=yahoo-gifs

PKG_CONFIG = pkg-config
GTK_METADATA = gtk+-2.0 dbus-glib-1 dbus-1 glib-2.0
INSTALL = install
#GS_LIBEXEC_DIR = `${PKG_CONFIG} --variable=privlibexecdir gnome-screensaver`
#GS_DESKTOP_DIR = `${PKG_CONFIG} --variable=themesdir      gnome-screensaver`

SAVER_EXE_DIR=/usr/lib/xscreensaver/xxcheats
SAVER_SHARE_DIR=/usr/share/simpleweather

SOURCES = gs-theme-window.c simpleclock.c simpleweather.c load_weather.c
TARGET = simpleweather
DESKTOP = ${TARGET}.desktop

OBJECTS = $(SOURCES:.c=.o)
CFLAGS = `${PKG_CONFIG} ${GTK_METADATA} --cflags` -Wall -I. -I./libmxml/include -DGIFS_DIR=\"${SAVER_SHARE_DIR}\"
LIBS   = `${PKG_CONFIG} ${GTK_METADATA} --libs` -lcurl ./libmxml/lib/libmxml.a

$(TARGET): ./libmxml/lib/libmxml.a weather_strings.h simpleclock.h $(OBJECTS) 
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -o $@ $(OBJECTS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $< -o $@

./libmxml/lib/libmxml.a:
	tar xfz libmxml.tgz
	cd mxml-2.7; ./configure --prefix=`pwd`/../libmxml && make && make install

clean:
	rm -f $(OBJECTS) $(TARGET) core* *.o *~

install:
	[ -d "${SAVER_EXE_DIR}" ] || ${INSTALL} -m 0755 -d ${SAVER_EXE_DIR}
	[ -d "${SAVER_SHARE_DIR}" ] || ${INSTALL} -m 0755 -d ${SAVER_SHARE_DIR}
	${INSTALL} -s -m 0755 ${TARGET}  ${SAVER_EXE_DIR}
	${INSTALL}    -m 0644 ${GIFS}/*.gif ${SAVER_SHARE_DIR}

#	${INSTALL}    -m 0644 ${DESKTOP} ${GS_DESKTOP_DIR}
