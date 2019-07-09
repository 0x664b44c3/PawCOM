QT=
INCLUDEPATH = /usr/avr/include
SOURCES += \
	 main.c \
	 twi.c \
    gptimer.c \
    i2c_worker.c \
    buttons.c

DEFINES += __AVR_ATmega324PB__

HEADERS += \
	 i2c.h \
    gptimer.h \
    buttons.h
OTHER_FILES += \
	Makefile


