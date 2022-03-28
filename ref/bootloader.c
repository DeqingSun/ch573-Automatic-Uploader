/** bootloader code in c for the ch552 device

   this is basically the same code as bootloader.a51 at if you link it to 0x3800
   and set BOOTLOADER 1. The order of the functions however is changed and there
   is a change in the XDATA memory layout.  
   
   In all other cases it creates a modified one running from 0x0000. The descriptors 
   are changed BCD_Device = 2.0 and it can report the chip number as a serial string.   
   Since this code uses the WCH IDs their driver gets loaded an you can use their 
   ISP Tool.     
     
   ATTENTION:
   If you set BOOTLOADER to 0 all security and rangechecks are removed,so you can flash
   what ever yo want. This also means you can brick your chip. Be carefull.  

   Although it can be compiled in SDCC too it will not produce a functional hex 
   because the CBYTE macro does not work correctly under SDCC. SO at this time is
   Keil only. Anyway with SDCC the code would be too big to fit in 0x3800.
   
   By pulling INT0 (P3.2) low while connecting VCC the original code from 0x3800 
   gets called. Aarons stick has pushbutton there.    
*/

#include <stdint.h>              
#include <string.h>
#include ".\..\inc\ch552_new.h" 
#include ".\..\inc\usb\usb11.h"
#ifdef __C51__
  #define LOW(x)  (*((unsigned char*)&(##x)+1))       // returns the low part
  #define HIGH(x) (*((unsigned char*)&(##x)+0))       // returns the high part
  #define CBYTE   ((unsigned char volatile code *) 0) 
#else
  #define LOW(x)  ((x) & 0xFF)       
  #define HIGH(x) ((x) >> 8)         
  #define CBYTE   ((unsigned char volatile code *) 0)  // todo this still does not work
#endif 

#define CALL(addr)   (((void(*)(void))(uint8_t code *)addr)()) //call any address


#define BOOTLOADER 0      // set to 1 for building the original bootloader

#if BOOTLOADER
  #define WITH_SERIAL  0  // no serial 
  #define AARONS_STICK 0  // no stick stick
#else
  #define LED1   TXD1 
//#define LED2   TXD      // does not work
  #define LED3   RXD 
  #define SW1    INT0
  #define WITH_SERIAL  1  // set this to 0 to disable the serial str
  #define AARONS_STICK 1  // enables some Leds and the button of Aarons stick
#endif

#define EP0SIZE      8   //defaultsize for EP0

bit      bWrProtect;    // 1  if CodeFlash is writeprotected 
bit      bSoftReset;    // 1  for going into reset
bit      bFirstSer;     // 1  for first serial
bit      bReqError;     // 1  for unknown requests


uint8_t  wValueLo;         // 0x21
uint8_t  Bootkey[8];       // 0x22 
uint8_t  rLen;             // 0x2B
uint8_t  reqlen;           // 0x2C
uint8_t  cmdbuffer[64];    // 0x2D

uint8_t  bRequest;         // 0x69
uint8_t  SnSum;            // 0x70 
uint8_t  *pDesc;           // 0x71

#ifdef __C51__
  xdata uint8_t  Ep0Buffer [EP0SIZE] _at_ 0x0000;  
  xdata uint8_t  Ep2Out    [64]      _at_ 0x0040;
  xdata uint8_t  Response  [64]      _at_ 0x0080;  //this is Ep2In
  xdata uint8_t  SerialStr [32]      _at_ 0x00C0;  
#else
  __xdata __at (0x0000) uint8_t  Ep0Buffer [EP0SIZE];
  __xdata __at (0x0040) uint8_t  Ep2Out    [64];
  __xdata __at (0x0080) uint8_t  Response  [64];
  __xdata __at (0x00C0) uint8_t  SerialStr [32];
#endif

code uint8_t   CopyrightMsg[] = "MCU ISP & WCH.CN"; 

code uint8_t   BLoaderVer[4]  = {0x00,0x02,0x03,0x01};

