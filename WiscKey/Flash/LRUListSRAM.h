//* _SRAM_h : All the simulation for Disposable FTL are defined in this file
//* dynamic wear leveling: using cost-benefit function for its GC

#ifndef _LRULISTSRAM_H
#define _LRULISTSRAM_H
#endif

#include "typedefine.h"

//***************** Defines ****************

#define LRU_ELEMENT_POINT_TO_NULL	(-2);	//# the NULL value for "next" and "previous"
#define LRU_ELEMENT_FREE			(-3);	//# the LRU element is free (i.e., in the free list)

//***************** Structure definitions ****************

// define the structure for one element of the LRU list
typedef struct
{
	int next;		// point to the next
	int previous;	// point to the previous
	int key;		// key: the id of virtual region
	int size;		// size of the pointed virtual region
	Boolean dirty;	// if this flash is set, it means the cached data are dirty and needs to be flushed back before being replaced.
} LRUSRAMElement_t;

// the data structure for the LBA list
// Len + FreeLen = the number of elements in the list
// currentSize + freeSize = the total size of this list in SRAM
typedef struct	
{
	LRUSRAMElement_t *Element;	//# the elements of the LRU list
	int Head;			//# head of the list
	int Tail;			//# tail of the list
	int Len;			//# number of elements in the list
	int FreeHead;		//# head of the free elements
	int FreeTail;		//# tail of the free elements
	int FreeLen;		//# the number of free elements
	int FreeSize;
} LRUSRAMList_t;

extern int SramSizeLimit;		   // Unit: B, The length of this LRU list is limited by the SRAM size
extern flash_size_t basedUnitSize; // Unit: B, A virtual Region is composed by (a) a block linked list (b) a usedPageBuffer (c) a MrM Tree

//***************** Functions *********************

Boolean CreateLRUListS(LRUSRAMList_t *list, int len);

Boolean FreeLRUListS(LRUSRAMList_t *list);

Boolean IsLRUListFullS(LRUSRAMList_t *list, int size);

Boolean IsElemenetInLRUListS(LRUSRAMList_t *list, int key, int *pos, int *num);

Boolean IsElementDirtyS(LRUSRAMList_t *list, int *pos);

Boolean SetElementDirtyS(LRUSRAMList_t *list, int key, int *pos);

Boolean ClearElementDirtyS(LRUSRAMList_t *list, int key, int *pos);

Boolean PutElementToFreeElementListS(LRUSRAMList_t *list, int *pos);

Boolean GetFreeElementFromFreeList(LRUSRAMList_t *list, int *pos, int size);

Boolean AddElemenetToTheHeadOfLRUListS(LRUSRAMList_t *list, int key, int *pos, int size, flash_huge_size_t *totalRequestTime, Boolean getFreeBlockMethod);

Boolean RemoveElemenetsFromLRUListS(LRUSRAMList_t *list, flash_huge_size_t *totalRequestTime);

Boolean MoveElemenetToTheHeadofLRUListS(LRUSRAMList_t *list, int key, int *pos);

Boolean RemoveTheTailElemenetFromLRUListS(LRUSRAMList_t *list, int *key, int *pos, flash_huge_size_t *totalRequestTime, Boolean getFreeBlockMethod);

Boolean PutElementToTheHeadOfLRUListS(LRUSRAMList_t *list, int key, int *pos, int size, flash_huge_size_t *totalRequestTime);

void UpdateLruItemSize(LRUSRAMList_t *list, int pos, int newSize, flash_huge_size_t *totalRequestTime, Boolean getFreeBlockMethod);
