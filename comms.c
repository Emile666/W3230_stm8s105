/*==================================================================
  File Name    : comms.c
  Author       : Emile
  ------------------------------------------------------------------
  Purpose : This files contains functions for the uart commands.
  ------------------------------------------------------------------
  This is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
 
  This file is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this file. If not, see <http://www.gnu.org/licenses/>.
  ==================================================================
*/ 
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include "uart.h"
#include "comms.h"
#include "i2c_bb.h"
#include "scheduler.h"
#include "eep.h"
#include "w3230_lib.h"

extern int16_t temp1_ow_10;    // Temperature from DS18B20 in °C * 10
extern uint8_t temp1_ow_err;   // 1 = Read error from DS18B20
extern int16_t setpoint;       // local copy of SP variable
extern uint8_t fan_ctrl;       // 1 = Use one-wire sensor instead of NTC probe 1
extern int16_t pid_out;        // Output from PID controller in E-1 %
extern bool    pid_sw;         // Switch for pid_out
extern int16_t pid_fx;         // Fix-value for pid_out
char rs232_inbuf[UART_BUFLEN]; // buffer for RS232 commands
uint8_t rs232_ptr = 0;         // index in RS232 buffer

extern char version[];

/*-----------------------------------------------------------------------------
  Purpose  : Scan all devices on the I2C bus
  Variables: -
 Returns  : -
  ---------------------------------------------------------------------------*/
void i2c_scan(void)
{
    char    s[50]; // needed for printing to serial terminal
    uint8_t x = 0;
    int     i;     // Leave this as an int!
    
    xputs("I2C: ");
    for (i = 0x00; i < 0xff; i+=2)
    {
        if (i2c_start_bb(i) == I2C_ACK)
        {
            sprintf(s,"0x%0x ",i);
            xputs(s);
            x++;
        } // if
        i2c_stop_bb();
    } // for
    if (!x) xputs("-");
    xputs("\n");
} // i2c_scan()

/*-----------------------------------------------------------------------------
  Purpose  : Non-blocking RS232 command-handler via the UART
  Variables: -
  Returns  : [NO_ERR, ERR_CMD, ERR_NUM, ERR_I2C]
  ---------------------------------------------------------------------------*/
uint8_t rs232_command_handler(void)
{
  uint8_t ch;
  static bool cmd_rcvd = 0;
  
  if (!cmd_rcvd && uart_kbhit())
  { // A new character has been received
    ch = tolower(uart_read()); // get character as lowercase
    uart_write(ch);
    switch (ch)
    {
        case '\r': break;
        case '\n': cmd_rcvd  = true;
                   rs232_inbuf[rs232_ptr] = '\0';
                   rs232_ptr = 0;
                   break;
        default  : if (rs232_ptr < UART_BUFLEN-1)
                        rs232_inbuf[rs232_ptr++] = ch;
                   break;
    } // switch
  } // if
  if (cmd_rcvd)
  {
    cmd_rcvd = false;
    return execute_single_command(rs232_inbuf);
  } // if
  else if (rs232_ptr >= UART_BUFLEN-1) 
       rs232_ptr = 0; // Reset if Buffer full and no command received
  return NO_ERR; // Continu if Buffer not full and no command received
} // rs232_command_handler()

void print_value10(int16_t x)
{
    char     s[20];
    uint16_t temp = divu10(x);
    
    sprintf(s,"%d.",temp);
    xputs(s);
    temp = x - 10 * temp;
    sprintf(s,"%d\n",temp);
    xputs(s);
} // print_value10()

/*-----------------------------------------------------------------------------
  Purpose  : separates a command into sub-strings, e.g. 'wb 1234 ff' or 'sp=120'
  Variables: -
  Returns  : number of substrings found in command string s
  ---------------------------------------------------------------------------*/
uint8_t process_string(char *s, char *s1, uint16_t *d1, uint16_t *d2)
{
    uint8_t i = 0;
    uint8_t len = strlen(s);
    
    s1[0] = '\0';
    *d1   = *d2 = 0;
    while ((i < len) && (s[i] != ' ') && (s[i] != '=')) i++;
    strncpy(s1,s,i); // copy command into 1st string
    s1[i] = '\0';    // terminate string
    if (i >= len) return 1; // only 1 item in command
    else if (s[i] == '=')
    {
        *d1 = (uint16_t)strtol(&s[i+1],NULL,10);
        return 2; // 2 items, return 2nd substring as a decimal number
    } // if
    *d1 = (uint16_t)strtol(&s[++i],NULL,16); // address in hex
    while ((i < len) && (s[i] != ' ')) i++;  // find next space
    if (i >= len) return 2;                  // no more data
    *d2 = (uint16_t)strtol(&s[i+1],NULL,16); // data in hex
    return 3;
} // process_string()