code uint8_t   DeviceDesc[18] = 
{
   18,
   1,
   0x10,0x01,
   0xFF,
   0x80,
   0x55,
   EP0SIZE,
   0x48,0x43,
   0xE0,0x55,
#if BOOTLOADER
   0x00,0x01,
#else   
   0x00,0x02, // changed ver 2.0 to see which one is running 
#endif
   0x00,
   0x00,
#if WITH_SERIAL
   0x03,     // changed serial no
#else
	 0x00,
#endif	
   0x01 
};

code uint8_t  ConfigDesc [32] =
{
   9,
   2,
   0x20,0x00,
   1,
   1,
   0,
   0x80,
   0x32,
   
   9,4,0,0,2,0xFF,0x80,0x55,0,
   7,5,0x82,2,0x40,0x00,0,
   7,5,0x02,2,0x40,0x00,0
};

#if(WITH_SERIAL)  

code  uint8_t LangStr  [4] =
{
   sizeof(LangStr),USB_STRING_DESCRIPTOR,
   0x09,0x04
};

/** convert a byte into 2 hex chars 
    \param byte input 
    \result converted Hex chars */
uint16_t ToHex(uint8_t byte)
{
   uint8_t  tmp;
   uint16_t result;

   tmp = byte >> 4;
   result  = ((tmp< 10) ? tmp+'0' : tmp+'A'-10) << 8;
   tmp = byte & 0x0F;
   result |= ((tmp< 10) ? tmp+'0' : tmp+'A'-10) & 0xFF;
   return result;
}

/** build a USB serial number string

    read the unique chip serial number and build a USB String from it */
void CreateSerialNoString(void)
{
   uint16_t hex;
   uint8_t  i = 0;
   uint16_t address = ROM_CHIP_ID_HX +1; 
   SerialStr[i++] = 22;   // strlen == 2 + 10 digits * sizeof(uint16_t) 
   SerialStr[i++] = USB_STRING_DESCRIPTOR;  
   while  (i < 22)
   {
      hex= ToHex(CBYTE[address++]);
      SerialStr[i++]  = HIGH (hex);
      SerialStr[i++]  = 0;
      SerialStr[i++]  = LOW  (hex);
      SerialStr[i++]  = 0;
   }
}
#endif

/** send one char over the serial port */
void SerialPutChar(uint8_t ch)
{
   if (bFirstSer)
   {
      TI   = 0;
      SBUF = ch;
      while (!TI) ;
      TI = 0;
      return;
   }
   U1TI  = 0;
   SBUF1 = ch;
   while (!U1TI) ;
   U1TI  = 0;
} 

/** get one char over the serial port */
uint8_t SerialGetChar (void)
{
   if (bFirstSer)
   {
      while (!RI) ;
      RI  = 0;
      return SBUF;
   }
   while (!U1RI) ;
   U1RI = 0;
   return SBUF1;
}

/* store one word into the codeflash*/
uint8_t WriteCodeFlash(uint16_t Address, uint16_t Data)
{
#if BOOTLOADER
   if (  (Address < BOOT_LOAD_ADDR) || 
        ((Address >= 0x3FF0 ) && (Address < ROM_CFG_ADDR))
      )
#else

#endif      
   {
      ROM_ADDR = Address;
      ROM_DATA = Data;
      if (ROM_CTRL & bROM_ADDR_OK)
      {
         ROM_CTRL = ROM_CMD_WRITE;
         return ROM_CTRL ^ bROM_ADDR_OK;
      }         
   }  
   return bROM_ADDR_OK;
}

/* store one byte into the data flash */
uint8_t WriteDataFlash(uint8_t Address,uint8_t Data)
{
   ROM_ADDR_H = DATA_FLASH_ADDR>>8;
   ROM_ADDR_L = Address << 1;
   ROM_DATA_L = Data;
   if (ROM_CTRL & bROM_ADDR_OK) 
   {
      ROM_CTRL = ROM_CMD_WRITE;
      return ROM_CTRL ^ bROM_ADDR_OK;
   }  
   return bROM_ADDR_OK;   
}

