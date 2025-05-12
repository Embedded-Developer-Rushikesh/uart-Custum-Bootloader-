/*
 * bsp.c
 *
 *  Created on: Feb 11, 2025
 *      Author: RushikeshKamble
 */

/*******************************************************************************
 *  HEADER FILE INCLUDES
 *****************************************************************************/
#include"bsp.h"
#include"stdarg.h"
#include"string.h"
#include"stdio.h"
/*******************************************************************************
 *  EXTERN VARIABLES DEFINITION
 ******************************************************************************/
 extern CRC_HandleTypeDef hcrc;
 extern UART_HandleTypeDef huart2;
 extern UART_HandleTypeDef huart3;
 /*******************************************************************************
 *  MACRO DEFINITION
 ******************************************************************************/
#define D_UART   &huart3
#define C_UART   &huart2
#define BL_RX_LEN  200
/*******************************************************************************
 *  GLOBAL VARIABLES DEFINITION
 ******************************************************************************/
 uint8_t supported_commands[] = {
                                BL_GET_VER ,
                                BL_FLASH_ERASE,
                                BL_MEM_WRITE,
								BL_GO_TO_ADDR,
 } ;

 uint8_t bl_rx_buffer[BL_RX_LEN];

/*******************************************************************************
 *  STATIC FUNCTION PROTOTYPES
 ******************************************************************************/
static void printmsg(char *format,...);

/*******************************************************************************
 *  FUNCTION DEFINITIONS
 ******************************************************************************/
