/*
*********************************************************************************************************
*
*   模块名称 : 错误码FIFO模块
*   文件名称 : errcode_fifo.c
*   版    本 : V1.0
*   说    明 : 实现错误日志存储的FIFO，尽量保证错误码不丢失
*   修改记录 :
*       版本号  	日期        作者     	说明
*       V1.0    2025-09-4  汤金铖    	实现基本功能
*
*********************************************************************************************************
*/

#include "errcode_fifo.h"

#ifndef ERRFIFO_CAPACITY
#define ERRFIFO_CAPACITY 32
#endif

#ifndef ERRFIFO_OVERWRITE_ON_FULL
#define ERRFIFO_OVERWRITE_ON_FULL 1
#endif

#ifndef ERRFIFO_USE_CRITICAL
#define ERRFIFO_USE_CRITICAL 1
#endif

#if ERRFIFO_USE_CRITICAL
#include "stm32f10x.h"  
#endif

typedef struct
{
	ErrCodeFifoItem_t buf[ERRFIFO_CAPACITY];
	u16 head;   
	u16 tail;   
	u16 count;  
} ErrCodeFifo_t;

ErrCodeFifo_t s_fifo;
static ErrCodeGetTimeMsFn s_get_time_ms = 0;

#if ERRFIFO_USE_CRITICAL
/*
***************************************************************************************
* 函 数 名: _errfifo_enter
* 功能说明: 进入临界区，短暂屏蔽中断，返回进入前的 PRIMASK 状态
* 形   参: 无
* 返 回 值: 进入前的 PRIMASK 值（0 表示原本开中断，1 表示原本关中断）
***************************************************************************************
*/
static inline u32 _errfifo_enter(void)
{
	u32 pm = __get_PRIMASK();
	__disable_irq();
	return pm;
}

/*
***************************************************************************************
* 函 数 名: _errfifo_exit
* 功能说明: 退出临界区，根据进入前的 PRIMASK 状态决定是否恢复中断
* 形   参: pm - 进入临界区前保存的 PRIMASK 值
* 返 回 值: 无
***************************************************************************************
*/
static inline void _errfifo_exit(u32 pm)
{
	if ((pm & 1U) == 0U)
	{
		__enable_irq();
	}
}
#else
static inline u32 _errfifo_enter(void) { return 0U; }
static inline void _errfifo_exit(u32 pm) { (void)pm; }
#endif

/*
***************************************************************************************
* 函 数 名: _inc
* 功能说明: 环形缓冲索引自增，超出容量后从 0 重新开始
* 形   参: x - 当前索引
* 返 回 值: 自增后的索引值
***************************************************************************************
*/
static inline u16 _inc(u16 x)
{
	return (u16)((x + 1U) % ERRFIFO_CAPACITY);
}

/*
***************************************************************************************
* 函 数 名: ErrCodeFIFO_Init
* 功能说明: 初始化错误码 FIFO，清空队列并设置时间获取函数
* 形   参: get_time_ms - 获取当前时间(毫秒)的回调函数指针，可为 NULL
* 返 回 值: 无
***************************************************************************************
*/
void ErrCodeFIFO_Init(ErrCodeGetTimeMsFn get_time_ms)
{
	u32 pm = _errfifo_enter();
	s_fifo.head = 0;
	s_fifo.tail = 0;
	s_fifo.count = 0;
	s_get_time_ms = get_time_ms;
	_errfifo_exit(pm);
}

/*
***************************************************************************************
* 函 数 名: ErrCodeFIFO_IsEmpty
* 功能说明: 判断 FIFO 是否为空
* 形   参: 无
* 返 回 值: 1 表示空，0 表示非空
***************************************************************************************
*/
int ErrCodeFIFO_IsEmpty(void)
{
	return (s_fifo.count == 0) ? 1 : 0;
}

/*
***************************************************************************************
* 函 数 名: ErrCodeFIFO_IsFull
* 功能说明: 判断 FIFO 是否已满
* 形   参: 无
* 返 回 值: 1 表示满，0 表示未满
***************************************************************************************
*/
int ErrCodeFIFO_IsFull(void)
{
	return (s_fifo.count == ERRFIFO_CAPACITY) ? 1 : 0;
}

/*
***************************************************************************************
* 函 数 名: ErrCodeFIFO_Size
* 功能说明: 获取 FIFO 当前已存放的元素个数
* 形   参: 无
* 返 回 值: 当前元素个数
***************************************************************************************
*/
u16 ErrCodeFIFO_Size(void)
{
	return s_fifo.count;
}

