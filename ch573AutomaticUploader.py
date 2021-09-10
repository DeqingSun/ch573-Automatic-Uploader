#!/bin/env python
# -*- coding: utf-8 -*-

#need 32bit python

import ctypes
import serial
import serial.tools.list_ports
from time import sleep
from intelhex import IntelHex
import sys
import os

class CH375Driver:
	def __init__(self):
		self.ch375lib= ctypes.windll.LoadLibrary('CH375DLL.dll')
		print("CH375DLL version:" + str(self.ch375lib.CH375GetVersion()))
		print("CH375 driver version:" + str(self.ch375lib.CH375GetDrvVersion()))
		self.sendArrayType = ctypes.c_byte * 64
		self.sendArray = self.sendArrayType()
		self.receiveArrayType = ctypes.c_byte * 64
		self.receiveArray = self.receiveArrayType()
		
	def open(self):
		ch375USBID = self.ch375lib.CH375GetUsbID(ctypes.c_long(0))
		ch375USBVID = ch375USBID&0xFFFF
		ch375USBPID = (ch375USBID>>16)&0xFFFF
		#print("usbID on index 0:" + hex(ch375USBVID) + "," + hex(ch375USBPID) )
		if (ch375USBVID == 0x4348 and ch375USBPID == 0x55E0):
			print("PID VID matches the bootloader PID/VID")
		else:
			return False
		if (self.ch375lib.CH375OpenDevice(ctypes.c_long(0))>0):
			print("Device Opened")
			return True
		else:
			return False
		
	def close(self):
		self.ch375lib.CH375CloseDevice(ctypes.c_long(0))
		
	def send(self,sendBuf):
		sendLen = min( len(sendBuf) , 64 )
		for i in range(sendLen):
			self.sendArray[i]=ctypes.c_byte(sendBuf[i])
		sendLength = ctypes.c_ulong(sendLen)
		if (self.ch375lib.CH375WriteData(ctypes.c_long(0),ctypes.pointer(self.sendArray),ctypes.pointer(sendLength)) > 0):
			return True
		else:
			return False
	
	def receive(self):
		receiveLength = ctypes.c_ulong(64)
		if (self.ch375lib.CH375ReadData(ctypes.c_long(0),ctypes.pointer(self.receiveArray),ctypes.pointer(receiveLength)) > 0):
			returnArr = [0]*receiveLength.value
			for i in range(receiveLength.value):
				returnArr[i] = self.receiveArray[i]&0xFF
			return returnArr
		else:
			return []
			

	
#CH552 bootloader: // <a1> <x> <x> <x> <x> "MCU ISP & WCH.CN"
DETECT_CHIP_CMD = [ 0xa1, 0x12,  0x00, 0x00, 0x00, 0x4d, 0x43, 0x55, 0x20, 0x49, 0x53, 0x50, 0x20, 0x26, 0x20, 0x57, 0x43, 0x48, 0x2e, 0x43, 0x4e]

#CH552 bootloader: // <a7> <x> <x> <cfg>
#seems follow CH552 protocol, 0x07 will output a 10 bytes, address unconfirmed but likely to be in 0x7E000. 0x08 will output BT version, 0x10 will read the sn and calc a snSum
#recv[10] bit 1 controls boot pin, 1 means PB22, 0 means PB11. recv[14] bit 3 controls RSTEN (1 is EN)
READ_CFG_CMD = [0xa7, 0x02, 0x00, 0x1f, 0x00]

#skip A8 for now
#<a8> <x> <x> <cfg> <x> <cfg data>
#cfg need to be 7 to work, cfg data is mostly a copy of recv[6]~recv[17]

#CH552 bootloader: // <a3> <len> <x> <x> ...
#response byte 4 should be sum of mcu's calculated key
SEND_KEY_CMD = [0xa3, 0x1e, 0x00] + [0x00] * (0x1e)

#CH552 bootloader: // // <a4> <x> <x> <page>
ERASE_CHIP_CMD = [0xa4, 0x01, 0x00, 0x08]

RESET_RUN_CMD = [0xa2, 0x01, 0x00, 0x01]

def convertListToHex(listToConv):
	return ", ".join("0x{:02x}".format(num) for num in listToConv)