/* a simplified version for init the bootkey
   
   originally this is done by shift operations and a loop in main
   because the result is const this init is sufficient and saves proc space.
   Bootkey={0xA5,0xF6,0x7F,0x23,0x1D,0xC1,0xD3,0x43}; */
void InitBootKey(void)
{
   Bootkey[0]= 0xA5;
   Bootkey[1]= 0xF6;
   Bootkey[2]= 0x7F;
   Bootkey[3]= 0x23;
   Bootkey[4]= 0x1D;
   Bootkey[5]= 0xC1;
   Bootkey[6]= 0xD3;
   Bootkey[7]= 0x43;
}  

/** Dispatcher for the bootloader functions

    There are 11 functions available to deal with FlashCode and FlashData. 
    All functions expect the data in cmdbuffer and return results in Response. 
    Programming Data are transfered encoded using a 64 bit Key. For the default 
    key see InitBootKey(). All functions should work exactly as the original 
    ones exept for 0xA3. Function 0xA3 (Delete Code Page) is slighly changed 
    to handle pages other than 0 and to erase all bytes.
*/
void FunctionDispatcher(void) 
{   
   register uint8_t  result;
   register uint8_t  i;
   register uint8_t  len;
            uint16_t addr;
            uint16_t Data;
     
   Response[5] = 0x00;
   rLen        = 6;
   result      = 0xFE;
   
   switch (cmdbuffer[0])
   {
      case 0xA1: // chipdetect V2 
           {  // <a1> <x> <x> <x> <x> "MCU ISP & WCH.CN"
              result = CHIP_ID;
              for (i = 0; i < 16; i ++)
              {
                 if (cmdbuffer[5+i] != CopyrightMsg[i])
                 {  //no match
                    result = 0xF1;
                    break;
                 }  
              }               
              Response[5]  = 0x11;
           }
           break;

      case 0xA5:  // flash code disabled until page 0 cleared
           {  // <a5> <len> <0> <addrL> <addrH> <x> <x> <x> <data[len+5]>
              if (bWrProtect) return;
           }    

      case 0xAA:  // encoded flash code or flash data
           {  // <aa> <len> <x> <addrL> <addrH> <x> <x> <x> <data[len+5]>
              len    = cmdbuffer[1] - 5;
              addr   = cmdbuffer[3] | (cmdbuffer[4] << 8);
              if (cmdbuffer[0] == 0xA5)
              {  // flashing code
                 for (i=0;i < len; i+=2)
                 {
                    Data   =  Bootkey[(0+i) & 0x07] ^ cmdbuffer[8+i];
                    Data  |= (Bootkey[(1+i) & 0x07] ^ cmdbuffer[9+i]) << 8;
                    result = WriteCodeFlash(addr, Data);
                    addr+=2;                 
                 }
              }
              else // writing data flash
              {
                 for (i=0; i<len;i++)
                 {  
                    Data   = Bootkey[(0+i) & 0x07] ^ cmdbuffer[8+i];
                    result=WriteDataFlash(addr,Data);
                    addr ++;
                 }
              }
           }
           break;
      
      case 0xA6:  // verify 
           {  // <a6> <len> <x> <addrL> <addrH>  <x> <x> <x> <data[len-5]>
              len  = cmdbuffer[1]-5;
              if (len & 0x07) break; //must start at 8 byte boundary              
              addr   = cmdbuffer[3] | cmdbuffer[4] << 8; 
              for (i=0;i <len;i++)
              {
                 if (   Bootkey[i & 0x07] 
                      ^ cmdbuffer[8+i] 
                      ^ CBYTE [addr])
                 {
                    result = 0xF1;
                    break;
                 }
                 addr++;
              }
              result = 0;
           }
           break;

      case 0xAB: // read dataflash
           {  // <ab> <x> <x> <addrL> <addrh> <x> <x> <rLen> <data[rLen]>
              rLen +=cmdbuffer[7];
              ROM_ADDR_H = DATA_FLASH_ADDR >> 8;
              for (i=6;i < rLen; i++)
              {
                 ROM_ADDR_L = (cmdbuffer[3+i] - 6) << 1;
                 ROM_CTRL   = ROM_CMD_READ;
                 Response[i]= ROM_ADDR_L; 
              }
              result = 0;
           }
           break;  

      case 0xA4:  // erase code Flash page 2k
           {  // <a4> <x> <x> <page>
#if BOOTLOADER
              if (cmdbuffer[3] < 8) 
              {
                 addr = ((uint16_t) cmdbuffer[3]) << 10;
                 i    = 4;
                 do
                 {
                    result = WriteCodeFlash(addr ,0xFFFF);
                    addr  += 2;
                    if (!(addr & 0x3FF)) i--;
                 } while(i);
                 bWrProtect = 0;
              } 
#else //disable delete page but report ok otherwise this code is gone
{
              bWrProtect = 0;
              result = 0;
}
#endif
              /* the original broken one
              if (cmdbuffer[3] < 8) 
              {
                 cmdbuffer[3]=4;
                 addr        =0;  //bugbug just page 0
                 do
                 {
                    result= WriteCodeFlash(addr ,0xFFFF);
                    addr+=4;  // bugbug leaving every second word untouched
                    if (!(addr & 0x3FF)) cmdbuffer[3]--;
                 } while(cmdbuffer[3]);
                 bWrProtect = 0;
              } 
              */
           }
           break;

      case 0xA9: // erase data Flash
           {  // <A9> <x> <x> <x>
              for (i=0;i < 128;i++)
              {
                 WriteDataFlash(i,0xFF);
              }
           }
           break;

      case 0xA3: // calc a new key
           {  // <a3> <len> <x> <x> ...
              if (cmdbuffer[1] < 30) break;            
              i = cmdbuffer[1] / 7;   
              Bootkey[0] = cmdbuffer[3+i*4] ^ SnSum;
              Bootkey[2] = cmdbuffer[3+i*1] ^ SnSum;
              Bootkey[3] = cmdbuffer[3+i*6] ^ SnSum;
              Bootkey[4] = cmdbuffer[3+i*3] ^ SnSum;
              Bootkey[6] = cmdbuffer[3+i*5] ^ SnSum;
              i = cmdbuffer[1] / 5 ;
              Bootkey[1] = cmdbuffer[3+i*1] ^ SnSum;
              Bootkey[5] = cmdbuffer[3+i*3] ^ SnSum;
              Bootkey[7] = CHIP_ID + Bootkey[0];
              result = 0;
              for (i=0; i<8; i++)
              {
                  result += Bootkey[i];
              }
           }
           break;
           
      case 0xA8: // save config
           {  // <a8> <x> <x> <cfg> <x> <cfg data>
              if  (cmdbuffer[3] & 0x07 != 0x07) break;
              cmdbuffer[14] = cmdbuffer[14] & 0x7F | 0x40;
              addr = 0x3FF0;
              for (i=0;i < 10; i +=2)
              {
                 //adr+=i;
                 result = WriteCodeFlash(addr+i, cmdbuffer[6+i]<< 8 | cmdbuffer[5+i]);
              }
           }
           break;
       
      case 0xA7:  // read config data        
           {  //<a7> <x> <x> <cfg>
              result = 0;
              if (cmdbuffer[3] & 0x07==0x07)
              {  // return 10 bytes starting at 0x3FF0
                 result = 0x07;
                 addr   = 0x3FF0;
                 for (i=0; i < 10; i++)  Response[rLen]= CBYTE[addr++];
                 rLen+=2;
                 Response[10]  = GLOBAL_CFG & bBOOT_LOAD | Response[10] & ~bBOOT_LOAD; 
              }
              if (cmdbuffer[3] & 0x08)
              {  // bootloader version in BCD actually 02.31
                 result |= 0x08;
                 addr    = (uint16_t) &BLoaderVer;
                 for (i=0; i < 4; i++)
                 {
                    Response[rLen] = CBYTE[addr++];
                    rLen++;
                 }
              } 
              if (cmdbuffer[3] & 0x10)
              { // read the sn and calc a snSum
                 result |= 0x10;
                 addr    = ROM_CHIP_ID_LO;
                 SnSum   = 0;
                 for (i=0; i < 4; i++)
                 {
                    Response[rLen] = CBYTE[addr++];
                    SnSum += Response[rLen];
                    rLen ++;
                 }
                 rLen+=4;
              }
              SAFE_MOD   = 0x55;
              SAFE_MOD   = 0xAA;
              GLOBAL_CFG = bCODE_WE | bDATA_WE;
              SAFE_MOD   = 0; 
           }
           break;
           
      case 0xA2: // connect to the flasher
           {
              if (cmdbuffer[3]==1) bSoftReset= 1;
              else                 bWrProtect= 1;     
              result = 1;
           }
           break;            
   }
   Response[0]  = cmdbuffer[0];
   Response[2]  = rLen-4;
   Response[3]  = 0;
   Response[4]  = result;   
}

