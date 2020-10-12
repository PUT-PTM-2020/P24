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
#define CREATE_DIRECTORY 	'\x0a'
#define END_SEND_FILE		'\x0b'
#define LOGIN 				'\x0c'
#define LOGOUT				'\x0d'
#define PUBLIC				'\x0e'

#define RESP_OK				'\x00'
#define RESP_ERR			'\x01'
#define RESP_MORE			'\x02'
#define RESP_AUTH_ERR		'\x03'
#define RESP_NO_PATH		'\x04'
#define RESP_NOT_LOGGED		'\x05'

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
uint16_t header_size = 6;
uint16_t header_with_usrname = 12;
uint16_t msg_size = 0;
int control_val = 0;
uint8_t isLogged[256];
char packet_control[256][3];



void udp_server_receive_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p,
		const ip_addr_t *addr, u16_t port);
u16_t copy_to_pbuf(const struct pbuf *buf, void *dataptr, u16_t len, u16_t offset);
uint16_t find_end_of_new_msg();
void process_data();
uint16_t find_end_of_msg();
void get_path_info();
void save_to_file(uint16_t append);
void parse_path_and_delete_file();
void parse_path_and_delete_directory();
void read_and_send_file();
void parse_path_and_create_directory();
uint16_t logout();
uint16_t login_public();


