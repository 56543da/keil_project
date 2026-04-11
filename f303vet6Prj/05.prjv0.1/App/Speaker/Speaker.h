 #ifndef SPEAKER_H
 #define SPEAKER_H
 
 #include "gd32f30x.h"
 
 void Speaker_Init(void);
 void Speaker_SetVolume(uint8_t vol);
 void Speaker_SetAlarmActive(uint8_t active);
 void Speaker_2msTask(void);
 
 #endif
