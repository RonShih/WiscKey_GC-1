#ifndef _LRULIST_C
#define _LRULIST_C
#endif

#include "main.h"
#include "BEYONDADDR.h"

Boolean CreateLRUListS(LRUSRAMList_t *list, int len) {
	int i;
	int tmp = len;
	// Len is large, reassign
	if (len == 0 || len * basedUnitSize > SramSizeLimit) {
		len = SramSizeLimit / basedUnitSize;
	}

	printf("LRUSRAMList, SramSizeLimit = %d B, basedUnitSize = %d B, length %d (original %d)\n", SramSizeLimit, basedUnitSize, len, tmp);

	// Allocate elements
	list->Element = (LRUSRAMElement_t *) malloc(sizeof(LRUSRAMElement_t) * (len));

	// Initialize the elements -> every element is a free element, and the list starts from the first element, then the second element and so on.
	for (i = 0; i < len; i++) {
		list->Element[i].key = LRU_ELEMENT_FREE;
		list->Element[i].next = i + 1;
		list->Element[i].previous = i - 1;
		list->Element[i].size = 0;
		list->Element[i].dirty = False;	// Set the dirty flag as False
	}
	list->Element[0].previous = LRU_ELEMENT_POINT_TO_NULL;
	list->Element[len - 1].next = LRU_ELEMENT_POINT_TO_NULL;

	//# Initialize the parameters
	list->Head = 0;
	list->Tail = 0;
	list->Len = 0;
	list->FreeHead = 0;
	list->FreeTail = len - 1;
	list->FreeLen = len;
	list->FreeSize = SramSizeLimit;

	return (True);
}

Boolean FreeLRUListS(LRUSRAMList_t *list) {
	free(list->Element);
	return (True);
}

Boolean IsLRUListFullS(LRUSRAMList_t *list, int size) {
	if (list->FreeLen == 0 || list->FreeSize < size)
		return (True);
	else
		return (False);
}

void SelfCheckSize(LRUSRAMList_t *list) {
	int size = 0;
	int null = LRU_ELEMENT_POINT_TO_NULL;
	int pos = list->Head;

	while (pos != null) {
		size += list->Element[pos].size;
		pos = list->Element[pos].next;
	};

	if(size + list->FreeSize != SramSizeLimit) {
		printf("SelfCheckSize failed in LRU list Sram \n");
		exit(-1);
	}
}

void SelfCheck(LRUSRAMList_t *list) {
	int length = 0;
	int null = LRU_ELEMENT_POINT_TO_NULL;
	int pos = list->Head;

	while (pos != null) {
		pos = list->Element[pos].next;
		length += 1;
	};

	if(length != list->Len) {
		printf("SelfCheck failed in LRU list Sram \n");
		exit(-1);
	}
}

//# ****************************************************************
//# Called to check whether a item is in the LRU list or not, return the pos if found
//# ****************************************************************
Boolean IsElemenetInLRUListS(LRUSRAMList_t *list, int key, int *pos, int *num) {
	int i;
	int ptr;

	int null = LRU_ELEMENT_POINT_TO_NULL;
	// Initialize
	*num = 0;	// Initialize the counter, i.e., the number of checked elements
	*pos = null;	// Initialize the "pos" as "the element not found"

	//# search the elements for the "key"
	ptr = list->Head;
	for (i = 0; i < list->Len; i++) {
		(*num)++;	//# advance the number of checked elements by 1
		if (list->Element[ptr].key == key && (ptr != null)) {
			//# found: "pos" keeps which element contains the "key"
			*pos = ptr;
			break;
		} else {
			ptr = list->Element[ptr].next; //# point to the next element	
		}
	}

	if (i == list->Len)
		return (False); //# not found
	else
		return (True); //# found
}

Boolean IsElementDirtyS(LRUSRAMList_t *list, int *pos) {
	return (list->Element[*pos].dirty); //# return the status of the dirty flag
}

Boolean SetElementDirtyS(LRUSRAMList_t *list, int key, int *pos) {
	if (list->Element[*pos].key != key)
		return (False);	//# can't find the element whose key is "key"

	list->Element[*pos].dirty = True; //# set the dirty flag as True

	return (True);
}

Boolean ClearElementDirtyS(LRUSRAMList_t *list, int key, int *pos) {
	if (list->Element[*pos].key != key)
		return (False);	//# can't find the element whose key is "key"

	list->Element[*pos].dirty = False;	//# set the dirty flag as False

	return (True);
}

void UpdateLruItemSize(LRUSRAMList_t *list, int pos, int newSize, flash_huge_size_t *totalRequestTime, Boolean getFreeBlockMethod) {
	if (newSize < list->Element[pos].size) {
		// Cached size Decreased
		list->FreeSize += (list->Element[pos].size - newSize);
		list->Element[pos].size = newSize;
	} else if (newSize > list->Element[pos].size) {
		// Cached size increased
		// Check free size first:)
		while((newSize - list->Element[pos].size) > list->FreeSize) {
			int RemovedKey = -1;
			int RemovedPos = -1;
			RemoveTheTailElemenetFromLRUListS(list, &RemovedKey, &RemovedPos, totalRequestTime, getFreeBlockMethod);
		}
		// Update size
		list->FreeSize -= (newSize - list->Element[pos].size);
		list->Element[pos].size = newSize;
	} else if (newSize == list->Element[pos].size) {
		list->Element[pos].size = newSize;
	}
	//printf("INFO: SRAM Free Size %d", list->FreeSize);
	SelfCheckSize(list);
}

