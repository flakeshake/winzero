#PREFIX=i686-w64-mingw32-
#PREFIX=
PREFIX=x86_64-w64-mingw32-
CC=$(PREFIX)gcc
LIBS=-lm
name=winzero

all: $(name)

debug:
	$(CC) $(LIBS) -DDEBUG --std=c99 -Wall -Wextra -g $(name).c -o bin/$(name).exe
	
$(name): $(name).c
	$(CC) $(LIBS) --std=c99 -Wall $(name).c -o bin/$(name).exe
	
release:
	$(CC) $(LIBS) --std=c99 -O2 $(name).c -o bin/$(name).exe
	$(PREFIX)strip bin/$(name).exe
	
allstatic:
	$(CC) --static-libc --static-libgcc $(LIBS) --std=c99 -O2 $(name).c -o bin/$(name).exe
	
static:
	$(CC) --static --std=c99 -O2 $(name).c -obin/ $(name).exe
	
clean:
	rm -f bin/$(name).exe
	rm -f *.img
