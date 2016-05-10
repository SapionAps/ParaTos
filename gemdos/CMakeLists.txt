project (gemdos)

include_directories( .. ../mushashi )


set (SOURCE_FILES cdev.c date.c dir.c file.c mem.c net.c proc.c sys.c loader.c gemdos_dispatch.c cookiejar.c)
add_library (gemdos STATIC ${SOURCE_FILES})
