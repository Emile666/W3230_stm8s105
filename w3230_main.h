/*==================================================================
  File Name    : w3230_main.h
  Author       : Emile
  ------------------------------------------------------------------
  This is the header file for w3230_main.c, which is the main-body
  W3230 temperature controller. This version is made for the STM8S105C6T6 uC.
 
  This file is part of the W3230 project.
  ------------------------------------------------------------------
  This is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
 
  This is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this file.  If not, see <http://www.gnu.org/licenses/>.
  ------------------------------------------------------------------
  Schematic of the connections to the MCU.
 
                        STM8S105C6T6 HW version 01
      MCU pin-name            Function |    MCU pin-name        Function
   ------------------------------------|--------------------------------
   01 NRST                             | 13 VDDA
   02 PA1/OSC                 HEAT     | 14 VSSA
   03 PA2/OSCOUT              COOL     | 15 PB7/AIN7            UP
   04 VSSIO_1                          | 16 PB6/AIN6            PWR
   05 VSS                              | 17 PB5/AIN5[I2C_SDA]   DOWN
   06 VCAP                             | 18 PB4/AIN4[I2C_SCL]   SET
   07 VDD                              | 19 PB3/AIN3[TIM1_ETR]  NTC1 
   08 VDDIO_1                          | 20 PB2/AIN2[TIM1_CH3N] NTC2
   09 PA3/TIM2_CH3[TIME3_CH1] SSR      | 21 PB1/AIN1[TIM1_CH2N] -
   10 PA4                     BUZZER   | 22 PB0/AIN0[TIM1_CH1N] -
   11 PA5                     -        | 23 PE7/AIN8            CC6
   12 PA6                     ISR      | 24 PE6/AIN9            CC5
   ------------------------------------|--------------------------------
   25 PE5/SPI_NSS             CC4      | 37 PE3/TIM1_BKIN       -
   26 PC1/TIM1_CH1/UART2_CK   CC1      | 38 PE2/I2C_SDA         I2C_SDA
   27 PC2/TIM1_CH2            CC2      | 39 PE1/I2C_SCL         I2C_SCL
   28 PC3/TIM1_CH3            CC3      | 40 PE0/CLK_CC0         C
   29 PC4/TIM1_CH4            -        | 41 PD0/TIM3_CH2...     B
   30 PC5/SPI_SCK             -        | 42 PD1/SWIM            SWIM
   31 VSSIO_2                          | 43 PD2/TIM3_CH1...     A 
   32 VDDIO_2                          | 44 PD3/TIM2_CH2...     DP
   33 PC6/SPI_MOSI            LD2_COOL | 45 PD4/TIM2_CH1[BEEP]  G
   34 PC7/SPI_MISO            LD3_HEAT | 46 PD5/UART2_TX        TX
   35 PG0                     F        | 47 PD6/UART2_RX        RX
   36 PG1                     E        | 48 PD7/TLI[TIM1_CH4]   D
   ---------------------------------------------------------------------

  Schematic of the bit numbers for the display LED's. Useful if custom characters are needed.
 
            * a * b   --------    *    --------       * C
                     /   a   /    g   /   a   /       e f
             d    f /       / b    f /       / b    ----
            ---     -------          -------     f / a / b
            *     /   g   /        /   g   /       ---
            c  e /       / c    e /       / c  e / g / c
                 --------    *    --------   *   ----  *
                   d         dp     d        dp   d    dp
  ==================================================================
*/
#ifndef __W3230_MAIN_H__
#define __W3230_MAIN_H__

#include <iostm8s105c6.h>
#include <stdint.h>
#include "delay.h"
     
//-------------------------------------------------------------
// PORTG
//-------------------------------------------------------------
#define S7_E     (0x02)
#define S7_F     (0x01)
#define PG_SEG7  (S7_E | S7_F)

//-------------------------------------------------------------
// PORTE
//-------------------------------------------------------------
#define CC6      (0x80) /* lower display, tens */
#define CC5      (0x40) /* lower display, ones */
#define CC4      (0x20) /* Lower display, 0.1  */
#define FAN      (0x08) /* Fan control  */
#define I2C_SDA  (0x04) /* Controlled by I2C peripheral */
#define I2C_SCL  (0x02) /* Controlled by I2C peripheral */
#define S7_C     (0x01)
#define PE_SEG7  (S7_C)
#define PE_CC    (CC6 | CC5 | CC4)

#define FAN_ON     (PE_ODR |= FAN)
#define FAN_OFF    (PE_ODR &= ~FAN)
#define FAN_STATUS ((PE_IDR & FAN) == FAN)

//-------------------------------------------------------------
// PORTD
//-------------------------------------------------------------
#define S7_D     (0x80)
#define UART_RX  (0x40) /* Controlled by UART 2 */
#define UART_TX  (0x20) /* Controlled by UART 2 */
#define S7_G     (0x10)
#define S7_DP    (0x08)
#define S7_A     (0x04)
#define SWIM     (0x02) /* Do not Initialize */
#define S7_B     (0x01)
#define PD_SEG7  (S7_D | S7_G | S7_DP | S7_A | S7_B)

//-------------------------------------------------------------
// PORTC
//-------------------------------------------------------------
#define LD3_HEAT    (0x80) /* Red LED, shows heating */
#define LD2_COOL    (0x40) /* Blue LED, shows cooling */

