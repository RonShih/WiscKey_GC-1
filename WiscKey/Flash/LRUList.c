//# ****************************************************************
//# LRULIST.c : All the operations to handle a LRU list are defined here 
//# elements are put at the head of the list, and removed from the tail of the list
//# ****************************************************************

#ifndef _LRULIST_C
#define _LRULIST_C
#endif


#include "main.h"
#include "LRUList.h"

//# ************************* Defines *************************


//* ******************************** functions *********************

//# ****************************************************************
//# Allocate the memory space for the elements of the LRU list and also initialize the list.
//# "list" points the LRU list
//# "len" is the number of elements to be created in the "list"
//# ****************************************************************
Boolean CreateLRUList(LRUList_t *list, int len)
{
	int i;
	
	list->Element = (LRUElement_t *)malloc(sizeof(LRUElement_t)*(len));

	//# initizlie the elements -> every element is a free element, and the list starts from the first element, then the second element and so on.
	for(i=0;i<len; i++)
	{
		list->Element[i].key = LRU_ELEMENT_FREE;
		list->Element[i].next = i+1;
		list->Element[i].previous = i-1;
		list->Element[i].dirty = False;	//# set the dirty flag as False
	}
	list->Element[0].previous = LRU_ELEMENT_POINT_TO_NULL;
	list->Element[len-1].next = LRU_ELEMENT_POINT_TO_NULL;

	//# initialize the parameters
	list->Head = 0;
	list->Tail = 0;
	list->Len = 0;
	list->FreeHead = 0;
	list->FreeTail = len-1;
	list->FreeLen = len;
	
	return(True);
}


//# ****************************************************************
//# Free the memory space allocated for the LRU list.
//# "list" points the LRU list
//# ****************************************************************
Boolean FreeLRUList(LRUList_t *list)
{
	free(list->Element);

	return(True);
}


//# ****************************************************************
//# Check whether the LRU list is full
//# "list" points the LRU list
//# Reutnr value: return "True" if the list is full; otherwise, return "False"
//# ****************************************************************
Boolean IsLRUListFull(LRUList_t *list)
{
	if(list->FreeLen ==0) return(True);
	else return(False);
}


//# ****************************************************************
//# Check whether a "key" is already kept in the the LRU list
//# "list" points the LRU list
//# "key" is the key value that is going to be searched
//# "pos" points the element having the "key" if found; otherwise, "pos" points to -1
//# "num" will return the number of elements that are checked 
//#   -> If not found, the number of checked elements equals to the number of elements in the LRU list.
//# Reutnr value: return "True" if the key is found; otherwise, return "False"
//# ****************************************************************
Boolean IsElemenetInLRUList(LRUList_t *list, int key, int *pos, int *num)
{
	int i;
	int ptr;

	//# initialize
	*num = 0;	//# initialize  the counter, i.e., the number of checked elements
	*pos = -1;				//# initialide the "pos" as "the element not found"

	//# search the elements for the "key"
	ptr = list->Head;
	for(i=0;i<list->Len;i++)
	{
		(*num)++;	//# advance the number of checked elements by 1

		if(list->Element[ptr].key == key)
		{
			//# found: "pos" keeps which element contains the "key"
			*pos = ptr;	
			break; 
		}
		else
		{
			ptr =list->Element[ptr].next; //# point to the next element	
		}
	}

	if(i==list->Len) return(False); //# not found
	else return(True);	//# found
}


//# ****************************************************************
//# Check whether the element is dirty(/modified) or not
//# "list" points the LRU list
//# "pos" points to the element that is going to be removed
//# Reutnr value: If "key" is equal to the key of element "pos", return the status of its dirty flagS
//# ****************************************************************
Boolean IsElementDirty(LRUList_t *list, int *pos)
{
	return(list->Element[*pos].dirty);	//# return the status of the dirty flag
}


