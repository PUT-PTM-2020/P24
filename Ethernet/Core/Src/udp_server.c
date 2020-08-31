/*
 * udp_server.c
 *
 *  Created on: 28 sie 2020
 *      Author: roder
 */

#include "main.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "ff.h"

#include <stdlib.h>
#include <string.h>

#define UDP_SERVER_PORT		20000
#define UDP_CLIENT_PORT		20000
#define START_SEND_FILE		'\x01'
#define NEXT_SEND_FILE 		'\x02'
#define DOWNLOAD_FILE 		'\x03'
#define GET_PATH_INFO 		'\x04'
#define GET_FILE_INFO 		'\x05'
#define CHANGE_DIRECTORY 	'\x06'
#define DELETE_FILE			'\x07'
#define DELETE_DIRECTORY	'\x08'
#define GET_FILE			'\x09'
#define CREATE_DIRECTORY 	'\b'

#define RESP_OK				'\x00'
#define RESP_ERR			'\x01'
#define RESP_MORE			'\x02'

char msg_buffer[1024];
char ans[1];
char operation[1];
char id[1];

static FATFS FatFs;
char file_buffer[1024];
FIL file;
uint16_t bytes_read;
uint16_t i=0;
uint16_t fname_size=0;
static FILINFO file_info;
char slashsign[1]= {'/'};
char divider[1] = {'*'};

void udp_server_receive_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p,
		const ip_addr_t *addr, u16_t port);
u16_t copy_to_pbuf(const struct pbuf *buf, void *dataptr, u16_t len, u16_t offset);
void process_data();
uint16_t find_end_of_msg();
void get_path_info();
void save_to_file(uint16_t append);
void parse_path_and_delete_file();
void parse_path_and_delete_directory();
void read_and_send_file();
void parse_path_and_create_directory();

uint16_t read_file(char* filename, int p_counter);
uint16_t write_file(char* filename, uint16_t append);
uint16_t find_size(TCHAR * fname);
uint16_t scan_dir(char* path);
uint16_t check_existance(char * path);
uint16_t delete_file(char * path);
uint16_t delete_directory(char * path);
uint16_t create_directory(char * path);


void udp_server_init(void)
{
   struct udp_pcb *upcb;
   err_t err;
   upcb = udp_new();

   if (upcb)
   {
      err = udp_bind(upcb, IP_ADDR_ANY, UDP_SERVER_PORT);
      if(err == ERR_OK)
      {
        udp_recv(upcb, udp_server_receive_callback, NULL);
      }
   }
}

void udp_server_receive_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
  pbuf_copy_partial(p, msg_buffer, 1024, 0);
  process_data();
  p->tot_len = 1024;
  p->len = 1024;
  uint16_t x = copy_to_pbuf(p, msg_buffer, 1024, 0);
  memset(msg_buffer,0,sizeof(msg_buffer));
  memset(file_buffer, 0, sizeof(file_buffer));
  udp_sendto(upcb, p, addr, UDP_CLIENT_PORT);
  pbuf_free(p);

}
u16_t copy_to_pbuf(const struct pbuf *buf, void *dataptr, u16_t len, u16_t offset)
{
  const struct pbuf *p;
  u16_t left = 0;
  u16_t buf_copy_len;
  u16_t copied_total = 0;

  LWIP_ERROR("pbuf_copy_partial: invalid buf", (buf != NULL), return 0;);
  LWIP_ERROR("pbuf_copy_partial: invalid dataptr", (dataptr != NULL), return 0;);
  for (p = buf; len != 0 && p != NULL; p = p->next) {
    if ((offset != 0) && (offset >= p->len)) {
      offset = (u16_t)(offset - p->len);
    } else {
      buf_copy_len = (u16_t)(p->len - offset);
      if (buf_copy_len > len) {
        buf_copy_len = len;
      }
      MEMCPY(&((char *)p->payload)[offset],&((char *)dataptr)[left] , buf_copy_len);
      copied_total = (u16_t)(copied_total + buf_copy_len);
      left = (u16_t)(left + buf_copy_len);
      len = (u16_t)(len - buf_copy_len);
      offset = 0;
    }
  }
  return copied_total;
}
void process_data()
{
	memcpy(operation, msg_buffer,1);
	if (operation[0] - GET_FILE_INFO == 0)
	{
		get_path_info();
	}
	else if (operation[0] - CHANGE_DIRECTORY == 0)
	{
		check_if_dir_exists();
	}
	else if (operation[0] - START_SEND_FILE == 0)
	{
		save_to_file(0);
	}
	else if(operation[0] - NEXT_SEND_FILE == 0)
	{
		save_to_file(1);
	}
	else if(operation[0] - DELETE_FILE == 0)
	{
		parse_path_and_delete_file();
	}
	else if(operation[0] - DELETE_DIRECTORY == 0)
	{
		parse_path_and_delete_directory();
	}
	else if(operation[0] - GET_FILE == 0)
	{
		read_and_send_file();
	}
	else if(operation[0] - CREATE_DIRECTORY == 0)
	{
		parse_path_and_create_directory();
	}
	else
	{
		memset(msg_buffer, 0, sizeof(msg_buffer));
		memcpy(msg_buffer, operation, 1);
		memcpy(msg_buffer+1, RESP_ERR, 1);
	}
}

