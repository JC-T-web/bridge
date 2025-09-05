#include "log.h"
#include "debug.h"
#include "stm32f10x.h"
#include "app.h"
#include "stm32f10x_flash.h"
#include "string.h"
#include "stdbool.h"
#include "lfs.h"
#include "errcode_fifo.h"


int   	param_A = 1;
float	param_B = 2.5;
int 	param_C = 2;
char    Version[4] = "V1.0";

/*参数表定义（使用枚举名称）*/ 
param_entry_t param_table[MAX_PARAMS] = {
    {PARAM_ID_A,           PARAM_TYPE_INT,    {.i = 1},  	   sizeof(int)},       // param_A
    {PARAM_ID_B,           PARAM_TYPE_FLOAT,  {.f = 2.5f},     sizeof(float)},     // param_B
    {PARAM_ID_C,           PARAM_TYPE_INT,    {.i = 2},        sizeof(int)},       // param_C
    {PARAM_ID_TEMP_SENSOR, PARAM_TYPE_UINT8,  {.u8 = 100},     sizeof(uint8_t)},   
    {PARAM_ID_DEVICE_NAME, PARAM_TYPE_STRING, {.str = "V1.0"},  4},                
    {PARAM_ID_SPEED,       PARAM_TYPE_UINT16, {.u16 = 1000},   sizeof(uint16_t)},  
    {PARAM_ID_VOLTAGE,     PARAM_TYPE_UINT32, {.u32 = 12000},  sizeof(uint32_t)},  
    {PARAM_ID_CONFIG_FLAG, PARAM_TYPE_UINT8,  {.u8 = 1},       sizeof(uint8_t)},   
};


/*
***************************************************************************************
* 函 数 名: hal_logNVM
* 功能说明: 写入日志到FLASH中
* 形   参: type - 选择要操作的Flash，内部还是外部
		  format: 格式化字符串，支持的大部分标准C的标签参数
* 返 回 值: -1:写入失败
*		  正数:写入的字节长度
***************************************************************************************
*/
int hal_logNVM(FLASH_TYPE type,const char * format, ...)
{
	#define LOG_BUFF_SIZE			256

	FLASH_TYPE choose_type;
    char log_buf[LOG_BUFF_SIZE];
    int written_len;
    va_list args;
    bool needs_newline = true;
    
	choose_type = type;
	if(choose_type != INTER_FLASH && choose_type != OUTER_FLASH)
	{
		always_Print(0,("Invalid Flash type!\n"));
		return -1;
	}
	
    va_start(args, format);
    written_len = vsnprintf(log_buf, sizeof(log_buf), format, args);
    va_end(args);

    if (written_len < 0) 
	{
        return -1;
    }

    if (written_len >= sizeof(log_buf)) 
	{
        written_len = sizeof(log_buf) - 1;
    }

    /*检查是否需要添加换行符*/  
    if (written_len > 0) 
	{
        /*检查最后是否已经是结束符*/ 
        if (log_buf[written_len - 1] == '/') 
		{
            needs_newline = false;
        }
        
         /*如果需要换行且缓冲区有空间*/  
        if (needs_newline && (written_len < sizeof(log_buf) - 1)) 
		{
			log_buf[written_len] = '/'; 
			written_len+=1;     
        }
    }
    
	
	if(choose_type == INTER_FLASH)
	{    
		lfs_store_log_internal(log_buf,written_len);
	}
	if(choose_type == OUTER_FLASH)
	{
		lfs_store_log_outernal(log_buf,written_len);
	}

    return written_len;
}
 
/*
***************************************************************************************
* 函 数 名: hal_logNVM_bin
* 功能说明: 写入原始二进制数据到FLASH中（不做格式化）
* 形   参: type - 选择要操作的Flash，内部还是外部
*          data - 数据指针
*          len  - 数据长度
* 返 回 值: -1/LOG_*:失败；正数:写入的字节数
***************************************************************************************
*/
int hal_logNVM_bin(FLASH_TYPE type,const void * data, int len)
{
	if (data == NULL || len <= 0)
	{
		return LOG_BUFF_ERR;
	}

	if (type == INTER_FLASH)
	{
		return lfs_store_log_internal(data, len);
	}
	else if (type == OUTER_FLASH)
	{
		return lfs_store_log_outernal(data, len);
	}
	else
	{
		return LOG_WRITE_ERR;
	}
}



/*
***************************************************************************************
*    函 数 名: hal_logNVM_Read
*    功能说明: 从非易失存储器区读取日志内容
*    形   参: type - 选择要操作的Flash，内部还是外部
*			 logBuf - 字符串缓存
*  			 offset - 读取起始偏移，单位字节
*			 maxBytesToRead - 可以返回日志内容的字节长度最大值
*    返 回 值: n - 返回读到的字节数
*			 0 - 读取失败
***************************************************************************************
*/
int hal_logNVM_Read(FLASH_TYPE type,void * logBuf, int maxBytesToRead)
{
	#define LOG_READ_ADDR	(LOG_DATA_ADDR)
	if(logBuf == NULL || maxBytesToRead <= 0) 
	{
        return LOG_BUFF_ERR; 
    }
	if(type == INTER_FLASH)
	{
		return lfs_log_inter_read(logBuf, maxBytesToRead);
	}
	if(type == OUTER_FLASH)
	{
		return lfs_log_outer_read(logBuf, maxBytesToRead);
	}	
	
}

/*
***************************************************************************************
*    函 数 名: hal_log_print
*    功能说明: 打印日志内容
*    形   参: type - 选择要操作的Flash，内部还是外部
*    返 回 值: 无
***************************************************************************************
*/
void hal_log_print(FLASH_TYPE type)
{
	lfs_print_logs(type);
}


