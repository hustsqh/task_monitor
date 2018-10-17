CC=gcc
CXX=g++
CFLAGS=-Wall -g -fPIC $(CINCLUDE)
CXXFLAGS=$(CFLAGS) -std=gnu++11 -Wall

TARGET=taskd libtask_client.so task_cli

all:$(TARGET)

taskd_sources = task_config.cpp task_core.cpp task_main.cpp task_msg.cpp TaskObjClient.cpp TaskObj.cpp TaskServer.cpp task_utils.cpp
taskd_objs =$(patsubst %.cpp, %.o,$(taskd_sources))

libtask_client_sources = task_client.cpp task_msg.cpp
libtask_client_objs = $(patsubst %.cpp, %.o,$(libtask_client_sources))

task_cli_sources = task_client.cpp task_msg.cpp
task_cli_objs = $(patsubst %.cpp, %.o,$(task_cli_sources))

LDFLAGS := -lpthread -L../ring_buf -lring_buf -L../libev_api -lev_api -l

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

taskd:$(taskd_objs)
	$(CXX) $(LDFLAGS) -o taskd $^

libtask_client.so:$(libtask_client_objs)
	$(CXX) $(LDFLAGS) -shared -o libtask_client.so $^

task_cli:$(task_cli_objs)
	$(CXX) $(LDFLAGS) -o task_cli $^

$(TARGET):$(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -rf *.o $(TARGET)

install:
	cp $(TARGET) ../main