//# ****************************************************************
//# Set the element to be dirty
//# "list" points the LRU list
//# "key" is the key value that is going to be set
//# "pos" points to the element that is going to be removed
//# Reutrn value: If "key" is equal to the key of element "pos", set the flag as True and return True. Otherwise, return False
//# ****************************************************************
Boolean SetElementDirty(LRUList_t *list, int key, int *pos)
{
	if(list->Element[*pos].key != key) return(False);	//# can't find the element whose key is "key"
		
	list->Element[*pos].dirty = True;	//# set the dirty flag as True
	
	return(True);
}


//# ****************************************************************
//# Clear the element to be non-dirty
//# "list" points the LRU list
//# "key" is the key value that is going to be set
//# "pos" points to the element that is going to be removed
//# Reutrn value: If "key" is equal to the key of element "pos", set the flag as Flase and return True. Otherwise, return False
//# ****************************************************************
Boolean ClearElementDirty(LRUList_t *list, int key, int *pos)
{
	if(list->Element[*pos].key != key) return(False);	//# can't find the element whose key is "key"
		
	list->Element[*pos].dirty = False;	//# set the dirty flag as False
	
	return(True);
}


//# ****************************************************************
//# Put an element pointed by "pos" to the head of the free element list
//# "list" points the LRU list
//# "pos" points the (position of) element that is going to be inserted to the head of the free block list
//# Reutnr value: always True, because we can always insert a new element to the free element list
//# ****************************************************************
Boolean PutElementToFreeElementList(LRUList_t *list, int *pos)
{
	//# Debug
/*	{
		int i;
		int head = VPTLRUList.FreeHead;
		if(*pos<0) {printf("Put error"); exit(-1);}
		if(list==&VPTLRUList)
		{
			printf("(pos=%2d)P %4d: ",*pos, VPTLRUList.FreeLen);
			for(i=0;i<VPTLRUList.FreeLen;i++) 
			{
				printf("%4d", head);
				head = VPTLRUList.Element[head].next;
			}
			printf("(Tail:%d, TailPrevious=%d,TailNext=%d)\n", VPTLRUList.FreeTail, VPTLRUList.Element[VPTLRUList.FreeTail].previous,VPTLRUList.Element[VPTLRUList.FreeTail].next);
		}
	}	
*/
	
	
	list->FreeLen++;
	if(list->FreeLen == 1) //# the first element in the free element list
	{
		list->Element[*pos].next = LRU_ELEMENT_POINT_TO_NULL;
		list->Element[*pos].previous = LRU_ELEMENT_POINT_TO_NULL;
		list->FreeHead = *pos;
		list->FreeTail = *pos;
	}
	else
	{
		list->Element[*pos].previous = LRU_ELEMENT_POINT_TO_NULL;
		list->Element[*pos].next = list->FreeHead;
		list->Element[list->FreeHead].previous = *pos;
		list->FreeHead = *pos; //# put it to the head of the free element list
	}
	list->Element[*pos].key = LRU_ELEMENT_FREE;

	//# Debug
/*	{
		int i;
		int head = VPTLRUList.FreeHead;
		if(*pos<0) {printf("error"); exit(-1);}
		if(list==&VPTLRUList)
		{
			printf("(pos=%2d)P %4d: ",*pos, VPTLRUList.FreeLen);
			for(i=0;i<VPTLRUList.FreeLen;i++) 
			{
				printf("%4d", head);
				head = VPTLRUList.Element[head].next;
			}
			printf("(Tail:%d, TailPrevious=%d,TailNext=%d)\n", VPTLRUList.FreeTail, VPTLRUList.Element[VPTLRUList.FreeTail].previous,VPTLRUList.Element[VPTLRUList.FreeTail].next);
		}
	}
*/

	return(True);
}

