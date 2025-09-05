#ifndef __LOG_H
#define __LOG_H

#include "stm32f10x.h"
#include "user_def.h"
#include "lfs_port.h"
#include "param_bridge.h"

#define INTER_FLASH_PAGE_SIZE 	V_FLS_PageSize
#define LOG_SIZE				2*INTER_FLASH_PAGE_SIZE //4K
#define LOG_ADDR_START 			0x08032000
#define MCU_FLASH_END			0x0803FFFF	
#define INIT_FLAG				0x6666
/*用来存放日志区已使用的字节数*/
#define LOG_INFO_SIZE			1*INTER_FLASH_PAGE_SIZE
/*用来存放具体的日志*/
#define LOG_DATA_SIZE			1*INTER_FLASH_PAGE_SIZE
#define LOG_DATA_ADDR			(LOG_ADDR_START+LOG_INFO_SIZE)



typedef enum
{
	INTER_FLASH, /*选择内部Flash保存日志*/
	OUTER_FLASH  /*选择外部Flash（GD25Q80）保存日志*/
}FLASH_TYPE;



typedef enum
{
	LOG_READ_ERR,
	LOG_WRITE_ERR,
	LOG_ADDR_ERR,
	LOG_BUFF_ERR
}LOG_INFO;


void hal_log_init(void);
int hal_logNVM(FLASH_TYPE type,const char * format, ...);
int hal_logNVM_bin(FLASH_TYPE type,const void * data, int len);
int hal_logNVM_Read(FLASH_TYPE type,void * logBuf, int maxBytesToRead);
void hal_log_print(FLASH_TYPE type);
param_value_t hal_statNVM_read(param_id_enum_t id);	
//int API_statNVM_write(ID_LIST id,const char * format, ...);
int hal_statNVM_write(param_id_enum_t id,const void *value);\
void hal_err_code_store(void);
#endif




