#include "lfs.h"
#include "hal_QDflash.h"
#include "stm32f10x_flash.h"
#include <string.h>
#include "debug.h"
#include "g.h"

#define OUTERFLASH_ADDR_START		0

#if defined(PCB_VCU_BOARD_P02)
#define PAGE_SIZE					2048
#elif defined(PCB_EPB_BOARD_P02)
#define PAGE_SIZE					1024
#endif

/*日志区域所用页数*/ 
#define BLOCK_NUM					6
/*日志区域的起始地址*/ 
#define LFS_INTER_FLASH_START_ADDR  0x08032000UL 
/*日志区域空间*/ 
#define LFS_INTER_FLASH_SIZE        (BLOCK_NUM * PAGE_SIZE)    // 4页存储空间




/* 静态内存使用方式必须设定这四个缓存*/
__align(4) static uint8_t inter_read_buffer[16];
__align(4) static uint8_t inter_prog_buffer[16];
__align(4) static uint8_t inter_lookahead_buffer[16];
__align(4) static uint8_t outer_read_buffer[16];
__align(4) static uint8_t outer_prog_buffer[16];
__align(4) static uint8_t outer_lookahead_buffer[16];

/*为文件操作增加独立缓冲区，避免与全局缓冲区冲突*/ 
__align(4) static uint8_t file_read_buffer[16];
__align(4) static uint8_t file_write_buffer[16];

/*lfs句柄,内外部flash*/ 
lfs_t lfs_inter_flash;
lfs_t lfs_outer_flash;


/**
 * lfs与内部flash读数据接口
 * @param  c
 * @param  block  块编号
 * @param  off    块内偏移地址
 * @param  buffer 用于存储读取到的数据
 * @param  size   要读取的字节数
 * @return
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


/**
 * lfs与内部flash写数据接口
 * @param  c
 * @param  block  块编号
 * @param  off    块内偏移地址
 * @param  buffer 待写入的数据
 * @param  size   待写入数据的大小
 * @return
 */
static int lfs_inter_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    uint32_t flash_addr = LFS_INTER_FLASH_START_ADDR + (block * c->block_size) + off;
 
    if (buffer == NULL || size == 0)
	{
        return LFS_ERR_INVAL;
    }
    
    if (flash_addr + size > 0x08040000UL) // STM32F105RCT6 Flash结束地址
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
		__enable_irq(); // 开启总中断
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

/**
 * lfs与内部flash擦除接口
 * @param  c
 * @param  block 块编号
 * @return
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
/**
 * lfs与外部flash读数据接口
 * @param  c
 * @param  block  块编号
 * @param  off    块内偏移地址
 * @param  buffer 用于存储读取到的数据
 * @param  size   要读取的字节数
 * @return
 */
static int lfs_outer_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
	hal_GD25Q80_read((uint8_t *)buffer,c->block_size * block + off,size);
	return LFS_ERR_OK;
}

/**
 * lfs与外部flash写数据接口
 * @param  c
 * @param  block  块编号
 * @param  off    块内偏移地址
 * @param  buffer 待写入的数据
 * @param  size   待写入数据的大小
 * @return
 */
static int lfs_outer_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
	hal_GD25Q80_write((uint8_t *)buffer, c->block_size * block + off, size);
	return LFS_ERR_OK;
}

/**
 * lfs与外部flash擦除接口
 * @param  c
 * @param  block 块编号
 * @return
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
	.lookahead_size = 16,
	.block_cycles = 500,

	.file_max = 2048, 
	.read_buffer = inter_read_buffer,
	.prog_buffer = inter_prog_buffer,
	.lookahead_buffer = inter_lookahead_buffer,
};

const struct lfs_config outer_cfg =
{
	// block device operations
	.read  = lfs_outer_read,
	.prog  = lfs_outer_prog,
	.erase = lfs_outer_erase,
	.sync  = lfs_deskio_sync,

	// block device configuration
	.read_size = 16,
	.prog_size = 16,
	.block_size = 4096,
	.block_count = 128,
	.cache_size = 16,
	.lookahead_size = 16,
	.block_cycles = 500,

	
	// 使用静态内存必须设置这几个缓存
	.read_buffer = outer_read_buffer,
	.prog_buffer = outer_prog_buffer,
	.lookahead_buffer = outer_lookahead_buffer,
};


struct lfs_file_config fcfg1 = {
	.buffer = file_write_buffer,  // 必填：文件级缓存（静态或在close前有效）
	
};

lfs_size_t get_file_size(const char *filename)
{
    struct lfs_info info;
    if (lfs_stat(&lfs_inter_flash, filename, &info) == 0) 
	{
        return info.size;
    }
    return 0;
}


// --- 配置文件名和属性ID ---
#define LOG_FILENAME 			"datalog.txt"
#define LOG_OFFSET_ATTR_ID 		0x01
static lfs_off_t g_write_offset = 0;
#define LOG_FILE_MAX_SIZE		4*2048
/**
 * 使用LittleFS存储日志到内部Flash
 * @param log_message 日志消息字符串
 * @param message_len 消息长度
 * @return 成功时返回写入的字节长度 ，-1表示失败
 */ 