void PreparePacket(void)
{
	 register uint8_t len; 
	 len =(reqlen > EP0SIZE) ? EP0SIZE: reqlen;
	 if (len)
	 {		   
      memcpy(Ep0Buffer,pDesc,len);
      reqlen-=len;
      pDesc +=len;
   }
   UEP0_T_LEN = len;
}

/** UsbDeviceHandler for a basic USB device
    
    The device is enumerated as vendorspecific device with bulk IO. 
    Just very few setup reqests are supported just enough to bring the device
    into a working state.
    If enabled the device supports the serial number string generated from the 
    serial number within the device.
*/
void HandleUsbEvents(void)
{
   uint8_t len; 

   if (UIF_TRANSFER)
   {
      switch (USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP))
      {
         // bulk out copy the data to the internal buffer
         // and call the dispatcher
         case UIS_TOKEN_OUT | 2: //ep2 out
              {
                 if (U_TOG_OK)
                 {
                    len= USB_RX_LEN;
                    memcpy(cmdbuffer,Ep2Out,len);
                    FunctionDispatcher();
                    UEP2_T_LEN = rLen;
                    UEP2_CTRL  &= ~ MASK_UEP_T_RES;
                 }  
              }
              break;   

         // bulk in 
         case UIS_TOKEN_IN | 2: //ep2 in
              {
                 UEP2_CTRL  = UEP_T_RES_NAK | (UEP2_CTRL & ~MASK_UEP_T_RES);
              }
              break;
         
         // setup    
         case UIS_TOKEN_SETUP | 0:
              {  // size must match and it has to be a std request
                 if ((USB_RX_LEN != EP0SIZE) || (Ep0Buffer[0] & 0x60))
                 {
                    bReqError = 1;
                    break;
                 }
                 bReqError = 0;            // default is no error               
                 bRequest  = Ep0Buffer[1]; // brequest
                 wValueLo  = Ep0Buffer[2];
                 reqlen    = Ep0Buffer[6]; // wlengthLo
                 switch(bRequest)
                 {
                    case USB_GET_DESCRIPTOR: 
                         {
                            switch (Ep0Buffer[3]) // wValueHi
                            {
                               case USB_DEVICE_DESCRIPTOR:
                                    pDesc   = DeviceDesc;
                                    if(reqlen > sizeof(DeviceDesc)) reqlen = sizeof(DeviceDesc);
                                    PreparePacket();
                                    break; 
                               case USB_CONFIGURATION_DESCRIPTOR:
                                    pDesc   = ConfigDesc;
                                    if(reqlen > sizeof(ConfigDesc)) reqlen = sizeof(ConfigDesc);
                                    PreparePacket();
                                    break;
#if(WITH_SERIAL)
                               case USB_STRING_DESCRIPTOR:
                                    if (wValueLo==0) // id 0
                                    { 
                                       pDesc   = LangStr;
                                       if(reqlen > sizeof(LangStr)) reqlen = sizeof(LangStr);
                                       PreparePacket();
                                       break;
                                    }
                                    if(wValueLo==3) // id 3
                                    {
                                       pDesc   = SerialStr;
                                       //hardcoded to 22 bytes
                                       if(reqlen > 22) reqlen = 22;//sizeof(SerialStr);
                                       PreparePacket();
                                       break;
                                    } 
#endif                              // all athers go to default      
                               default:
                                    bReqError = 1;
                                    break;      
                            }   
                         }
                         break;  
                    case USB_SET_ADDRESS:

                    case USB_SET_CONFIGURATION:
                         // ConfigValue=Ep0Buffer[2];  
                         // todo enable EP 2 is missing 
                         // they do it in main its just allowed after Set Config
                         UEP0_T_LEN = 0;
                         break;
                    case USB_GET_CONFIGURATION:   
                         Ep0Buffer[0]=  wValueLo; 
                         UEP0_T_LEN = 1;
                         break; 
                    default:
                         bReqError = 1;
//                       break; 
                 }        
                 
                 if (bReqError)
                 {
                    UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | 
                                UEP_R_RES_STALL | UEP_T_RES_STALL;
                 }
                 else UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG;  
              } 
              break;         
         
         // status out
         case UIS_TOKEN_IN | 0:
              {
                 switch (bRequest)
                 {
                   case USB_GET_DESCRIPTOR:
                        PreparePacket();
                        UEP0_CTRL ^=bUEP_T_TOG;
                        UIF_TRANSFER=0;
                        return;                 
                   case USB_SET_ADDRESS:
                        USB_DEV_AD  = wValueLo; //wValue Lo
                        break;
                   default:   
                   	    UEP0_T_LEN = 0;
                 }                 
              }
              //break; //falltrough
         
         // status in
         case UIS_TOKEN_OUT | 0:
         	    {
         	       UEP0_CTRL   = UEP_T_RES_NAK;
         	    }
//         	  break;   
      }
      UIF_TRANSFER=0;  
      return;     
   }
   if(UIF_BUS_RST)
   {
      UEP0_CTRL  = UEP_T_RES_NAK; 
      UEP2_CTRL  = UEP_T_RES_NAK | bUEP_AUTO_TOG; 
      USB_DEV_AD = 0;
   }
   if(UIF_SUSPEND)
   {
      UIF_SUSPEND = 0;
      return;   
   }    
   USB_INT_FG = 0xFF;
}

