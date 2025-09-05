#ifndef __PARAM_BRIDGE
#define __PARAM_BRIDGE

/*---------- 解决lfs_port.h和log.h重复包含的问题----------*/

/*-------------------- 参数键值对相关 --------------------*/
/*参数ID枚举定义*/ 
#define MAX_PARAMS 10
typedef enum {
    PARAM_ID_OFFSET = 0x01,        // 保留给日志偏移量
    PARAM_ID_A = 0x02,             // param_A (int类型)
    PARAM_ID_B = 0x03,             // param_B (float类型)  
    PARAM_ID_C = 0x04,             // param_C (int类型)
    PARAM_ID_TEMP_SENSOR = 0x05,   // 温度传感器值 (uint8类型)
    PARAM_ID_DEVICE_NAME = 0x06,   // 设备名称 (string类型)
    PARAM_ID_SPEED = 0x07,         // 速度参数 (uint16类型)
    PARAM_ID_VOLTAGE = 0x08,       // 电压参数 (uint32类型)
    PARAM_ID_CONFIG_FLAG = 0x09,   // 配置标志 (uint8类型)
    PARAM_ID_MAX                   // 枚举结束标记
} param_id_enum_t;


#endif
