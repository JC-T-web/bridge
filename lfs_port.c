
/*
*********************************************************************************************************
*
*   模块名称 : littlefs接口模块
*   文件名称 : lfs_port.c
*   版    本 : V1.0
*   说    明 : 利用littlefs完成日志和关键参数的存储，支持掉电保护、磨损均衡、写满自动回滚
*   修改记录 :
*       版本号  	日期        作者     	说明
*       V1.0    2025-08-25 汤金铖    	实现基本功能--内部存储参数，外部存储日志
*
*********************************************************************************************************
*/

#include "lfs.h"
#include "hal_QDflash.h"
#include "stm32f10x_flash.h"
#include <string.h>
#include "debug.h"
#include "g.h"
#include "lfs_port.h"
 

/* 静态内存使用方式必须设定这四个缓存*/
__align(4) static uint8_t inter_read_buffer[16];
__align(4) static uint8_t inter_prog_buffer[16];
__align(4) static uint8_t inter_lookahead_buffer[16];
__align(4) static uint8_t outer_read_buffer[64];
__align(4) static uint8_t outer_prog_buffer[64];
__align(4) static uint8_t outer_lookahead_buffer[64];

/*为文件操作增加独立缓冲区，避免与全局缓冲区冲突*/ 
__align(4) static uint8_t file_inter_buffer[16];
__align(4) static uint8_t file_outer_buffer[64];

/*lfs句柄,内外部flash实列*/ 
lfs_t lfs_inter_flash;
lfs_t lfs_outer_flash;

rotation_state_t g_rotation = {0, 0, 0, 0, 0};  

/*
***************************************************************************************
* 函 数 名: lfs_inter_read
* 功能说明: lfs内部flash读数据接口
* 形   参: c		 - 初始化文件系统配置
*		  block  - 块编号
*		  off 	 - 块内偏移地址	
*		  buffer - 暂存待读取的数据	
*		  size 	 - 待读取数据的大小
* 返 回 值: lfs的状态码
***************************************************************************************
*/
static int lfs_inter_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    uint32_t flash_addr = LFS_INTER_FLASH_START_ADDR + (block * c->block_size) + off;
    
    if (buffer == NULL || size == 0) {
        return LFS_ERR_INVAL;
    }
    
    if (flash_addr < LFS_INTER_FLASH_START_ADDR || 
        flash_addr + size > LFS_INTER_FLASH_START_ADDR + LFS_INTER_FLASH_SIZE) {
        return LFS_ERR_INVAL;
    }
    
    for (uint32_t i = 0; i < size; i++) {
        ((uint8_t *)buffer)[i] = *(__IO uint8_t *)(flash_addr + i);
    }
    
    return LFS_ERR_OK;
}

/*
***************************************************************************************
* 函 数 名: lfs_inter_prog
* 功能说明: lfs内部flash写数据接口
* 形   参: c 	 - 初始化文件系统配置
*		  block  - 块编号
*		  off 	 - 块内偏移地址	
*		  buffer - 暂存待写入的数据	
*		  size 	 - 待写入数据的大小
* 返 回 值: lfs的状态码
***************************************************************************************
*/
static int lfs_inter_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    uint32_t flash_addr = LFS_INTER_FLASH_START_ADDR + (block * c->block_size) + off;
 
    if (buffer == NULL || size == 0)
	{
        return LFS_ERR_INVAL;
    }
    
    if (flash_addr + size > 0x08040000UL) 
	{ 
        return LFS_ERR_NOSPC;
    }
    
    FLASH_Unlock();
    
	uint16_t *data_ptr = (uint16_t *)buffer;
	uint32_t half_word_count = size/2; 
	
	for (uint32_t i = 0; i < half_word_count; i++) 
	{
		uint16_t half_word;
		half_word = data_ptr[i];

		__disable_irq(); 
		FLASH_Status status = FLASH_ProgramHalfWord(flash_addr + i * 2, half_word);
		__enable_irq();
		if (status != FLASH_COMPLETE) 
		{
			FLASH_Lock();
			return LFS_ERR_IO;
		}
		while (FLASH_GetFlagStatus(FLASH_FLAG_BSY) == SET) 
		{
			
		}
	}
	FLASH_Lock();
    
    return LFS_ERR_OK;
}



