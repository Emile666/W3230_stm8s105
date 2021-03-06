/*==================================================================
   File Name    : i2c_bb.h
   Author       : Emile
  ------------------------------------------------------------------
  Purpose : This files contains the generic I2C related functions 
            for the STM8 uC. It is needed for every I2C device
	    connected to the STM8 uC.
  ------------------------------------------------------------------
  This is free software: you can redistribute it and/or modify
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
#ifndef _I2C_BB_H
#define _I2C_BB_H

#include <stdbool.h>
#include <intrinsics.h> 
#include <stdint.h>
#include "delay.h"
#include "w3230_main.h"

//----------------------------
// I2C defines
//----------------------------
#define I2C_ACK     (0)
#define I2C_NACK    (1)
#define I2C_ERROR   (2)
#define I2C_WRITE   (0)
#define I2C_READ    (1)
#define I2C_RETRIES (3)

// DS2482 Configuration Register
// Standard speed (1WS==0), Strong Pullup disabled (SPU==0), Active Pullup enabled (APU==1)
#define DS2482_ADDR          (0x30)
#define DS2482_CONFIG        (0xE1)
#define DS2482_OW_POLL_LIMIT  (200)

// DS2482 commands
#define CMD_DRST   (0xF0)
#define CMD_WCFG   (0xD2)
#define CMD_CHSL   (0xC3)
#define CMD_SRP    (0xE1)
#define CMD_1WRS   (0xB4)
#define CMD_1WWB   (0xA5)
#define CMD_1WRB   (0x96)
#define CMD_1WSB   (0x87)
#define CMD_1WT    (0x78)

// DS2482 status bits 
#define STATUS_1WB  (0x01)
#define STATUS_PPD  (0x02)
#define STATUS_SD   (0x04)
#define STATUS_LL   (0x08)
#define STATUS_RST  (0x10)
#define STATUS_SBR  (0x20)
#define STATUS_TSB  (0x40)
#define STATUS_DIR  (0x80)

//----------------------------
// I2C-peripheral routines
//----------------------------
void    i2c_delay_5usec(uint16_t x);    // Standard I2C bus delay
uint8_t i2c_reset_bus(void);
void    i2c_init_bb(void);              // Initializes the I2C Interface. Needs to be called only once
uint8_t i2c_start_bb(uint8_t addr);     // Issues a start condition and sends address and transfer direction
uint8_t i2c_rep_start_bb(uint8_t addr); // Issues a repeated start condition and sends address and transfer direction
void    i2c_stop_bb(void);              // Terminates the data transfer and releases the I2C bus
uint8_t i2c_write_bb(uint8_t data);     // Send one byte to I2C device
uint8_t i2c_read_bb(uint8_t ack);       // Read one byte from I2C device and calls i2c_stop()

int8_t  ds2482_reset(uint8_t addr);
int8_t  ds2482_write_config(uint8_t addr);
int8_t  ds2482_detect(uint8_t addr);
uint8_t ds2482_search_triplet(uint8_t search_direction, uint8_t addr);

#endif
