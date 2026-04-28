CFLAGS=-D__LINUX__ -c -Wall -O2 
LIBS = -ltrmnl -L/opt/homebrew/opt/curl/lib -lcurl

all: trmnl_demo

trmnl_demo: main.o
	$(CXX) main.o $(LIBS) -o trmnl_demo 

main.o: main.cpp
	$(CXX) $(CFLAGS) main.cpp

clean:
	rm -rf *.o demo