/*
***************************************************************************************
* 函 数 名: lfs_inter_erase
* 功能说明: lfs内部flash擦除接口
* 形   参: c 	 - 初始化文件系统配置
*		  block  - 块编号
* 返 回 值: lfs的状态码
***************************************************************************************
*/
static int lfs_inter_erase(const struct lfs_config *c, lfs_block_t block)
{
    uint32_t flash_addr = LFS_INTER_FLASH_START_ADDR + (block * c->block_size);
    
    if (flash_addr < LFS_INTER_FLASH_START_ADDR || 
        flash_addr + c->block_size > LFS_INTER_FLASH_START_ADDR + LFS_INTER_FLASH_SIZE) 
	{
        return LFS_ERR_INVAL;
    }
    
    FLASH_Unlock();
    
    FLASH_Status status = FLASH_ErasePage(flash_addr);
    
    FLASH_Lock();
    
    if (status != FLASH_COMPLETE)		
	{
        return LFS_ERR_IO;
    }
    
    return LFS_ERR_OK;
}  
 
/*
***************************************************************************************
* 函 数 名: lfs_outer_read
* 功能说明: lfs外部flash读数据接口
* 形   参: c		 - 初始化文件系统配置
*		  block  - 块编号
*		  off 	 - 块内偏移地址	
*		  buffer - 暂存待读取的数据	
*		  size 	 - 待读取数据的大小
* 返 回 值: lfs的状态码
***************************************************************************************
*/
static int lfs_outer_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
	hal_GD25Q80_read((uint8_t *)buffer,OUTERFLASH_ADDR_START+c->block_size * block + off,size);
	return LFS_ERR_OK;
}

/*
***************************************************************************************
* 函 数 名: lfs_outer_prog
* 功能说明: lfs外部flash写数据接口
* 形   参: c		 - 初始化文件系统配置
*		  block  - 块编号
*		  off 	 - 块内偏移地址	
*		  buffer - 暂存待读取的数据	
*		  size 	 - 待读取数据的大小
* 返 回 值: lfs的状态码
***************************************************************************************
*/
static int lfs_outer_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
	hal_GD25Q80_write((uint8_t *)buffer,OUTERFLASH_ADDR_START+c->block_size * block + off, size);
	return LFS_ERR_OK;
}


/*
***************************************************************************************
* 函 数 名: lfs_outer_erase
* 功能说明: lfs外部flash擦除接口
* 形   参: c 	 - 初始化文件系统配置
*		  block  - 块编号
* 返 回 值: lfs的状态码
***************************************************************************************
*/
static int lfs_outer_erase(const struct lfs_config *c, lfs_block_t block)
{
	hal_GD25Q80_erase_sector(block);
	return LFS_ERR_OK;
}



static int lfs_deskio_sync(const struct lfs_config *c)
{
	return LFS_ERR_OK;
}

/*内外部flash配置，相当于两个文件系统的属性设置*/
const struct lfs_config inter_cfg =
{
	.read  = lfs_inter_read,
	.prog  = lfs_inter_prog,
	.erase = lfs_inter_erase,
	.sync  = lfs_deskio_sync,

	.read_size = 16,
	.prog_size = 16,
	.block_size = 2048,
	.block_count = BLOCK_NUM,
	.cache_size = 16,
	.lookahead_size =  16,
	.block_cycles = 500,

	/*使用静态内存必须设置这几个缓存*/ 
	.read_buffer = inter_read_buffer,
	.prog_buffer = inter_prog_buffer,
	.lookahead_buffer = inter_lookahead_buffer,
};

