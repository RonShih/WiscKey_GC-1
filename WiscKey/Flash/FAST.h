//+ FAST.h : All the simulation for FAST are defined in this file
//+ Dynamic wear leveling: using round-robin strategy for its GC
//+ Static wear leveling: activated when the BlockErase_count/flag_count > SWLThreshold "T"

//+ CPP SUPPORT +//
#ifdef __cplusplus
extern "C" {
#endif

//+ INCLUDE PROTECT +//
#ifndef _FAST_H
#define _FAST_H

//+ Used in FASTTable
#define FAST_POINT_TO_NULL (-1) //* point to NULL 

#include "typedefine.h"

//* ****************************************************************
//* Simulate the FAST method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void FASTSimulation(FILE *fp);

//+ INCLUDE PROTECT +//
#endif

//+ CPP SUPPORT +//
#ifdef __cplusplus
}
#endif
