/*==================================================================
  File Name: one_wire.c
  Author   : Emile
  ------------------------------------------------------------------
  Purpose  : This file contains one-wire routines for use with a
             DS2482 I2C to One-Wire bridge.
             The original software was based on Application Note 
             3684 example implementation using CMAXQUSB.
             The original software is Copyright (C) 2008 Maxim Integrated Products
             June 2015: Adapted for Atmel Studio / Atmega328 / Arduino Uno by Emile
  ------------------------------------------------------------------
  This file is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
 
  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this software.  If not, see <http://www.gnu.org/licenses/>.
  ================================================================== */ 
#include "one_wire.h"
#include "delay.h"         /* for delay_msec() */

// Search state
uint8_t ROM_NO[8];
int     LastDiscrepancy;
int     LastFamilyDiscrepancy;
int     LastDeviceFlag;
uint8_t crc8;
int8_t  short_detected;

//---------------------------------------------------------------------------
//-------- Basic 1-Wire functions
//---------------------------------------------------------------------------

//--------------------------------------------------------------------------
// Reset all of the devices on the 1-Wire Bus and returns the result.
//
// 1-Wire reset (Case B)
//   S AD,0 [A] 1WRS [A] Sr AD,1 [A] [Status] A [Status] A\ P
//                                   \--------/        
//                       Repeat until 1WB bit has changed to 0
//  [] indicates from slave
//
// Returns: true(1):  presence pulse(s) detected, device(s) reset
//          false(0): no presence pulses detected
//--------------------------------------------------------------------------
uint8_t OW_reset(uint8_t addr)
{
    uint8_t err, status;
    uint8_t poll_count = 0;
    
    // generate I2C start + output address to I2C bus
    err = (i2c_start_bb(addr | I2C_WRITE) == I2C_NACK);
    if (!err)
    {
        err  = (i2c_write_bb(CMD_1WRS) == I2C_NACK); // write register address
        i2c_rep_start_bb(addr | I2C_READ);
        // loop checking 1WB bit for completion of 1-Wire operation 
        // abort if poll limit reached
        status = i2c_read_bb(I2C_ACK); // Read byte
        do
        {
            if (status & STATUS_1WB) status = i2c_read_bb(I2C_ACK);
        }
        while ((status & STATUS_1WB) && (poll_count++ < DS2482_OW_POLL_LIMIT));
        status = i2c_read_bb(I2C_NACK);
        i2c_stop_bb();
        // check for failure due to poll limit reached
        if (poll_count >= DS2482_OW_POLL_LIMIT)
        {
            ds2482_reset(addr); // handle error
            return false;
        } // if
        if (status & STATUS_SD) // check for short condition
            short_detected = true;
        else short_detected = false;
        // check for presence detect
        if (status & STATUS_PPD)
            return true;
        else return false;
    } // if
    else return false;   
} // OW_reset()

//--------------------------------------------------------------------------
// Send 1 bit of communication to the 1-Wire Bus and returns the
// result 1 bit read from the 1-Wire Bus.  The parameter 'sendbit' 
// least significant bit is used and the least significant bit
// of the result is the return bit.
//
// 'sendbit' - the least significant bit is the bit to send
//
// Returns: 0:   0 bit read from sendbit
//          1:   1 bit read from sendbit
//--------------------------------------------------------------------------
uint8_t OW_touch_bit(uint8_t sendbit, uint8_t addr)
{
    uint8_t err, status;
    uint8_t poll_count = 0;
    
    // 1-Wire bit (Case B)
    //   S AD,0 [A] 1WSB [A] BB [A] Sr AD,1 [A] [Status] A [Status] A\ P
    //                                          \--------/        
    //                           Repeat until 1WB bit has changed to 0
    //  [] indicates from slave
    //  BB indicates byte containing bit value in msbit
    // generate I2C start + output address to I2C bus
    err = (i2c_start_bb(addr | I2C_WRITE) == I2C_NACK);
    if (!err)
    {
        err  = (i2c_write_bb(CMD_1WSB) == I2C_NACK); // write register address
        err  = (i2c_write_bb(sendbit ? 0x80 : 0x00) == I2C_NACK); // write register address
        i2c_rep_start_bb(addr | I2C_READ);
        // loop checking 1WB bit for completion of 1-Wire operation 
        // abort if poll limit reached
        status = i2c_read_bb(I2C_ACK); // Read byte
        do
        {
            if (status & STATUS_1WB) status = i2c_read_bb(I2C_ACK);
        }
        while ((status & STATUS_1WB) && (poll_count++ < DS2482_OW_POLL_LIMIT));
        status = i2c_read_bb(I2C_NACK);
        i2c_stop_bb();
        // check for failure due to poll limit reached
        if (poll_count >= DS2482_OW_POLL_LIMIT)
        {
            ds2482_reset(addr); // handle error
            return false;
        } // if
        // return bit state
        if (status & STATUS_SBR)
            return 1;
        else return 0;
    } // if
    else return false;
} // OW_touch_bit()

