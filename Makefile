target = src/xinb
objs = src/xinb.o src/xmpp.o src/commands.o src/logs.o src/error.o
headers = include/xinb.h include/commands.h include/xmpp.h \
			include/logs.h include/error.h

LDFLAGS += `pkg-config --libs loudmouth-1.0`
CFLAGS += `pkg-config --cflags loudmouth-1.0` -g3

.PHONY: all clean
all: $(objs)
	gcc -o $(target) $(objs) $(LDFLAGS) $(CFLAGS)

$(objs): $(headers)

clean:
	-rm -fv $(objs) $(target) 
