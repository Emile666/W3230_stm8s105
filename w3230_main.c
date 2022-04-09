/*==================================================================
  File Name    : w3230_main.c
  Author       : Emile
  ------------------------------------------------------------------
  Purpose : This files contains the main() function and all the 
            hardware related functions for the STM8S105C6T6 uC.
            It is meant for the custom-build W3230 hardware.
  ------------------------------------------------------------------
  This file is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
 
  This is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this file.  If not, see <http://www.gnu.org/licenses/>.
  ==================================================================
*/ 
#include <intrinsics.h> 
#include <stdio.h>
#include "w3230_main.h"
#include "w3230_lib.h"
#include "scheduler.h"
#include "adc.h"
#include "eep.h"
#include "i2c_bb.h"
#include "one_wire.h"
#include "comms.h"
#include "uart.h"

// Version number for W3230 firmware
char version[] = "W3230-stm8s105c6 V0.11\n";

// Global variables
bool      ad_err1 = false; // used for adc range checking
bool      ad_err2 = false; // used for adc range checking
bool      probe2  = false; // cached flag indicating whether 2nd probe is active
bool      show_sa_alarm = false;
bool      ad_ch   = false; // used in adc_task()
uint16_t  ad_ntc1 = (512L << FILTER_SHIFT);
uint16_t  ad_ntc2 = (512L << FILTER_SHIFT);
int16_t   temp_ntc1;         // The temperature in E-1 °C from NTC probe 1
int16_t   temp_ntc2;         // The temperature in E-1 °C from NTC probe 2
uint8_t   mpx_nr = 0;        // Used in multiplexer() function
int16_t   pwr_on_tmr = 1000; // Needed for 7-segment display test
int16_t   temp1_ow_10;       // Temperature from DS18B20 in °C * 10
uint8_t   temp1_ow_err = 0;  // 1 = Read error from DS18B20
uint8_t   fan_ctrl = 0;      // 1 = Use one-wire sensor for FAN control
bool      pid_sw = false;    // Switch for pid_out
int16_t   pid_fx = 0;        // Fix-value for pid_out
uint16_t  hp_lim;            // Heating-power limit for SSR & PID-controller
uint16_t  hp_tot;            // Rated power for heating element

// External variables, defined in other files
extern uint8_t  top_10, top_1, top_01; // values of 10s, 1s and 0.1s
extern uint8_t  bot_10, bot_1, bot_01; // values of 10s, 1s and 0.1s
extern bool     pwr_on;           // True = power ON, False = power OFF
extern uint8_t  sensor2_selected; // DOWN button pressed < 3 sec. shows 2nd temperature / pid_output
extern bool     minutes;          // timing control: false = hours, true = minutes
extern bool     menu_is_idle;     // No menus in STD active
extern bool     fahrenheit;       // false = Celsius, true = Fahrenheit
extern uint16_t cooling_delay;    // Initial cooling delay
extern uint16_t heating_delay;    // Initial heating delay
extern int16_t  setpoint;         // local copy of SP variable
extern uint8_t  ts;               // Parameter value for sample time [sec.]
extern int16_t  pid_out;          // Output from PID controller in E-1 %
extern uint32_t t2_millis;        // needed for delay_msec()
extern uint8_t  rs232_inbuf[];

/*-----------------------------------------------------------------------------
  Purpose  : This routine multiplexes the 6 segments of the 7-segment displays.
             It runs at 1 kHz, so full update frequency is 166 Hz.
  Variables: -
  Returns  : -
  ---------------------------------------------------------------------------*/
