/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Alexey Selyutin.
*
*****************************************************************************/

#ifndef __ADCC_H__
#define __ADCC_H__

typedef struct _ADC_DATA
{
    unsigned long UG1;
    unsigned long UG2;
    unsigned long UG3;
    unsigned long UG4;
    unsigned long UG5;
    unsigned long UG6;
    unsigned long UG7;
    unsigned long UG8;
}ADC_DATA;
// типы обнаруживаемых конфликтов
typedef enum __FAULT_TYPE
{
	FT_NO           = 0x00,   // Нет ошибок
	FT_GREEN_SENS   = 0x01,	// Зелёный сенсор
	FT_GREEN_CHAN   = 0x02,	// Зелёный канал неисправен
        FT_GREEN_CONFL  = 0x04,	// Зелёный - конфликт (не установлен, а напряжение есть)
	//
        FT_RED_SENS   = 0x08,	// Красный сенсор
	FT_RED_CHAN   = 0x10,	// Красный канал неисправен
        FT_RED_CONFL  = 0x20,	// Красный - конфликт (не установлен, а напряжение есть)

}FAULT_TYPE;

// кол-во каналов наряжений
#define  U_CH_N  8
// кол-во каналов токов
#define  I_CH_N  8
#define  SENS_N  9
//
extern unsigned char  U_STAT[U_CH_N]; //зафиксированное значение напряжения
extern unsigned char  I_STAT[I_CH_N]; //зафиксированное значение токов
extern unsigned char  U_STAT_PW;
extern unsigned char  U_STAT_PW_last; // последнее значение


extern unsigned long  adc_middle[I_CH_N]; //усреднение состояний каналов напряжений
extern unsigned long  udc_middle[U_CH_N]; //усреднение состояний каналов напряжений
extern unsigned long  udc_middle_PW; //усреднение состояний каналов напряжений

extern unsigned long sens_plus_count[SENS_N];
extern unsigned long sens_zero_count[SENS_N];
//extern volatile unsigned char ADC_WORK_FLAG;
extern unsigned long sens_count; //общий счетчик
/*----------------------------------------------------------------------------*/
void   adc_init();
void   Get_real_adc(ADC_DATA *data); // for cmd
void   Get_real_pwm(ADC_DATA *data); // for cmd
// interrupts
void adc0_seq1_int_handler();
//
int Check_Channels(); // for light

void IntGPIO_U(void);
void IntGPIO_PW_CONTR(void);

void Clear_UDC_Arrays();
//
void adc_seq0_int_handler();
//void adc0_seq2_int_handler();

#endif // __ADCC_H__