const struct lfs_config outer_cfg =
{
	.read  = lfs_outer_read,
	.prog  = lfs_outer_prog,
	.erase = lfs_outer_erase,
	.sync  = lfs_deskio_sync,

	.read_size = 64,
	.prog_size = 64,
	.block_size = 4096,
	.block_count = OUTER_BLOCK_NUM,
	.cache_size =64,
	.lookahead_size = 64,
	.block_cycles = 500,

	/*使用静态内存必须设置这几个缓存*/ 
	.read_buffer = outer_read_buffer,
	.prog_buffer = outer_prog_buffer,
	.lookahead_buffer = outer_lookahead_buffer,
};



/*
***************************************************************************************
* 函 数 名: get_file_size
* 功能说明: 获取某个文件数据域的大小
* 形   参: lfs 	 	- 文件系统实例
*		  filename  - 文件名
* 返 回 值: 文件的数据大小，（字节）
***************************************************************************************
*/
lfs_size_t get_file_size(lfs_t *lfs,const char *filename)
{
    struct lfs_info info;
    if (lfs_stat(lfs, filename, &info) == 0) 
	{
        return info.size;
    }
    return 0;
}



int rotation_write(lfs_t *lfs, const void *data, uint32_t size);
int lfs_store_log_internal(const void *log_message, int message_len)
{
	return rotation_write(&lfs_inter_flash, log_message, message_len);
}

int lfs_store_log_outernal(const void *log_message, int message_len)
{
	return rotation_write(&lfs_outer_flash, log_message, message_len);
}

/*
***************************************************************************************
* 函 数 名: find_param_entry
* 功能说明: 根据参数ID查找参数条目
* 形   参: param_id - 参数ID
* 返 回 值: 参数条目指针，未找到返回NULL
***************************************************************************************
*/
extern param_entry_t param_table[MAX_PARAMS];
param_entry_t* find_param_entry(param_id_enum_t param_id)
{
    for (int i = 0; i < MAX_PARAMS; i++) 
	{
        if (param_table[i].id == param_id) 
		{
            return &param_table[i];
        }
    }
    return NULL;
}


/*
***************************************************************************************
* 函 数 名: param_set
* 功能说明: 设置参数值
* 形   参: param_id - 参数ID
*		  value    - 参数值指针
* 返 回 值: 0成功，-1失败
***************************************************************************************
*/
int param_set(param_id_enum_t param_id, const void* value)
{
    param_entry_t* entry = find_param_entry(param_id);
    if (entry == NULL) {
        always_Print(0, ("param_set: param_id=%d not found\r\n", param_id));
        return -1;
    }
    
    /*根据类型复制数据*/ 
    switch (entry->type) {
        case PARAM_TYPE_INT:
            entry->value.i = *(int*)value;
            break;
        case PARAM_TYPE_FLOAT:
            entry->value.f = *(float*)value;
            break;
        case PARAM_TYPE_UINT8:
            entry->value.u8 = *(uint8_t*)value;
            break;
        case PARAM_TYPE_UINT16:
            entry->value.u16 = *(uint16_t*)value;
            break;
        case PARAM_TYPE_UINT32:
            entry->value.u32 = *(uint32_t*)value;
            break;
        case PARAM_TYPE_STRING:
            strncpy(entry->value.str, (char*)value, sizeof(entry->value.str)-1);
            entry->value.str[sizeof(entry->value.str)-1] = '\0';
            break;
        default:
            return -1;
    }
    
    int result = lfs_setattr(&lfs_inter_flash, PARAM_FILENAME, param_id, 
                            &entry->value, entry->size);
    
    always_Print(0, ("param_set: param_id=%d, type=%d, result=%d\r\n", 
                    param_id, entry->type, result));
    return result;
}