void multiplexer(void)
{
    // Disable all 7-segment LEDs and common-cathode pins
    // PD7 PG1 PG0 PD4 PD3 PD2 PE0 PD0
    //  D   E   F   G   dp  A   C   B 
    PG_ODR    &= ~PG_SEG7; // Clear LEDs
    PD_ODR    &= ~PD_SEG7; // Clear LEDs
    PE_ODR    &= ~PE_SEG7; // Clear LEDs
    PC_ODR    |=  PC_CC;   // Disable common-cathodes top-display
    PE_ODR    |=  PE_CC;   // Disable common-cathodes bottom-display
    
    switch (mpx_nr)
    {
        case 0: // output 10s digit top display
            PG_ODR |= ((top_10 >> 5) & PG_SEG7); // Update 7-segment E+F
            PD_ODR |= (top_10 & PD_SEG7);        // Update 7-segments
            PE_ODR |= ((top_10 >> 1) & PE_SEG7); // Update 7-segment C
            PC_ODR &= ~CC1;    // Enable  common-cathode for 10s
            mpx_nr = 1;
            break;
        case 1: // output 1s digit top display
            PG_ODR |= ((top_1 >> 5) & PG_SEG7); // Update 7-segment E+F
            PD_ODR |= (top_1 & PD_SEG7);        // Update 7-segments
            PE_ODR |= ((top_1 >> 1) & PE_SEG7); // Update 7-segment C
            PC_ODR &= ~CC2;    // Enable  common-cathode for 1s
            mpx_nr = 2;
            break;
        case 2: // output 01s digit top display
            PG_ODR |= ((top_01 >> 5) & PG_SEG7); // Update 7-segment E+F
            PD_ODR |= (top_01 & PD_SEG7);        // Update 7-segments
            PE_ODR |= ((top_01 >> 1) & PE_SEG7); // Update 7-segment C
            PC_ODR &= ~CC3;    // Enable common-cathode for 0.1s
            mpx_nr = 3;
            break;
        case 3: // outputs 10s digit bottom display
            PG_ODR |= ((bot_10 >> 5) & PG_SEG7); // Update 7-segment E+F
            PD_ODR |= (bot_10 & PD_SEG7);        // Update 7-segments
            PE_ODR |= ((bot_10 >> 1) & PE_SEG7); // Update 7-segment C
            PE_ODR &= ~CC4;     // Enable common-cathode for 10s
            mpx_nr = 4;
            break;
        case 4: // outputs 1s digit bottom display
            PG_ODR |= ((bot_1 >> 5) & PG_SEG7); // Update 7-segment E+F
            PD_ODR |= (bot_1 & PD_SEG7);        // Update 7-segments
            PE_ODR |= ((bot_1 >> 1) & PE_SEG7); // Update 7-segment C
            PE_ODR &= ~CC5;     // Enable common-cathode for 10s
            mpx_nr = 5;
            break;
        case 5: // outputs 0.10 digit bottom display
            PG_ODR |= ((bot_01 >> 5) & PG_SEG7); // Update 7-segment E+F
            PD_ODR |= (bot_01 & PD_SEG7);        // Update 7-segments
            PE_ODR |= ((bot_01 >> 1) & PE_SEG7); // Update 7-segment C
            PE_ODR &= ~CC6;     // Enable common-cathode for 0.1s
        default: // FALL-THROUGH (less code-size)
            mpx_nr = 0;
            break;
    } // switch            
} // multiplexer()

/*-----------------------------------------------------------------------------
  Purpose  : This is the interrupt routine for the Timer 2 Overflow handler.
             It runs at 1 kHz and drives the scheduler and the multiplexer.
             Measured timing: 1.0 msec and 9 usec duration (9.1 %).
  Variables: -
  Returns  : -
  ---------------------------------------------------------------------------*/
#pragma vector=TIM2_OVR_UIF_vector
__interrupt void TIM2_UPD_OVF_IRQHandler(void)
{
    PA_ODR |= ISR_OUT; // Time-measurement interrupt routine
    t2_millis++;       // update millisecond counter
    scheduler_isr();   // Run scheduler interrupt function
    
    if (!pwr_on)
    {   // Display OFF on dispay
        top_10 = LED_O;
        top_1  = top_01 = LED_F;
        bot_10 = bot_1 = bot_01 = LED_OFF;
        pwr_on_tmr = 1000; // 1 second
    } // if
    else if (pwr_on_tmr > 0)
    {	// 7-segment display test for 1 second
        pwr_on_tmr--;
        top_10 = top_1 = top_01 = LED_ON;
        bot_10 = bot_1 = bot_01 = LED_ON;
    } // else if
    multiplexer();        // Run multiplexer for Display
    PA_ODR   &= ~ISR_OUT; // Time-measurement interrupt routine
    TIM2_SR1_UIF = 0;     // Reset interrupt (UIF bit) so it will not fire again straight away.
} // TIM2_UPD_OVF_IRQHandler()