uint16_t find_end_of_msg()
{
	for (int i=3;i<1024;i++)
	{
		if (msg_buffer[i] == '\0' || msg_buffer[i] == 42) return i - 3;
	}
	return -1;
}
void get_path_info()
{
	uint16_t len;
	uint16_t it = find_end_of_msg();
	memcpy(file_buffer, msg_buffer+3, it);
	if (it < 1)
	{
		//error
	}
	len = scan_dir(file_buffer);
	memset(msg_buffer,0,sizeof(msg_buffer));
	if(len > 0)
	{
		memcpy(msg_buffer, operation, 1);

		memcpy(msg_buffer+1, RESP_OK, 1);
		memcpy(msg_buffer+2, id, 1);
		memcpy(msg_buffer+3, file_buffer, len);
	}
}

void check_if_dir_exists()
{
	uint16_t len;
	uint16_t it = find_end_of_msg();
	memcpy(file_buffer, msg_buffer+3, it);
	if (it < 1)
		{
			//error
		}
	FRESULT fr = f_chdir(file_buffer);
	memset(msg_buffer,0,sizeof(msg_buffer));
	memcpy(msg_buffer, operation, 1);
	if(fr == FR_OK) memcpy(msg_buffer+1, RESP_OK, 1);
	else memcpy(msg_buffer+1, RESP_ERR, 1);
	memcpy(msg_buffer+3, id, 1);
}

void save_to_file(uint16_t append)
{
	uint16_t it = find_end_of_msg();
	char filename[100];
	memset(filename,0, sizeof(filename));
	memcpy(filename, msg_buffer + 3, it);
	memcpy(file_buffer, msg_buffer + 3 + it + 1, 1024 - it - 3);
	memset(msg_buffer, 0, sizeof(msg_buffer));
	FRESULT fres = write_file(filename, append);
	memcpy(msg_buffer, operation, 1);
	if (fres == FR_OK)
	{
		memcpy(msg_buffer+1, RESP_OK, 1);

	}
	else
	{
		memcpy(msg_buffer+1, RESP_ERR, 1);
	}
	memcpy(msg_buffer+2, id, 1);
}
void parse_path_and_delete_file()
{
	uint16_t it = find_end_of_msg();
	char filename[100];
	memset(filename, 0, sizeof(filename));
	memcpy(filename, msg_buffer + 3, it);
	memset(msg_buffer, 0, sizeof(msg_buffer));
	FRESULT fres = delete_file(filename);
	memcpy(msg_buffer, operation, 1);
	if (fres == FR_OK)
	{
		memcpy(msg_buffer+1, RESP_OK, 1);
	}
	else
	{
		memcpy(msg_buffer+1, RESP_ERR, 1);
	}
	memcpy(msg_buffer+2, id, 1);
}

void parse_path_and_delete_directory()
{
	uint16_t it = find_end_of_msg();
	char filename[100];
	memset(filename, 0, sizeof(filename));
	memcpy(filename, msg_buffer + 3, it);
	memset(msg_buffer, 0, sizeof(msg_buffer));
	FRESULT fres = delete_file(filename);
	memcpy(msg_buffer, operation, 1);
	if (fres == FR_OK)
	{
		memcpy(msg_buffer+1, RESP_OK, 1);
	}
	else
	{
		memcpy(msg_buffer+1, RESP_ERR, 1);
	}
	memcpy(msg_buffer+2, id, 1);
}

void read_and_send_file()
{
	FRESULT fres;
	uint16_t it = find_end_of_msg();
	char filename[100];
	memset(filename,0, sizeof(filename));
	memcpy(filename, msg_buffer + 3, it);
	char packet_counter[1] = {'\0'};
	memcpy(packet_counter, msg_buffer + it + 4, 1);
	int p_counter = 0;
	p_counter = packet_counter[0];
	memset(msg_buffer, 0, sizeof(msg_buffer));
	fres = read_file(filename, p_counter);
	if (fres == FR_OK)
		{
		if(bytes_read == 512)
		{
			memcpy(msg_buffer+1, RESP_OK, 1);
		}
		else if(bytes_read < 512)
		{
			memcpy(msg_buffer+1, RESP_OK, 1);
		}
		}
		else
		{
			memcpy(msg_buffer+1, RESP_ERR, 1);
		}
		memcpy(msg_buffer+2, id, 1);
		memcpy(msg_buffer+3, file_buffer, 512);
		memset(file_buffer, 0, sizeof(file_buffer));
}

