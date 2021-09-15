# CH573 Automatic Uploader

![connection](https://raw.githubusercontent.com/DeqingSun/ch573-Automatic-Uploader/main/img/ch55xRebootToolConnection.jpg)

This tool is designed to accelerate CH573 development. In the development stage, the CH573 firmware can be updated either by USB, or SWD via WCH-LINK. 

## Offical upload tools and methods

You may click the "Download" button in the MounRiver Studio to download the firmware. But it takes more time than USB to update the firmware.

When using the USB, you may need to manually re-plug your CH573 board, hold a button, and click a few times in the WCHISP tool. Although the update speed is faster, it takes some time for the user to run the process.

## How this tool works

The hardware side of the tool is a [ch55xRebootTool](https://github.com/DeqingSun/ch55xduino/tree/ch55xduino/pcb/ch55xRebootTool) board. You may send a command from the computer and the board will pull up D+ and pull down a GPIO, while re-power the target. So the user can kick the CH573 into bootloader mode from the computer. 

The software side of the tool is a python script. It takes a parameter as the target HEX file. The script will constantly check the modify time of the HEX file. Once the modify time changes, the script will try to upload the firmware with a reverse-engineered protocol to CH573. The USB communication part is done by calling "CH375DLL.dll". So this tool can co-exist with the official WCHISP tool, and no need to do driver change. Note the dll is a 32-bit one and the Python needs to be 32-bit as well (tested with 32-bit Python 2.7).  

