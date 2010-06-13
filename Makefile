TARGET = build/xinb
OBJ = build/xinb.o build/xmpp.o build/commands.o
HEADER = include/xinb.h include/commands.h include/xmpp.h
DEBUG = -g
LDFLAGS = -lloudmouth-1 -lglib-2.0
CFLAGS = -I /usr/include/loudmouth-1.0 \
	`pkg-config --libs --cflags glib-2.0` \
	-I /usr/lib/glib/include

all: $(OBJ) $(HEADER)
	mv -v *.o build/
	gcc -o $(TARGET) $(OBJ) $(LDFLAGS) $(CFLAGS) $(DEBUG)

build/xinb.o: src/xinb.c $(HEADER)
	gcc -c src/xinb.c $(CFLAGS) $(DEBUG)

build/xmpp.o: src/xmpp.c $(HEADER)
	gcc -c src/xmpp.c $(CFLAGS) $(DEBUG)

build/commands.o: src/commands.c $(HEADER)
	gcc -c src/commands.c $(CFLAGS) $(DEBUG)

clean:
	rm -fv $(OBJ) $(TARGET)

