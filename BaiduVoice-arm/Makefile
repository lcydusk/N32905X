.SUFFIXES : .x .o .c .s

ROOT = /usr/local/arm_linux_4.2/
LIB = $(ROOT)/lib/gcc/arm-linux/4.2.1
LIB1 =$(ROOT)/arm-linux/lib
INCSYS:=$(ROOT)/arm-linux/sys-include
INC :=$(ROOT)/arm-linux/include

CC=arm-linux-gcc -O2 -I$(INC) -I$(INCSYS) -static
CXX=arm-linux-g++ -O2 -I$(INC) -I$(INCSYS) -static
WEC_LDFLAGS=-L$(LIB) -L$(LIB1)
STRIP=arm-linux-strip

TARGET = BaiduVoice
SRCS := sample.cpp base64.cpp 
#LIBS= -lc -lgcc -lpthread
LIBS=
LDFLAGS= ./json-cpp/lib/libjson.a ./curl/lib/libcurl.a

all: 
	$(CXX) $(WEC_LDFLAGS) $(SRCS) -o $(TARGET) $(LIBS) $(LDFLAGS) -static
	$(STRIP) $(TARGET) 
	#cp $(TARGET) ../../initramfs/usr/
	echo "Please also copy input patterns to storage"

clean:
	rm -f *.o 
	rm -f *.x 
	rm -f *.flat
	rm -f *.map
	rm -f temp
	rm -f *.img
	rm -f $(TARGET)	
	rm -f *.gdb
	rm -f *.bak