//--------------------------------------------------------------------------
// Send 1 bit of communication to the 1-Wire Bus.
// The parameter 'sendbit' least significant bit is used.
//
// 'sendbit' - 1 bit to send (least significant byte)
//--------------------------------------------------------------------------
void OW_write_bit(uint8_t sendbit, uint8_t addr)
{
   OW_touch_bit(sendbit, addr);
} // OW_write_bit()

//--------------------------------------------------------------------------
// Reads 1 bit of communication from the 1-Wire Bus and returns the result.
//
// Returns:  1 bit read from 1-Wire Net
//--------------------------------------------------------------------------
uint8_t OW_read_bit(uint8_t addr)
{
   return OW_touch_bit(0x01, addr);
} // OW_read_bit()

//--------------------------------------------------------------------------
// Send 8 bits of communication to the 1-Wire Bus and verify that the
// write operation has succeeded.
// The parameter 'sendbyte' least significant 8 bits are used.
//
// 'sendbyte' - 8 bits to send (least significant byte)
//
// Returns:  true: byte is written
//           false: error
//--------------------------------------------------------------------------
uint8_t OW_write_byte(uint8_t sendbyte, uint8_t addr)
{
    uint8_t err, status;
    uint8_t poll_count = 0;
    
    // 1-Wire Write Byte (Case B)
    //   S AD,0 [A] 1WWB [A] DD [A] Sr AD,1 [A] [Status] A [Status] A\ P
    //                                          \--------/        
    //                             Repeat until 1WB bit has changed to 0
    //  [] indicates from slave
    //  DD data to write
    // generate I2C start + output address to I2C bus
    err = (i2c_start_bb(addr | I2C_WRITE) == I2C_NACK);
    if (!err)
    {
        err  = (i2c_write_bb(CMD_1WWB) == I2C_NACK); // write register address
        err  = (i2c_write_bb(sendbyte) == I2C_NACK); // write register address
        i2c_rep_start_bb(addr | I2C_READ);
        // loop checking 1WB bit for completion of 1-Wire operation 
        // abort if poll limit reached
        status = i2c_read_bb(I2C_ACK); // Read byte
        do
        {
            if (status & STATUS_1WB) status = i2c_read_bb(I2C_ACK);
        }
        while ((status & STATUS_1WB) && (poll_count++ < DS2482_OW_POLL_LIMIT));
        status = i2c_read_bb(I2C_NACK);
        i2c_stop_bb();
        // check for failure due to poll limit reached
        if (poll_count >= DS2482_OW_POLL_LIMIT)
        {
            ds2482_reset(addr); // handle error
            return false;
        } // if
        return true;
    } // if
    else return false;
} // OW_write_byte()

//--------------------------------------------------------------------------
// Send 8 bits of read communication to the 1-Wire Bus and return the
// result 8 bits read from the 1-Wire Bus.
//
// Returns:  8 bits read from 1-Wire Net
//--------------------------------------------------------------------------
uint8_t OW_read_byte(uint8_t addr)
{
    uint8_t err, data, status;
    int poll_count = 0;
    
    // 1-Wire Read Bytes (Case C)
    //   S AD,0 [A] 1WRB [A] Sr AD,1 [A] [Status] A [Status] !A 
    //                                   \--------/        
    //                     Repeat until 1WB bit has changed to 0
    //   Sr AD,0 [A] SRP [A] E1 [A] Sr AD,1 [A] DD A\ P
    //                                  
    //  [] indicates from slave
    //  DD data read
    // generate I2C start + output address to I2C bus
    err = (i2c_start_bb(addr | I2C_WRITE) == I2C_NACK);
    if (!err)
    {
        err  = (i2c_write_bb(CMD_1WRB) == I2C_NACK); // write register address
        i2c_rep_start_bb(addr | I2C_READ);
        // loop checking 1WB bit for completion of 1-Wire operation 
        // abort if poll limit reached
        status = i2c_read_bb(I2C_ACK); // Read byte
        do
        {
            if (status & STATUS_1WB) status = i2c_read_bb(I2C_ACK);
        }
        while ((status & STATUS_1WB) && (poll_count++ < DS2482_OW_POLL_LIMIT));
        status = i2c_read_bb(I2C_NACK);
        // check for failure due to poll limit reached
        if (poll_count >= DS2482_OW_POLL_LIMIT)
        {
            ds2482_reset(addr); // handle error
            return false;
        } // if
        i2c_rep_start_bb(addr | I2C_WRITE);
        err  = (i2c_write_bb(CMD_SRP) == I2C_NACK); // write register address
        err  = (i2c_write_bb(0xE1)    == I2C_NACK); // write register address
        i2c_rep_start_bb(addr | I2C_READ);
        data = i2c_read_bb(I2C_NACK);	
        i2c_stop_bb();
        return data;
    } // if
    else return false;
} // OW_read_byte()

