 #include "Speaker.h"
#include "gd32f30x_dac.h"
 #include "gd32f30x_rcu.h"
 #include "gd32f30x_gpio.h"
 
 #define AD5160_CLK_PORT GPIOB
 #define AD5160_CLK_PIN  GPIO_PIN_13
 #define AD5160_CS_PORT  GPIOB
 #define AD5160_CS_PIN   GPIO_PIN_14
 #define AD5160_SDI_PORT GPIOB
 #define AD5160_SDI_PIN  GPIO_PIN_15
/* 功放使能（SHDN）：低电平开启，PE0 */
#define AMP_SHDN_PORT   GPIOE
#define AMP_SHDN_PIN    GPIO_PIN_0
 
 static uint8_t s_active = 0;
 static uint8_t s_volume = 10;
 static uint16_t s_amp = 200;
 static uint8_t s_toggle = 0;
 
 static void ad5160_delay(void)
 {
     __NOP(); __NOP(); __NOP(); __NOP();
 }
 
 static void ad5160_write(uint8_t val)
 {
     gpio_bit_reset(AD5160_CS_PORT, AD5160_CS_PIN);
     for(int i = 7; i >= 0; i--)
     {
         if(val & (1 << i)) gpio_bit_set(AD5160_SDI_PORT, AD5160_SDI_PIN);
         else gpio_bit_reset(AD5160_SDI_PORT, AD5160_SDI_PIN);
         ad5160_delay();
         gpio_bit_set(AD5160_CLK_PORT, AD5160_CLK_PIN);
         ad5160_delay();
         gpio_bit_reset(AD5160_CLK_PORT, AD5160_CLK_PIN);
     }
     gpio_bit_set(AD5160_CS_PORT, AD5160_CS_PIN);
 }
 
 void Speaker_Init(void)
 {
     rcu_periph_clock_enable(RCU_GPIOA);
     rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_DAC);
    rcu_periph_clock_enable(RCU_GPIOE);
 
     gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, GPIO_PIN_4);
 
     gpio_init(AD5160_CLK_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, AD5160_CLK_PIN);
     gpio_init(AD5160_CS_PORT,  GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, AD5160_CS_PIN);
     gpio_init(AD5160_SDI_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, AD5160_SDI_PIN);
    /* 功放 SHDN 默认关闭（高电平） */
    gpio_init(AMP_SHDN_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, AMP_SHDN_PIN);
    gpio_bit_set(AMP_SHDN_PORT, AMP_SHDN_PIN);
 
     gpio_bit_set(AD5160_CS_PORT, AD5160_CS_PIN);
     gpio_bit_reset(AD5160_CLK_PORT, AD5160_CLK_PIN);
     gpio_bit_reset(AD5160_SDI_PORT, AD5160_SDI_PIN);
 
    /* 直接寄存器方式初始化 DAC0, 禁用触发与波形，开启输出 */
    DAC_CTL &= ~DAC_CTL_DTEN0;                 /* 关闭触发 */
    DAC_CTL &= ~DAC_CTL_DWM0;                  /* 关闭波形 */
    DAC_CTL |= DAC_CTL_DEN0;                   /* 使能 DAC0 */
    DAC0_R12DH = 2048;                         /* 中点电平 */
 
     Speaker_SetVolume(s_volume);
 }
 
 void Speaker_SetVolume(uint8_t vol)
 {
     if(vol > 31) vol = 31;
     s_volume = vol;
     uint8_t wiper = (uint8_t)((vol * 255) / 31);
     ad5160_write(wiper);
     s_amp = (uint16_t)((vol * 1024) / 31 + 50);
 }
 
 void Speaker_SetAlarmActive(uint8_t active)
 {
     s_active = active ? 1 : 0;
    /* SHDN 低电平开启，静音时拉高关闭功放 */
    if(s_active){
        gpio_bit_reset(AMP_SHDN_PORT, AMP_SHDN_PIN);
    }else{
        gpio_bit_set(AMP_SHDN_PORT, AMP_SHDN_PIN);
    }
     if(!s_active)
     {
        DAC0_R12DH = 2048;
     }
 }
 
 void Speaker_2msTask(void)
 {
     if(!s_active)
     {
         return;
     }
     s_toggle ^= 1;
     if(s_toggle)
     {
         uint16_t v = (uint16_t)(2048 + s_amp);
         if(v > 4095) v = 4095;
        DAC0_R12DH = v;
     }
     else
     {
         uint16_t v = (uint16_t)(2048 - s_amp);
        DAC0_R12DH = v;
     }
 }
