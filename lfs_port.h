#ifndef __LFS_PORT
#define __LFS_PORT

#include "stm32f10x.h"
#include "param_bridge.h"
/*-------------------- 地址配置 --------------------*/
#define OUTERFLASH_ADDR_START		0 /*外部区域的起始地址*/
#define OUTER_BLOCK_NUM				6 /*日志区域所用页数*/

#if defined(PCB_VCU_BOARD_P02) || defined(PCB_VCU_BOARD_P03)
#define PAGE_SIZE					2048
#elif defined(PCB_EPB_BOARD_P02)
#define PAGE_SIZE					1024
#endif

#define LFS_INTER_FLASH_START_ADDR  0x08032000UL /*内部日志区域的起始地址*/ 
#define BLOCK_NUM					20 /*日志区域所用页数*/ 


/*日志区域空间*/ 
#define LFS_INTER_FLASH_SIZE        (BLOCK_NUM * PAGE_SIZE)   

/*-------------------- 自动回滚 --------------------*/
#define ROTATION_INFO_FILE_NAME		"rotation.txt" /*轮状信息暂存的文件*/
#define ROTATION_NEWEST_FILE_ID 	0x01 /*存储最新文件的ID*/
#define ROTATION_OLDEST_FILE_ID 	0x02 /*存储最旧文件的ID*/
#define ROTATION_CURRENT_OFFSET_ID 	0x03 /*存储最新文件写入偏移量的ID*/
#define ROTATION_ACTIVE_FILE_ID		0x04 /*存储有效文件的ID*/

#define MAX_ROTATION_FILES     		3      /*轮状的文件个数*/
#define FILE_PREFIX            		"log"  /*文件前缀 "log"*/ 
#define FILE_EXTENSION         		".txt" /*文件扩展名*/ 
#define MAX_FILE_SIZE          		4096   /*单个文件的大小*/      
#define FILENAME_BUFFER_SIZE   		16     /*文件名暂存数组大小*/     

typedef struct {
    uint16_t newest_file_id;        
    uint16_t oldest_file_id;          
    uint16_t active_file_count;     
    uint32_t current_file_offset;   
    uint32_t total_writes;          
} rotation_state_t;

/*-------------------- 参数键值对 --------------------*/
/*存储参数键值对的文件名*/ 
#define PARAM_FILENAME 			"param.txt"

/*支持的数据类型枚举*/ 
typedef enum {
    PARAM_TYPE_INT = 0,
    PARAM_TYPE_FLOAT,
    PARAM_TYPE_UINT8,
    PARAM_TYPE_UINT16,
    PARAM_TYPE_UINT32,
    PARAM_TYPE_STRING
} param_type_t;

/*参数值联合体*/ 
typedef union {
    int i;
    float f;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    char str[16];  /*文件名最大长度*/ 
} param_value_t;

/*参数结构体*/ 
typedef struct {
    param_id_enum_t id;
    param_type_t type;
    param_value_t value;
    uint8_t size;
} param_entry_t;



int param_set(param_id_enum_t param_id, const void* value);
param_value_t param_get_value(param_id_enum_t param_id);
int lfs_store_log_outernal(const void *log_message, int message_len);
int lfs_store_log_internal(const void *log_message, int message_len);
void log_lfs_init(void);
void lfs_print_logs(uint8_t type);
int lfs_log_inter_read(void *logBuf, int maxBytesToRead);
int lfs_log_outer_read(void *logBuf, int maxBytesToRead);
#endif
