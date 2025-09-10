#ifndef __LIB_RNG_H_
#define __LIB_RNG_H_

#include <stdint.h>
#include <stdbool.h>


typedef struct
{
	char *pBuf;		/*指向ring缓冲区的指针*/
	int  capaticy;  /*ring的大小*/
	int  fromBuf;    /*指向可取数据*/
	int  toBuf;	 	/*指向可存数据*/

}T_Ring;

typedef int RING_ID;

RING_ID rngInit(T_Ring *ring ,char *buffer, int bufLen);
bool rngGet(RING_ID ring, char* rcv);
bool rngPut(RING_ID ring, char val);
void rngPutForce(RING_ID ring, char val);
int rngBufGet(RING_ID ring, char*buf, int len);
int rngBufPut(RING_ID ring, char*buf, int len);
bool rngCpy(RING_ID ring, int where, char* rcv);
int rngBufCpy(RING_ID ring , char* buf, int bufLen);
int rngLen(RING_ID ring);
bool rngIsFull(RING_ID ring);
bool rngIsEmpty(RING_ID ring);
void rngClear(RING_ID ring); 

#endif /*__LIB_RNG_H_*/
