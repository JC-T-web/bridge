#include "bsp_uart.h"



#define UART_GPIO_SPEED     GPIO_Speed_50MHz


#if UART1_FIFO_EN == 1
    /* 串口1的GPIO  PA9, PA10   RS232 DB9接口 */
    #define USART1_CLK_ENABLE()             RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE)

    #define USART1_TX_GPIO_CLK_ENABLE()     RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE)
    #define USART1_TX_GPIO_PORT             GPIOA
    #define USART1_TX_PIN                   GPIO_Pin_9

    #define USART1_RX_GPIO_CLK_ENABLE()     RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE)
    #define USART1_RX_GPIO_PORT             GPIOA   
    #define USART1_RX_PIN                   GPIO_Pin_10
#endif

#if UART2_FIFO_EN == 1
    /* 串口2的GPIO --- PA2 PA3  GPS (只用RX。 TX被以太网占用） */
    #define USART2_CLK_ENABLE()             RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE)

    #define USART2_TX_GPIO_CLK_ENABLE()     RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE)
    #define USART2_TX_GPIO_PORT             GPIOA
    #define USART2_TX_PIN                   GPIO_Pin_2

    #define USART2_RX_GPIO_CLK_ENABLE()     RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE)
    #define USART2_RX_GPIO_PORT             GPIOA
    #define USART2_RX_PIN                   GPIO_Pin_3
#endif

#if UART3_FIFO_EN == 1
    /* 串口3的GPIO --- PB10 PB11  RS485 */
    #define USART3_CLK_ENABLE()             RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE)

    #define USART3_TX_GPIO_CLK_ENABLE()     RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE)
    #define USART3_TX_GPIO_PORT             GPIOB
    #define USART3_TX_PIN                   GPIO_Pin_10

    #define USART3_RX_GPIO_CLK_ENABLE()     RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE)
    #define USART3_RX_GPIO_PORT             GPIOB
    #define USART3_RX_PIN                   GPIO_Pin_11
#endif


/* 定义每个串口结构体变量 */
#if UART1_FIFO_EN == 1
UART_T g_tUart1;
uint8_t g_TxBuf1[UART1_TX_BUF_SIZE]; /* 发送缓冲区 */
uint8_t g_RxBuf1[UART1_RX_BUF_SIZE]; /* 接收缓冲区 */
#endif

#if UART2_FIFO_EN == 1
static UART_T g_tUart2;
static uint8_t g_TxBuf2[UART2_TX_BUF_SIZE]; /* 发送缓冲区 */
static uint8_t g_RxBuf2[UART2_RX_BUF_SIZE]; /* 接收缓冲区 */
#endif

#if UART3_FIFO_EN == 1
static UART_T g_tUart3;
static uint8_t g_TxBuf3[UART3_TX_BUF_SIZE]; /* 发送缓冲区 */
static uint8_t g_RxBuf3[UART3_RX_BUF_SIZE]; /* 接收缓冲区 */
#endif

void RS485_SendBefor()
{


}

void RS485_SendOver()
{



}


void UART1_RevCallBack(uint8_t  byte)
{
	if(byte == 0x55 && (g_tUart1.pRxBuf[g_tUart1.usRxWrite-3] == 0xAA))
	{
		g_tUart1.usRxFlag = 1;
	}
}
/*
*********************************************************************************************************
*    函 数 名: UartVarInit
*    功能说明: 初始化串口相关的变量
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void UartVarInit(void)
{
#if UART1_FIFO_EN == 1
    memset(&g_tUart1, 0, sizeof(UART_T));
	g_tUart1.uart = USART1;
	g_tUart1.usRxFlag = 0;
    g_tUart1.pTxBuf = g_TxBuf1;                             /* 发送缓冲区指针 */
    g_tUart1.pRxBuf = g_RxBuf1;                             /* 接收缓冲区指针 */
    g_tUart1.usTxBufSize = UART1_TX_BUF_SIZE;               /* 发送缓冲区大小 */
    g_tUart1.usRxBufSize = UART1_RX_BUF_SIZE;               /* 接收缓冲区大小 */
    g_tUart1.usTxWrite = 0;                                 /* 发送FIFO写索引 */
    g_tUart1.usTxRead = 0;                                  /* 发送FIFO读索引 */
    g_tUart1.usRxWrite = 0;                                 /* 接收FIFO写索引 */
    g_tUart1.usRxRead = 0;                                  /* 接收FIFO读索引 */
    g_tUart1.usRxCount = 0;                                 /* 接收到的新数据个数 */
    g_tUart1.usTxCount = 0;                                 /* 待发送的数据个数 */
    g_tUart1.SendBefor = RS485_SendBefor;                   /* 发送数据前的回调函数 */
    g_tUart1.SendOver = RS485_SendOver;                     /* 发送完毕后的回调函数 */
    g_tUart1.ReciveNew = UART1_RevCallBack;                 /* 接收到新数据后的回调函数 */
    g_tUart1.Sending = 0;                                   /* 正在发送中标志 */
