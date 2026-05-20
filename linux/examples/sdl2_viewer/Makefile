LIBS=-g -ltrmnl -lpngdec -lJPEGDEC -L/opt/homebrew/opt/curl/lib -lcurl $(shell pkg-config sdl2 --libs)
CFLAGS=-ggdb -D__LINUX__ -Wall -O2 -I../../ $(shell pkg-config sdl2 --cflags)

sdl2_viewer: main.o
	$(CXX) main.o $(LIBS) -o sdl2_viewer

main.o: main.cpp
	$(CXX) $(CFLAGS) -c main.cpp

clean:
	rm -rf *.o sdl2_viewer
