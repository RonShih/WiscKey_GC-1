//* FTL.c : All the simulation for FTL are defined in this file
//* dynamic wear leveling: using best fit strategy for its GC
//* static wear leveling: activated when the BlockErase_count/flag_count > SWLThreshold "T"

#ifndef _FTL_H
#define _FTL_H

#include "typedefine.h"
#include "map"
#include "vector"
//***********************GLOBAL VARIABLE*******************
extern flash_size_t CurrentCluster;
//* ************************* MACROs ***************

//* Used in FTLTable
#define FTL_POINT_TO_NULL	(-1)		//* point to NULL 


//* **************** structure definitions ****************

//* define the elements of each entry of the FTL table: Each LBA is stored in a (Block, Page) location
typedef struct
{
	flash_size_t Block;
	flash_size_t Page;
} FTLElement_t;



//* ****************************************************************
//* Simulate the FTL method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void InitializeFTL();
void WritewithFTL(AccessType_t AccessType,flash_size_t Length);












#endif