def ch573WriteData(hexObj):
	ch375Driver = CH375Driver()
	while(True):
		opened = False
		for i in range(30):
			sleep(0.1)
			if (ch375Driver.open()):
				opened = True
				break
		if not opened:
			break
		
		sleep(0.1)
		
		ch375Driver.send(DETECT_CHIP_CMD)
		recvList = ch375Driver.receive()
		if (len(recvList)<6):
			print("DETECT_CHIP_CMD no response")
			break
		familyID = recvList[5]
		deviceID = recvList[4]
		print("WCH chip family "+hex(familyID)+" devive "+hex(deviceID))
		if (familyID == 0x13 and deviceID == 0x73):
			pass
		else:
			break
		
		ch375Driver.send(READ_CFG_CMD)
		recvList = ch375Driver.receive()
		print("Bootloader Version: "+("%d%d.%d%d"%(recvList[18],recvList[19],recvList[20],recvList[21])))
		if (recvList[10]&(1<<1)):
			print("Bootloader entry with PB22 low")
		else:
			print("Bootloader entry with PB11 high")
		snList = recvList[22:22+8]
		snSum = sum(snList)&0xFF
		xorMask = [snSum]*8
		xorMask[7] += deviceID
		print("XOR mask : "+convertListToHex(xorMask))
		
		#skip config write and then read again
		
		ch375Driver.send(SEND_KEY_CMD)
		recvList = ch375Driver.receive()
		
		hexObj = ih
		writeAddr = hexObj.minaddr()
		maxAddr = hexObj.maxaddr()
		
		pagesNeeded = max( 8 , (maxAddr+(1023))//1024 );
		ERASE_CHIP_CMD[3] = pagesNeeded
		
		ch375Driver.send(ERASE_CHIP_CMD)
		recvList = ch375Driver.receive()
		
		while(writeAddr<=maxAddr):
			writePackageLen = min(56,maxAddr-writeAddr+1)
			writePackage = [0]*writePackageLen
			for i in range(writePackageLen):
				xorMaskByte = xorMask[i&0x07]
				writePackage[i] = (hexObj[writeAddr+i] ^ xorMaskByte )&0xFF
			flashCmd = [0xA5 , writePackageLen+5 , 0 , writeAddr&0xFF, (writeAddr>>8)&0xFF , (writeAddr>>16)&0xFF, (writeAddr>>32)&0xFF , 0 ]+writePackage
			#print(convertListToHex(flashCmd))
			ch375Driver.send(flashCmd)
			recvList = ch375Driver.receive()
			writeAddr+=writePackageLen
		
		ch375Driver.send(SEND_KEY_CMD)
		recvList = ch375Driver.receive()
		
		verifyOK = True
		
		while(writeAddr<=maxAddr):
			writePackageLen = min(56,maxAddr-writeAddr+1)
			writePackage = [0]*writePackageLen
			for i in range(writePackageLen):
				xorMaskByte = xorMask[i&0x07]
				writePackage[i] = (hexObj[writeAddr+i] ^ xorMaskByte )&0xFF
			verifyCmd = [0xA6 , writePackageLen+5 , 0 , writeAddr&0xFF, (writeAddr>>8)&0xFF , (writeAddr>>16)&0xFF, (writeAddr>>32)&0xFF , 0 ]+writePackage
			#print(convertListToHex(flashCmd))
			ch375Driver.send(verifyCmd)
			recvList = ch375Driver.receive()
			if (recvList[4]!=0 or recvList[5]!=0 ):
				verifyOK = False
				print("Data not match @ %d",writeAddr)
				break
			writeAddr+=writePackageLen
			
		if (verifyOK):
			ch375Driver.send(RESET_RUN_CMD)
			recvList = ch375Driver.receive()
	
		break
	ch375Driver.close()

def rebootCH573WithTool():
	comlist = serial.tools.list_ports.comports()
	ch55xRebootToolDevice = None
	for element in comlist:
		#print(element)
		if (element.vid == 0x1209 and element.pid == 0xC550):
			if (element.serial_number == "CH_RBT"):
				ch55xRebootToolDevice = element.device

	if (ch55xRebootToolDevice!=None):
		print("ch55xRebootTool Found on: " + ch55xRebootToolDevice)
		ch55xRebootToolSerial = serial.Serial(ch55xRebootToolDevice,baudrate=1200,timeout=0.01,rtscts=1)
		sleep(0.3)
		ch55xRebootToolSerial.close()
		#sleep(1)
	else:
		print("ch55xRebootTool not found.")


if (len(sys.argv)<2):
	print("Need parameter for HEX file")
	exit()
	
hexFilePath = sys.argv[1]

oldEditTime = os.path.getmtime(hexFilePath)

while(1):
	try:
		newEditTime = os.path.getmtime(hexFilePath)
	except:
		newEditTime = oldEditTime #ignore if file disappeared
	if (newEditTime!=oldEditTime):
		print("File Changed")
		ih = IntelHex(hexFilePath)
		print("Hex address ranging from %d to %d" % (ih.minaddr(),ih.maxaddr()))
		rebootCH573WithTool()
		ch573WriteData(ih)
		oldEditTime = newEditTime
	sleep(0.1)





