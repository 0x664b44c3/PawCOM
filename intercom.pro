QT+=core
INCLUDEPATH = /usr/avr/include
SOURCES += \
	 main.c \
	 twi.c \
    gptimer.c \
    i2c_worker.c \
    buttons.c \
    crc.c \
    hardware.c

DEFINES += __AVR_ATmega324PB__

HEADERS += \
	 i2c.h \
    gptimer.h \
    buttons.h \
    crc.h \
    hardware.h
OTHER_FILES += \
	Makefile