int lfs_store_log_internal(const void *log_message, int message_len)
{

	if (log_message == NULL || message_len <= 0) 
	{
        return -1;
    }
	
    struct lfs_file_config fcfg = 
	{
        .buffer = file_write_buffer, 
    };
	lfs_file_t file;

	if (g_write_offset + message_len > LOG_FILE_MAX_SIZE) 
	{
        g_write_offset = 0; 
        printf("Log file wrapped around. Seeking to beginning.\n");
    }

    int err = lfs_file_opencfg(&lfs_inter_flash, &file,LOG_FILENAME,LFS_O_WRONLY | LFS_O_CREAT , &fcfg);
	//int err = lfs_file_opencfg(&lfs_inter_flash, &file, "Exception_log.txt",LFS_O_WRONLY | LFS_O_CREAT , &fcfg);
    if (err < 0) 
	{
        lfs_format(&lfs_inter_flash, &inter_cfg);
        lfs_mount(&lfs_inter_flash, &inter_cfg);
        err = lfs_file_opencfg(&lfs_inter_flash, &file,LOG_FILENAME,LFS_O_WRONLY | LFS_O_CREAT, &fcfg);
		//err = lfs_file_opencfg(&lfs_inter_flash, &file, "Exception_log.txt",LFS_O_WRONLY | LFS_O_CREAT , &fcfg);
        if (err < 0)
		{
			return -1;
		}			
    }
	err = lfs_file_seek(&lfs_inter_flash, &file, g_write_offset, LFS_SEEK_SET);
    if (err < 0) {
        printf("Error: log_append failed to seek. Code: %d\n", err);
        return err;
    }
    lfs_ssize_t written = lfs_file_write(&lfs_inter_flash, &file,log_message, message_len);
	g_write_offset += written;
    lfs_setattr(&lfs_inter_flash, LOG_FILENAME, LOG_OFFSET_ATTR_ID, &g_write_offset, sizeof(g_write_offset));
	
    lfs_file_close(&lfs_inter_flash, &file);

    return written;
}


char buffer[64];

int remove_err = -1;


