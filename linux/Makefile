CFLAGS=-c -Wall -O2 -I../src -D__LINUX__ -I/opt/homebrew/opt/curl/include
LIBS = -lm -lpthread -L/opt/homebrew/opt/curl/lib -lcurl

all: libtrmnl.a

libtrmnl.a: trmnl_lib.o cJSON_Utils.o cJSON.o
	ar -rc libtrmnl.a trmnl_lib.o cJSON.o cJSON_Utils.o ;\
	sudo cp libtrmnl.a /usr/local/lib ;\
	sudo cp ../src/trmnl_lib.h /usr/local/include

trmnl_lib.o: ../src/trmnl_lib.cpp ../src/trmnl_lib.h
	$(CXX) $(CFLAGS) ../src/trmnl_lib.cpp

cJSON.o: cJSON.h cJSON.c
	$(CC) $(CFLAGS) cJSON.c

cJSON_Utils.o: cJSON_Utils.c cJSON_Utils.h
	$(CC) $(CFLAGS) cJSON_Utils.c

clean:
	rm *.o libtrmnl.a