/*
***************************************************************************************
* 函 数 名: param_get_value
* 功能说明: 从Flash读取参数值并更新参数表
* 形   参: param_id - 参数ID
* 返 回 值: 0成功，-1失败
***************************************************************************************
*/
param_value_t param_get_value(param_id_enum_t param_id)
{
	param_value_t result = {0};

	param_entry_t* entry = find_param_entry(param_id);
	if (entry == NULL) 
	{
		always_Print(0, ("param_get_value: param_id=%d not found\r\n", param_id));
		return result;
	}

	/*从Flash读取数据*/ 
	param_value_t temp_value;
	int lfs_result = lfs_getattr(&lfs_inter_flash, PARAM_FILENAME, param_id, 
	&temp_value, entry->size);

	if (lfs_result >= 0) 
	{
		/*读取成功，更新参数表并返回值*/ 
		entry->value = temp_value;
		result = temp_value;
		always_Print(0, ("param_get_value: param_id=%d loaded from flash\r\n", param_id));
	} 
	else 
	{
		/*读取失败，返回参数表中的默认值*/ 
		result = entry->value;
		always_Print(0, ("param_get_value: param_id=%d failed, using default\r\n", param_id));
	}

	return result;
}




/***************************************************************************************
* 函 数 名: param_file_init
* 功能说明: lfs内部flash擦除接口
* 形   参: lfs - 文件系统实例
* 返 回 值: 0 - 成功，-1 失败
***************************************************************************************
*/
int param_file_init(lfs_t *lfs)
{
    lfs_file_t file;
    struct lfs_file_config fcfg = 
	{
        .buffer = file_outer_buffer,
    };
    
    /*创建参数文件（如果不存在）*/ 
    int err = lfs_file_opencfg(lfs, &file, PARAM_FILENAME, 
                              LFS_O_WRONLY | LFS_O_CREAT, &fcfg);
    if (err < 0) 
	{
        always_Print(0, ("param_file_init: failed to create param file, error=%d\r\n", err));
        return -1;
    }
    
    lfs_file_close(&lfs_inter_flash, &file);
    always_Print(0, ("param_file_init: param file created/verified\r\n"));
    return 0;
}




/***************************************************************************************
* 函 数 名: lfs_inter_erase
* 功能说明: lfs内部flash擦除接口
* 形   参: c 	 - lfs的系统配置结构体
*		  block  - 块编号
* 返 回 值: lfs的状态码
***************************************************************************************
*/
static void generate_filename(uint16_t file_id, char *filename)
{
    snprintf(filename,FILENAME_BUFFER_SIZE,"%s%03d%s",FILE_PREFIX,file_id,FILE_EXTENSION);
}


/*
***************************************************************************************
* 函 数 名: delete_oldest_file
* 功能说明: 删除最旧的文件
* 形   参: lfs - 文件系统实例
* 返 回 值: 0成功，-1失败
***************************************************************************************
*/
static int delete_oldest_file(lfs_t *lfs)
{
    char filename[FILENAME_BUFFER_SIZE];
    generate_filename(g_rotation.oldest_file_id, filename);
    
    int result = lfs_remove(lfs, filename);
	lfs_fs_gc(lfs);
    if (result == 0) 
	{
        always_Print(0, ("Deleted oldest file: %s\r\n", filename));
        
        /*更新最旧文件ID,循环递增*/ 
        g_rotation.oldest_file_id = (g_rotation.oldest_file_id + 1) % MAX_ROTATION_FILES;
        g_rotation.active_file_count--;
        return 0;
    } 
	else 
	{
        always_Print(0, ("Failed to delete file: %s, error: %d\r\n", filename, result));
        return -1;
    }
}