//--------------------------------------------------------------------------
// Send 8 bits of communication to the 1-Wire Bus and return the
// result 8 bits read from the 1-Wire Bus.  The parameter 'sendbyte'
// least significant 8 bits are used and the least significant 8 bits
// of the result is the return byte.
//
// 'sendbyte' - 8 bits to send (least significant byte)
//
// Returns:  8 bits read from sendbyte
//--------------------------------------------------------------------------
uint8_t OW_touch_byte(uint8_t sendbyte, uint8_t addr)
{
   if (sendbyte == 0xFF)
      return OW_read_byte(addr);
   else
   {  
      OW_write_byte(sendbyte, addr);
      return sendbyte;
   } // else
} // OW_touch_byte()

//--------------------------------------------------------------------------
// The 'OWBlock' transfers a block of data to and from the
// 1-Wire Net. The result is returned in the same buffer.
//
// 'tran_buf' - pointer to a block of unsigned
//              chars of length 'tran_len' that will be sent
//              to the 1-Wire Net
// 'tran_len' - length in bytes to transfer
//--------------------------------------------------------------------------
void OW_block(uint8_t *tran_buf, uint8_t tran_len, uint8_t addr)
{
   uint8_t i;

   for (i = 0; i < tran_len; i++)
      tran_buf[i] = OW_touch_byte(tran_buf[i], addr);
} // OW_block()

//--------------------------------------------------------------------------
// Find the 'first' devices on the 1-Wire network
// Return true  : device found, ROM number in ROM_NO buffer
//        false : no device present
//--------------------------------------------------------------------------
uint8_t OW_first(uint8_t addr)
{
   // reset the search state
   LastDiscrepancy       = 0;
   LastDeviceFlag        = false;
   LastFamilyDiscrepancy = 0;
   return OW_search(addr);
} // OW_first()

//--------------------------------------------------------------------------
// Find the 'next' devices on the 1-Wire network
// Return true  : device found, ROM number in ROM_NO buffer
//        false : device not found, end of search
//--------------------------------------------------------------------------
uint8_t OW_next(uint8_t addr)
{
   // leave the search state alone
   return OW_search(addr);
} // OW_next()

//--------------------------------------------------------------------------
// Verify the device with the ROM number in ROM_NO buffer is present.
// Return true  : device verified present
//        false : device not present
//--------------------------------------------------------------------------
uint8_t OW_verify(uint8_t addr)
{
    uint8_t rom_backup[8];
    uint8_t i,rslt,ld_backup,ldf_backup,lfd_backup;
    
    // keep a backup copy of the current state
    for (i = 0; i < 8; i++)
        rom_backup[i] = ROM_NO[i];
    ld_backup  = LastDiscrepancy;
    ldf_backup = LastDeviceFlag;
    lfd_backup = LastFamilyDiscrepancy;
    
    // set search to find the same device
    LastDiscrepancy = 64;
    LastDeviceFlag  = false;
    
    if (OW_search(addr))
    {
        // check if same device found
        rslt = true;
        for (i = 0; i < 8; i++)
        {
            if (rom_backup[i] != ROM_NO[i])
            {
                rslt = false;
                break;
            } // if
        } // for
    } // if
    else
        rslt = false;
    
    // restore the search state 
    for (i = 0; i < 8; i++)
        ROM_NO[i] = rom_backup[i];
    LastDiscrepancy       = ld_backup;
    LastDeviceFlag        = ldf_backup;
    LastFamilyDiscrepancy = lfd_backup;
    
    return rslt; // return the result of the verify
} // OW_verify()