//# ****************************************************************
//# Called when re-access a item and move it to the head of the LRU list
//# ****************************************************************
Boolean MoveElemenetToTheHeadofLRUListS(LRUSRAMList_t *list, int key, int *pos) {
	//# can't find the element whose key is "key"
	if (list->Element[*pos].key != key)
		return (False);

	int null = LRU_ELEMENT_POINT_TO_NULL;

	//# If there is only one element in the list, don't do anything because it is already at the head of the list
	if ((*pos!=null)
			&& (list->Len > 1)
			&& (list->Element[list->Head].key != key)) {
		//# Remove the element "pos" from the LRU list
		if (*pos == list->Head) { //# the "key" is in the head of the LRU list
			list->Head = list->Element[*pos].next;
			list->Element[list->Head].previous = LRU_ELEMENT_POINT_TO_NULL;
		} else if (*pos == list->Tail) { //# the "key" is in the tail of the LRU list
			list->Tail = list->Element[*pos].previous;
			list->Element[list->Tail].next = LRU_ELEMENT_POINT_TO_NULL;
		} else { //# the "key" is in the middle of the LRU list
			//# remove it from the list
			list->Element[list->Element[*pos].previous].next =
					list->Element[*pos].next;
			list->Element[list->Element[*pos].next].previous =
					list->Element[*pos].previous;
		}

		//# Put the removed element to the lead of the LRU list
		list->Element[*pos].next = list->Head;
		list->Element[*pos].previous = LRU_ELEMENT_POINT_TO_NULL;
		list->Element[list->Head].previous = *pos;
		list->Head = *pos;
	}

	SelfCheck(list);
	return (True);
}

//# ****************************************************************
//# Put an element back to the free list
//# ****************************************************************
Boolean PutElementToFreeElementListS(LRUSRAMList_t *list, int *pos) {
	list->FreeLen++;
	list->FreeSize += list->Element[*pos].size;
	if (list->FreeLen == 1) {
		//# the first element in the free element list
		list->Element[*pos].next = LRU_ELEMENT_POINT_TO_NULL;
		list->Element[*pos].previous = LRU_ELEMENT_POINT_TO_NULL;
		list->FreeHead = *pos;
		list->FreeTail = *pos;
	} else {
		//# put it to the head of the free element list
		list->Element[*pos].previous = LRU_ELEMENT_POINT_TO_NULL;
		list->Element[*pos].next = list->FreeHead;
		list->Element[list->FreeHead].previous = *pos;
		list->FreeHead = *pos;
	}
	//# Reset
	list->Element[*pos].key = LRU_ELEMENT_FREE;
	list->Element[*pos].dirty = False;
	list->Element[*pos].size = 0;

	SelfCheckSize(list);
	return (True);
}

//# ****************************************************************
//# Remove element from the LRU list
//# ****************************************************************
Boolean RemoveTheTailElemenetFromLRUListS(LRUSRAMList_t *list, int *key,
		int *pos, flash_huge_size_t *totalRequestTime, Boolean getFreeBlockMethod) {
	//# no element in the list
	if (list->Len == 0)
		return (False);

	//# remove it from the LRU list
	*key = list->Element[list->Tail].key; //# get the key of the tail element
	*pos = list->Tail; //# get the last element

	if(DEBUG_HANK)
		printf("INFO : Remove region %d from LRU list. \n", list->Element[list->Tail].key);

	//# Flush the cached page cache to flash
	FlushVirtualRegion(*key, *pos, getFreeBlockMethod, totalRequestTime);

	// FlushVirtualRegion() May start GC and cause the SRAM list to be change
	// Therefore, the original tail may become head
	if((*pos == list->Tail && *pos != list->Head) || (*pos == list->Tail && list->Head == list->Tail && list->Len == 1)) {
		// OK, list is not change :)
		// Continue removing
#ifdef BEYONDADDR_META_COUNT
		//# Statistics
		if (list->Element[*pos].dirty == True) {
			totalRequestTime += (list->Element[*pos].size / PageSize) * TIME_WRITE_PAGE; //# increase the time on writes or reads
		}
#endif

		list->Len--;	//# decrease the number of elements in the LRU list by 1
		if (list->Len > 0) {
			list->Tail = list->Element[*pos].previous;
			list->Element[list->Tail].next = LRU_ELEMENT_POINT_TO_NULL;
		} else {
			list->Head = LRU_ELEMENT_POINT_TO_NULL;
			list->Tail = LRU_ELEMENT_POINT_TO_NULL;
		}

		//# put the removed element to the head of the free element list
		PutElementToFreeElementListS(list, pos);

		if(DEBUG_HANK)
				printf("INFO : SRAM Free Size %d. \n", list->FreeSize);
	} else {
		// The request element is already moved to the head
		// no further action is required
	}

	SelfCheck(list);
	SelfCheckSize(list);
	return (True);
}