#endif

#if UART2_FIFO_EN == 1
    memset(&g_tUart2, 0, sizeof(UART_T));
    g_tUart2.huart.Instance = USART2;                       /* STM32 串口设备 */
    g_tUart2.pTxBuf = g_TxBuf2;                             /* 发送缓冲区指针 */
    g_tUart2.pRxBuf = g_RxBuf2;                             /* 接收缓冲区指针 */
    g_tUart2.usTxBufSize = UART2_TX_BUF_SIZE;               /* 发送缓冲区大小 */
    g_tUart2.usRxBufSize = UART2_RX_BUF_SIZE;               /* 接收缓冲区大小 */
    g_tUart2.usTxWrite = 0;                                 /* 发送FIFO写索引 */
    g_tUart2.usTxRead = 0;                                  /* 发送FIFO读索引 */
    g_tUart2.usRxWrite = 0;                                 /* 接收FIFO写索引 */
    g_tUart2.usRxRead = 0;                                  /* 接收FIFO读索引 */
    g_tUart2.usRxCount = 0;                                 /* 接收到的新数据个数 */
    g_tUart2.usTxCount = 0;                                 /* 待发送的数据个数 */
    g_tUart2.SendBefor = 0;                                 /* 发送数据前的回调函数 */
    g_tUart2.SendOver = 0;                                  /* 发送完毕后的回调函数 */
    g_tUart2.ReciveNew = 0;                                 /* 接收到新数据后的回调函数 */
    g_tUart2.Sending = 0;                                   /* 正在发送中标志 */
#endif

#if UART3_FIFO_EN == 1
    memset(&g_tUart3, 0, sizeof(UART_T));
    g_tUart3.huart.Instance = USART3;                       /* STM32 串口设备 */
    g_tUart3.pTxBuf = g_TxBuf3;                             /* 发送缓冲区指针 */
    g_tUart3.pRxBuf = g_RxBuf3;                             /* 接收缓冲区指针 */
    g_tUart3.usTxBufSize = UART3_TX_BUF_SIZE;               /* 发送缓冲区大小 */
    g_tUart3.usRxBufSize = UART3_RX_BUF_SIZE;               /* 接收缓冲区大小 */
    g_tUart3.usTxWrite = 0;                                 /* 发送FIFO写索引 */
    g_tUart3.usTxRead = 0;                                  /* 发送FIFO读索引 */
    g_tUart3.usRxWrite = 0;                                 /* 接收FIFO写索引 */
    g_tUart3.usRxRead = 0;                                  /* 接收FIFO读索引 */
    g_tUart3.usRxCount = 0;                                 /* 接收到的新数据个数 */
    g_tUart3.usTxCount = 0;                                 /* 待发送的数据个数 */
    g_tUart3.SendBefor = RS485_SendBefor;                   /* 发送数据前的回调函数 */
    g_tUart3.SendOver = RS485_SendOver;                     /* 发送完毕后的回调函数 */
    g_tUart3.ReciveNew = RS485_ReciveNew;                   /* 接收到新数据后的回调函数 */
    g_tUart3.Sending = 0;                                   /* 正在发送中标志 */
#endif
}