/*-----------------------------------------------------------------------------
  Purpose  : This routine initialises the system clock to run at 16 MHz.
             It uses the internal HSI oscillator.
  Variables: -
  Returns  : -
  ---------------------------------------------------------------------------*/
void initialise_system_clock(void)
{
    CLK_ICKR       = 0;           //  Reset the Internal Clock Register.
    CLK_ICKR_HSIEN = 1;           //  Enable the HSI.
    while (CLK_ICKR_HSIRDY == 0); //  Wait for the HSI to be ready for use.
    CLK_CKDIVR     = 0;           //  Ensure the clocks are running at full speed.
 
    // The datasheet lists that the max. ADC clock is equal to 6 MHz (4 MHz when on 3.3V).
    // Because fMASTER is now at 16 MHz, we need to set the ADC-prescaler to 4.
    ADC_CR1_SPSEL = 0x02;         //  Set prescaler (SPSEL bits) to 4, fADC = 4 MHz
    CLK_SWIMCCR   = 0x00;         //  Set SWIM to run at clock / 2.
    CLK_SWR       = 0xE1;         //  Use HSI as the clock source.
    CLK_SWCR      = 0;            //  Reset the clock switch control register.
    CLK_SWCR_SWEN = 1;            //  Enable switching.
    while (CLK_SWCR_SWBSY != 0);  //  Pause while the clock switch is busy.
} // initialise_system_clock()

/*-----------------------------------------------------------------------------
  Purpose  : This routine initialises Timer 2 to generate a 1 kHz interrupt.
             16 MHz / (16 * 1000) = 1000 Hz (1000 = 0x03E8)
  Variables: -
  Returns  : -
  ---------------------------------------------------------------------------*/
void setup_timer2(void)
{
    TIM2_PSCR    = 0x04; //  Prescaler = 16
    TIM2_ARRH    = 0x03; //  High byte of 1000
    TIM2_ARRL    = 0xE8; //  Low  byte of 1000
    TIM2_IER_UIE = 1;    //  Enable the update interrupts, disable all others
    TIM2_CR1_CEN = 1;    //  Finally enable the timer
} // setup_timer2()

/*-----------------------------------------------------------------------------
  Purpose  : This routine initialises all the GPIO pins of the STM8 uC.
             See header-file for a pin-connections and functions.
  Variables: -
  Returns  : -
  ---------------------------------------------------------------------------*/
void setup_gpio_ports(void)
{
    PA_DDR     |=  (SSR | COOL | HEAT | ALARM | ISR_OUT); // Set as output
    PA_CR1     |=  (SSR | COOL | HEAT | ALARM | ISR_OUT); // Set to Push-Pull
    PA_ODR     &= ~(SSR | COOL | HEAT | ALARM | ISR_OUT); // Disable PORTA outputs
    
    PB_DDR     &= ~(PB_KEYS | AD_CHANNELS); // Set as input
    PB_CR1     &= ~AD_CHANNELS; // Set to floating-inputs (required by ADC)
    PB_CR1     |= PB_KEYS;      // Enable pull-ups
    
    PC_DDR     |=  (LD3_HEAT | LD2_COOL | PC_CC); // Set as outputs
    PC_CR1     |=  (LD3_HEAT | LD2_COOL | PC_CC); // Set to Push-Pull
    PC_ODR     &= ~(LD3_HEAT | LD2_COOL);         // Disable LEDs
    PC_ODR     |=  PC_CC;                         // Disable Common-Cathodes
    
    PD_DDR     |=  PD_SEG7;          // Set 7-segment/key pins as output
    PD_CR1     |=  PD_SEG7;          // Set 7-segment/key pins to Push-Pull
    PD_ODR     &= ~PD_SEG7;          // Turn off all 7-segment/key pins
    
    PE_ODR     |=  (I2C_SCL | I2C_SDA); // Must be set here, or I2C will not work
    PE_DDR     |=  (PE_CC | PE_SEG7 | I2C_SCL | I2C_SDA | FAN); // Set as outputs
    PE_CR1     |=  (PE_CC | PE_SEG7 | FAN);                    // Set to Push-Pull
    PE_ODR     &= ~(PE_SEG7 | FAN);     // Turn off 7-segment C and FAN
    PE_ODR     |=  PE_CC;               // Disable Common-Cathodes
    
    PG_DDR     |=  PG_SEG7;      // Set 7-segment/key pins as output
    PG_CR1     |=  PG_SEG7;      // Set 7-segment/key pins to Push-Pull
    PG_ODR     &= ~PG_SEG7;      // Turn off all 7-segment/key pins
} // setup_output_ports()

