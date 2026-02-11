TARGET := libq3d.so
all: $(TARGET)

$(TARGET): qcam.c qcam.h
	gcc -shared -o $(TARGET) qcam.c

install: $(TARGET) qcam.h
	cp $(TARGET) /work/root/lib
	cp qcam.h /work/root/include
