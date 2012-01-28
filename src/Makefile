
OBJECTS = lowlevel-linux.o libnetlink.o ll_map.o utils.o framework.o

HEADERS = Portable.h

COPTS = -Wall -O0 $(DEBUG)
#COPTS = -Wall -Wextra -O0 $(DEBUG)

DEBUG = -g
#DEBUG = -g #-lefence


all: portproxy

portproxy: $(OBJECTS) $(HEADERS) portproxy.c 
	$(CC) $(COPTS) $(LDFLAGS) portproxy.c -o portproxy $(OBJECTS)

clean:
	rm -f *.o portproxy

tags:
	rm -f TAGS
	find . -name '*.c' -or -name '*.h' | xargs etags

version:
	perl -p -i -e "s/PORTPROXY_VERSION\ \"[0-9]+-[0-9]+-[0-9]+\"/PORTPROXY_VERSION \"`date +%F`\"/g" Portable.h

#
#
#
framework.o: framework.c portproxy.h
	$(CC) $(COPTS) -c $<


# ============================================================
# === C low level stuff ======================================
# ============================================================
lowlevel-linux.o: lowlevel-linux.c 
	$(CC) $(COPTS) -I. -c lowlevel-linux.c 

libnetlink.o: libnetlink.c
	$(CC) $(COPTS) -c libnetlink.c

ll_map.o: ll_map.c 
	$(CC) $(COPTS) -c ll_map.c

utils.o: utils.c
	$(CC) $(COPTS) -c utils.c