/*-----------------------------------------------------------------------------
  Purpose  : This task is called every 500 msec. and processes the NTC 
             temperature probes from NTC1 (PB3/AIN3) and NTC2 (PB2/AIN2)
  Variables: -
  Returns  : -
  ---------------------------------------------------------------------------*/
void adc_task(void)
{
  uint16_t temp;
  
  if (ad_ch)
  {  // Process NTC probe 1
     temp       = read_adc(AD_NTC1);
     ad_ntc1    = ((ad_ntc1 - (ad_ntc1 >> FILTER_SHIFT)) + temp);
     temp_ntc1  = ad_to_temp(ad_ntc1,&ad_err1);
     temp_ntc1 += eeprom_read_config(EEADR_MENU_ITEM(tc));
  } // if
  else
  {  // Process NTC probe 2
     temp       = read_adc(AD_NTC2);
     ad_ntc2    = ((ad_ntc2 - (ad_ntc2 >> FILTER_SHIFT)) + temp);
     temp_ntc2  = ad_to_temp(ad_ntc2,&ad_err2);
     temp_ntc2 += eeprom_read_config(EEADR_MENU_ITEM(tc2));
  } // else
  ad_ch = !ad_ch;
} // adc_task()

/*-----------------------------------------------------------------------------
  Purpose  : This task is called every 100 msec. and creates a slow PWM signal
             from pid_output: T = 12.5 seconds. This signal can be used to
             drive a Solid-State Relay (SSR).
  Variables: pid_out (global) is used
  Returns  : -
  ---------------------------------------------------------------------------*/
void pid_to_time(void)
{
    static uint8_t std_ptt = 1; // state [on, off]
    static uint8_t ltmr    = 0; // #times to set SSR to 0
    static uint8_t htmr    = 0; // #times to set SSR to 1
    uint16_t x;                 // temp. variable
     
    x = pid_out >> 3; // divide by 8 to give 1.25 * pid_out
    
    switch (std_ptt)
    {
        case 0: // OFF
            if (ltmr == 0)
            {   // End of low-time
                htmr = (uint8_t)x; // htmr = 1.25 * pid_out
                if ((htmr > 0) && pwr_on) std_ptt = 1;
            } // if
            else ltmr--; // decrease timer
            SSR_OFF;     // SSR output = 0
            break;
        case 1: // ON
            if (htmr == 0)
            {   // End of high-time
                ltmr = (uint8_t)(125 - x); // ltmr = 1.25 * (100 - pid_out)
                if ((ltmr > 0) || !pwr_on) std_ptt = 0;
            } // if
            else htmr--; // decrease timer
            SSR_ON;      // SSR output = 1
            break;
    } // switch
} // pid_to_time()

/*-----------------------------------------------------------------------------
  Purpose  : This task is called every 100 msec. and reads the buttons, runs
             the STD and updates the SSR slow-PWM from the PID-output.
  Variables: -
  Returns  : -
  ---------------------------------------------------------------------------*/
void std_task(void)
{
    read_buttons(); // reads the buttons keys, result is stored in _buttons
    menu_fsm();     // Finite State Machine menu
    pid_to_time();  // Make Slow-PWM signal and send to SSR output-port
} // std_task()

/*-----------------------------------------------------------------------------
  Purpose  : This task is called every second and contains the main control
             task for the device. It also calls temperature_control() / 
             pid_ctrl() and one_wire_task().
  Variables: -
  Returns  : -
  ---------------------------------------------------------------------------*/