//# ****************************************************************
//# Get an element from the tail of the free element list. 
//# "list" points the LRU list
//# "pos" keeps the return element that is got from the tail of the free block list
//# Reutnr value: always True, because we can always insert a new element to the free element list
//# ****************************************************************
Boolean GetElementFromFreeElementList(LRUList_t *list, int *pos)
{
	if(list->FreeLen == 0) return(False);

	list->FreeLen--;
	*pos = list->FreeTail;
	if(list->FreeLen == 0) //# only one element in the free element list
	{
		list->FreeHead = LRU_ELEMENT_POINT_TO_NULL;
		list->FreeTail = LRU_ELEMENT_POINT_TO_NULL;
	}
	else
	{
		list->FreeTail = list->Element[list->FreeTail].previous;
		list->Element[list->FreeTail].next = LRU_ELEMENT_POINT_TO_NULL;
	}

	//# Debug
/*	{
		int i;
		int head = VPTLRUList.FreeHead;
		if(*pos<0) {printf("error"); exit(-1);}
		if(list==&VPTLRUList)
		{
			printf("(pos=%2d)G %4d: ",*pos, VPTLRUList.FreeLen);
			for(i=0;i<VPTLRUList.FreeLen;i++) 
			{
				printf("%4d", head);
				head = VPTLRUList.Element[head].next;
			}
			printf("(Tail:%d, TailPrevious=%d,TailNext=%d)\n", VPTLRUList.FreeTail, VPTLRUList.Element[VPTLRUList.FreeTail].previous,VPTLRUList.Element[VPTLRUList.FreeTail].next);
		}
	}
*/

	return(True);
}


//# ****************************************************************
//# Remove an element from the list whenever the key of the element equals to "key"
//# "list" points the LRU list
//# "key" is the key value that is going to be searched
//# "pos" points to the element that is going to be removed
//# Reutnr value: If "key" is equal to the key of element "pos", delete it and return True.
//#               otherwise, return False.
//# ****************************************************************
Boolean RemoveElemenetFromLRUList(LRUList_t *list, int key, int *pos)
{

	if(list->Element[*pos].key != key) return(False);	//# can't find the elemen whose key is "key"


	//# remove the element "pos" from the LRU list
	if(list->Len > 1)
	{
		if(*pos == list->Head) //# the "key" is in the head of the LRU list
		{
			list->Head = list->Element[*pos].next;
			list->Element[list->Head].previous = LRU_ELEMENT_POINT_TO_NULL; 
		}
		else if(*pos == list->Tail) //# the "key" is in the tail of the LRU list
		{
			list->Tail = list->Element[*pos].previous;
			list->Element[list->Tail].next = LRU_ELEMENT_POINT_TO_NULL; 
		}
		else	//# the "key" is in the middle of the LRU list
		{
			//# remove it from the list
			list->Element[list->Element[*pos].previous].next = list->Element[*pos].next; 
			list->Element[list->Element[*pos].next].previous = list->Element[*pos].previous;			
		}
	}
	
	list->Len--; //# decrease the length of the LRU list by 1
	//# update the list of free elements
	PutElementToFreeElementList(list, pos);

	return(True);
}

//# ****************************************************************
//# Move an element to the head of the list whenever the key of the element equals to "key"
//# "list" points the LRU list
//# "key" is the key value that is going to be searched
//# "pos" points to the element that is going to be moved
//# Reutrn value: If "key" is equal to the key of element "pos", delete it and return True.
//#               otherwise, return False.
//# ****************************************************************
Boolean MoveElemenetToTheHeadofLRUList(LRUList_t *list, int key, int *pos)
{
	if(list->Element[*pos].key != key) return(False);	//# can't find the elemen whose key is "key"
		
	//# If there is only one element in the list, don't do anything because it is already at the head of the list
	if(list->Len > 1) 
	{
		//# Remove the element "pos" from the LRU list
		if(*pos == list->Head) //# the "key" is in the head of the LRU list
		{
			list->Head = list->Element[*pos].next;
			list->Element[list->Head].previous = LRU_ELEMENT_POINT_TO_NULL; 
		}
		else if(*pos == list->Tail) //# the "key" is in the tail of the LRU list
		{
			list->Tail = list->Element[*pos].previous;
			list->Element[list->Tail].next = LRU_ELEMENT_POINT_TO_NULL; 
		}
		else	//# the "key" is in the middle of the LRU list
		{
			//# remove it from the list
			list->Element[list->Element[*pos].previous].next = list->Element[*pos].next; 
			list->Element[list->Element[*pos].next].previous = list->Element[*pos].previous;			
		}
		
		//# Put the removed element to the lead of the LRU list
		list->Element[*pos].next = list->Head;
		list->Element[*pos].previous = LRU_ELEMENT_POINT_TO_NULL;
		list->Element[list->Head].previous = *pos;
		list->Head = *pos;	
	}			
				
	return(True);
}

