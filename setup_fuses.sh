#/bin/sh
avrdude -c dragon_isp -B 20 -p m324pb -U efuse:w:0xfc:m -U hfuse:w:0xd9:m -U lfuse:w:0xef:m
