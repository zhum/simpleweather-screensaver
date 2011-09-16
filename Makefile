CC = gcc
PKG_CONFIG = pkg-config
GTK_METADATA = gtk+-2.0 dbus-glib-1 dbus-1 glib-2.0
INSTALL = install
GS_LIBEXEC_DIR = `${PKG_CONFIG} --variable=privlibexecdir gnome-screensaver`
GS_DESKTOP_DIR = `${PKG_CONFIG} --variable=themesdir      gnome-screensaver`

SOURCES = gs-theme-window.c simpleclock.c simpleweather.c load_weather.c
TARGET = simpleweather
DESKTOP = ${TARGET}.desktop

OBJECTS = $(SOURCES:.c=.o)
CFLAGS = `${PKG_CONFIG} ${GTK_METADATA} --cflags` -g -Wall
EXTRAFLAGS = -pg
LIBS   = `${PKG_CONFIG} ${GTK_METADATA} --libs` -lcurl 

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -o $@ $(OBJECTS) $(LIBS)
.c.o:
	$(CC) $(CFLAGS) $(EXTRAFLAGS) -c $< -o $@
clean:
	rm -f $(OBJECTS) $(TARGET) core* *.o *~
install:
	${INSTALL} -s -m 0755 ${TARGET}  ${GS_LIBEXEC_DIR}
	${INSTALL}    -m 0644 ${DESKTOP} ${GS_DESKTOP_DIR}