void ctrl_task(void)
{
   int16_t sa, diff, temp;
   
    if (eeprom_read_config(EEADR_MENU_ITEM(CF))) // true = Fahrenheit
         fahrenheit = true;
    else fahrenheit = false;
    if (eeprom_read_config(EEADR_MENU_ITEM(HrS))) // true = hours
         minutes = false; // control-timing is in hours 
    else minutes = true;  // control-timing is in minutes

   // Start with updating the alarm
   // cache whether the 2nd probe is enabled or not.
   if (eeprom_read_config(EEADR_MENU_ITEM(Pb2))) 
        probe2 = true;
   else probe2 = false;
   if (ad_err1 || (ad_err2 && probe2))
   {
       ALARM_ON;   // enable the piezo buzzer
       RELAYS_OFF; // disable the output relays
       if (menu_is_idle)
       {  // Make it less anoying to nagivate menu during alarm
          top_10 = LED_A;
	  top_1  = LED_L;
	  top_01 = LED_OFF;
       } // if
       cooling_delay = heating_delay = 60;
   } else {
       ALARM_OFF;   // reset the piezo buzzer
//       if(((uint8_t)eeprom_read_config(EEADR_MENU_ITEM(rn))) < THERMOSTAT_MODE)
//            led_e |=  LED_SET; // Indicate profile mode
//       else led_e &= ~LED_SET;
 
       ts        = eeprom_read_config(EEADR_MENU_ITEM(Ts));  // Read Ts [seconds]
       sa        = eeprom_read_config(EEADR_MENU_ITEM(SA));  // Show Alarm parameter
       fan_ctrl  = eeprom_read_config(EEADR_MENU_ITEM(FAn)); // 1 = use OW sensor for fan-control
       temp      = temp_ntc1;   // use NTC1 temp. sensor
       
       //-------------------------------------------------------------------------------
       // This is the compressor-fan control, it uses the One-Wire temperature
       // sensor to detect when the compressor of the climate-chamber is getting too
       // warm. If so, the fan is enabled. 
       //-------------------------------------------------------------------------------
       if (fan_ctrl)
       {
           if (temp1_ow_10 >= eeprom_read_config(EEADR_MENU_ITEM(FHI)))
               FAN_ON;
           else if (temp1_ow_10 <= eeprom_read_config(EEADR_MENU_ITEM(FLo)))
               FAN_OFF;
       } // if
       if (sa)
       {
           if (minutes) // is timing-control in minutes?
                diff = temp - setpoint;
           else diff = temp - eeprom_read_config(EEADR_MENU_ITEM(SP));

           if (diff < 0) diff = -diff;
	   if (sa < 0)
           {
  	      sa = -sa;
              if (diff <= sa)
                   ALARM_ON;  // enable the piezo buzzer
              else ALARM_OFF; // reset the piezo buzzer
           } else {
              if (diff >= sa)
                   ALARM_ON;  // enable the piezo buzzer
              else ALARM_OFF; // reset the piezo buzzer
	   } // if
       } // if
       // Thermostat control and PID-control
       temperature_control(temp); // Run thermostat control logic
       if (ts == 0) // PID Ts parameter is 0?
            pid_out = 0;          // Disable PID-output
       else pid_control(temp);    // Run PID controller in parallel with thermostat
       // --------- Manual switch/fix for pid-output --------------------
       if (pid_sw) pid_out = pid_fx; // can be set via UART

       if (menu_is_idle)          // show temperature if menu is idle
       {
           if ((ALARM_STATUS) && show_sa_alarm)
           {
               top_10 = LED_S;
               top_1  = LED_A;
	       top_01 = LED_OFF;
           } // if
           else 
           {
               switch (sensor2_selected)
               {
                case 0:
                     value_to_led(temp    ,LEDS_TEMP,ROW_TOP); // display temperature on top row
                     value_to_led(setpoint,LEDS_TEMP,ROW_BOT); // display setpoint on bottom row
                     break;
                case 1:
                     value_to_led(temp_ntc2,LEDS_TEMP,ROW_TOP); // display temp_ntc2 on top row
                     bot_10 = LED_t; bot_1 = LED_2; bot_01 = LED_OFF;
                     break;
                case 2:
                     value_to_led(temp1_ow_10,LEDS_TEMP,ROW_TOP); // display OW temp. on top row
                     bot_10 = LED_O; bot_1 = LED_n; bot_01 = LED_E;
                     break;
                case 3:
                     value_to_led(pid_out,LEDS_TEMP,ROW_TOP); // display pid-output on top row
                     bot_10 = LED_P; bot_1 = LED_I; bot_01 = LED_d;
                     break;
               } // switch
           } // else
           show_sa_alarm = !show_sa_alarm;
       } // if
   } // else
   one_wire_task(); // read from one-wire temperature sensor
} // ctrl_task()

