/*
 * bsp.h
 *
 *  Created on: Feb 11, 2025
 *      Author: RushikeshKamble
 */

#ifndef INC_BSP_H_
#define INC_BSP_H_

/*******************************************************************************
 *  HEARDER FILE INCLUDES
 ********************************************************************************/
#include"main.h"
/*******************************************************************************
 *  MACRO DEFINITION
 ******************************************************************************/
#define FLASH_SECTOR2_BASE_ADDRESS 0x08008000U
#define BL_VERSION 0x10
#define BL_GET_VER				0x51
/*This command is used to know what are the commands supported by the bootloader*/
#define BL_GET_HELP				0x52
/*This command is used to read the MCU chip identification number*/
#define BL_GET_CID				0x53
/*This command is used to read the FLASH Read Protection level.*/
#define BL_GET_RDP_STATUS		0x54
/*This command is used to jump bootloader to specified address.*/
#define BL_GO_TO_ADDR			0x55
/*This command is used to mass erase or sector erase of the user flash .*/
#define BL_FLASH_ERASE          0x56
/*This command is used to write data in to different memories of the MCU*/
#define BL_MEM_WRITE			0x57
/*This command is used to enable or disable read/write protect on different sectors of the user flash .*/
#define BL_EN_RW_PROTECT		0x58
/*This command is used to read data from different memories of the microcontroller.*/
#define BL_MEM_READ				0x59
/*This command is used to read all the sector protection status.*/
#define BL_READ_SECTOR_P_STATUS	0x5A
/*This command is used to read the OTP contents.*/
#define BL_OTP_READ				0x5B
/*This command is used disable all sector read/write protection*/
#define BL_DIS_R_W_PROTECT				0x5C

/* ACK and NACK bytes*/
#define BL_ACK   0XA5
#define BL_NACK  0X7F

/*CRC*/
#define VERIFY_CRC_FAIL    1
#define VERIFY_CRC_SUCCESS 0

#define ADDR_VALID 0x00
#define ADDR_INVALID 0x01

#define INVALID_SECTOR 0x04

/*Some Start and End addresses of different memories of STM32F446xx MCU */
/*Change this according to your MCU */
#define SRAM1_SIZE            112*1024     // STM32F446RE has 112KB of SRAM1
#define SRAM1_END             (SRAM1_BASE + SRAM1_SIZE)
#define SRAM2_SIZE            16*1024     // STM32F446RE has 16KB of SRAM2
#define SRAM2_END             (SRAM2_BASE + SRAM2_SIZE)
#define FLASH_SIZE             512*1024     // STM32F446RE has 512KB of SRAM2
#define BKPSRAM_SIZE           4*1024     // STM32F446RE has 4KB of SRAM2
#define BKPSRAM_END            (BKPSRAM_BASE + BKPSRAM_SIZE)

/*******************************************************************************
 *  STRUCTURES, ENUMS and TYPEDEFS
 *****************************************************************************/

/*******************************************************************************
 *  EXTERN GLOBAL VARIABLES

 ******************************************************************************/
void _Error_Handler(char *, int);


/*******************************************************************************
 *  EXTERN FUNCTION
 *****************************************************************************/

void  bootloader_uart_read_data(void);
void bootloader_jump_to_user_app(void);

void bootloader_handle_getver_cmd(uint8_t *bl_rx_buffer);
void bootloader_handle_go_cmd(uint8_t *pBuffer);
void bootloader_handle_flash_erase_cmd(uint8_t *pBuffer);
void bootloader_handle_mem_write_cmd(uint8_t *pBuffer);
void bootloader_send_ack(uint8_t command_code, uint8_t follow_len);
void bootloader_send_nack(void);

uint8_t bootloader_verify_crc (uint8_t *pData, uint32_t len,uint32_t crc_host);
uint8_t get_bootloader_version(void);
void bootloader_uart_write_data(uint8_t *pBuffer,uint32_t len);

uint8_t verify_address(uint32_t go_address);
uint8_t execute_flash_erase(uint8_t sector_number , uint8_t number_of_sector);
uint8_t execute_mem_write(uint8_t *pBuffer, uint32_t mem_address, uint32_t len);


#endif /* INC_BSP_H_ */
