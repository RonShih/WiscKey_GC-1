//+ CPP SUPPORT +//
#ifdef __cplusplus
extern "C" {
#endif

#ifndef _MAIN_H
#define _MAIN_H

#define DEBUG_HANK 0

//#define BEYONDADDR
//#define BEYONDADDR_GC_FIND_MINIMUM
#define BEYONDADDR_META_COUNT

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "utility.h"
#include "typedefine.h"
#include "flash.h"
//#include "SWL.h"
#include "OutputResult.h"
#include "LRUList.h"
#include "LRUListSRAM.h"
#include "LRUListDFTL.h"

//* ********************* MACROs ****************************
#define LBA_SIZE	(512)	//* the size of a sector referenced by an LBA

//* Flash Translation Layer (FTL) definition
#define FTL_FTL			"FTL"
#define FTL_NFTL		"NFTL"
#define FTL_BL			"BL"
#define FTL_STRIPE_STATIC		"STRIPE_STATIC"		//# the static method of Li-Pin Chang's stripping
#define FTL_STRIPE_DYNAMIC		"STRIPE_DYNAMIC"	//# the dynamic method of Li-Pin Chang's stripping
#define FTL_DISPOSABLE	"DISPOSABLE"
#define FTL_BEYONDADDR	"BEYONDADDR"
#define FTL_FAST		"FAST"
#define FTL_DFTL		"DFTL"


//# Evaluation Metric
#define METRIC_AVERAGE_RESPONSE_TIME	"ART"
#define METRIC_TOAL_TRANSMISSION_TIME	"TTT"

//# the type of requests included in the evalution
#define TYPE_READ	"R"
#define TYPE_WRITE	"W"
#define TYPE_READWRITE	"RW"

//* ********************* Global variable *********************

extern flash_size_t InitialPercentage;	//* the percentage of the data stored in the flash memory initially (Unit: %)
extern flash_size_t InputFileRewindTimes;	//* the number of times to rewind the input trace file
extern char *InputFileName;		//* point to the input file name
extern char FTLMethod[20];	//* the method of Flash Translation Layer (FTL/NFTL/BL) (Note: BL=Block Level)
//extern Boolean IsDoSimulation;		//* To check whether to do simulation or not.
extern Boolean IsDoSimulationUntilFirstBlockWornOut;	//# To check whether to do simulation until the first block's worn-out
extern flash_size_t BlockEndurance;		//# the maximal erase cycle of a block
extern char *MetricType;	//# the type of metric ART/TTT (ART=average response time, TTT=total transmission time)
extern char *EvaluatedAccessType;	//# the type of eavluated: R/W/RW (R=read only, W: write only, RW: both read and write requests)


//# the parameters for DISPOSABLE management scheme
extern flash_size_t SpareBlocksPerChip;	//# the number of spare blocks in a chip
extern flash_size_t BlocksPerPhysicalRegion; //# the number of blocks in a physical region
extern flash_size_t BlocksPerVirtualRegion;	//# the number of block in a virtual region
extern flash_size_t RegionSwappingThreshold;	//# the number of new developed bad blocks to trigger region swapping


#endif

//+ CPP SUPPORT +//
#ifdef __cplusplus
}
#endif