/*
***************************************************************************************
* 函 数 名: switch_to_next_file
* 功能说明: 切换到下一个文件
* 形   参: lfs - 文件系统实例
* 返 回 值: 0成功，-1失败
***************************************************************************************
*/
static int switch_to_next_file(lfs_t *lfs)
{
    always_Print(0, ("switch_to_next_file: BEFORE - active_count=%d, newest_id=%d, oldest_id=%d\r\n", 
                   g_rotation.active_file_count, g_rotation.newest_file_id, g_rotation.oldest_file_id));
    
    /*如果已经达到最大文件数，删除最旧的文件*/ 
    if (g_rotation.active_file_count >= MAX_ROTATION_FILES) 
	{
        always_Print(0, ("Max files reached, deleting oldest file\r\n"));
        if (delete_oldest_file(lfs) < 0) 
		{
			always_Print(0, ("Failed to delete file"));
            return -1;
        }
    }
    
    /*切换到下一个文件*/ 
    g_rotation.newest_file_id = (g_rotation.newest_file_id + 1) % MAX_ROTATION_FILES;
    g_rotation.current_file_offset = 0;
    
    g_rotation.active_file_count++;

	lfs_setattr(lfs,ROTATION_INFO_FILE_NAME,ROTATION_ACTIVE_FILE_ID,
				&g_rotation.active_file_count,sizeof(g_rotation.active_file_count));
	lfs_setattr(lfs,ROTATION_INFO_FILE_NAME,ROTATION_NEWEST_FILE_ID,
				&g_rotation.newest_file_id,sizeof(g_rotation.newest_file_id));
	lfs_setattr(lfs,ROTATION_INFO_FILE_NAME,ROTATION_OLDEST_FILE_ID,
				&g_rotation.oldest_file_id,sizeof(g_rotation.oldest_file_id));
    char filename[FILENAME_BUFFER_SIZE];
    generate_filename(g_rotation.newest_file_id, filename);
    always_Print(0, ("Switched to new file: %s - active_count=%d, newest_id=%d, oldest_id=%d\r\n", 
                   filename, g_rotation.active_file_count, g_rotation.newest_file_id, g_rotation.oldest_file_id));
    
    return 0;
}



/***************************************************************************************
* 函 数 名: rotation_write
* 功能说明: lfs内部flash擦除接口
* 形   参: lfs  - 文件系统实例
*		  data - 写入的数据
*		  size - 大小
* 返 回 值: written写入的字节数，-1失败
***************************************************************************************
*/
int rotation_write(lfs_t *lfs, const void *data, uint32_t size)
{
    if (data == NULL || size == 0) 
	{
        return -1;
    }
     /*第一次写入时的初始化检查*/
    if (g_rotation.active_file_count == 0) 
	{
        g_rotation.active_file_count = 1;
		lfs_setattr(lfs,ROTATION_INFO_FILE_NAME,ROTATION_ACTIVE_FILE_ID,
					&g_rotation.active_file_count,sizeof(g_rotation.active_file_count));
        always_Print(0, ("First write, initialized active_file_count to 1\r\n"));
    }
    
    /*检查当前文件是否需要轮转*/ 
    if (g_rotation.current_file_offset + size >= MAX_FILE_SIZE) 
	{
        always_Print(0, ("Current file full (%d + %d > %d), switching to next file\r\n",
                       g_rotation.current_file_offset, size, MAX_FILE_SIZE));
		switch_to_next_file(lfs);
    }
    
    /*生成当前文件名*/ 
    char filename[FILENAME_BUFFER_SIZE];
    generate_filename(g_rotation.newest_file_id, filename);
	struct lfs_file_config fcfg = 
	{
		.buffer = file_outer_buffer, 
	};
	
    /*追加写入数据*/ 
    lfs_file_t file;
    int err = lfs_file_opencfg(lfs, &file, filename, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND,&fcfg);
    if (err < 0) 
	{
        always_Print(0, ("Failed to open file: %s, error: %d\r\n", filename, err));
        return -1;
    }
    
    int written = lfs_file_write(lfs, &file, data, size);
    if (written < 0) 
	{
        always_Print(0, ("Failed to write to file: %s, error: %d\r\n", filename, written));
        lfs_file_close(lfs, &file);
        return -1;
    }
	g_rotation.current_file_offset += written;
	lfs_setattr(lfs,ROTATION_INFO_FILE_NAME,ROTATION_CURRENT_OFFSET_ID,
				&g_rotation.current_file_offset,sizeof(g_rotation.current_file_offset));
//	lfs_setattr(lfs,ROTATION_INFO_FILE_NAME,ROTATION_NEWEST_FILE_ID,
//			&g_rotation.newest_file_id,sizeof(g_rotation.newest_file_id));
//	lfs_setattr(lfs,ROTATION_INFO_FILE_NAME,ROTATION_OLDEST_FILE_ID,
//				&g_rotation.oldest_file_id,sizeof(g_rotation.oldest_file_id));
    lfs_file_close(lfs, &file);
	//lfs_file_sync(lfs, lfs_file_t *file);
    always_Print(0, ("Written %d bytes to %s, offset now: %d\r\n", written, filename, g_rotation.current_file_offset));
    
    return written;
}




