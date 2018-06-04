uvcvideo-objs  := uvc_driver.o uvc_queue.o  uvc_video.o uvc_ctrl.o \
	             uvc_status.o uvc_isight.o uvc_debugfs.o uvc_entity.o


obj-m += uvcvideo.o



all :
	   make -C /lib/modules/$(shell uname -r)/build   M=$(PWD) modules
clean:
	   make -C /lib/modules/$(shell uname -r)/build  M=$(PWD) clean