/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : bootloader_uart_read_data
*   Description   : The function read the UART based cmd data and based on cmd it will process
*   Parameters    : p_args - NULL
*   Return Value  : NULL
*  ---------------------------------------------------------------------------*/
void  bootloader_uart_read_data(void)
{
    uint8_t rcv_len=0;

	while(1)
	{
		//memset(bl_rx_buffer,0,200);
		HAL_UART_Receive(C_UART,bl_rx_buffer,1,HAL_MAX_DELAY);
		rcv_len= bl_rx_buffer[0];
		HAL_UART_Receive(C_UART,&bl_rx_buffer[1],rcv_len,HAL_MAX_DELAY);

		switch(bl_rx_buffer[1])
		{
            case BL_GET_VER:
            {
                bootloader_handle_getver_cmd(bl_rx_buffer);
                break;
            }
            case BL_FLASH_ERASE:
            {
                bootloader_handle_flash_erase_cmd(bl_rx_buffer);
                break;
            }
            case BL_MEM_WRITE:
            {
                bootloader_handle_mem_write_cmd(bl_rx_buffer);
                break;
            }
            case BL_GO_TO_ADDR:
            {
                bootloader_handle_go_cmd(bl_rx_buffer);
                break;
            }
             default:
             {
                printmsg("BL_DEBUG_MSG:Invalid command code received from host \n");
                break;
             }


		}

	}

}
/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : bootloader_uart_read_data
*   Description   : The function read the UART based cmd data and based on cmd it will process
*   Parameters    : p_args - NULL
*   Return Value  : NULL
*  ---------------------------------------------------------------------------*/
void bootloader_jump_to_user_app(void)
{

    void (*app_reset_handler)(void);

    printmsg("BL_DEBUG_MSG:bootloader_jump_to_user_app\n");

    uint32_t msp_value = *(volatile uint32_t *)FLASH_SECTOR2_BASE_ADDRESS;
    printmsg("BL_DEBUG_MSG:MSP value : %#x\n",msp_value);
    __set_MSP(msp_value);
    uint32_t resethandler_address = *(volatile uint32_t *) (FLASH_SECTOR2_BASE_ADDRESS + 4);
    app_reset_handler = (void*) resethandler_address;
    printmsg("BL_DEBUG_MSG: app reset handler addr : %#x\n",app_reset_handler);
    /*3. jump to reset handler of the user application*/
    app_reset_handler();

}
/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : bootloader_handle_getver_cmd
*   Description   : Helper function to handle BL_GET_VER command *
*   Parameters    : p_args - bl_rx_buffer
*   Return Value  : NULL
*  ---------------------------------------------------------------------------*/
void bootloader_handle_getver_cmd(uint8_t *bl_rx_buffer)
{
    uint8_t bl_version;

    /* verify the checksum*/
      printmsg("BL_DEBUG_MSG:bootloader_handle_getver_cmd\n");

	 /*Total length of the command packet*/
	  uint32_t command_packet_len = bl_rx_buffer[0]+1 ;

	  /*extract the CRC32 sent by the Host*/
	  uint32_t host_crc = *((uint32_t * ) (bl_rx_buffer+command_packet_len - 4) ) ;

    if (! bootloader_verify_crc(&bl_rx_buffer[0],command_packet_len-4,host_crc))
    {
        printmsg("BL_DEBUG_MSG:checksum success !!\n");
        /*checksum is correct..*/
        bootloader_send_ack(bl_rx_buffer[0],1);
        bl_version=get_bootloader_version();
        printmsg("BL_DEBUG_MSG:BL_VER : %d %#x\n",bl_version,bl_version);
        bootloader_uart_write_data(&bl_version,1);

    }else
    {
    	  /*checksum is wrong send NACK*/
        printmsg("BL_DEBUG_MSG:checksum fail !!\n");

        bootloader_send_nack();
    }


}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : bootloader_handle_go_cmd
*   Description   : Helper function to handle BL_GO_TO_ADDR command
*   Parameters    : p_args - *pBuffer
*   Return Value  : NULL
*  ---------------------------------------------------------------------------*/
void bootloader_handle_go_cmd(uint8_t *pBuffer)
{
    uint32_t go_address=0;
    uint8_t addr_valid = ADDR_VALID;
    uint8_t addr_invalid = ADDR_INVALID;

    printmsg("BL_DEBUG_MSG:bootloader_handle_go_cmd\n");

	uint32_t command_packet_len = bl_rx_buffer[0]+1 ;

	uint32_t host_crc = *((uint32_t * ) (bl_rx_buffer+command_packet_len - 4) ) ;

	if (! bootloader_verify_crc(&bl_rx_buffer[0],command_packet_len-4,host_crc))
	{
        printmsg("BL_DEBUG_MSG:checksum success !!\n");
        bootloader_send_ack(pBuffer[0],1);
        go_address = *((uint32_t *)&pBuffer[2] );
        printmsg("BL_DEBUG_MSG:GO addr: %#x\n",go_address);

        if( verify_address(go_address) == ADDR_VALID )
        {
            bootloader_uart_write_data(&addr_valid,1);
            go_address+=1; //make T bit =1
            void (*lets_jump)(void) = (void *)go_address;
            printmsg("BL_DEBUG_MSG: jumping to go address! \n");
            lets_jump();

		}else
		{
            printmsg("BL_DEBUG_MSG:GO addr invalid ! \n");
            bootloader_uart_write_data(&addr_invalid,1);
		}

	}else
	{
        printmsg("BL_DEBUG_MSG:checksum fail !!\n");
        bootloader_send_nack();
	}


}
/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : bootloader_handle_flash_erase_cmd
*   Description   :Helper function to handle BL_FLASH_ERASE command
*   Parameters    : p_args - *pBuffer
*   Return Value  : NULL
*  ---------------------------------------------------------------------------*/
void bootloader_handle_flash_erase_cmd(uint8_t *pBuffer)
{
    uint8_t erase_status = 0x00;
    printmsg("BL_DEBUG_MSG:bootloader_handle_flash_erase_cmd\n");

    //Total length of the command packet
	uint32_t command_packet_len = bl_rx_buffer[0]+1 ;

	//extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (bl_rx_buffer+command_packet_len - 4) ) ;

	if (! bootloader_verify_crc(&bl_rx_buffer[0],command_packet_len-4,host_crc))
	{
        printmsg("BL_DEBUG_MSG:checksum success !!\n");
        bootloader_send_ack(pBuffer[0],1);
        printmsg("BL_DEBUG_MSG:initial_sector : %d  no_ofsectors: %d\n",pBuffer[2],pBuffer[3]);

        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin,1);
        erase_status = execute_flash_erase(pBuffer[2] , pBuffer[3]);
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin,0);

        printmsg("BL_DEBUG_MSG: flash erase status: %#x\n",erase_status);

        bootloader_uart_write_data(&erase_status,1);

	}else
	{
        printmsg("BL_DEBUG_MSG:checksum fail !!\n");
        bootloader_send_nack();
	}
}
/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : bootloader_handle_mem_write_cmd
*   Description   :Helper function to handle BL_MEM_WRITE command
*   Parameters    : p_args - *pBuffer
*   Return Value  : NULL
*  ---------------------------------------------------------------------------*/
void bootloader_handle_mem_write_cmd(uint8_t *pBuffer)
{
	uint8_t write_status = 0x00;
	uint8_t payload_len = pBuffer[6];
	uint32_t mem_address = *((uint32_t *) ( &pBuffer[2]) );
    printmsg("BL_DEBUG_MSG:bootloader_handle_mem_write_cmd\n");
	uint32_t command_packet_len = bl_rx_buffer[0]+1 ;
	uint32_t host_crc = *((uint32_t * ) (bl_rx_buffer+command_packet_len - 4) ) ;
	if (! bootloader_verify_crc(&bl_rx_buffer[0],command_packet_len-4,host_crc))
	{
        printmsg("BL_DEBUG_MSG:checksum success !!\n");
        bootloader_send_ack(pBuffer[0],1);
        printmsg("BL_DEBUG_MSG: mem write address : %#x\n",mem_address);
		if( verify_address(mem_address) == ADDR_VALID )
		{
            printmsg("BL_DEBUG_MSG: valid mem write address\n");
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
            write_status = execute_mem_write(&pBuffer[7],mem_address, payload_len);
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
            bootloader_uart_write_data(&write_status,1);

		}else
		{
            printmsg("BL_DEBUG_MSG: invalid mem write address\n");
            write_status = ADDR_INVALID;
            bootloader_uart_write_data(&write_status,1);
		}


	}else
	{
        printmsg("BL_DEBUG_MSG:checksum fail !!\n");
        bootloader_send_nack();
	}

}
/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : bootloader_send_ack
*   Description   :This function sends ACK if CRC matches along with "len to follow"
*   Parameters    : p_args - int8_t command_code,uint8_t follow_len
*   Return Value  : NULL
*  ---------------------------------------------------------------------------*/
void bootloader_send_ack(uint8_t command_code, uint8_t follow_len)
{
	uint8_t ack_buf[2];
	ack_buf[0] = BL_ACK;
	ack_buf[1] = follow_len;
	HAL_UART_Transmit(C_UART,ack_buf,2,HAL_MAX_DELAY);

}
/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : bootloader_send_nack
*   Description   :This function sends NACK "
*   Parameters    : p_args -NULL
*   Return Value  : NULL
*  ---------------------------------------------------------------------------*/
void bootloader_send_nack(void)
{
	uint8_t nack = BL_NACK;
	HAL_UART_Transmit(C_UART,&nack,1,HAL_MAX_DELAY);
}
/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : bootloader_verify_crc
*   Description   :This verifies the CRC of the given buffer in pData
*   Parameters    : p_args -uint8_t *pData,uint32_t crc_host
*   Return Value  : uint8_t
*  ---------------------------------------------------------------------------*/
uint8_t bootloader_verify_crc (uint8_t *pData, uint32_t len, uint32_t crc_host)
{
    uint32_t uwCRCValue=0xff;

    for (uint32_t i=0 ; i < len ; i++)
	{
        uint32_t i_data = pData[i];
        uwCRCValue = HAL_CRC_Accumulate(&hcrc, &i_data, 1);
	}

	 /* Reset CRC Calculation Unit */
  __HAL_CRC_DR_RESET(&hcrc);

	if( uwCRCValue == crc_host)
	{
		return VERIFY_CRC_SUCCESS;
	}

	return VERIFY_CRC_FAIL;
}
/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : bootloader_uart_write_data
*   Description   :This function writes data in to C_UART
*   Parameters    : p_args -uint8_t *pBuffer,uint32_t len
*   Return Value  : NULL
*  ---------------------------------------------------------------------------*/
/* This function writes data in to C_UART */
void bootloader_uart_write_data(uint8_t *pBuffer,uint32_t len)
{
	HAL_UART_Transmit(C_UART,pBuffer,len,HAL_MAX_DELAY);

}