//# ****************************************************************
//# Remove the last element from the list whenever the key of the element located at "pos" equals to "key"
//# "list" points the LRU list
//# "key" is to return the key value of the (tail) element that is removed
//# "pos" points the position of element that is removed (i.e., pointing the the tail element)
//# Reutnr value: If the list is not empty, return True; otherwise, return False.
//# ****************************************************************
Boolean RemoveTheTailElemenetFromLRUList(LRUList_t *list, int *key, int *pos)
{
	if(list->Len == 0) return(False);	//# no element in the list

	//# remove it from the LRU list
	list->Len--;	//# decrease the number of elements in the LRU list by 1
	*key = list->Element[list->Tail].key; //# get the key of the tail element
	*pos = list->Tail; //# get the last element
	if(list->Len > 0)
	{
		list->Tail = list->Element[list->Tail].previous;
		list->Element[list->Tail].next = LRU_ELEMENT_POINT_TO_NULL;	
	}

	//# put the removed element to the head of the free element list
	PutElementToFreeElementList(list, pos);


	return(True);
}


//# ****************************************************************
//# Put a "key" (or element) into the head of the LRU list if the LRU list is not full
//# "list" points the LRU list
//# "key" is the key value that is going to be searched
//# "pos" points to the element that the "key" is stored
//# Reutnr value: return "True" if the LRU list is not full and the "key" is put to the head successfully; otherwise, return "False".
//# ****************************************************************
Boolean PutElementToTheHeadOfLRUList(LRUList_t *list, int key, int *pos)
{
	if(GetElementFromFreeElementList(list,pos) == False) return(False); //# the LRU list is full
	
	//# put the "key" to the new allocated elemetn to the head of the LRU list
	list->Element[*pos].key = key;
	list->Len++;
	if(list->Len == 1)
	{
		list->Element[*pos].next = LRU_ELEMENT_POINT_TO_NULL;
		list->Element[*pos].previous = LRU_ELEMENT_POINT_TO_NULL;
		list->Head = *pos;
		list->Tail = *pos;		
	}	
	else
	{	
		list->Element[*pos].next = list->Head;
		list->Element[*pos].previous = LRU_ELEMENT_POINT_TO_NULL;
		list->Element[list->Head].previous = *pos;
		list->Head = *pos;	
	}
		
	return(True);
}


//# ****************************************************************
//# Put a "key" (or element) into the head of the LRU list no matter what kind of situation. 
//# If the "key" is already in the list, move it to the head of the list. If not, add it to the head of the list.
//# If the list is already full, remove the tail of the element from the list.
//# "list" points the LRU list
//# "key" is the key value that is going to be searched
//# "pos" points the element where the "key" is inserted.
//# Reutnr value: Always return True.
//# ****************************************************************
Boolean PutElemenetToLRUList(LRUList_t *list, int key, int *pos)
{
	int RemovedKey;			//# the key of the removed element

	//# put it at the head of the list
	while(PutElementToTheHeadOfLRUList(list,key, pos)==False)
	{
		RemoveTheTailElemenetFromLRUList(list, &RemovedKey, pos);
	}

	return True;
}
