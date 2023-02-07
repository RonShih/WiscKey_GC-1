//* DFTL.c : All the simulation for FTL are defined in this file
//* dynamic wear leveling: using best fit strategy for its GC
//* static wear leveling: activated when the BlockErase_count/flag_count > SWLThreshold "T"

#include "typedefine.h"

//**************** MACROs ****************

// Used in FTLTable
#define DFTL_POINT_TO_NULL	(-1)		//* point to NULL

typedef struct blockLinkedListDFTL
{
	flash_size_t blockNum;
	int validPageNum;
	struct blockLinkedListDFTL *next;

} blockLinkedListDFTL;

// DFTL table: Each LBA is stored in a (Block, Page) location
typedef struct
{
	flash_size_t Block;
	flash_size_t Page;
	int LruOffset;
} DFTLElement_t;

// Cached Mapping Table, replaced by LRUElement_D
/*typedef struct
{
	flash_size_t Dvpn;
	flash_size_t Dppn;
	Boolean dirty;
} CMTElement_t;
*/
// Global Translation Directory
typedef struct
{
	flash_size_t Mvpn;
	flash_size_t Mppn;
} GTDElement_t;

// Supported Operations
typedef enum
{
	READ,
	WRITE,
	ERASE
}Command_t;

// His/Miss counts in Cached Mapping Table
extern double hitCnt;
extern double missCnt;

void InitializeDFTL(void);

void DFTLEraseOneBlock(flash_size_t BlockID);

void DFTLGarbageCollection(void);

Boolean DFTLWriteOneCluster(flash_size_t ClusterID);

Boolean DFTLReadOneCluster(flash_size_t ClusterID);

void DFTLSimulation(FILE *fp);