void log_lfs_printf(void)
{
	lfs_file_t file;
	struct lfs_file_config fcfg = 
	{
        .buffer = file_read_buffer, 
    };
	
	// 打开日志文件进行读取
	int err = lfs_file_opencfg(&lfs_inter_flash, &file, LOG_FILENAME, LFS_O_RDONLY, &fcfg);
	if (err < 0) 
	{
		printf("Error: Failed to open log file for reading. Code: %d\r\n", err);
		return;
	}
	
	// 获取文件大小
	lfs_soff_t file_size = lfs_file_size(&lfs_inter_flash, &file);
	if (file_size < 0) 
	{
		printf("Error: Failed to get file size. Code: %d\r\n", (int)file_size);
		lfs_file_close(&lfs_inter_flash, &file);
		return;
	}
	
	if (file_size == 0) 
	{
		printf("Log file is empty.\r\n");
		lfs_file_close(&lfs_inter_flash, &file);
		return;
	}
	
	printf("=== Log File Contents (Size: %d bytes) ===\r\n", (int)file_size);
	
	// 日志条目缓冲区，用于累积一条完整的日志
	char log_entry[128]; // 单条日志缓冲区
	int log_entry_pos = 0; // 当前日志条目位置
	int log_count = 0; // 日志条目计数
	
	// 逐字节读取文件内容
	lfs_soff_t total_read = 0;
	
	while (total_read < file_size) 
	{
		char current_char;
		
		// 读取一个字节
		lfs_ssize_t bytes_read = lfs_file_read(&lfs_inter_flash, &file, &current_char, 1);
		if (bytes_read < 0) 
		{
			printf("Error: Failed to read from file. Code: %d\r\n", (int)bytes_read);
			break;
		}
		
		if (bytes_read == 0) 
		{
			break; // 达到文件末尾
		}
		
		total_read++;
		
		// 检查是否是日志结束符 '/'
		if (current_char == '/') 
		{
			// 遇到结束符，打印这条日志
			if (log_entry_pos > 0) 
			{
				log_entry[log_entry_pos] = '\0'; // 添加字符串结束符
				log_count++;
				printf("[%d] %s\r\n", log_count, log_entry);
				log_entry_pos = 0; // 重置缓冲区位置
			}
		}
		else 
		{
			// 累积字符到日志条目缓冲区
			if (log_entry_pos < sizeof(log_entry) - 1) 
			{
				log_entry[log_entry_pos] = current_char;
				log_entry_pos++;
			}
			else 
			{
				// 缓冲区满了，强制输出这条日志（可能是异常情况）
				log_entry[sizeof(log_entry) - 1] = '\0';
				log_count++;
				printf("[%d] %s (TRUNCATED)\r\n", log_count, log_entry);
				log_entry_pos = 0;
				
				// 将当前字符作为新日志的开始
				log_entry[log_entry_pos] = current_char;
				log_entry_pos++;
			}
		}
	}
	
	// 处理最后一条未完成的日志（如果存在）
	if (log_entry_pos > 0) 
	{
		log_entry[log_entry_pos] = '\0';
		log_count++;
		printf("[%d] %s (INCOMPLETE)\r\n", log_count, log_entry);
	}
	
	printf("=== Total %d log entries found ===\r\n", log_count);
	
	// 关闭文件
	lfs_file_close(&lfs_inter_flash, &file);
}

static uint8_t key_down_num = 0;
void lfs_RW_test()
{
	char log_buffer[64];
	int message_len;
	if (g.user_key_button.clicked == ON) 
	{
		key_down_num++;
		g.user_key_button.clicked = OFF; 
		always_Print(0, ("key down %d\r\n",key_down_num));
		always_Print(0, ("before write file size = %d\r\n",get_file_size(LOG_FILENAME)));
		for(int i= (key_down_num-1)*50;i<100;i++)
		{
			message_len = sprintf(log_buffer, "this is #%3d write,hello world/", i);
			lfs_store_log_internal(log_buffer,message_len);
		}
		always_Print(0, ("after write file size = %d\r\n",get_file_size(LOG_FILENAME)));
		always_Print(0, ("\r\n"));
		log_lfs_printf();
	}
}



void log_lfs_init(void)
{

	struct lfs_file_config fcfg = 
	{
        .buffer = file_write_buffer, 
    };
	lfs_file_t file;
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
	lfs_file_opencfg(&lfs_inter_flash, &file,LOG_FILENAME,LFS_O_WRONLY | LFS_O_CREAT , &fcfg);
	int err = lfs_getattr(&lfs_inter_flash, LOG_FILENAME, LOG_OFFSET_ATTR_ID, &g_write_offset, sizeof(g_write_offset));
	if (err >= 0) {
        // --- 成功获取属性 ---
        // err 返回的是读取到的字节数，我们最好验证一下
        if (err == sizeof(g_write_offset)) {
            printf("Success: Loaded log write offset from attribute: %lu\n", (unsigned long)g_write_offset);
        } else {
            // 字节数不匹配，可能意味着数据损坏或版本不兼容。
            // 作为安全措施，我们从头开始。
            printf("Warning: Attribute size mismatch (read %d, expected %d). Resetting offset to 0.\n", err, (int)sizeof(g_write_offset));
            g_write_offset = 0;
        }
    } else if (err == LFS_ERR_NOATTR) {
        // --- 属性不存在 ---
        // 这是一个“正常的失败”，说明文件是新的，或者从未设置过此属性。
        // 我们只需将内存中的写指针初始化为0即可。
        printf("Info: No offset attribute found. This is normal for a new file. Initializing offset to 0.\n");
        g_write_offset = 0;
    } else {
        // --- 发生其他严重错误 ---
        // 例如，LFS_ERR_CORRUPT 等。
        printf("Error: Failed to get log offset attribute. Code: %d\n", err);
        lfs_file_close(&lfs_inter_flash, &file); // 清理资源
    }

	lfs_file_close(&lfs_inter_flash, &file);
}

