import serial
import struct
import os
import sys
import glob

# Status codes
Flash_HAL_OK = 0x00
Flash_HAL_ERROR = 0x01
Flash_HAL_BUSY = 0x02
Flash_HAL_TIMEOUT = 0x03
Flash_HAL_INV_ADDR = 0x04

# BL Commands
COMMAND_BL_GET_VER = 0x80
COMMAND_BL_GO_TO_ADDR = 0x83
COMMAND_BL_FLASH_ERASE = 0x81
COMMAND_BL_MEM_WRITE = 0x82

# Command lengths
COMMAND_BL_GET_VER_LEN = 6
COMMAND_BL_GO_TO_ADDR_LEN = 10
COMMAND_BL_FLASH_ERASE_LEN = 8
COMMAND_BL_MEM_WRITE_LEN = 11

# Global variables
verbose_mode = 1
mem_write_active = 0
ser = None
bin_file = None

# ----------------------------- File Operations -----------------------------

# ----------------------------- File Operations -----------------------------

def get_sector_count(file_size):
    """
    Calculate the number of sectors to erase based on the file size.
    Assuming each sector is 128 KB (131072 bytes).
    """
    sector_size = 131072  # 128 KB
    return (file_size + sector_size - 1) // sector_size  # Round up to the nearest sector

def calc_file_len():
    return os.path.getsize("user_app.bin")

def open_the_file():
    global bin_file
    bin_file = open('user_app.bin', 'rb')

def close_the_file():
    if bin_file:
        bin_file.close()

# ----------------------------- Utilities -----------------------------

def word_to_byte(addr, index, lowerfirst):
    return (addr >> (8 * (index - 1))) & 0x000000FF

def get_crc(buff, length):
    crc = 0xFFFFFFFF
    for data in buff[0:length]:
        crc ^= data
        for _ in range(32):
            if crc & 0x80000000:
                crc = (crc << 1) ^ 0x04C11DB7
            else:
                crc <<= 1
    return crc

# ----------------------------- Serial Port -----------------------------

def serial_ports():
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Unsupported platform')

    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):
            pass
    return result

def Serial_Port_Configuration(port):
    global ser
    try:
        ser = serial.Serial(port, 115200, timeout=2)
    except:
        print("\n   Oops! That was not a valid port")
        port = serial_ports()
        if not port:
            print("\n   No ports Detected")
        else:
            print("\n   Here are some available ports on your PC. Try Again!")
            print("\n   ", port)
        return -1
    if ser.is_open:
        print("\n   Port Open Success")
    else:
        print("\n   Port Open Failed")
    return 0

def read_serial_port(length):
    return ser.read(length)

def Close_serial_port():
    if ser:
        ser.close()

def purge_serial_port():
    ser.reset_input_buffer()

def Write_to_serial_port(value, *length):
    data = struct.pack('>B', value)
    if verbose_mode:
        value = bytearray(data)
        print("   0x{:02x}".format(value[0]), end=' ')
    if mem_write_active and (not verbose_mode):
        print("#", end=' ')
    ser.write(data)

# ----------------------------- Command Processing -----------------------------

def process_COMMAND_BL_GET_VER(length):
    ver = read_serial_port(1)
    value = bytearray(ver)
    print("\n   Bootloader Ver. : ", hex(value[0]))

def process_COMMAND_BL_GO_TO_ADDR(length):
    addr_status = read_serial_port(length)
    addr_status = bytearray(addr_status)
    print("\n   Address Status : ", hex(addr_status[0]))

def process_COMMAND_BL_FLASH_ERASE(length):
    erase_status = read_serial_port(length)
    if len(erase_status):
        erase_status = bytearray(erase_status)
        if erase_status[0] == Flash_HAL_OK:
            print("\n   Erase Status: Success  Code: FLASH_HAL_OK")
        elif erase_status[0] == Flash_HAL_ERROR:
            print("\n   Erase Status: Fail  Code: FLASH_HAL_ERROR")
        elif erase_status[0] == Flash_HAL_BUSY:
            print("\n   Erase Status: Fail  Code: FLASH_HAL_BUSY")
        elif erase_status[0] == Flash_HAL_TIMEOUT:
            print("\n   Erase Status: Fail  Code: FLASH_HAL_TIMEOUT")
        elif erase_status[0] == Flash_HAL_INV_ADDR:
            print("\n   Erase Status: Fail  Code: FLASH_HAL_INV_SECTOR")
        else:
            print("\n   Erase Status: Fail  Code: UNKNOWN_ERROR_CODE")
    else:
        print("Timeout: Bootloader is not responding")