//# ****************************************************************
//# End simulation with this function to flush SRAM cached virtual region
//# ****************************************************************
Boolean RemoveElemenetsFromLRUListS(LRUSRAMList_t *list, flash_huge_size_t *totalRequestTime) {
	if (list->Len > 0) {
		// There is some virtual region cached in LRU list, start flushing
		int ptr = list->Head;
		int key = list->Element[ptr].key;

		do {
			ptr = list->Head;
			key = list->Element[ptr].key; //# get the key of the tail element

			if(DEBUG_HANK)
				printf("==== FLUSH SRAM : Remove region %d from LRU list. \n", key);

			//# Flush the cached page cache to flash
			FlushVirtualRegion(key, ptr, False, totalRequestTime);

#ifdef BEYONDADDR_META_COUNT
			//# Statistics
			if (list->Element[ptr].dirty == True) {
				*totalRequestTime += ceil((double)((double)list->Element[ptr].size / (double)PageSize)) * TIME_WRITE_PAGE; //# increase the time on writes or reads
			}
#endif
			list->Len--; //# decrease the number of elements in the LRU list by 1
			if (list->Len > 0) {
				list->Head = list->Element[ptr].next;
				list->Element[list->Head].previous = LRU_ELEMENT_POINT_TO_NULL;
			} else {
				list->Head = LRU_ELEMENT_POINT_TO_NULL;
				list->Tail = LRU_ELEMENT_POINT_TO_NULL;
			}

			//# put the removed element to the head of the free element list
			PutElementToFreeElementListS(list, &ptr);

			if(DEBUG_HANK)
				printf("INFO : SRAM Free Size %d. \n", list->FreeSize);

		} while (list->Len > 0);

		SelfCheck(list);
		SelfCheckSize(list);
		return (True);
	} else {
		// List is empty, do nothing
		return (False);
	}
}

//# ****************************************************************
//# Try to get a free item from the free list, return false when list full
//# ****************************************************************
Boolean GetFreeElementFromFreeList(LRUSRAMList_t *list, int *pos, int size) {
	// Do both length check and size check
	if (list->FreeLen == 0 || list->FreeSize < size)
		return (False);

	list->FreeLen--;
	list->FreeSize -= size;

	*pos = list->FreeTail;

	if (list->FreeLen == 0) { //# only one element in the free element list
		list->FreeHead = LRU_ELEMENT_POINT_TO_NULL;
		list->FreeTail = LRU_ELEMENT_POINT_TO_NULL;
	} else {
		list->FreeTail = list->Element[list->FreeTail].previous;
		list->Element[list->FreeTail].next = LRU_ELEMENT_POINT_TO_NULL;
	}

	return (True);
}

//# ****************************************************************
//# Try to put a new region item to the head of LRU list, return false when list full
//# ****************************************************************
Boolean PutElementToTheHeadOfLRUListS(LRUSRAMList_t *list, int key, int *pos,
		int size, flash_huge_size_t *totalRequestTime) {
	if (GetFreeElementFromFreeList(list, pos, size) == False)
		return (False); //# the LRU list is full

	list->Len++;
	//# put the "key" to the new allocated element to the head of the LRU list
	list->Element[*pos].key = key;
	list->Element[*pos].size = size;

	if (list->Len == 1) {
		list->Element[*pos].next = LRU_ELEMENT_POINT_TO_NULL;
		list->Element[*pos].previous = LRU_ELEMENT_POINT_TO_NULL;
		list->Head = *pos;
		list->Tail = *pos;
	} else {
		list->Element[*pos].next = list->Head;
		list->Element[*pos].previous = LRU_ELEMENT_POINT_TO_NULL;
		list->Element[list->Head].previous = *pos;
		list->Head = *pos;
	}
#ifdef BEYONDADDR_META_COUNT
	//# Statistics
	*totalRequestTime+= ceil((double)((double)list->Element[*pos].size / (double)PageSize)) * TIME_READ_PAGE; //# increase the time on writes or reads
#endif

	SelfCheck(list);
	SelfCheckSize(list);
	return (True);
}

//# ****************************************************************
//# Add a new region item to the head of LRU list
//# ****************************************************************
Boolean AddElemenetToTheHeadOfLRUListS(LRUSRAMList_t *list, int key, int *pos, int size, flash_huge_size_t *totalRequestTime, Boolean getFreeBlockMethod) {
	//# Put it at the head of the list
	while (PutElementToTheHeadOfLRUListS(list, key, pos, size, totalRequestTime) == False) {
		//# the key of the removed element
		int RemovedKey = -1;
		int RemovedPos = -1;
		RemoveTheTailElemenetFromLRUListS(list, &RemovedKey, &RemovedPos, totalRequestTime, getFreeBlockMethod);
		if(DEBUG_HANK)
			printf("item size %d, free size %d, len %d, free len %d \n", size, list->FreeSize, list->Len, list->FreeLen);
	}
	SelfCheck(list);
	return True;
}