uint16_t read_file(char* filename, int p_counter);
uint16_t write_file(char* filename, uint16_t append);
uint16_t find_size(TCHAR * fname);
uint16_t scan_dir(char* path);
uint16_t check_existance(char * path);
uint16_t delete_file(char * path);
uint16_t delete_directory(char * path);
uint16_t create_directory(char * path);
uint16_t login_user();
uint16_t check_if_logged();
void udp_server_init(void)
{
	memset(isLogged,0, sizeof(isLogged));
	memset(packet_control, 0, sizeof(packet_control));
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
  pbuf_copy_partial(p, msg_buffer, p->len, 0);
  process_data();
  msg_size = find_end_of_new_msg();
  p->tot_len = msg_size;
  p->len = msg_size;
  uint16_t x = copy_to_pbuf(p, msg_buffer, msg_size, 0);
  memset(msg_buffer,0,sizeof(msg_buffer));
  memset(file_buffer, 0, sizeof(file_buffer));
  memset(operation, 0, 1);
  memset(id, 0, 1);
  udp_sendto(upcb, p, addr, UDP_CLIENT_PORT);
  pbuf_free(p);

}
uint16_t find_end_of_new_msg()
{
	for (int i=header_with_usrname;i<1024;i++)
	{
		if (msg_buffer[i] == '\0') return i;
	}
	return -1;
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
uint16_t login_user()
{
	char cli_username[] = "******";
	char cli_password[] = "********";
	memcpy(cli_username, msg_buffer+header_size, 6);
	memcpy(cli_password, msg_buffer+header_with_usrname, 8);
	memset(msg_buffer, 0, 1024);
	FRESULT fr;
	int pointer=0;
	uint16_t read = 17;
	fr = f_mount(&FatFs, "", 0);
	fr = f_open(&file, "user database.txt", FA_READ);
	char data_buffer[17];
	char data_username[] = "******";
	char data_password[] = "********";
	while(read==17 || pointer<255)
	{
		fr = f_read(&file, data_buffer, 17, &bytes_read);
		if (fr) {
			prepare_response(operation, RESP_ERR, id, 0, 0);
				return -1;
			};
		memcpy(data_username, data_buffer+1, 6);
		memcpy(data_password, data_buffer+7, 8);
		if(strcmp(cli_username, data_username) == 0 && strcmp(cli_password, data_password) == 0)
		{
			memcpy(id, data_buffer, 1);
			prepare_response(operation, RESP_OK, id, 0, 0);
			isLogged[(int)id[0]] = 2;
			break;
		}
		pointer++;
	}
	if (fr) {
		prepare_response(operation[0], RESP_AUTH_ERR, id, 0, 0);
	};
	fr = f_close(&file);
	return fr;

}
uint16_t check_if_logged()
{
	if(isLogged[(int)id[0]] == 1)
	{
		return 1;
	}
	else if(isLogged[(int)id[0]] == 2)
	{
		return 2;
	}
	else
	{
		prepare_response(operation, RESP_NOT_LOGGED, id, 0,0 );
		return 0;
	}

}
void process_data()
{
	memcpy(operation, msg_buffer,1);
	memcpy(id, msg_buffer+2, 1);
	if (operation[0] - LOGIN == 0)
	{
		login_user();
		return;
	}
	else if(operation[0] - PUBLIC == 0)
	{
		login_public();
		return;
	}
	uint16_t isLogged = check_if_logged();
	if(isLogged > 0)
	{

		if (operation[0] - GET_FILE_INFO == 0)
		{
			get_path_info();
		}
		else if (operation[0] - CHANGE_DIRECTORY == 0)
		{
			check_if_dir_exists();
		}
		else if (operation[0] - START_SEND_FILE == 0 && isLogged>1)
		{
			save_to_file(0);
		}
		else if(operation[0] - NEXT_SEND_FILE == 0 && isLogged>1)
		{
			save_to_file(1);
		}
		else if(operation[0] - DELETE_FILE == 0 && isLogged>1)
		{
			parse_path_and_delete_file();
		}
		else if(operation[0] - DELETE_DIRECTORY == 0 && isLogged>1)
		{
			parse_path_and_delete_directory();
		}
		else if(operation[0] - GET_FILE == 0)
		{
			read_and_send_file();
		}
		else if(operation[0] - CREATE_DIRECTORY == 0 && isLogged>1)
		{
			parse_path_and_create_directory();
		}
		else if(operation[0] - END_SEND_FILE == 0 && isLogged>1)
		{
			clear_control_val();
		}
		else if(operation[0] - LOGOUT == 0)
		{
			logout();
		}

		else
		{
			prepare_response(operation, RESP_AUTH_ERR, id, 0, 0);
		}
	}
	else
	{
		//ur not logged in!
	}
}
uint16_t login_public()
{
	char cli_username[] = "public";
	char empty_field[] = "******";
	memset(msg_buffer, 0, 1024);
	FRESULT fr;
	int pointer=0;
	uint16_t read = 17;
	fr = f_mount(&FatFs, "", 0);
	fr = f_open(&file, "user database.txt", FA_READ | FA_WRITE);
	char data_buffer[17];
	char data_username[] = "******";
	char data_password[] = "********";
	fr = f_read(&file, data_buffer, 17, &bytes_read);
	pointer++;
	while(read==17)
	{
		fr = f_read(&file, data_buffer, 17, &bytes_read);
		if (fr) {
			prepare_response(operation, RESP_ERR, id, 0, 0);
				return -1;
			};
		memcpy(data_username, data_buffer+1, 6);
		memcpy(data_password, data_buffer+7, 8);
		if(strcmp(empty_field, data_username) == 0)
		{
			f_lseek(&file, f_tell(&file) - 17);
			id[0] = pointer;
			f_write(&file, id,1,0);
			f_write(&file, cli_username, 6,0);
			prepare_response(operation, RESP_OK, id, 0, 0);
			isLogged[(int)id[0]] = 1;
			break;
		}
		pointer++;
	}
	if (fr) {
		prepare_response(operation[0], RESP_AUTH_ERR, id, 0, 0);
	};
	fr = f_close(&file);
	return fr;

}
uint16_t logout()
{
	isLogged[(int)id[0]] = 0;
	prepare_response(operation, RESP_OK, id, 0, 0);
	return 0;
}

void clear_control_val()
{
	control_val = 0;
	memset(msg_buffer, 0, sizeof(msg_buffer));
	memcpy(msg_buffer, operation, 1);
	memcpy(msg_buffer+1, RESP_OK, 1);
}

uint16_t find_end_of_msg()
{
	for (int i=header_with_usrname;i<1024;i++)
	{
		if (msg_buffer[i] == '\0' || msg_buffer[i] == 42) return i - header_with_usrname;
	}
	return -1;
}
void prepare_response(char* op, char resp,char* id, char* file_buffer, uint16_t len)
{
	memcpy(msg_buffer, op, 1);
	memset(msg_buffer+1, resp, 1);
	memcpy(msg_buffer+2, id, 1);
	if(len !=0)	memcpy(msg_buffer+header_size, file_buffer, len);
}
void get_path_info()
{
	uint16_t len;
	uint16_t it = find_end_of_msg();
	memcpy(file_buffer, msg_buffer+header_with_usrname, it);
	if (it < 1)
	{
		//error
	}
	len = scan_dir(file_buffer);
	memset(msg_buffer,0,sizeof(msg_buffer));
	if(len > 0)
	{
		prepare_response(operation, RESP_OK, id, file_buffer,len);
	}
}

void check_if_dir_exists()
{
	uint16_t len;
	uint16_t it = find_end_of_msg();
	memcpy(file_buffer, msg_buffer+header_with_usrname, it);
	if (it < 1)
		{
			//error
		}
	FRESULT fr = f_chdir(file_buffer);
	memset(msg_buffer,0,sizeof(msg_buffer));
	if(fr == FR_OK)
	{
		prepare_response(operation, RESP_OK, id, 0, 0);
	}
	else if(fr == FR_NO_PATH)
	{
		prepare_response(operation, RESP_NO_PATH, id, 0,0);
	}
	else
	{
		prepare_response(operation, RESP_ERR, id, 0, 0);
	}
}

void save_to_file(uint16_t append)
{
	uint16_t it = find_end_of_msg();
	char filename[100];
	memset(filename,0, sizeof(filename));
	memcpy(filename, msg_buffer + header_with_usrname, it);
	memcpy(file_buffer, msg_buffer + header_with_usrname + it + 1, 1024 - it - header_with_usrname);
	char packet_counter[3] = {'\0','\0', '\0'};
	memcpy(packet_counter, msg_buffer + 3, 3);
	int p_counter = 0;
	p_counter = packet_counter[0]*256*256+packet_counter[1]*256+packet_counter[2];
	memset(msg_buffer, 0, sizeof(msg_buffer));
	FRESULT fres;
	if(p_counter == control_val + 1)
	{
		fres = write_file(filename, append);
		control_val++;
	}
	else if (p_counter == control_val)
	{
		fres = FR_OK;
	}
	else
	{
		fres = 1;
	}
	memcpy(msg_buffer, operation, 1);
	if (fres == FR_OK)
	{
		memcpy(msg_buffer+1, RESP_OK, 1);

	}
	else
	{
		memcpy(msg_buffer+1, RESP_ERR, 1);
		parse_path_and_delete_file();
		control_val = 0;
	}
	memcpy(msg_buffer+2, id, 1);
	memcpy(msg_buffer+3, packet_counter, 2);
}
void parse_path_and_delete_file()
{
	uint16_t it = find_end_of_msg();
	char filename[100];
	memset(filename, 0, sizeof(filename));
	memcpy(filename, msg_buffer + header_with_usrname, it);
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
	memcpy(filename, msg_buffer + header_with_usrname, it);
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
	memcpy(filename, msg_buffer + header_with_usrname, it);
	char packet_counter[3] = {'\0','\0','\0'};
	memcpy(packet_counter, msg_buffer + 3, 3);
	int p_counter = 0;
	p_counter = packet_counter[0]*256*256+packet_counter[1]*256+packet_counter[2];
	memset(msg_buffer, 0, sizeof(msg_buffer));
	fres = read_file(filename, p_counter);
	if (fres == FR_OK)
		{
		if(bytes_read == 512)
		{
			prepare_response(operation, RESP_MORE, id, file_buffer, bytes_read);
		}
		else if(bytes_read < 512)
		{
			prepare_response(operation, RESP_OK, id, file_buffer, bytes_read);
		}
		}
		else
		{
			prepare_response(operation, RESP_ERR, id, file_buffer, bytes_read);
		}
		memset(file_buffer, 0, sizeof(file_buffer));
}

void parse_path_and_create_directory()
{
	uint16_t it = find_end_of_msg();
		char dirname[100];
		memset(dirname, 0, sizeof(dirname));
		memcpy(dirname, msg_buffer + header_with_usrname, it);
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
	fr = f_lseek(&file, p_counter*512);
	if (fr) return (int)fr;
	fr = f_read(&file, file_buffer,512,&bytes_read);
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


uint16_t login()
{
	char username[6];
	char password[6];
	return 0;
}