#define CC3         (0x08) /* this was CC_01 */
#define CC2         (0x04) /* this was CC_1 */
#define CC1         (0x02) /* this was CC_10 */
#define PC_CC       (CC3 | CC2 | CC1)

#define LED_COOL_ON  (PC_ODR |=  LD2_COOL)
#define LED_COOL_OFF (PC_ODR &= ~LD2_COOL)
#define LED_COOL_INV (PC_ODR ^=  LD2_COOL)
#define LED_HEAT_ON  (PC_ODR |=  LD3_HEAT)
#define LED_HEAT_OFF (PC_ODR &= ~LD3_HEAT)
#define LED_HEAT_INV (PC_ODR ^=  LD3_HEAT)

//-------------------------------------------------------------
// PORTB
//-------------------------------------------------------------
#define KEY_UP     (0x80)
#define KEY_PWR    (0x40)
#define KEY_DOWN   (0x20)
#define KEY_SET    (0x10)

#define AD_CHANNELS (0x0C)
#define PB_KEYS     (KEY_UP | KEY_PWR | KEY_DOWN | KEY_SET)

#define AD_NTC1     (0x03) /* AIN3 */
#define AD_NTC2     (0x02) /* AIN2 */

//-------------------------------------------------------------
// PORTA
//-------------------------------------------------------------
#define ISR_OUT  (0x40)
#define ALARM    (0x10) /* PA4, Buzzer in schematic */
#define SSR      (0x08)
#define COOL     (0x04)
#define HEAT     (0x02)

#define ALARM_ON     (PA_ODR |=  ALARM)
#define ALARM_OFF    (PA_ODR &= ~ALARM)
#define ALARM_STATUS ((PA_IDR & ALARM) == ALARM)
#define SSR_ON       (PA_ODR |=  SSR)
#define SSR_OFF      (PA_ODR &= ~SSR)
#define COOL_ON      (PA_ODR |=  COOL)
#define COOL_OFF     (PA_ODR &= ~COOL)
#define COOL_STATUS  ((PA_IDR & COOL) == COOL)
#define HEAT_ON      (PA_ODR |=  HEAT)
#define HEAT_OFF     (PA_ODR &= ~HEAT)
#define HEAT_STATUS  ((PA_IDR & HEAT) == HEAT)
#define RELAYS_OFF   (PA_ODR &= ~(HEAT | COOL))

#define SCL_in    (PE_DDR &= ~I2C_SCL) 			    /* Set SCL to input */
#define SDA_in    (PE_DDR &= ~I2C_SDA) 			    /* Set SDA to input */
#define SCL_out   {PE_DDR |=  I2C_SCL; PE_CR1 |=  I2C_SCL;} /* Set SCL to push-pull output */
#define SDA_out   {PE_DDR |=  I2C_SDA; PE_CR1 |=  I2C_SDA;} /* Set SDA to push-pull output */
#define SCL_read  (PE_IDR &   I2C_SCL) 			    /* Read from SCL */
#define SDA_read  (PE_IDR &   I2C_SDA) 			    /* Read from SDA */
#define SCL_0     {PE_ODR &= ~I2C_SCL; i2c_delay_5usec(1);} /* Set SCL to 0 */
#define SCL_1     {PE_ODR |=  I2C_SCL; i2c_delay_5usec(1);} /* Set SCL to 1 */
#define SDA_1     (PE_ODR |=  I2C_SDA) 			    /* Set SDA to 1 */
#define SDA_0     (PE_ODR &= ~I2C_SDA) 			    /* Set SDA to 0 */

// PD7 PG1 PG0 PD4 PD3 PD2 PE0 PD0
//  D   E   F   G   dp  A   C   B 
#define LED_OFF	(0x00)
#define LED_ON  (0xFF)
#define LED_MIN (0x10)
#define LED_DP  (0x08)
#define LED_EQ  (0x90)
#define LED_0	(0xE7)
#define LED_1	(0x03)
#define LED_2	(0xD5) 
#define LED_3  	(0x97)
#define LED_4  	(0x33)
#define LED_5  	(0xB6) 
#define LED_6  	(0xF6) 
#define LED_7  	(0x07)
#define LED_8  	(0xF7)
#define LED_9  	(0xB7)
#define LED_A  	(0x77)
#define LED_a	(0xD7)
#define LED_b	(0xF2) 
#define LED_C	(0xE4)
#define LED_c	(0xD0)
#define LED_d	(0xD3)
#define LED_e	(0xF5) 
#define LED_E	(0xF4)
#define LED_F	(0x74)
#define LED_H	(0x73)
#define LED_h	(0x72) 
#define LED_I	(0x03)
#define LED_J	(0xC3)
#define LED_L	(0xE0)
#define LED_n	(0x52) 
#define LED_O	(0xE7)
#define LED_o	(0xD2) 
#define LED_P	(0x75) 
#define LED_r	(0x50)	
#define LED_S	(0xB6) 
#define LED_t	(0xF0)
#define LED_U	(0xE3)
#define LED_u	(0xC2) 
#define LED_y	(0xB3)

// Function prototypes
void save_display_state(void);
void restore_display_state(void);
void multiplexer(void);
void initialise_system_clock(void);
void initialise_timer2(void);
void setup_timer2(void);
void setup_gpio_ports(void);
void adc_task(void);
void std_task(void);
void ctrl_task(void);
void prfl_task(void);
void one_wire_task(void);

#endif // __STC1000P_H__
