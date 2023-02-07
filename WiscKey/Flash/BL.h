//* BL.c : All the simulation for Block Level mapping are defined in this file
//* dynamic wear leveling: When a page is updated, the block is erased immediately
//* static wear leveling: activated when the BlockErase_count/flag_count > SWLThreshold "T"

#ifndef _BL_H
#define _BL_H


#include "typedefine.h"

//* ************************* MACROs ***************

//* Used in BLTable (Block Level Mapping Table) and UpdatedClusters array
#define BL_POINT_TO_NULL	(-1)		//* point to NULL 


//* ****************************************************************
//* Simulate the Block Level mapping method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void BLSimulation(FILE *fp);




#endif


