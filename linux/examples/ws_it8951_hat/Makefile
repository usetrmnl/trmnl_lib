OPSYS:=$(shell uname)
ifeq ($(OPSYS), Linux)
LIBS=-g -ltrmnl -lFastEPD -lpngdec -lJPEGDEC -lgpiod -lbb_scd41 -lbb_temperature -lcurl
else
LIBS=-g -ltrmnl -lFastEPD -lpngdec -lJPEGDEC -L/opt/homebrew/opt/curl/lib -lcurl
endif
CFLAGS=-ggdb -D__LINUX__ -Wall -O2 -I../../

trmnl_hat: main.o
	$(CXX) main.o $(LIBS) -o trmnl_hat

main.o: main.cpp
	$(CXX) $(CFLAGS) -c main.cpp

clean:
	rm -rf *.o trmnl_hat