/*
***************************************************************************************
* 函 数 名: rotation_print_logs
* 功能说明: 打印指定轮转文件的日志内容
* 形   参: lfs 	  - 文件系统实例
*		  file_id - 文件ID
* 返 回 值: 无
***************************************************************************************
*/
void rotation_print_logs(lfs_t *lfs, uint16_t file_id)
{
    if (file_id >= MAX_ROTATION_FILES) 
	{
        always_Print(0, ("Invalid file ID: %d (max: %d)\r\n", file_id, MAX_ROTATION_FILES - 1));
        return;
    }
    
    char filename[FILENAME_BUFFER_SIZE];
    generate_filename(file_id, filename);
    
    lfs_file_t file;
    struct lfs_file_config fcfg = 
	{
        .buffer = file_inter_buffer,
    };
    
    /*打开日志文件进行读取*/ 
    int err = lfs_file_opencfg(lfs, &file, filename, LFS_O_RDONLY, &fcfg);
    if (err < 0) 
	{
        always_Print(0, ("Error: Failed to open log file %s for reading. Code: %d\r\n", filename, err));
        return;
    }
    
    /*获取文件大小*/ 
    lfs_soff_t file_size = lfs_file_size(lfs, &file);
    if (file_size < 0) {
        always_Print(0, ("Error: Failed to get file size for %s. Code: %d\r\n", filename, (int)file_size));
        lfs_file_close(lfs, &file);
        return;
    }
    
    if (file_size == 0) 
	{
        always_Print(0, ("Log file %s is empty.\r\n", filename));
        lfs_file_close(lfs, &file);
        return;
    }
    
    always_Print(0, ("=== Log File %s Contents (Size: %d bytes) ===\r\n", filename, (int)file_size));
    
    /*日志条目缓冲区，用于累积一条完整的日志*/ 
    char log_entry[128]; 
    int log_entry_pos = 0; 
    int log_count = 0; 
    lfs_soff_t total_read = 0;
    
    while (total_read < file_size) 
	{
        char current_char;
        /*读取一个字节*/ 
        lfs_ssize_t bytes_read = lfs_file_read(lfs, &file, &current_char, 1);
        if (bytes_read < 0) {
            always_Print(0, ("Error: Failed to read from file %s. Code: %d\r\n", filename, (int)bytes_read));
            break;
        }
        
		/*达到文件末尾*/ 
        if (bytes_read == 0) 
		{
            break; 
        }
        total_read++;
        
        /* 检查是否是日志结束符'/' */ 
        if (current_char == '/') 
		{
            if (log_entry_pos > 0) 
			{
                log_entry[log_entry_pos] = '\0';
                log_count++;
                always_Print(0, ("[%s:%d] %s\r\n", filename, log_count, log_entry));
                log_entry_pos = 0; 
            }
        } 
		else 
		{
            if (log_entry_pos < sizeof(log_entry) - 1) 
			{
                log_entry[log_entry_pos] = current_char;
                log_entry_pos++;
            } 
			else
			{
                log_entry[sizeof(log_entry) - 1] = '\0';
                log_count++;
                always_Print(0, ("[%s:%d] %s (TRUNCATED)\r\n", filename, log_count, log_entry));
                log_entry_pos = 0;
                
                log_entry[log_entry_pos] = current_char;
                log_entry_pos++;
            }
        }
    }
    
    /*处理最后一条未完成的日志（如果存在）*/ 
    if (log_entry_pos > 0) {
        log_entry[log_entry_pos] = '\0';
        log_count++;
        always_Print(0, ("[%s:%d] %s (INCOMPLETE)\r\n", filename, log_count, log_entry));
    }
    
    always_Print(0, ("=== File %s: Total %d log entries found ===\r\n", filename, log_count));
    
    lfs_file_close(lfs, &file);
}


