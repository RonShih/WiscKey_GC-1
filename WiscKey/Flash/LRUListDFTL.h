//# ****************************************************************
//# LRULIST.h : All the operations to handle a LRU list are defined here 
//# ****************************************************************

#include "typedefine.h"

//* ************************* Defines ***************

//#define LRU_ELEMENT_POINT_TO_NULL	(-1);	//# the NULL value for "next" and "previous"
//#define LRU_ELEMENT_FREE			(-1);	//# the LRU element is free (i.e., in the free list)

//* **************** structure definitions ****************


//# define the structure for one element of the LRU list
typedef struct
{
	int next;	//# point to the next
	int previous;	//# point to the previus
	int key;	//# key: the logical block ID or physical block ID
	// int Dppn; // Skip the real implementation
	Boolean dirty;	//# if this flash is set, it means the cached data are dirty and needs to be flushed back before being replaced.
} LRUElement_D;

typedef struct	//# the data structure for the hot LBA list: Len+ FreeLen = the number of elements in the list
{
	LRUElement_D *Element;	//# the elements of the LRU list
	int Head;			//# head of the list
	int Tail;			//# tail of the list
	int Len;			//# number of elements in the list
	int FreeHead;		//# head of the free elements
	int FreeTail;		//# tail of the free elements
	int FreeLen;		//# the number of free elements 
} LRUList_D;



//* ******************************** functions *********************



//# ****************************************************************
//# Allocate the memory space for the elements of the LRU list and also initialize the list.
//# "list" points the LRU list
//# "len" is the number of elements to be created in the "list"
//# ****************************************************************
Boolean CreateLRUListD(LRUList_D *list, int len);
//# ****************************************************************
//# Free the memory space allocated for the LRU list.
//# "list" points the LRU list
//# ****************************************************************
Boolean FreeLRUListD(LRUList_D *list);


//# ****************************************************************
//# Check whether the LRU list is full
//# "list" points the LRU list
//# Reutnr value: return "True" if the list is not full; otherwise, return "False"
//# ****************************************************************
Boolean IsLRUListFullD(LRUList_D *list);


//# ****************************************************************
//# Check whether a "key" is already kept in the the LRU list
//# "list" points the LRU list
//# "key" is the key value that is going to be searched
//# "pos" points the element having the "key" if found; otherwise, "pos" points to -1
//# "num" will return the number of elements that are checked 
//#   -> If not found, the number of checked elements equals to the number of elements in the LRU list.
//# Reutnr value: return "True" if the key is found; otherwise, return "False"
//# ****************************************************************
Boolean IsElemenetInLRUListD(LRUList_D *list, int key, int *pos, int *num);


//# ****************************************************************
//# Check whether the element is dirty(/modified) or not
//# "list" points the LRU list
//# "pos" points to the element that is going to be removed
//# Reutnr value: If "key" is equal to the key of element "pos", return the status of its dirty flagS
//# ****************************************************************
Boolean IsElementDirtyD(LRUList_D *list, int *pos);
//# ****************************************************************
//# Set the element to be dirty
//# "list" points the LRU list
//# "key" is the key value that is going to be set
//# "pos" points to the element that is going to be removed
//# Reutrn value: If "key" is equal to the key of element "pos", set the flag as True and return True. Otherwise, return False
//# ****************************************************************
Boolean SetElementDirtyD(LRUList_D *list, int key, int *pos);
//# ****************************************************************
//# Clear the element to be non-dirty
//# "list" points the LRU list
//# "key" is the key value that is going to be set
//# "pos" points to the element that is going to be removed
//# Reutrn value: If "key" is equal to the key of element "pos", set the flag as Flase and return True. Otherwise, return False
//# ****************************************************************
Boolean ClearElementDirtyD(LRUList_D *list, int key, int *pos);


//# ****************************************************************
//# Put an element pointed by "pos" to the head of the free element list
//# "list" points the LRU list
//# "pos" points the (position of) element that is going to be inserted to the head of the free block list
//# Reutnr value: always True, because we can always insert a new element to the free element list
//# ****************************************************************
Boolean PutElementToFreeElementListD(LRUList_D *list, int *pos);


//# ****************************************************************
//# Get an element from the tail of the free element list. 
//# "list" points the LRU list
//# "pos" keeps the return element that is got from the tail of the free block list
//# Reutnr value: always True, because we can always insert a new element to the free element list
//# ****************************************************************
Boolean GetElementFromFreeElementListD(LRUList_D *list, int *pos);


//# ****************************************************************
//# Remove an element from the list whenever the key of the element equals to "key"
//# "list" points the LRU list
//# "key" is the key value that is going to be searched
//# "pos" points to the element that is going to be removed
//# Reutnr value: If "key" is equal to the key of element "pos", delete it and return True.
//#               otherwise, return False.
//# ****************************************************************
Boolean RemoveElemenetFromLRUListD(LRUList_D *list, int key, int *pos);
                 

//# ****************************************************************
//# Move an element to the head of the list whenever the key of the element equals to "key"
//# "list" points the LRU list
//# "key" is the key value that is going to be searched
//# "pos" points to the element that is going to be moved
//# Reutrn value: If "key" is equal to the key of element "pos", delete it and return True.
//#               otherwise, return False.
//# ****************************************************************
Boolean MoveElemenetToTheHeadofLRUListD(LRUList_D *list, int key, int *pos);


//# ****************************************************************
//# Remove the last element from the list whenever the key of the element located at "pos" equals to "key"
//# "list" points the LRU list
//# "key" is to return the key value of the (tail) element that is removed
//# "pos" points the position of element that is removed (i.e., pointing the the tail element)
//# Reutnr value: If the list is not empty, return True; otherwise, return False.
//# ****************************************************************
Boolean RemoveTheTailElemenetFromLRUListD(LRUList_D *list, int *key, int *pos, flash_huge_size_t *totalRequestTime);


//# ****************************************************************
//# Put a "key" (or element) into the head of the LRU list if the LRU list is not full
//# "list" points the LRU list
//# "key" is the key value that is going to be searched
//# "pos" points to the element that the "key" is stored
//# Reutnr value: return "True" if the LRU list is not full and the "key" is put to the head successfully; otherwise, return "False".
//# ****************************************************************
Boolean PutElementToTheHeadOfLRUListD(LRUList_D *list, int key, int *pos);


//# ****************************************************************
//# Put a "key" (or element) into the head of the LRU list no matter what kind of situation. 
//# If the "key" is already in the list, move it to the head of the list. If not, add it to the head of the list.
//# If the list is already full, remove the tail of the element from the list.
//# "list" points the LRU list
//# "key" is the key value that is going to be searched
//# "pos" points the element where the "key" is inserted.
//# Reutnr value: Always return True.
//# ****************************************************************
Boolean PutElemenetToLRUListD(LRUList_D *list, int key, int *pos, flash_huge_size_t *totalRequestTime);