//--------------------------------------------------------------------------
// Setup the search to find the device type 'family_code' on the next call
// to OW_next() if it is present.
//--------------------------------------------------------------------------
void OW_target_setup(uint8_t family_code)
{
   uint8_t i;

   // set the search state to find SearchFamily type devices
   ROM_NO[0] = family_code;
   for (i = 1; i < 8; i++)
      ROM_NO[i] = 0;
   LastDiscrepancy       = 64;
   LastFamilyDiscrepancy = 0;
   LastDeviceFlag        = false;
} // OW_target_setup()

//--------------------------------------------------------------------------
// Setup the search to skip the current device type on the next call
// to OW_next().
//--------------------------------------------------------------------------
void OW_family_skip_setup(void)
{
   // set the Last discrepancy to last family discrepancy
   LastDiscrepancy = LastFamilyDiscrepancy;

   // clear the last family discrepancy
   LastFamilyDiscrepancy = 0;

   // check for end of list
   if (LastDiscrepancy == 0) 
      LastDeviceFlag = true;
} // OW_family_skip_setup()

//--------------------------------------------------------------------------
// The 'OWSearch' function does a general search.  This function
// continues from the previous search state. The search state
// can be reset by using the 'OWFirst' function.
// This function contains one parameter 'alarm_only'.
// When 'alarm_only' is true (1) the find alarm command
// 0xEC is sent instead of the normal search command 0xF0.
// Using the find alarm command 0xEC will limit the search to only
// 1-Wire devices that are in an 'alarm' state.
//
// Returns:   true (1) : when a 1-Wire device was found and its
//                       Serial Number placed in the global ROM 
//            false (0): when no new device was found.  Either the
//                       last search was the last device or there
//                       are no devices on the 1-Wire Net.
//--------------------------------------------------------------------------
uint8_t OW_search(uint8_t addr)
{
    uint8_t id_bit_number;
    uint8_t last_zero, rom_byte_number, search_result;
    uint8_t id_bit, cmp_id_bit;
    uint8_t rom_byte_mask, search_direction, status;
    
    // initialize for search
    id_bit_number = 1;
    last_zero = 0;
    rom_byte_number = 0;
    rom_byte_mask = 1;
    search_result = false;
    crc8 = 0;
    
    // if the last call was not the last one
    if (!LastDeviceFlag)
    {       
        // 1-Wire reset
        if (!OW_reset(addr))
        {
            // reset the search
            LastDiscrepancy = 0;
            LastDeviceFlag = false;
            LastFamilyDiscrepancy = 0;
            return false;
        } // if
        
        OW_write_byte(0xF0, addr);  // issue the search command 
        
        // loop to do the search
        do
        {
            // if this discrepancy if before the Last Discrepancy
            // on a previous next then pick the same as last time
            if (id_bit_number < LastDiscrepancy)
            {
                if ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0)
                    search_direction = 1;
                else search_direction = 0;
            } // if
            else
            {  // if equal to last pick 1, if not then pick 0
                if (id_bit_number == LastDiscrepancy)
                    search_direction = 1;
                else search_direction = 0;
            } // else
            
            // Perform a triple operation on the DS2482 which will perform 2 read bits and 1 write bit
            status = ds2482_search_triplet(search_direction, addr);
            
            // check bit results in status byte
            id_bit     = ((status & STATUS_SBR) == STATUS_SBR);
            cmp_id_bit = ((status & STATUS_TSB) == STATUS_TSB);
            search_direction = ((status & STATUS_DIR) == STATUS_DIR) ? 1 : 0;
            
            // check for no devices on 1-Wire
            if ((id_bit) && (cmp_id_bit))
                break;
            else
            {
                if ((!id_bit) && (!cmp_id_bit) && (search_direction == 0))
                {
                    last_zero = id_bit_number;
                    
                    // check for Last discrepancy in family
                    if (last_zero < 9)
                        LastFamilyDiscrepancy = last_zero;
                } // if
                
                // set or clear the bit in the ROM byte rom_byte_number
                // with mask rom_byte_mask
                if (search_direction == 1)
                    ROM_NO[rom_byte_number] |= rom_byte_mask;
                else ROM_NO[rom_byte_number] &= (uint8_t)~rom_byte_mask;
                
                // increment the byte counter id_bit_number
                // and shift the mask rom_byte_mask
                id_bit_number++;
                rom_byte_mask <<= 1;
                
                // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
                if (rom_byte_mask == 0)
                {
                    calc_crc8(ROM_NO[rom_byte_number]);  // accumulate the CRC
                    rom_byte_number++;
                    rom_byte_mask = 1;
                } // if
            } // else
        } // do
        while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7
        
        // if the search was successful then
        if (!((id_bit_number < 65) || (crc8 != 0)))
        {
            // search successful so set LastDiscrepancy,LastDeviceFlag,search_result
            LastDiscrepancy = last_zero;
            
            // check for last device
            if (LastDiscrepancy == 0)
                LastDeviceFlag = true;
            
            search_result = true;
        } // if
    } // if
    
    // if no device found then reset counters so next 'search' will be like a first
    if (!search_result || (ROM_NO[0] == 0))
    {
        LastDiscrepancy = 0;
        LastDeviceFlag = false;
        LastFamilyDiscrepancy = 0;
        search_result = false;
    } // if
    return search_result;
} // OW_search()