def process_COMMAND_BL_MEM_WRITE(length):
    write_status = read_serial_port(length)
    write_status = bytearray(write_status)
    if write_status[0] == Flash_HAL_OK:
        print("\n   Write_status: FLASH_HAL_OK")
    elif write_status[0] == Flash_HAL_ERROR:
        print("\n   Write_status: FLASH_HAL_ERROR")
    elif write_status[0] == Flash_HAL_BUSY:
        print("\n   Write_status: FLASH_HAL_BUSY")
    elif write_status[0] == Flash_HAL_TIMEOUT:
        print("\n   Write_status: FLASH_HAL_TIMEOUT")
    elif write_status[0] == Flash_HAL_INV_ADDR:
        print("\n   Write_status: FLASH_HAL_INV_ADDR")
    else:
        print("\n   Write_status: UNKNOWN_ERROR")
    print("\n")

# ----------------------------- Command Decoding -----------------------------
def decode_menu_command_code(command, *args):
    ret_value = 0
    data_buf = [0] * 255

    if command == 0:
        print("\n   Exiting...!")
        raise SystemExit
    elif command == 1:
        print("\n   Command == > BL_GET_VER")
        data_buf[0] = COMMAND_BL_GET_VER_LEN - 1
        data_buf[1] = COMMAND_BL_GET_VER
        crc32 = get_crc(data_buf, COMMAND_BL_GET_VER_LEN - 4)
        crc32 &= 0xFFFFFFFF
        data_buf[2:6] = [word_to_byte(crc32, i, 1) for i in range(1, 5)]

        Write_to_serial_port(data_buf[0], 1)
        for i in data_buf[1:COMMAND_BL_GET_VER_LEN]:
            Write_to_serial_port(i, COMMAND_BL_GET_VER_LEN - 1)

        ret_value = read_bootloader_reply(data_buf[1])

    elif command == 2:
        print("\n   Command == > BL_GO_TO_ADDR")
        go_address = args[0] if args else int(input("\n   Please enter 4 bytes go address in hex:"), 16)
        data_buf[0] = COMMAND_BL_GO_TO_ADDR_LEN - 1
        data_buf[1] = COMMAND_BL_GO_TO_ADDR
        data_buf[2:6] = [word_to_byte(go_address, i, 1) for i in range(1, 5)]
        crc32 = get_crc(data_buf, COMMAND_BL_GO_TO_ADDR_LEN - 4)
        data_buf[6:10] = [word_to_byte(crc32, i, 1) for i in range(1, 5)]

        Write_to_serial_port(data_buf[0], 1)
        for i in data_buf[1:COMMAND_BL_GO_TO_ADDR_LEN]:
            Write_to_serial_port(i, COMMAND_BL_GO_TO_ADDR_LEN - 1)

        ret_value = read_bootloader_reply(data_buf[1])

    elif command == 3:
        print("\n   Command == > BL_FLASH_ERASE")
        sector_num = args[0] if args else int(input("\n   Enter sector number(0-7 or 0xFF) here:"), 16)
        nsec = args[1] if args else int(input("\n   Enter number of sectors to erase(max 8) here:"))

        data_buf[0] = COMMAND_BL_FLASH_ERASE_LEN - 1
        data_buf[1] = COMMAND_BL_FLASH_ERASE
        data_buf[2] = sector_num
        data_buf[3] = nsec
        crc32 = get_crc(data_buf, COMMAND_BL_FLASH_ERASE_LEN - 4)
        data_buf[4:8] = [word_to_byte(crc32, i, 1) for i in range(1, 5)]

        Write_to_serial_port(data_buf[0], 1)
        for i in data_buf[1:COMMAND_BL_FLASH_ERASE_LEN]:
            Write_to_serial_port(i, COMMAND_BL_FLASH_ERASE_LEN - 1)

        ret_value = read_bootloader_reply(data_buf[1])

    elif command == 4:
        print("\n   Command == > BL_MEM_WRITE")
        bytes_remaining = 0
        t_len_of_file = 0
        bytes_so_far_sent = 0
        len_to_read = 0
        base_mem_address = args[0] if args else int(input("\n   Enter the memory write address here:"), 16)

        data_buf[1] = COMMAND_BL_MEM_WRITE

        t_len_of_file = calc_file_len()
        open_the_file()

        bytes_remaining = t_len_of_file - bytes_so_far_sent
        global mem_write_active
        mem_write_active = 1

        while bytes_remaining:
            if bytes_remaining >= 128:  # Changed from 128 to 10
                len_to_read = 128  # Changed from 128 to 10
            else:
                len_to_read = bytes_remaining

            for x in range(len_to_read):
                file_read_value = bin_file.read(1)
                file_read_value = bytearray(file_read_value)
                data_buf[7 + x] = int(file_read_value[0])

            data_buf[2:6] = [word_to_byte(base_mem_address, i, 1) for i in range(1, 5)]
            data_buf[6] = len_to_read

            mem_write_cmd_total_len = COMMAND_BL_MEM_WRITE_LEN + len_to_read
            data_buf[0] = mem_write_cmd_total_len - 1

            crc32 = get_crc(data_buf, mem_write_cmd_total_len - 4)
            data_buf[7 + len_to_read:11 + len_to_read] = [word_to_byte(crc32, i, 1) for i in range(1, 5)]

            Write_to_serial_port(data_buf[0], 1)
            for i in data_buf[1:mem_write_cmd_total_len]:
                Write_to_serial_port(i, mem_write_cmd_total_len - 1)

            base_mem_address += len_to_read
            bytes_so_far_sent += len_to_read
            bytes_remaining = t_len_of_file - bytes_so_far_sent
            print("\n   bytes_so_far_sent:{0} -- bytes_remaining:{1}\n".format(bytes_so_far_sent, bytes_remaining))

            ret_value = read_bootloader_reply(data_buf[1])

        mem_write_active = 0

    else:
        print("\n   Please input valid command code\n")
        return

    if ret_value == -2:
        print("\n   TimeOut : No response from the bootloader")
        print("\n   Reset the board and Try Again !")
        return