/** Protocol handler for the serial Interface

    The protocol used for serial transmission is basically the same as for USB
    There are however a few exptions:
    - reciving starts with a preamble 0x57 0xAB
    - all other bytes are covered with a modulo checksume 
    - sending also starts with a preamble 0x55 0xAA
    - send bytes are covered with onother checksum
*/
void HandleSerialEvents(void)
{
   register uint8_t ch;
   register uint8_t i;
   register uint8_t chk = 0;

   if (SerialGetChar()!= 0x57) return;
   if (SerialGetChar()!= 0xAB) return;
   
   // get the first 3 chars for the commandbuffer
   ch   = SerialGetChar();  // this is len         
   chk += ch;
   cmdbuffer[0] = ch; 

   ch   = SerialGetChar();        
   chk += ch;
   cmdbuffer[1] = ch; 

   ch   = SerialGetChar();       
   chk += ch;
   cmdbuffer[2] = ch; 

   for (i=0;i < cmdbuffer[1];i++)
   {  // get len chars
      ch   = SerialGetChar();  
      chk += ch;
      cmdbuffer[3+i] = ch;
   }
   ch = SerialGetChar();   // this is the modulo chksum
   if (ch != chk) return;

   TR0 = 0;
   TF0 = 0;

   FunctionDispatcher();

   chk=0;
   SerialPutChar(0x55);
   SerialPutChar(0xAA);
   
   for (i=0; i<rLen; i++)
   {
      ch   = Response[i];
      chk += ch;
      SerialPutChar(ch);
   }   
   SerialPutChar(chk);
}

  
void main (void)
{
   register uint8_t cfg;
   register uint8_t pins;

   SAFE_MOD   = 0x55;
   SAFE_MOD   = 0xAA;
   CLOCK_CFG  = (CLOCK_CFG &~MASK_SYS_CK_SEL) | 0x04;  // 12 MHz fsys

   bSoftReset = 0;
   bWrProtect = 1;
   bFirstSer  = CHIP_ID & 0x01;
   EA         = 0;
   TR0        = 0;
   TF0        = 0;

#if(AARONS_STICK)
   LED1 = 0;
   LED3 = 0; 
   
   if (SW1 == 0) // arrons board has a button there 
   {  //start the original bootloader
   	  CALL (0x3800); 
   }	 
#endif 
  
#if(WITH_SERIAL)   	 
   CreateSerialNoString();
#endif


   if(bFirstSer)
   {  // setup first serial
      SCON  = 0x50;
      T2CON = 0;
      PCON  = SMOD;               // double speed
      TMOD  = 0x20;               // T1 8 Bit autoreload
      T2MOD = bTMR_CLK | bT1_CLK; // clock speed
      TH1   = 0xF3;               // reload value
      TR1   = 1;
   }
   else
   {  // setup second serial
      SCON1  = 0x30;
      SBAUD1 = 0xF3;
   }

   cfg = CBYTE[0x3FF4];  //bootloader config
#if BOOTLOADER
   if (cfg & 0x02) pins = UDP;  
   else         	 pins = !MOSI;
#else
   pins  = 1;
#endif
   //
   //  check boot loader contitions 
   //  if one of them is true init the usb core
   //
   if ( (pins)                                                  // pin boot
        || (GLOBAL_CFG & bBOOT_LOAD)                            // software boot
        || ((CBYTE[0x0000] == 0xFF) && (CBYTE[0x0001] == 0xFF)) // no code
      )
   {  // init usb
      USB_CTRL   = 0;
      UEP2_3_MOD = bUEP2_RX_EN | bUEP2_TX_EN;   
      UEP0_DMA   = (uint16_t) &Ep0Buffer;  
      UEP2_DMA   = (uint16_t) &Ep2Out;       
      USB_CTRL   = bUC_DEV_PU_EN | bUC_INT_BUSY | bUC_DMA_EN;
      UDEV_CTRL  = bUD_PD_DIS | bUD_PORT_EN;
      USB_INT_FG = 0xFF;
      USB_INT_EN = bUIE_SUSPEND | bUIE_TRANSFER | bUIE_BUS_RST;
   }
   else
   {  // no boot contition
      if(cfg & 0x01)
      {  //timeout after 40 ms @ 12MHz
         TMOD |= 0x01;
         TL0   = -40000  & 0xFF; 
         TH0   = -40000  >> 8;
         TR0   = 1;
      }
      else bSoftReset=1;
   }
   //
   // init a 128 bit bootkey 
   // this is different from the original code 
   //
   InitBootKey();
   
   while (1)
   {
      if (bSoftReset)
      {  // do a reset
         SAFE_MOD   =  0x55;
         SAFE_MOD   =  0xAA;
         GLOBAL_CFG =  bSW_RESET;
         while(1); //and wait for it 
      }
      else
      {  // handle serial and usb
         if ((U1RI) || (RI))
         {
            HandleSerialEvents();
         }

         if (USB_INT_FG & 0x07)
         {
            HandleUsbEvents();
         }
      }
      if (TF0)
      {  //timeout
         TF0        = 0;
         TR0        = 0;
         bSoftReset = 1;
      }
   }
}
