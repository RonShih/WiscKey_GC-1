//* NFTL.c : All the simulation for NFTL type 2 are defined in this file
//* dynamic wear leveling: using best fit strategy for its GC
//* static wear leveling: activated when the BlockErase_count/flag_count > SWLThreshold "T"

#ifndef _NFTL_H
#define _NFTL_H


#include "typedefine.h"

//* ************************* MACROs ***************

//* Used in NFTLTable
#define NFTL_POINT_TO_NULL	(-1)		//* point to NULL 


//* define the elements of each entry of the NFTL-type2 table: Each PBA is stored in a (PrimaryBlock, ReplacementBlock) location
typedef struct
{
	flash_size_t PrimaryBlcok;		//* the primary block
	flash_size_t CurrentFreePageIDinPB; //# the current free page that can be written
	flash_size_t ReplacementBlock;	//* the raplacement block
	flash_size_t CurrentFreePageIDinRB;	//* current free page ID in the replacement block
} NFTLElement_t;





//* ****************************************************************
//* Simulate the NFTL type 2 method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void NFTLSimulation(FILE *fp);
















#endif

