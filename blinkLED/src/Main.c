#include "CH57x_common.h"

__attribute__((section(".highcode")))
void mDelayuS( UINT16 t )
{
    UINT32 i;
#if     (FREQ_SYS == 60000000)
    i = t*15;
#elif (FREQ_SYS == 48000000)
    i = t*12;
#elif (FREQ_SYS == 40000000)
    i = t*10;
#elif   (FREQ_SYS == 32000000)
    i = t<<3;
#elif   (FREQ_SYS == 24000000)
    i = t*6;
#elif   (FREQ_SYS == 16000000)
    i = t<<2;
#elif   (FREQ_SYS == 8000000)
    i = t<<1;
#elif   (FREQ_SYS == 4000000)
    i = t;
#elif   (FREQ_SYS == 2000000)
    i = t>>1;
#elif   (FREQ_SYS == 1000000)
    i = t>>2;
#endif
    do
    {
        __nop();
    }while(--i);
}

int main(){
    //SetSysClock( CLK_SOURCE_PLL_60MHz ); //0x48
    {
        UINT32 i;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        R8_PLL_CONFIG &= ~(1<<5);   //
        R8_SAFE_ACCESS_SIG = 0;

        // PLL div
        if ( !( R8_HFCK_PWR_CTRL & RB_CLK_PLL_PON ) ){
            R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
            R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
            R8_HFCK_PWR_CTRL |= RB_CLK_PLL_PON;    // PLL power on
            for(i=0;i<2000;i++){  __nop();__nop();  }
        }
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        R16_CLK_SYS_CFG = ( 1 << 6 ) | ( 0x48 & 0x1f );   //CLK_SOURCE_PLL_60MHz = 0x48
        __nop();__nop();__nop();__nop();
        R8_SAFE_ACCESS_SIG = 0;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        R8_FLASH_CFG = 0X03;
        R8_SAFE_ACCESS_SIG = 0;
    }

  R32_PB_PD_DRV &= ~(1<<4);
  R32_PB_DIR    |= (1<<4);

  while(1){
      R32_PB_OUT |= (1<<4);
      for (int i=0;i<100;i++){
          mDelayuS(1000);
      }
      R32_PB_OUT &= ~(1<<4);
      for (int i=0;i<100;i++){
          mDelayuS(1000);
      }
  }
}