//--------------------------------------------------------------------------
// Calculate the CRC8 of the byte value provided with the current 
// global 'crc8' value. 
// Returns current global crc8 value
//--------------------------------------------------------------------------
uint8_t calc_crc8(uint8_t data)
{
   uint8_t i; 

   // See Application Note 27
   crc8 = crc8 ^ data;
   for (i = 0; i < 8; ++i)
   {
      if (crc8 & 1)
         crc8 = (crc8 >> 1) ^ 0x8c;
      else
         crc8 = (crc8 >> 1);
   } // for
   return crc8;
} // calc_crc8()

//--------------------------------------------------------------------------
// Start a temperature conversion. At the highest resolution 
// (12-bit, default at power-up), it takes approx. 750 msec.
//
//     i2c_addr : DS2482 base address where DS18B20 is connection to
// Return true  : device found, conversion started
//        false : device not found
//--------------------------------------------------------------------------
uint8_t ds18b20_start_conversion(uint8_t i2c_addr)
{
    uint8_t rval;
    
    rval = OW_reset(i2c_addr);
    if (rval == true)
    {	// DS18B20 is present
        OW_write_byte(OW_SKIP_ROM_CMD  , i2c_addr); // only 1 sensor, use SKIP ROM command
        OW_write_byte(OW_CONVERT_T_FCMD, i2c_addr); // Start temperature conversion
    } // if
    return rval;
} // ds18b20_start_conversion()

//--------------------------------------------------------------------------
// Read a temperature from the DS18B20. At the highest resolution
// (12-bit, default at power-up), it takes approx. 750 msec.
//
//     i2c_addr : DS2482 base address where DS18B20 is connection to
// Return true  : device found, conversion started
//        false : device not found
// Variables:
//      dvc : THLT = Read from the HLT DS18B20 sensor
//            TMLT = Read from the MLT DS18B20 sensor
// Returns  : The temperature from the DS18B20 in a signed Q8.4 format.
//--------------------------------------------------------------------------
int16_t ds18b20_read(uint8_t i2c_addr, uint8_t *err, uint8_t s2)
{
    int16_t  temp = 0;   // the Temp. from the DS18B20 as an int
    uint8_t  scratch[9]; // Scratchpad of DS18B20
    uint8_t  i;
    
    *err = !OW_reset(i2c_addr); // false: error
    if (!*err)
    {
        OW_write_byte(OW_SKIP_ROM_CMD        , i2c_addr); // only 1 sensor, use SKIP ROM command
        OW_write_byte(OW_READ_SCRATCHPAD_FCMD, i2c_addr); // Read scratchpad
        if (s2)
        {	// only read 2 temperature bytes
            scratch[0] = OW_read_byte(i2c_addr);
            scratch[1] = OW_read_byte(i2c_addr);
            *err = !OW_reset(i2c_addr); // false: error
        }
        else
        {
            crc8 = 0x00;
            for (i = 0; i < 9; i++)
            {
                scratch[i] = OW_read_byte(i2c_addr);
                if (i < 8) calc_crc8(scratch[i]);
                //sprintf(s2,"%02X ",scratch[i]); uart_printf(s2);
            } // for
            *err = (crc8 != scratch[8]);
            //if (*err) uart_printf("crc error\n");
        } // else
        temp   = ((int16_t)scratch[1] << 8) | scratch[0];
        //temp <<= 3; // From Q8.4 to Q8.7
    } // if	
    return temp;    // Return value now in °C << 7
} // ds18b20_read()