/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : get_bootloader_version
*   Description   :return value of MACRO
*   Parameters    : p_args -NULL
*   Return Value  : uint8_t
*  ---------------------------------------------------------------------------*/
uint8_t get_bootloader_version(void)
{
  return (uint8_t)BL_VERSION;
}
/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : verify_address
*   Description   :verify the address sent by the host .
*   Parameters    : p_args -uint32_t go_address
*   Return Value  : uint8_t
*  ---------------------------------------------------------------------------*/
uint8_t verify_address(uint32_t go_address)
{
	if ( go_address >= SRAM1_BASE && go_address <= SRAM1_END)
	{
		return ADDR_VALID;
	}
	else if ( go_address >= SRAM2_BASE && go_address <= SRAM2_END)
	{
		return ADDR_VALID;
	}
	else if ( go_address >= FLASH_BASE && go_address <= FLASH_END)
	{
		return ADDR_VALID;
	}
	else if ( go_address >= BKPSRAM_BASE && go_address <= BKPSRAM_END)
	{
		return ADDR_VALID;
	}
	else
		return ADDR_INVALID;
}
/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : execute_flash_erase
*   Description   :Erase the Entire Flash or sectors
*   Parameters    : p_args -uint8_t sector_number,uint8_t number_of_sector
*   Return Value  : uint8_t
*  ---------------------------------------------------------------------------*/
uint8_t execute_flash_erase(uint8_t sector_number , uint8_t number_of_sector)
{
	FLASH_EraseInitTypeDef flashErase_handle;
	uint32_t sectorError;
	HAL_StatusTypeDef status;


	if( number_of_sector > 8 )
		return INVALID_SECTOR;

	if( (sector_number == 0xff ) || (sector_number <= 7) )
	{
		if(sector_number == (uint8_t) 0xff)
		{
			flashErase_handle.TypeErase = FLASH_TYPEERASE_MASSERASE;
		}else
		{
		    /*Here we are just calculating how many sectors needs to erased */
			uint8_t remanining_sector = 8 - sector_number;
            if( number_of_sector > remanining_sector)
            {
            	number_of_sector = remanining_sector;
            }
			flashErase_handle.TypeErase = FLASH_TYPEERASE_SECTORS;
			flashErase_handle.Sector = sector_number; // this is the initial sector
			flashErase_handle.NbSectors = number_of_sector;
		}
		flashErase_handle.Banks = FLASH_BANK_1;

		/*Get access to touch the flash registers */
		HAL_FLASH_Unlock();
		flashErase_handle.VoltageRange = FLASH_VOLTAGE_RANGE_3;  // our mcu will work on this voltage range
		status = (uint8_t) HAL_FLASHEx_Erase(&flashErase_handle, &sectorError);
		HAL_FLASH_Lock();

		return status;
	}


	return INVALID_SECTOR;
}

 /* -----------------------------------------------------------------------------
 *  FUNCTION DESCRIPTION
 *  -----------------------------------------------------------------------------
 *   Function Name : execute_mem_write
 *   Description   :This function writes the contents of pBuffer to  "mem_address" byte by byte
 *   Parameters    : p_args -uint8_t *pBuffer,uint32_t mem_address, uint32_t len
 *   Return Value  : uint8_t
 *  ---------------------------------------------------------------------------*/