"""
def decode_menu_command_code(command, *args):
    ret_value = 0
    data_buf = [0] * 255

    if command == 0:
        print("\n   Exiting...!")
        raise SystemExit
    elif command == 1:
        print("\n   Command == > BL_GET_VER")
        data_buf[0] = COMMAND_BL_GET_VER_LEN - 1
        data_buf[1] = COMMAND_BL_GET_VER
        crc32 = get_crc(data_buf, COMMAND_BL_GET_VER_LEN - 4)
        crc32 &= 0xFFFFFFFF
        data_buf[2:6] = [word_to_byte(crc32, i, 1) for i in range(1, 5)]

        Write_to_serial_port(data_buf[0], 1)
        for i in data_buf[1:COMMAND_BL_GET_VER_LEN]:
            Write_to_serial_port(i, COMMAND_BL_GET_VER_LEN - 1)

        ret_value = read_bootloader_reply(data_buf[1])

    elif command == 2:
        print("\n   Command == > BL_GO_TO_ADDR")
        go_address = args[0] if args else int(input("\n   Please enter 4 bytes go address in hex:"), 16)
        data_buf[0] = COMMAND_BL_GO_TO_ADDR_LEN - 1
        data_buf[1] = COMMAND_BL_GO_TO_ADDR
        data_buf[2:6] = [word_to_byte(go_address, i, 1) for i in range(1, 5)]
        crc32 = get_crc(data_buf, COMMAND_BL_GO_TO_ADDR_LEN - 4)
        data_buf[6:10] = [word_to_byte(crc32, i, 1) for i in range(1, 5)]

        Write_to_serial_port(data_buf[0], 1)
        for i in data_buf[1:COMMAND_BL_GO_TO_ADDR_LEN]:
            Write_to_serial_port(i, COMMAND_BL_GO_TO_ADDR_LEN - 1)

        ret_value = read_bootloader_reply(data_buf[1])

    elif command == 3:
        print("\n   Command == > BL_FLASH_ERASE")
        sector_num = args[0] if args else int(input("\n   Enter sector number(0-7 or 0xFF) here:"), 16)
        nsec = args[1] if args else int(input("\n   Enter number of sectors to erase(max 8) here:"))

        data_buf[0] = COMMAND_BL_FLASH_ERASE_LEN - 1
        data_buf[1] = COMMAND_BL_FLASH_ERASE
        data_buf[2] = sector_num
        data_buf[3] = nsec
        crc32 = get_crc(data_buf, COMMAND_BL_FLASH_ERASE_LEN - 4)
        data_buf[4:8] = [word_to_byte(crc32, i, 1) for i in range(1, 5)]

        Write_to_serial_port(data_buf[0], 1)
        for i in data_buf[1:COMMAND_BL_FLASH_ERASE_LEN]:
            Write_to_serial_port(i, COMMAND_BL_FLASH_ERASE_LEN - 1)

        ret_value = read_bootloader_reply(data_buf[1])

    elif command == 4:
        print("\n   Command == > BL_MEM_WRITE")
        bytes_remaining = 0
        t_len_of_file = 0
        bytes_so_far_sent = 0
        len_to_read = 0
        base_mem_address = args[0] if args else int(input("\n   Enter the memory write address here:"), 16)

        data_buf[1] = COMMAND_BL_MEM_WRITE

        t_len_of_file = calc_file_len()
        open_the_file()

        bytes_remaining = t_len_of_file - bytes_so_far_sent
        global mem_write_active
        mem_write_active = 1

        while bytes_remaining:
            if bytes_remaining >= 128:
                len_to_read = 128
            else:
                len_to_read = bytes_remaining

            for x in range(len_to_read):
                file_read_value = bin_file.read(1)
                file_read_value = bytearray(file_read_value)
                data_buf[7+ x] = int(file_read_value[0])

            data_buf[2:6] = [word_to_byte(base_mem_address, i, 1) for i in range(1, 5)]
            data_buf[6] = len_to_read

            mem_write_cmd_total_len = COMMAND_BL_MEM_WRITE_LEN + len_to_read
            data_buf[0] = mem_write_cmd_total_len - 1

            crc32 = get_crc(data_buf, mem_write_cmd_total_len - 4)
            data_buf[7+ len_to_read:11 + len_to_read] = [word_to_byte(crc32, i, 1) for i in range(1, 5)]

            Write_to_serial_port(data_buf[0], 1)
            for i in data_buf[1:mem_write_cmd_total_len]:
                Write_to_serial_port(i, mem_write_cmd_total_len - 1)

            base_mem_address += len_to_read
            bytes_so_far_sent += len_to_read
            bytes_remaining = t_len_of_file - bytes_so_far_sent
            print("\n   bytes_so_far_sent:{0} -- bytes_remaining:{1}\n".format(bytes_so_far_sent, bytes_remaining))

            ret_value = read_bootloader_reply(data_buf[1])

        mem_write_active = 0

    else:
        print("\n   Please input valid command code\n")
        return

    if ret_value == -2:
        print("\n   TimeOut : No response from the bootloader")
        print("\n   Reset the board and Try Again !")
        return
"""
def read_bootloader_reply(command_code):
    len_to_follow = 0
    ret = -2

    ack = read_serial_port(2)
    if len(ack):
        a_array = bytearray(ack)
        if a_array[0] == 0xA5:
            len_to_follow = a_array[1]
            print("\n   CRC : SUCCESS Len :", len_to_follow)
            if command_code == COMMAND_BL_GET_VER:
                process_COMMAND_BL_GET_VER(len_to_follow)
            elif command_code == COMMAND_BL_GO_TO_ADDR:
                process_COMMAND_BL_GO_TO_ADDR(len_to_follow)
            elif command_code == COMMAND_BL_FLASH_ERASE:
                process_COMMAND_BL_FLASH_ERASE(len_to_follow)
            elif command_code == COMMAND_BL_MEM_WRITE:
                process_COMMAND_BL_MEM_WRITE(len_to_follow)
            else:
                print("\n   Invalid command code\n")
            ret = 0
        elif a_array[0] == 0x7F:
            print("\n   CRC: FAIL \n")
            ret = -1
    else:
        print("\n   Timeout : Bootloader not responding")
    return ret

