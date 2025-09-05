#ifndef __ERR_FIFO_H__
#define __ERR_FIFO_H__

#include "types.h"
#include "user_def.h"

#if 1
/*占用16个字节*/
typedef struct
{
	unsigned long long ts_ms;/*时间戳(ms)*/ 
	u8  src;      /*故障码来源*/ 
	u16 fault;    /*具体的故障码*/ 
} ErrCodeFifoItem_t;
#else
/*占用11个字节，但是非对其访问性能下降*/
#pragma pack(1)
typedef struct
{
	unsigned long long ts_ms;/*时间戳(ms)*/ 
	u8  src;      /*故障码来源*/ 
	u16 fault;    /*具体的故障码*/ 
} ErrCodeFifoItem_t;
#pragma pack()
#endif



typedef u32 (*ErrCodeGetTimeMsFn)(void);   // 获取当前时间的回调

void ErrCodeFIFO_Init(ErrCodeGetTimeMsFn get_time_ms);
int  ErrCodeFIFO_Push(ErrCodeSrc src, u16 fault);             // 0:OK, -1:满且不覆盖
int  ErrCodeFIFO_Pop(ErrCodeFifoItem_t *out);                 // 1:弹出, 0:空
int  ErrCodeFIFO_Peek(ErrCodeFifoItem_t *out);                // 1:有,   0:空
int  ErrCodeFIFO_IsEmpty(void);                               // 1:空
int  ErrCodeFIFO_IsFull(void);                                // 1:满
u16  ErrCodeFIFO_Size(void);                                  // 当前数量
int  ErrCodeFIFO_Contains(ErrCodeSrc src, u16 fault);         // 1:存在, 0:不存在
int  ErrCodeFIFO_PushIfAbsent(ErrCodeSrc src, u16 fault);     // 1:入队, 0:已存在, -1:满未入队

#endif
