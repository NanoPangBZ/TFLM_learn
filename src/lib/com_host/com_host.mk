COM_HOST_ROOT_DIR := ${FRAMEWORK_ROOT_DIR}/com_host

#com_host源文件列表
SRC += $(COM_HOST_ROOT_DIR)/com_host.c
SRC += $(COM_HOST_ROOT_DIR)/private/com_host_port.c
SRC += $(COM_HOST_ROOT_DIR)/private/com_host_packager.c
SRC += $(COM_HOST_ROOT_DIR)/private/com_host_lib/com_host_crc16.c
SRC += $(COM_HOST_ROOT_DIR)/private/com_host_lib/com_host_list.c
SRC += $(COM_HOST_ROOT_DIR)/private/com_host_lib/com_host_ringbuf.c
SRC += $(COM_HOST_ROOT_DIR)/private/com_host_ll_depend/com_host_ll_heap.c
SRC += $(COM_HOST_ROOT_DIR)/private/com_host_ll_depend/com_host_ll_systime.c
SRC += $(COM_HOST_ROOT_DIR)/private/com_host_package/com_host_your_way_studio_packager.c
