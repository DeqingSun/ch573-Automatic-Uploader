#!/bin/env python
# -*- coding: utf-8 -*-

import ctypes
import serial
import serial.tools.list_ports
from time import sleep
import sys
import os

def rebootCH573WithTool(rtbTool):
	ch55xRebootToolSerial = serial.Serial(rtbTool,baudrate=1200,timeout=0.01,rtscts=1)
	sleep(0.3)
	ch55xRebootToolSerial.close()
	#sleep(1)

comlist = serial.tools.list_ports.comports()
ch55xRebootToolDevice = None
for element in comlist:
	#print(element)
	if (element.vid == 0x1209 and element.pid == 0xC550):
		if (element.serial_number == "CH_RBT"):
			ch55xRebootToolDevice = element.device
			
if (ch55xRebootToolDevice!=None):
	print("ch55xRebootTool Found on: " + ch55xRebootToolDevice)
else:
	print("ch55xRebootTool not found.")
	exit()

rebootCH573WithTool(ch55xRebootToolDevice)