/*
***************************************************************************************
* 函 数 名: ErrCodeFIFO_Contains
* 功能说明: 判断 (src,fault) 是否已存在于 FIFO 中
* 形   参: src - 错误来源；fault - 故障码(16位)
* 返 回 值: 1 表示存在；0 表示不存在
***************************************************************************************
*/
int ErrCodeFIFO_Contains(ErrCodeSrc src, u16 fault)
{
	u32 pm = _errfifo_enter();

	u16 i = 0U;
	u16 idx = s_fifo.tail;
	for (i = 0U; i < s_fifo.count; i++)
	{
		const ErrCodeFifoItem_t *it = &s_fifo.buf[idx];
		if ((it->src == (u8)src) && (it->fault == fault))
		{
			_errfifo_exit(pm);
			return 1;
		}
		idx = _inc(idx);
	}

	_errfifo_exit(pm);
	return 0;
}

/*
***************************************************************************************
* 函 数 名: ErrCodeFIFO_Push
* 功能说明: 向 FIFO 末尾写入一条错误信息（满时可选择覆盖最旧元素）
* 形   参: src - 错误来源；fault - 故障码(16位)
* 返 回 值: 0 表示成功；-1 表示队列已满且未配置覆盖
***************************************************************************************
*/
int ErrCodeFIFO_Push(ErrCodeSrc src, u16 fault)
{
	u32 pm = _errfifo_enter();

	if (s_fifo.count == ERRFIFO_CAPACITY)
	{
#if ERRFIFO_OVERWRITE_ON_FULL
		s_fifo.tail = _inc(s_fifo.tail);
		s_fifo.count--;
#else
		_errfifo_exit(pm);
		return -1;
#endif
	}

	ErrCodeFifoItem_t *item = &s_fifo.buf[s_fifo.head];
	item->src   = (u8)src;
	item->ts_ms = (s_get_time_ms ? s_get_time_ms() : 0);
	item->fault = fault;

	s_fifo.head = _inc(s_fifo.head);
	s_fifo.count++;

	_errfifo_exit(pm);
	return 0;
}

/*
***************************************************************************************
* 函 数 名: ErrCodeFIFO_Pop
* 功能说明: 从 FIFO 头部弹出一条错误信息
* 形   参: out - 输出参数，接收弹出的元素，不能为空
* 返 回 值: 1 表示成功弹出；0 表示队列为空
***************************************************************************************
*/
int ErrCodeFIFO_Pop(ErrCodeFifoItem_t *out)
{
	if (out == 0) {return 0;}

	u32 pm = _errfifo_enter();

	if (s_fifo.count == 0)
	{
		_errfifo_exit(pm);
		return 0;
	}

	*out = s_fifo.buf[s_fifo.tail];
	s_fifo.tail = _inc(s_fifo.tail);
	s_fifo.count--;

	_errfifo_exit(pm);
	return 1;
}

/*
***************************************************************************************
* 函 数 名: ErrCodeFIFO_Peek
* 功能说明: 查看 FIFO 头部元素但不弹出
* 形   参: out - 输出参数，接收查看到的元素，不能为空
* 返 回 值: 1 表示存在元素；0 表示队列为空
***************************************************************************************
*/
int ErrCodeFIFO_Peek(ErrCodeFifoItem_t *out)
{
	if (out == 0)
		return 0;

	u32 pm = _errfifo_enter();

	if (s_fifo.count == 0U)
	{
		_errfifo_exit(pm);
		return 0;
	}

	*out = s_fifo.buf[s_fifo.tail];

	_errfifo_exit(pm);
	return 1;
}

/*
***************************************************************************************
* 函 数 名: ErrCodeFIFO_PushIfAbsent
* 功能说明: 若 (src,fault) 不存在则入队；已存在则不入队
* 形   参: src - 错误来源；fault - 故障码(16位)
* 返 回 值: 1 表示成功入队；0 表示已存在未入队；-1 表示队列满且未覆盖
***************************************************************************************
*/
int ErrCodeFIFO_PushIfAbsent(ErrCodeSrc src, u16 fault)
{
	u32 pm = _errfifo_enter();

	// 查重
	u16 i = 0U;
	u16 idx = s_fifo.tail;
	for (i = 0U; i < s_fifo.count; i++)
	{
		const ErrCodeFifoItem_t *it = &s_fifo.buf[idx];
		if ((it->src == (u8)src) && (it->fault == fault))
		{
			_errfifo_exit(pm);
			return 0;
		}
		idx = _inc(idx);
	}

	if (s_fifo.count == ERRFIFO_CAPACITY)
	{
#if ERRFIFO_OVERWRITE_ON_FULL
		s_fifo.tail = _inc(s_fifo.tail);
		s_fifo.count--;
#else
		_errfifo_exit(pm);
		return -1;
#endif
	}

	ErrCodeFifoItem_t *item = &s_fifo.buf[s_fifo.head];
	item->src   = (u8)src;
	item->ts_ms = (s_get_time_ms ? s_get_time_ms() : 0);
	item->fault = fault;

	s_fifo.head = _inc(s_fifo.head);
	s_fifo.count++;

	_errfifo_exit(pm);
	return 1;
}