void parse_path_and_create_directory()
{
	uint16_t it = find_end_of_msg();
		char dirname[100];
		memset(dirname, 0, sizeof(dirname));
		memcpy(dirname, msg_buffer + 3, it);
		memset(msg_buffer, 0, sizeof(msg_buffer));
		FRESULT fres = create_directory(dirname);
		memcpy(msg_buffer, operation, 1);
		if (fres == FR_OK)
		{
			memcpy(msg_buffer+1, RESP_OK, 1);
		}
		else
		{
			memcpy(msg_buffer+1, RESP_ERR, 1);
		}
		memcpy(msg_buffer+2, id, 1);
}




uint16_t read_file(char* filename, int p_counter)
{
	FRESULT fr;
	fr = f_mount(&FatFs, "", 0);
	fr = f_open(&file, filename, FA_READ);
	if (fr) return (int)fr;
	for (int i=0; i<p_counter; i++)
	{
		fr = f_read(&file, file_buffer,512,&bytes_read);
	}
	fr = f_close(&file);
	return fr;
}

uint16_t write_file(char* filename, uint16_t append)
{
	FRESULT fr;
	f_mount(&FatFs, "", 0);
	if(append == 0) fr = f_open(&file, filename, FA_WRITE | FA_CREATE_NEW);
	else if (append == 1) fr = f_open(&file, filename, FA_WRITE | FA_OPEN_APPEND);
	if (fr) return (int)fr;
	uint16_t it = 0;
	for (int i=0;i<1024;i++)
	{
		if (file_buffer[i] == '\0' || i == 1024)
		{
			it = i;
			break;
		}
	}
	fr =f_write(&file, file_buffer, it, NULL);
	fr = f_close(&file);
	memset(file_buffer, 0, sizeof(file_buffer));
	return fr;
}
uint16_t find_size(TCHAR * fname)
{
	uint16_t index = 0;
	while(fname[index] != '\0')
	{
		index++;
	}
	return index;
}

uint16_t scan_dir(char* path)
{
	FRESULT fr;
    DIR dir;
    uint16_t i = 0;
    uint16_t fname_size = 0;


    fr = f_mount(&FatFs, "", 0);
    if (fr != FR_OK) return 0;
    fr = f_opendir(&dir, path);
    if (fr == FR_OK) {
        for (;;)
        {
        	fr = f_readdir(&dir, &file_info);
            if (fr != FR_OK || file_info.fname[0] == 0) break;
            fname_size  = find_size(file_info.fname);
            memcpy(file_buffer + i, file_info.fname, fname_size);
            i += fname_size;
            memcpy(file_buffer + i, divider,1);
            i++;
        }
        f_closedir(&dir);
    }

    return i;
}
uint16_t check_existance(char * path)
{
	FRESULT fr;
	f_mount(&FatFs, "", 0);
	fr = f_stat(path, &file_info);
	return fr;
}
uint16_t delete_file(char * path)
{
	FRESULT fr;
	fr = f_mount(&FatFs, "", 0);
	if(fr == FR_OK)
		{
			fr = f_unlink(path);
		}
	return fr;
}
uint16_t delete_directory(char * path)
{
	static FRESULT fr;
	if (fr != FR_OK) return 1;
	fr = f_unlink(path);
	return fr;
	/*
	else if (fr == FR_DENIED)
	{
		DIR dir;
		FILINFO finfo;
		fr = f_chdir(path);
		fr = f_findfirst(&dir, &finfo, "", "*");
		char pathbuff[100];
		while(fr == FR_OK && finfo.fname[0])
				{
					strcpy(pathbuff, path);
					strcat(pathbuff,slashsign);
					strcat(pathbuff,finfo.fname);
					delete_directory(pathbuff);
					fr = f_findnext(&dir, &finfo);
					memset(pathbuff,0,sizeof(pathbuff));
				}
		fr = f_unlink(path);
		return fr;
	}
	*/
}

uint16_t create_directory(char * path)
{
	FRESULT fr;
	fr = f_mount(&FatFs, "", 0);
	if (fr == FR_OK)
	{
		fr = f_mkdir(path);
	}
	return fr;
}