uint8_t execute_mem_write(uint8_t *pBuffer, uint32_t mem_address, uint32_t len)
{
    uint8_t status = HAL_OK;
    uint32_t bytes_remaining = len;
    uint32_t bytes_written = 0;
    uint32_t chunk_size = 128;
    HAL_FLASH_Unlock();

    while (bytes_remaining > 0)
    {
        uint32_t current_chunk_size = (bytes_remaining >= chunk_size) ? chunk_size : bytes_remaining;
        for (uint32_t i = 0; i < current_chunk_size; i++)
        {
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, mem_address + bytes_written + i, pBuffer[bytes_written + i]);
            if (status != HAL_OK)
            {
                HAL_FLASH_Lock();
                return status;
            }
        }

        bytes_written += current_chunk_size;
        bytes_remaining -= current_chunk_size;
    }
    HAL_FLASH_Lock();

    return status;
}
/*
uint8_t execute_mem_write(uint8_t *pBuffer, uint32_t mem_address, uint32_t len)
{
    uint8_t status=HAL_OK;

    //We have to unlock flash module to get control of registers
    HAL_FLASH_Unlock();

    for(uint32_t i = 0 ; i <len ; i++)
    {
        //Here we program the flash byte by byte
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE,mem_address+i,pBuffer[i] );
    }

    HAL_FLASH_Lock();

    return status;
}*/
/* -----------------------------------------------------------------------------
*  FUNCTION DESCRIPTION
*  -----------------------------------------------------------------------------
*   Function Name : printmsg
*   Description   :prints formatted string to console over UART
*   Parameters    : p_args -char *format,...
*   Return Value  : NULL
*  ---------------------------------------------------------------------------*/
/*  */
 void printmsg(char *format,...)
 {

	char str[80];
	va_list args;
	va_start(args, format);
	vsprintf(str, format,args);
	HAL_UART_Transmit(D_UART,(uint8_t *)str, strlen(str),HAL_MAX_DELAY);
	va_end(args);
 }

