

#ifndef __CH57x_COMM_H__
#define __CH57x_COMM_H__

#ifdef __cplusplus
 extern "C" {
#endif


#ifndef NULL
#define  NULL			0
#endif
#define  ALL			0xFFFF


#define Debug_UART0        0
#define Debug_UART1        1
#define Debug_UART2        2
#define Debug_UART3        3
 
#ifndef DEBUG
#define DEBUG Debug_UART0
#endif
#ifdef DEBUG
#include <stdio.h>
#endif
   
#ifndef	 FREQ_SYS  
#define  FREQ_SYS		60000000
#endif   

#if ( CLK_OSC32K == 1 )
#define CAB_LSIFQ     	32000
#else
#define CAB_LSIFQ     	32768
#endif

#include <string.h>
#include <stdint.h>
#include "CH573SFR.h"
#include "core_riscv.h"

  
#define DelayMs(x)      mDelaymS(x)	  
#define DelayUs(x)      mDelayuS(x)	  


#ifdef __cplusplus
}
#endif

#endif  // __CH57x_COMM_H__	 