/*
***************************************************************************************
* 函 数 名: rotation_print_all_logs
* 功能说明: 打印所有轮转文件的日志内容（按时间顺序）
* 形   参: lfs 	  - 文件系统实例
* 返 回 值: 无
***************************************************************************************
*/
void rotation_print_all_logs(lfs_t *lfs)
{
   
    if (g_rotation.active_file_count == 0) 
	{
        always_Print(0, ("No active rotation files found\r\n"));
        return;
    }
    
    always_Print(0, ("=== Printing All Rotation Logs (Chronological Order) ===\r\n"));
    always_Print(0, ("Total active files: %d\r\n", g_rotation.active_file_count));
    always_Print(0, ("File range: %d to %d\r\n", g_rotation.oldest_file_id, g_rotation.newest_file_id));
    
    /*从最旧的文件开始打印到最新的文件*/ 
    uint16_t current_id = g_rotation.oldest_file_id;
    int files_printed = 0;
    
    while (files_printed < g_rotation.active_file_count) 
	{
        char filename[FILENAME_BUFFER_SIZE];
        generate_filename(current_id, filename);
        
        lfs_file_t test_file;
        struct lfs_file_config fcfg = 
		{
            .buffer = file_inter_buffer,
        };
        
        int err = lfs_file_opencfg(lfs, &test_file, filename, LFS_O_RDONLY, &fcfg);
        if (err >= 0) 
		{
            lfs_file_close(lfs, &test_file);
            
            /*文件存在，打印其内容*/ 
            always_Print(0, ("--- File %d/%d ---\r\n", files_printed + 1, g_rotation.active_file_count));
            rotation_print_logs(lfs, current_id);
            files_printed++;
        }
        
        current_id = (current_id + 1) % MAX_ROTATION_FILES;
        
        if (current_id == g_rotation.oldest_file_id && files_printed > 0) 
		{
            break;
        }
    }
    
    always_Print(0, ("\r\n=== All Rotation Logs Printed ===\r\n"));
}





/*
***************************************************************************************
* 函 数 名: lfs_inter_flash_init
* 功能说明: lfs内部flash初始化
* 形   参: 无
* 返 回 值: 无
***************************************************************************************
*/
static void lfs_inter_flash_init(void)
{
    uint16_t init_flag = *((volatile uint16_t*)LFS_INTER_FLASH_START_ADDR);
	
	/*第一次挂载判断*/
    if (init_flag == 0xFFFF) 
	{  
        lfs_format(&lfs_inter_flash, &inter_cfg);
		int err = lfs_mount(&lfs_inter_flash, &inter_cfg);
		if(err) 
		{
			lfs_format(&lfs_inter_flash, &inter_cfg);
			lfs_mount(&lfs_inter_flash, &inter_cfg);
		}
    }
	else
	{
		lfs_mount(&lfs_inter_flash, &inter_cfg);
	}
	
	/*先确保参数文件存在*/ 
	if (param_file_init(&lfs_inter_flash) < 0) 
	{
		always_Print(0, ("param_init: failed to initialize param file\r\n"));
		return;
	}
	always_Print(0, ("param_init: loading all parameters from flash\r\n"));

}