/*
***************************************************************************************
*    函 数 名: hal_log_clean
*    功能说明: 清除日志
*    形   参: type - 要操作的Flash，内部还是外部
*    返 回 值: 无
***************************************************************************************
*/
void hal_log_clean(FLASH_TYPE type)
{
	if(type == INTER_FLASH)
	{
		FLASH_Unlock();	
		for(int i=0; i<LOG_SIZE; i+=PAGE_SIZE)
		{
			FLASH_ErasePage(LOG_ADDR_START+i);
		}
		FLASH_Lock();
	}
	
}


/*
***************************************************************************************
* 函 数 名: param_init
* 功能说明: 系统参数初始化
* 形   参: 无
* 返 回 值: 无
***************************************************************************************
*/
void param_init(void)
{
	/*逐个加载参数并赋值给对应变量*/ 
	for (int i = 0; i < MAX_PARAMS; i++) 
	{
		if (param_table[i].id != 0) 
		{
			param_value_t value = param_get_value(param_table[i].id);

			switch (param_table[i].id) 
			{
				case PARAM_ID_A:
					param_A = value.i;
					always_Print(0, ("param_init: param_A = %d\r\n", param_A));
					break;
				case PARAM_ID_B:
					param_B = value.f;
					always_Print(0, ("param_init: param_B = %.2f\r\n", param_B));
					break;
				case PARAM_ID_C:
					param_C = value.i;
					always_Print(0, ("param_init: param_C = %d\r\n", param_C));
					break;
				case PARAM_ID_DEVICE_NAME:
					memcpy(Version,value.str,4);
					always_Print(0, ("param_init: Version = %s\r\n", Version));
					break;
				default:
				break;
			}
		}
	}

	always_Print(0, ("param_init: completed\r\n"));
}


/*
***************************************************************************************
* 函 数 名: hal_statNVM_read
* 功能说明: 根据传入的变量id，返回对应的数值
* 形   参: id - 参数id
* 返 回 值: 无
***************************************************************************************
*/
param_value_t hal_statNVM_read(param_id_enum_t id)
{
	return param_get_value(id);
}


/*
***************************************************************************************
* 函 数 名: hal_statNVM_write
* 功能说明: 根据传入的变量id，更改对应的数值
* 形   参: id 	- 参数id
*		  value - 数值
* 返 回 值: 0成功，-1失败
***************************************************************************************
*/
int hal_statNVM_write(param_id_enum_t id,const void *value)
{
	return param_set(id,value);
}



/*
***************************************************************************************
* 函 数 名: lfs_RW_test
* 功能说明: lfs读写测试
* 形   参: 无
* 返 回 值: 无
***************************************************************************************
*/
void lfs_RW_test(void)
{
	static uint8_t key_down_num = 0;
	char log_buffer[128];
	static int end = 0;
	static int start_time,end_time = 0;
	if (g.user_key_button.clicked == ON) 
	{
		g.user_key_button.clicked = OFF; 
		key_down_num++;
#if 1
		end += 50;		
		always_Print(0, ("key down %d\r\n",key_down_num));
		//always_Print(0, ("before write file size = %d\r\n",get_file_size(LOG_FILENAME)));
		start_time = g_systick_ms;
		for(int i=end-50;i<end;i++)
		{
			sprintf(log_buffer, "this is #%4d write, hello world", i);
			hal_logNVM(OUTER_FLASH,log_buffer);
		}
		end_time = g_systick_ms;
		//always_Print(0, ("after write file size = %d\r\n",get_file_size(LOG_FILENAME)));
		always_Print(0, ("\r\n"));
		//log_lfs_printf();
		hal_log_print(OUTER_FLASH);
		always_Print(0, ("used time = %d\r\n",end_time - start_time));
	
#else 
		param_A+=1;
		param_B+=0.1;
		param_C+=1;
		memcpy(Version,"V2.0",3);
		param_set(PARAM_ID_A,&param_A);
		param_set(PARAM_ID_B,&param_B);
		param_set(PARAM_ID_C,&param_C);
		param_set(PARAM_ID_DEVICE_NAME,Version);
		
		param_A = param_get_value(PARAM_ID_A).i;
		param_B = param_get_value(PARAM_ID_B).f;
		param_C = param_get_value(PARAM_ID_C).i;
		char *temp = param_get_value(PARAM_ID_DEVICE_NAME).str;
		memcpy(Version,temp,3);
		
		always_Print(0, ("param_A = %d\r\n",param_A));
		always_Print(0, ("param_B = %.1f\r\n",param_B));
		always_Print(0, ("param_C = %d\r\n",param_C));
		always_Print(0, ("str = %s\r\n",Version));
#endif
	}
}

/*
***************************************************************************************
* 函 数 名: hal_err_code_store
* 功能说明: 将错误码FIFO中的原始记录逐条写入log
* 形   参: 无
* 返 回 值: 无
***************************************************************************************
*/
void hal_err_code_store(void)
{
	ErrCodeFifoItem_t rec;

	if(ErrCodeFIFO_Pop(&rec))
	{
		hal_logNVM(OUTER_FLASH,"%d-%llu-%x", rec.src,rec.ts_ms,rec.fault);
		hal_log_print(OUTER_FLASH);
	}
}     

/*
***************************************************************************************
*    函 数 名: hal_log_init
*    功能说明: 日志初始化，打印初始化的结果
*    形   参:  无
*    返 回 值: 无
***************************************************************************************
*/
void hal_log_init(void)
{
	log_lfs_init();
	param_init();
}