# ----------------------------- Automated Process Flow -----------------------------
def automate_process_flow():
    # Step 1: Calculate the number of sectors to erase
    file_size = calc_file_len()
    sector_count = get_sector_count(file_size)
    print(f"\nFile size: {file_size} bytes")
    print(f"Number of sectors to erase: {sector_count}")

    # Step 2: Execute BL_GET_VER
    print("\nExecuting BL_GET_VER...")
    decode_menu_command_code(1)

    # Step 3: Execute BL_FLASH_ERASE
    print("\nExecuting BL_FLASH_ERASE...")
    decode_menu_command_code(3, 2, 3)  # Start from sector 0, erase 'sector_count' sectors

    # Step 4: Execute BL_MEM_WRITE
    print("\nExecuting BL_MEM_WRITE...")
    ret = decode_menu_command_code(4, 0x08008000)  # base_mem_address = 0x08008000

    # Step 5: Execute BL_GO_TO_ADDR
    print("\nExecuting BL_GO_TO_ADDR...")
    decode_menu_command_code(2, 0x08008848)  # go_address = 0x08008848
'''
def automate_process_flow():
    # Step 1: Execute BL_GET_VER
    print("\nExecuting BL_GET_VER...")
    decode_menu_command_code(1)
    print("\nExecuting BL_FLASH_ERASE...")
    decode_menu_command_code(3, 2, 2)  # sector_num = 2, nsec = 2
    print("\nExecuting BL_MEM_WRITE...")
    ret=decode_menu_command_code(4, 0x08008000)  # base_mem_address = 0x08008000
    print("\nExecuting BL_GO_TO_ADDR...")
    decode_menu_command_code(2, 0x08008848)  # go_address = 0x08008848

'''
# ----------------------------- Main Execution -----------------------------

name = input("Enter the Port Name of your device (Ex: COM3): ")
ret = Serial_Port_Configuration(name)
if ret < 0:
    decode_menu_command_code(0)

# Run the automated process flow
automate_process_flow()

# Close the serial port
Close_serial_port()
'''
name = input("Enter the Port Name of your device(Ex: COM3):")
ret = Serial_Port_Configuration(name)
if ret < 0:
    decode_menu_command_code(0)

# Run the automated process flow
automate_process_flow()

# Close the serial port
Close_serial_port()'''