/*-----------------------------------------------------------------------------
  Purpose  : send the contents of a profile set or the parameters to the UART.
  Variables: -
  Returns  : -
  ---------------------------------------------------------------------------*/
void send_eep_block(uint8_t num)
{
    char     s[35], s1[10];
    uint8_t  i,maxi,adr;
    uint16_t val;
	
    maxi = ((num < NO_OF_PROFILES) ? PROFILE_SIZE : MENU_SIZE);
    sprintf(s,"p%d ",num);
    for (i = 0; i < maxi; i++)
    {
       adr = MI_CI_TO_EEADR(num,i);
       val = eeprom_read_config(adr);
       sprintf(s1,"%d",val);
       if (i < maxi-1) strcat(s1,",");
       strcat(s,s1);
       if (strlen(s) > 25) 
       {
           xputs(s);
           s[0] = '\0';
       } // if
    } // for i
    strcat(s,"\n");
    xputs(s);
} // send_eep_block()

/*-----------------------------------------------------------------------------
  Purpose: interpret commands which are received via the UART:
   - O0/O1        : O0: select NTC temp. O1: select DS18B20 temp.
   - S0           : Display version number
     S1           : List all connected I2C devices  
     S2           : List all tasks
     S3           : Show DS18B20 temperature
  Variables: 
          s: the string that contains the command from UART
  Returns  : [NO_ERR, ERR_CMD, ERR_NUM, ERR_I2C] or ack. value for command
  ---------------------------------------------------------------------------*/
uint8_t execute_single_command(char *s)
{
   uint8_t  num    = atoi(&s[1]); // convert number in command (until space is found)
   uint8_t  rval   = NO_ERR;
   char     s2[30]; // Used for printing to uart
   char     s3[10]; // contains 1st sub-string of s
   uint16_t d1,d2;
   uint8_t  count;
   
   if (isalpha(s[1]))
   {   // 2-character command
       count = process_string(s,s3,&d1,&d2);
       if (!strcmp(s3,"sp"))
       {    // setpoint read/write
           if (count > 1)
           {   // write setpoint
               setpoint = d1;
               eeprom_write_config(EEADR_MENU_ITEM(SP), setpoint);
           } // if
           xputs("SP=");
           print_value10(setpoint);
       } // if
       else if (!strcmp(s3,"pid"))
       {   // pid-output read/write
           if (count > 1)
           {    // write pid-output
               if (d1 > 1000)
               {   // reset switch, auto-run pid controller
                   pid_fx = 0;
                   pid_sw = false;
               } // if
               else
               {   // fix pid-output to a fixed value
                   pid_fx = d1;
                   pid_sw = true;
               } // else
           } // if
           xputs("pid_out=");
           print_value10(pid_fx);
       } // else if
       else if (!strcmp(s3,"rb"))
       {   // Read Byte
           sprintf(s2,"0x%X (%d)\n",*(uint8_t *)d1,*(uint8_t *)d1);
           xputs(s2);
       } // else if
       else if (!strcmp(s3,"rw"))
       {   // Read Word
           sprintf(s2,"%X (%d)\n",*(uint16_t *)d1,*(uint16_t *)d1);
           xputs(s2);
       } // else if
       else if (!strcmp(s3,"wb"))
       {   // Write Byte
           *(uint8_t *)d1 = (uint8_t)d2;
       } // else if
       else if (!strcmp(s3,"ww"))
       {   // Write Word
           *(uint16_t *)d1 = d2;
       } // else if
   } // else
   else 
   {  // single letter command
      switch (s[0])
      {
       case 'p': // Request to send parameters or one of the profiles
               if (num <= NO_OF_PROFILES)
               {  // send Profile or parameters to the ESP8266 web-server
                      send_eep_block(num);  
               } // if
               break;
       case 's': // System commands
               switch (num)
               {
               case 0: // Revision number
                   xputs(version);
                   break;
               case 1: // List all I2C devices
                   i2c_scan();
                   break;
               case 2: // List all tasks
                   list_all_tasks(); 
                   break;	
               case 3: sprintf(s2,"ds18b20_read():%d, T=",(uint16_t)temp1_ow_err);
                   xputs(s2);
                   print_value10(temp1_ow_10);
                   break;
               default: rval = ERR_NUM;
                        break;
               } // switch
               break;
      case 'v': // A parameter of profile data-item has changed in the ESP8266 web-server
               count = process_string(s,s3,&d1,&d2);
               if (count > 1) eeprom_write_config(num,d1);
               break;
      default: rval = ERR_CMD;
	       break;
      } // switch
    } // else
   return rval;	
} // execute_single_command()