/*
*********************************************************************************************************
*    函 数 名: InitHardUart
*    功能说明: 配置串口的硬件参数（波特率，数据位，停止位，起始位，校验位，中断使能）适合于STM32-F4开发板
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void InitHardUart()
{
#if UART1_FIFO_EN == 1 /* 串口1 */
	
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); 
    
	/*USART1_TX*/  
	GPIO_InitStructure.GPIO_Pin = USART1_TX_PIN;
	GPIO_InitStructure.GPIO_Speed = UART_GPIO_SPEED;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; 
	GPIO_Init(USART1_TX_GPIO_PORT, &GPIO_InitStructure); 

	/*USART1_RX*/ 
	GPIO_InitStructure.GPIO_Pin = USART1_RX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; 
	GPIO_Init(USART1_RX_GPIO_PORT, &GPIO_InitStructure); 

	/*USART1 Config*/
	USART_InitStructure.USART_BaudRate = UART1_BAUD; 
	USART_InitStructure.USART_WordLength = USART_WordLength_8b; 
	USART_InitStructure.USART_StopBits = USART_StopBits_1; 
	USART_InitStructure.USART_Parity = USART_Parity_No; 
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; 
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; 
	USART_Init(USART1, &USART_InitStructure); 

	/*USART1 NVIC Config*/
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=2 ;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;		
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			
    NVIC_Init(&NVIC_InitStructure);	

    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);
    USART_DMACmd(USART1,USART_DMAReq_Rx,ENABLE); /*使能USART1的DMA接收请求,即允许DMA自动把USART1收到的数据搬到内存*/
    USART_DMACmd(USART1,USART_DMAReq_Tx,ENABLE);
    USART_Cmd(USART1,ENABLE);
	
	/*----------DMA相关----------*/
	DMA_InitTypeDef DMA_TxInitStructure;
	DMA_InitTypeDef DMA_RxInitStructure;
	
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1,ENABLE);
	
	DMA_DeInit(DMA1_Channel4);
    DMA_DeInit(DMA1_Channel5); 

	/* DMA1 USART1 Tx */
	DMA_TxInitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&USART1->DR);
	DMA_TxInitStructure.DMA_MemoryBaseAddr = (uint32_t)g_TxBuf1;
	DMA_TxInitStructure.DMA_DIR = DMA_DIR_PeripheralDST; /*Peripheral as Destination*/
	DMA_TxInitStructure.DMA_BufferSize = 0;
	DMA_TxInitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_TxInitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_TxInitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_TxInitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	DMA_TxInitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_TxInitStructure.DMA_Priority = DMA_Priority_VeryHigh;
	DMA_TxInitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel4, &DMA_TxInitStructure);

	/* DMA1 USART1 Rx */
    DMA_RxInitStructure.DMA_PeripheralBaseAddr =  (uint32_t)(&USART1->DR);
    DMA_RxInitStructure.DMA_MemoryBaseAddr     = (uint32_t)g_RxBuf1;
    DMA_RxInitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_RxInitStructure.DMA_BufferSize = UART1_RX_BUF_SIZE;
    DMA_RxInitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_RxInitStructure.DMA_MemoryInc =DMA_MemoryInc_Enable;
    DMA_RxInitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_RxInitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_RxInitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_RxInitStructure.DMA_Priority = DMA_Priority_High;
    DMA_RxInitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel5,&DMA_RxInitStructure);
	
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel4_IRQn; 
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2; 
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2; 
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; 
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel5_IRQn ;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2; 
    NVIC_InitStructure.NVIC_IRQChannelSubPriority  = 3; 
    NVIC_InitStructure.NVIC_IRQChannelCmd  = ENABLE; 
    NVIC_Init(&NVIC_InitStructure);

	DMA_Cmd(DMA1_Channel4,DISABLE);
    DMA_Cmd(DMA1_Channel5,ENABLE);
    //DMA_ITConfig(DMA1_Channel4,DMA_IT_TC,ENABLE);
    DMA_ITConfig(DMA1_Channel5,DMA_IT_TC,ENABLE); /*开启传输完成中断*/

#endif
}

/*
*********************************************************************************************************
*    函 数 名: ComToUart
*    功能说明: 将COM端口号转换为UART指针
*    形    参: _ucPort: 端口号(COM1 - COM6)
*    返 回 值: uart指针
*********************************************************************************************************
*/
UART_T *ComToUart(COM_PORT_E _ucPort)
{
    if (_ucPort == COM1)
    {
#if UART1_FIFO_EN == 1
        return &g_tUart1;
#else
        return 0;
#endif
    }
    else if (_ucPort == COM2)
    {
#if UART2_FIFO_EN == 1
        return &g_tUart2;
#else
        return 0;
#endif
    }
    else if (_ucPort == COM3)
    {
#if UART3_FIFO_EN == 1
        return &g_tUart3;
#else
        return 0;
#endif
	}
}