/*-----------------------------------------------------------------------------
  Purpose  : This task is called every minute or every hour and updates the
             current running temperature profile.
  Variables: minutes: timing control: false = hours, true = minutes
  Returns  : -
  ---------------------------------------------------------------------------*/
void prfl_task(void)
{
    static uint8_t min = 0;
    
    if (minutes)
    {   // call every minute
        update_profile();
        min = 0;
    } else {
        if (++min >= 60)
        {   // call every hour
            min = 0;
            update_profile(); 
        } // if
    } // else
} // prfl_task();

/*--------------------------------------------------------------------
  Purpose  : This is the task that processes the temperatures from
	     the One-Wire sensor. The sensor has its
	     own DS2482 I2C-to-One-Wire bridge. Since this sensor has 4
	     fractional bits (1/2, 1/4, 1/8, 1/16), a signed Q8.4 format
	     would be sufficient. 
	     This task is called every second so that every 2 seconds a new
	     temperature is present.
  Variables: temp1_ow_84 : Temperature read from sensor in Q8.4 format
			 temp1_ow_err: 1=error
  Returns  : -
  --------------------------------------------------------------------*/
void one_wire_task(void)
{
    static int ow_std = 0; // internal state
    
    switch (ow_std)
    {   
        case 0: // Start Conversion
            ds18b20_start_conversion(DS2482_ADDR);
            ow_std = 1;
            break;
        case 1: // Read Thlt device
            temp1_ow_10   = ds18b20_read(DS2482_ADDR, &temp1_ow_err,1);
            temp1_ow_10  *= 5; // * 5/8 = 10/16
            temp1_ow_10  += 4; // rounding
            temp1_ow_10 >>= 3; // div 8
            ow_std = 0;
            break;
    } // switch
} // one_wire_task()

/*-----------------------------------------------------------------------------
  Purpose  : This is the main entry-point for the entire program.
             It initialises everything, starts the scheduler and dispatches
             all tasks.
  Variables: -
  Returns  : -
  ---------------------------------------------------------------------------*/
int main(void)
{
    char    s[30];      // Needed for xputs() and sprintf()
    int8_t  ok;
    
    __disable_interrupt();
    initialise_system_clock(); // Set system-clock to 16 MHz
    setup_gpio_ports();        // Init. needed output-ports for LED and keys
    setup_timer2();            // Set Timer 2 to 1 kHz
    pwr_on = eeprom_read_config(EEADR_POWER_ON); // check pwr_on flag
    i2c_init_bb();             // Init. I2C bus
    uart_init();               // Init. serial communication
    
    // Initialise all tasks for the scheduler
    scheduler_init();                    // clear task_list struct
    add_task(adc_task ,"ADC",  0,  500); // every 500 msec.
    add_task(std_task ,"STD", 50,  100); // every 100 msec.
    add_task(ctrl_task,"CTL",200, 1000); // every second
    add_task(prfl_task,"PRF",300,60000); // every minute / hour
    __enable_interrupt();
    xputs(version); // print version number
    
    ok = ds2482_detect(DS2482_ADDR);   // true
    xputs("ds2482_detect:");
    if (ok) xputs("1\n");
    else    xputs("0\n");

    while (1)
    {   // background-processes
        dispatch_tasks();     // Run task-scheduler()
        switch (rs232_command_handler()) // run command handler continuously
        {
            case ERR_CMD: xputs("Cmd Error\n"); break;
            case ERR_NUM: sprintf(s,"Nr Error (%s)\n",rs232_inbuf);
                          xputs(s);  
                          break;
            default     : break;
        } // switch
    } // while
} // main()
