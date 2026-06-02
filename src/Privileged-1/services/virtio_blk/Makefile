SERVICE_NAME := virtio_blk
SERVICE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

OBJS := $(OBJ_DIR)/virtio_blk.service.o
TARGET := $(OBJ_DIR)/$(SERVICE_NAME).o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -r $^ -o $@

$(OBJ_DIR)/virtio_blk.service.o: service.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