/*
*********************************************************************************************************
*    函 数 名: UartIRQ
*    功能说明: 供中断服务程序调用，通用串口中断处理函数
*    形    参: _pUart : 串口设备
*    返 回 值: 无
*********************************************************************************************************
*/
static void UartIRQ(UART_T *_pUart)
{
    uint32_t sr = READ_REG(_pUart->uart->SR);
    uint32_t cr1 = READ_REG(_pUart->uart->CR1);
    // uint32_t cr3 = _pUart->uart->CR3; // F1标准库一般不用

    /* 处理接收中断 */
    if ((sr & USART_SR_RXNE) && (sr & USART_SR_PE) == 0)
    {
        uint8_t ch = (uint8_t)_pUart->uart->DR; // 读取数据

        _pUart->pRxBuf[_pUart->usRxWrite] = ch;
        if (++_pUart->usRxWrite >= _pUart->usRxBufSize)
        {
            _pUart->usRxWrite = 0;
        }
        if (_pUart->usRxCount < _pUart->usRxBufSize)
        {
            _pUart->usRxCount++;
        }

        if (_pUart->ReciveNew)
        {
            _pUart->ReciveNew(ch);
        }
    }

    /* 处理发送缓冲区空中断 */
    if ((sr & USART_SR_TXE) && (cr1 & USART_CR1_TXEIE))
    {
        if (_pUart->usTxCount == 0)
        {
            // 禁止发送缓冲区空中断
            _pUart->uart->CR1 &= ~USART_CR1_TXEIE;
            // 使能发送完成中断
            _pUart->uart->CR1 |= USART_CR1_TCIE;
        }
        else
        {
            _pUart->Sending = 1;
            _pUart->uart->DR = _pUart->pTxBuf[_pUart->usTxRead];
            if (++_pUart->usTxRead >= _pUart->usTxBufSize)
            {
                _pUart->usTxRead = 0;
            }
            _pUart->usTxCount--;
        }
    }

    // F1标准库下，错误标志通过读SR/DR自动清除，无需手动清ICR
}

void Uart1_SendDMA(uint8_t *buf, uint16_t len)
{

    DMA_Cmd(DMA1_Channel4, DISABLE);

    /*设置内存地址和数据长度*/ 
    DMA1_Channel4->CMAR = (uint32_t)buf;
    DMA_SetCurrDataCounter(DMA1_Channel4, len);

    /*使能DMA发送*/ 
    DMA_Cmd(DMA1_Channel4, ENABLE);
}

void debug_printf(char* fmt, ...) 
{
	UART_T *pUart = ComToUart(DEBUG_UART);
	va_list ap;
	va_start(ap, fmt);
	char tempBuf[128];
	int len = vsnprintf(tempBuf, sizeof(tempBuf), fmt, ap);
	va_end(ap);

	uint16_t firstPart = pUart->usTxBufSize - pUart->usTxWrite;
	if (len <= firstPart) 
	{
		memcpy(&pUart->pTxBuf[pUart->usTxWrite], tempBuf, len);
	} 
	else 
	{
		memcpy(&pUart->pTxBuf[pUart->usTxWrite], tempBuf, firstPart);
		memcpy(&pUart->pTxBuf[0], tempBuf + firstPart, len - firstPart);
	}
	
	// 启动DMA发送（假设DMA空闲或由DMA中断续发）
	Uart1_SendDMA(&pUart->pTxBuf[pUart->usTxRead], (len <= firstPart) ? len : firstPart);
	
	pUart->usTxWrite = (pUart->usTxWrite + len) % pUart->usTxBufSize;
	pUart->usTxRead = (pUart->usTxRead + len) % pUart->usTxBufSize;
}

/*
*********************************************************************************************************
*   函 数 名: comSetCallBackReciveNew
*   功能说明: 配置接收回调函数
*   形    参:  _pUart : 串口设备
*              _ReciveNew : 函数指针，0表示取消
*   返 回 值: 无
*********************************************************************************************************
*/
void comSetCallBackReciveNew(COM_PORT_E _ucPort, void (*_ReciveNew)(uint8_t byte))
{
    UART_T *pUart;
    pUart = ComToUart(_ucPort);
    if (pUart == 0)
    {
        return;
    }
    
    pUart->ReciveNew = _ReciveNew;
}
/*
*********************************************************************************************************
*    函 数 名: bsp_InitUart
*    功能说明: 初始化串口硬件，并对全局变量赋初值.
*    形    参:  无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUart(void)
{
    UartVarInit(); /* 必须先初始化全局变量,再配置硬件 */

    InitHardUart(); /* 配置串口的硬件参数(波特率等) */

}

/*
*********************************************************************************************************
*    函 数 名: USART1_IRQHandler  USART2_IRQHandler USART3_IRQHandler 
*    功能说明: USART中断服务程序
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
#if UART1_FIFO_EN == 1
void USART1_IRQHandler(void)
{
    UartIRQ(&g_tUart1);
}
#endif



