CXX = g++
CFLAGS = -std=c++17 -O2 -g -Wall

TARGET = serverApp
OBJS = ../code/buffer/*.cpp ../code/http/*.cpp ../code/logsys/*.cpp \
       ../code/pool/*.cpp ../code/server/*.cpp ../code/timer/*.cpp \
       ../main.cpp

all: 
	$(CXX) $(CFLAGS) $(OBJS) -o ../$(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf ../$(TARGET)