/*
***************************************************************************************
* 函 数 名: lfs_outer_flash_init
* 功能说明: lfs外部flash初始化
* 形   参:  无
* 返 回 值: 无
***************************************************************************************
*/
static void lfs_outer_flash_init(void)
{
	struct lfs_file_config fcfg = 
	{
        .buffer = file_outer_buffer, 
    };
	lfs_file_t file;
	u8 temp[2] = {0};
	hal_GD25Q80_read(temp,0,2);
    
	/*第一次挂载判断*/
    if (temp[0] == 0xFF && temp[1] == 0xFF) 
	{  
        lfs_format(&lfs_outer_flash, &outer_cfg);
		int err = lfs_mount(&lfs_outer_flash, &outer_cfg);
		if(err) 
		{
			lfs_format(&lfs_outer_flash, &outer_cfg);
			lfs_mount(&lfs_outer_flash, &outer_cfg);
		}
    }
	else
	{
		lfs_mount(&lfs_outer_flash, &outer_cfg);
	}
	
	lfs_file_opencfg(&lfs_outer_flash, &file,ROTATION_INFO_FILE_NAME,LFS_O_WRONLY | LFS_O_CREAT , &fcfg);
	lfs_getattr(&lfs_outer_flash, ROTATION_INFO_FILE_NAME, ROTATION_NEWEST_FILE_ID, 
				&g_rotation.newest_file_id, sizeof(g_rotation.newest_file_id));
	lfs_getattr(&lfs_outer_flash, ROTATION_INFO_FILE_NAME, ROTATION_OLDEST_FILE_ID, 
				&g_rotation.oldest_file_id, sizeof(g_rotation.oldest_file_id));
	lfs_getattr(&lfs_outer_flash, ROTATION_INFO_FILE_NAME, ROTATION_CURRENT_OFFSET_ID, 
				&g_rotation.current_file_offset, sizeof(g_rotation.current_file_offset));
	lfs_getattr(&lfs_outer_flash,ROTATION_INFO_FILE_NAME,ROTATION_ACTIVE_FILE_ID,
				&g_rotation.active_file_count,sizeof(g_rotation.active_file_count));
	always_Print(0,("newest_file_id = %d\n",g_rotation.newest_file_id));
	always_Print(0,("oldest_file_id = %d\n",g_rotation.oldest_file_id));
	always_Print(0,("current_file_offset = %d\n",g_rotation.current_file_offset));
	always_Print(0,("active_file_count = %d\n",g_rotation.active_file_count));
	lfs_file_close(&lfs_outer_flash, &file);
}




/*
***************************************************************************************
*
*  Global functions
*
***************************************************************************************
*/


/*
***************************************************************************************
* 函 数 名: lfs_print_logs
* 功能说明: 打印日志
* 形   参: 无
* 返 回 值: 无
***************************************************************************************
*/
void lfs_print_logs(uint8_t type)
{
	#define INTER_FLASH		0
	#define OUTER_FLASH		1
	if(type == OUTER_FLASH)
	{
		rotation_print_all_logs(&lfs_outer_flash);
	}
	if(type == INTER_FLASH)
	{
		rotation_print_all_logs(&lfs_inter_flash);
	}
	#undef INTER_FLASH
	#undef OUTER_FLASH
}

/*暂不需要实现*/
int lfs_log_inter_read(void *logBuf, int maxBytesToRead)
{
	return 1;
}

int lfs_log_outer_read(void *logBuf, int maxBytesToRead)
{
	return 1;
}
/*
***************************************************************************************
* 函 数 名: log_lfs_init
* 功能说明: lfs初始化
* 形   参:  无
* 返 回 值: 无
***************************************************************************************
*/
extern void hal_Delay_us(u32 ms);
void log_lfs_init(void)
{
	lfs_inter_flash_init();
	hal_Delay_us(100000); /*等待内部Flash初始化完成，必加，不然会报文件系统损坏*/ 
	lfs_outer_flash_init();
}

