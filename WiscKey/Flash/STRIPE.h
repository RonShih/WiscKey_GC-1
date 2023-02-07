//# ****************************************************************
//# STRIPE_C.h : All the simulation for Li-Ping Chang's striping algorithm are defined in this file
//# dynamic wear leveling: using cost-benefit strategy for its GC
//# ****************************************************************

#ifndef _STRIPE_H
#define _STRIPE_H

#include "typedefine.h"

//* ************************* Defines ***************

//#define HOTLIST_LENGTH			(512)
//#define CANDIDATELIST_LENGTH	(1024)
//#define HOTLIST_LENGTH			(256)
//#define CANDIDATELIST_LENGTH	(512)
//#define HOTLIST_LENGTH			(128)
//#define CANDIDATELIST_LENGTH	(256)
//#define HOTLIST_LENGTH			(64)
//#define CANDIDATELIST_LENGTH	(128)
#define HOTLIST_LENGTH			(8)
#define CANDIDATELIST_LENGTH	(16)

//* Used in FTLTable
#define STRIPE_POINT_TO_NULL	(-1)		//* point to NULL
//# used for "CachedClusters[]"
#define NO_CLUSTER_CACHED (-1)	

#define TIME_ACCESS_ONE_LRU_ELEMENT	(TIME_ACCESS_SRAM) //# read "key" and "next" needs 4 and 4 times of memory accesses, respectively
//#define TIME_ACCESS_ONE_LRU_ELEMENT	(TIME_ACCESS_SRAM*4*2) //# read "key" and "next" needs 4 and 4 times of memory accesses, respectively
//#define TIME_ACCESS_ONE_LRU_ELEMENT	(1) //# read "key" and "next" needs 4 and 4 times of memory accesses, respectively


//* **************** structure definitions ****************

//* define the elements of each entry of the stripe table: Each LBA is stored in a (Chip, Block, Page) location
typedef struct
{
	flash_size_t Chip;
	flash_size_t Block;
	flash_size_t Page;
} StripeElement_t;


//* define the elements of each free block
typedef struct
{
	flash_size_t Block; //# point to the current free block
	flash_size_t Page;	//# point to the current free page in the current free block
} StripeFreeBlockElement_t;



typedef enum //# for CachedClustersDataType[]
{
	HotData,
	ColdData
} DataType_t;


//* ****************************************************************
//* Ensure there is free space in each free block. This never triggers any garbage collection
//* ****************************************************************
static void EnsureFreeSpaceInFreeBlockWithoutGC(void);

//* ****************************************************************
//* Ensure there is free space in each free block. This might trigger garbage collection
//* ****************************************************************
static void EnsureFreeSpaceInFreeBlock(void);

//* ****************************************************************
//* Simulate Li-Ping Chang's STRIPE method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void StripeSimulation(FILE *fp);














#endif
