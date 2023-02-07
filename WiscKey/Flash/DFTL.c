//* DFTL.c : All the simulation for DFTL are defined in this file
//* dynamic wear leveling: using best fit strategy for its GC
//* static wear leveling: activated when the BlockErase_count/flag_count > SWLThreshold "T"

#ifndef _DFTL_C
#define _DFTL_C
#endif

/*
 #ifndef _MingChang_DEBUG
 #define _MingChang_DEBUG
 #endif
 */

#include "main.h"
#include "DFTL.h"

//**************** Structure Definitions ****************

// Global Mapping Table, denoted as DFTL Table, used to maintain all mapping information, fixed size,
// LBA to PBA translation should first check here to speed up the process
// LBA : index, PBA : Content
static DFTLElement_t *DFTLTable; // number of entry = BlocksInFreeBlockList * MaxPage
static struct blockLinkedListDFTL *allocatedDFTLTableBlkListHead; // aka translation blocks
static long DFTLTableSize;
static int gmtBlkCount; // aka translation block cnt

// Info for allocated data blocks
static int currentDataBlkNum;
static int dataBlkCount;
static int currentDataBlkPageNum;

// Cached Mapping Table.
static LRUList_D CMTLRUList = { 0 };
static int cmtListLength;
static int cmtEntryPerPage;
static int cmtEnteySize;

// Global Translation Directory. Not care so much, I didn't really use GTD
static GTDElement_t *GTD;
static int gtdListLength;
static int gtdEntryPerPage;
static int gtdEnteySize;

double hitCnt = 0;
double missCnt = 0;
int SramSizeLimitD;

//# ****************************************************************
//# Linked List Operation
//# ****************************************************************
struct blockLinkedListDFTL* CreateListD(int val) {
	if (DEBUG_HANK)
		printf("INFO: Creating a list with head block number [%d] \n", val);
	struct blockLinkedListDFTL *ptr = (struct blockLinkedListDFTL*) malloc(sizeof(struct blockLinkedListDFTL));
	if (NULL == ptr) {
		printf("ERROR: Node creation failed \n");
		return NULL;
	}
	ptr->blockNum = val;
	ptr->next = NULL;

	return ptr;
}

struct blockLinkedListDFTL* AddToListD(struct blockLinkedListDFTL *head, int val) {
	// Check if the list is initialized or not
	if (head == NULL) {
		head = CreateListD(val);
		return head;
	}
	// If the list is already initialized, let's create a new one and append to the list
	struct blockLinkedListDFTL *ptr = (struct blockLinkedListDFTL*) malloc(sizeof(struct blockLinkedListDFTL));
	if (NULL == ptr) {
		printf("ERROR: Node creation failed \n");
		return NULL;
	}

	ptr->blockNum = val;
	ptr->next = NULL;

	if (DEBUG_HANK)
		printf( "INFO : Add to the beginning of list with block number [%d] \n", val);
	ptr->next = head;
	head = ptr;

	return head;
}

struct blockLinkedListDFTL* SearchInListD(struct blockLinkedListDFTL *ptr, int val, struct blockLinkedListDFTL **prev) {
	struct blockLinkedListDFTL *tmp = NULL;
	Boolean found = False;

	if (DEBUG_HANK)
		printf("INFO: Searching the list for block number[%d] \n", val);
	while (ptr != NULL) {
		if (ptr->blockNum == val) {
			found = True;
			break;
		} else {
			tmp = ptr;
			ptr = ptr->next;
		}
	}

	if (found == True) {
		if (prev)
			*prev = tmp;

		return ptr;
	} else {
		return NULL;
	}
}

int DeleteFromListD(struct blockLinkedListDFTL *ptr, int val) {
	struct blockLinkedListDFTL *prev = NULL;
	struct blockLinkedListDFTL *del = NULL;

	if (DEBUG_HANK)
		printf("INFO: Deleting value [%d] from list \n", val);

	del = SearchInListD(ptr, val, &prev);
	if (del == NULL) {
		return -1;
	} else {
		if (prev != NULL)
			prev->next = del->next;

		if (del == ptr) {
			ptr = del->next;
		}
	}

	free(del);
	del = NULL;

	return 0;
}

int SizeOfListD(struct blockLinkedListDFTL *ptr) {
	int len = 0;
	while (ptr != NULL) {
		len++;
		ptr = ptr->next;
	}
	return len;
}

void PrintListD(struct blockLinkedListDFTL *ptr) {
	printf("\n -------Printing list Start \n");
	while (ptr != NULL) {
		printf("[%d] (FreePageCnt %d) \n", ptr->blockNum, FreePageCnt[ptr->blockNum]);
		ptr = ptr->next;
	}
	printf("\n -------Printing list End \n");

	return;
}

void AllocateDataBlk(Boolean fromGcRequest){
	if(currentDataBlkPageNum < MaxPage && currentDataBlkNum != DFTL_POINT_TO_NULL) {
		// there are still some pages left in current block, just return
		return;
	} else {
		// there is no pages left, allocate a new block
		int newBlockNum = -1;
		do {
			newBlockNum = GetFreeBlock(fromGcRequest); //* get one free block, if the block is not free, do garbage collection to reclaim one more free block
			//* Do garbage collection to reclaim one more free block if there is no free block in the free block list
			if(newBlockNum == -1) {
				DFTLGarbageCollection();
			}
		} while (newBlockNum == -1);
		// Update current data block info
		currentDataBlkNum = newBlockNum;
		dataBlkCount += 1;
		currentDataBlkPageNum = 0;
	}
}

void AllocateGMTBlk(Boolean fromGcRequest) {
	/*if(currentTransBlkPageNum < MaxPage && currentTransBlkNum != DFTL_POINT_TO_NULL) {
		// there are still some pages left in current block, just return
		return;
	} else {
		// there is no pages left, allocate a new block
		int newBlockNum = -1;
		do {
			newBlockNum = GetFreeBlock(fromGcRequest); //* get one free block, if the block is not free, do garbage collection to reclaim one more free block
			//* Do garbage collection to reclaim one more free block if there is no free block in the free block list
			if(newBlockNum == -1) {
				DFTLGarbageCollection();
			}
		} while (newBlockNum == -1);
		// Update current data block info
		currentTransBlkNum = newBlockNum;
		transBlkCount += 1;
		currentTransBlkPageNum = 0;
	}*/
}

//# ****************************************************************
//# Initialize the settings for flash memory in DFTL
//# ****************************************************************
void InitializeDFTL(void) {
	// Initialize the DFTL Table
	DFTLTable = (DFTLElement_t *) malloc(sizeof(DFTLElement_t) * (BlocksInFreeBlockList * MaxPage));
	memset(DFTLTable, DFTL_POINT_TO_NULL, sizeof(DFTLElement_t) * (BlocksInFreeBlockList * MaxPage));
	int i = 0;
	// Initialize each entry in the the DFTL Table
	for (i = 0; i < VirtualRegionNum; i++) {
		DFTLTable[i].Block = DFTL_POINT_TO_NULL;
		DFTLTable[i].Page = DFTL_POINT_TO_NULL;
		DFTLTable[i].LruOffset = DFTL_POINT_TO_NULL;
	}

	// Info for allocated data blocks
	currentDataBlkNum = DFTL_POINT_TO_NULL;
	dataBlkCount = 0;
	currentDataBlkPageNum = 0;

	// Set SRAM size limit (SramSize -> MB, SramSizeLimitD -> B)
	SramSizeLimitD = SramSize * 1024 * 1024;
	// Divide up the SRAM for global mapping table &¡@cmt lru list
	// Officially, each GMT entries is 8 Bytes
	// B = number of pages * 8 Bytes
	DFTLTableSize = ((FlashSize * 1024) / (PageSize / 1024)) * 8;
	gmtBlkCount = ceil((double)((double)DFTLTableSize / (double)(BlockSize * 1024)));

	cmtEnteySize = 8; // Bytes
	gtdEnteySize = 8; // Bytes

	cmtEntryPerPage = PageSize / cmtEnteySize; // 512 entries
	gtdEntryPerPage = PageSize / gtdEnteySize; // 512 entries

	cmtListLength = (SramSizeLimitD / 2) / cmtEnteySize; // stay in memory
	gtdListLength = (SramSizeLimitD / 2) / gtdEnteySize; // stay in memory

	// Correction to prevent over-size gtdListLength
	if(gtdListLength > gmtBlkCount) {
		gtdListLength = gmtBlkCount;
		cmtListLength = (SramSizeLimitD - (gtdEnteySize * gtdListLength))/ gtdEnteySize;
	}

	// Initialize the GTD Table
	GTD = (GTDElement_t *) malloc(sizeof(GTDElement_t) * gtdListLength);
	memset(GTD, 0, sizeof(GTDElement_t) * gtdListLength);
	// Initialize each entry in the the GTD Table
	for (i = 0; i < gtdListLength; i++) {
		GTD[i].Mvpn = i;
		GTD[i].Mppn = -1;
	}

	// Create the lru list for cmt
	CreateLRUListD(&CMTLRUList, cmtListLength);

	printf("DFTL, GMT = (%d Bytes, %d entries, %d blks), CMT = (%d Bytes, %d cached entries), GTD = (%d Bytes, %d cached entries) \n",
			DFTLTableSize, ((FlashSize * 1024 * 1024) / PageSize), gmtBlkCount,
			cmtListLength * cmtEnteySize, cmtListLength,
			gtdListLength * gtdEnteySize, gtdListLength);

	// Check remaining pages after allocating GMT & GTD
	int remainingBlock = BlocksInFreeBlockList - (gmtBlkCount + LeastFreeBlocksInFBL());
	if (remainingBlock < 0) {
		printf("DFTL, No sufficient space to store mapping table\n");
		exit(1);
	}

	// Allocate blocks for the GMT
	int j = 0;
	int blkNum = -1;
	do {
		// Get a free block
		blkNum = GetFreeBlock(False);
		// Add to allocated block list for management (not part of DFTL)
		AddToListD(allocatedDFTLTableBlkListHead, blkNum);
		// Set gtd at the same time
		GTD[j].Mppn = blkNum;
		int k = 0;
		for(k = 0;k < MaxPage;k++) {
			FlashMemory[blkNum][k] = VALID_PAGE;
		}

		j ++;
	} while (j < gmtBlkCount);

	// Get a free data block
	AllocateDataBlk(False);
}

//# ****************************************************************
//# Release the memory space allocated for DFTL
//# ****************************************************************
static void FinalizeDFTL(void) {
	free(GTD);
	free(DFTLTable);
}

//* ****************************************************************
//* Read a cluster from the flash memory
//* "clusterID" is the Cluster that is going to be updated or written
//* Return value: If the "clusterID" exists, return True. Otherwise, return False.
//* *****************************************************************
Boolean DFTLReadOneCluster(flash_size_t clusterID) {
	int lruPos = -1;
	// Quick check if we can find the mapping information from the LRU list
	if(DFTLTable[clusterID].LruOffset != DFTL_POINT_TO_NULL) {
		// Found, move to the head of lru
		lruPos = DFTLTable[clusterID].LruOffset;
		MoveElemenetToTheHeadofLRUListD(&CMTLRUList, clusterID, &lruPos);
		// Change GMT info
		DFTLTable[clusterID].LruOffset = CMTLRUList.Head;
		// Set the final lruPos for further use
		lruPos = DFTLTable[clusterID].LruOffset;
		hitCnt ++;

		if (DEBUG_HANK)
			printf(", 'FETCH' from LRU list. \n");
	} else {
		// Not found, Check if the mapping is already created
		if(DFTLTable[clusterID].Block != DFTL_POINT_TO_NULL) {
			// The mapping already exist, but not in the cmt lru
			PutElemenetToLRUListD(&CMTLRUList, clusterID, &lruPos, &AccessStatistics.TotalReadRequestTime);
			// Change GMT info
			DFTLTable[clusterID].LruOffset = lruPos;
			// Skip check to gmt and only count the read time
			// Time to load the cached mapping info (gdt + gmt)
			AccessStatistics.TotalReadRequestTime += TIME_READ_PAGE * 2;
			missCnt ++;

			if (DEBUG_HANK)
				printf(", 'INSERT' into LRU list. \n");
		} else {
			// The mapping doesn't exist, create one and insert into cmt lru
			return (False);
		}
	}

	// The time to read a page
	AccessStatistics.TotalReadRequestTime += TIME_READ_PAGE;
	AccessStatistics.TotalReadPageCount += 1;

	return (True);
}

//* ****************************************************************
//* Write a cluster to the flash memory and also update the DFTL table
//* "clusterID" is the Cluster that is going to be updated or written
//* *****************************************************************
Boolean DFTLWriteOneCluster(flash_size_t clusterID) {
	int lruPos = -1;
	// Check residual page num before continue
	if (currentDataBlkPageNum >= MaxPage) {
		AllocateDataBlk(False);
	}
	if (DEBUG_HANK)
		printf("Write one cluster to %d \n", clusterID);
	// Quick check if we can find the mapping information from the LRU list
	if(DFTLTable[clusterID].LruOffset != DFTL_POINT_TO_NULL) {
		// Found, move to the head of lru
		lruPos = DFTLTable[clusterID].LruOffset;
		MoveElemenetToTheHeadofLRUListD(&CMTLRUList, clusterID, &lruPos);
		// Set dirty since it'a a write req
		SetElementDirtyD(&CMTLRUList, clusterID, &lruPos);
		// Invalidate original page
		InvalidPageCnt[DFTLTable[clusterID].Block]++;
		FlashMemory[DFTLTable[clusterID].Block][DFTLTable[clusterID].Page] = INVALID_PAGE;
		// Change GMT info
		DFTLTable[clusterID].LruOffset = CMTLRUList.Head;
		// Set the final lruPos for further use
		lruPos = DFTLTable[clusterID].LruOffset;
		hitCnt ++;

		if (DEBUG_HANK)
			printf(", 'FETCH' from LRU list. \n");
	} else {
		// Not found, Check if the mapping is already created
		if(DFTLTable[clusterID].Block != DFTL_POINT_TO_NULL) {
			// The mapping already exist, but not in the cmt lru
			PutElemenetToLRUListD(&CMTLRUList, clusterID, &lruPos, &AccessStatistics.TotalWriteRequestTime);
			// Set dirty since it'a a write req
			SetElementDirtyD(&CMTLRUList, clusterID, &lruPos);
			// Invalidate original page
			InvalidPageCnt[DFTLTable[clusterID].Block]++;
			FlashMemory[DFTLTable[clusterID].Block][DFTLTable[clusterID].Page] = INVALID_PAGE;
			// Change GMT info
			DFTLTable[clusterID].LruOffset = lruPos;
			// Skip check to gmt and only count the read time
			// Time to load the cached mapping info (gdt + gmt)
			AccessStatistics.TotalWriteRequestTime += TIME_READ_PAGE * 2;
			missCnt ++;

			if (DEBUG_HANK)
				printf(", 'INSERT' into LRU list. \n");
		} else {
			// The mapping doesn't exist, create one and insert into cmt lru
			// Put to cmt lru
			PutElemenetToLRUListD(&CMTLRUList, clusterID, &lruPos, &AccessStatistics.TotalWriteRequestTime);
			// Set dirty since it'a a write req
			SetElementDirtyD(&CMTLRUList, clusterID, &lruPos);
			// Change GMT info
			DFTLTable[clusterID].LruOffset = lruPos;

			if (DEBUG_HANK)
				printf(", 'CREATE & INSERT' into LRU list. \n");
		}
	}
	// Store the data of this LBA in current (Block, Page)
	DFTLTable[clusterID].Block = currentDataBlkNum;
	DFTLTable[clusterID].Page = currentDataBlkPageNum;
	// Write to flash by setting the corresponding page
	FlashMemory[currentDataBlkNum][currentDataBlkPageNum] = clusterID;
	// Advance the number of valid pages in the block
	ValidPageCnt[currentDataBlkNum]++;
	FreePageCnt[currentDataBlkNum]--;
	// Move to next free page
	currentDataBlkPageNum++;
	// The time to write a page
	AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE;
	AccessStatistics.TotalWritePageCount += 1;

	return (True);
}

//* ****************************************************************
//* Erase one block
//* ****************************************************************
void DFTLEraseOneBlock(flash_size_t BlockID) {
	flash_size_t i;

	//* reclaim the selected block
	for (i = 0; i < MaxPage; i++) {
		//* if current free block is filled up, get a free block from the lsit
		if (currentDataBlkPageNum >= MaxPage) {
			AllocateDataBlk(True);
		}

		if ((FlashMemory[BlockID][i] != INVALID_PAGE) && (FlashMemory[BlockID][i] != FREE_PAGE)) {
			int lruPos = -1;
			int clusterID = FlashMemory[BlockID][i];

			DFTLTable[clusterID].Block = currentDataBlkNum;
			DFTLTable[clusterID].Page = currentDataBlkPageNum;

			if(DFTLTable[clusterID].LruOffset != DFTL_POINT_TO_NULL) {
				RemoveElemenetFromLRUListD(&CMTLRUList, clusterID, &lruPos);
			}
			// Write to flash by setting the corresponding page
			FlashMemory[currentDataBlkNum][currentDataBlkPageNum] = clusterID;
			// Advance the number of valid pages in the block
			ValidPageCnt[currentDataBlkNum]++;
			FreePageCnt[currentDataBlkNum]--;
			// Move to next free page
			currentDataBlkPageNum += 1;
			//# Statistics
			AccessStatistics.TotalLivePageCopyings++;//* advnace the total number of live-page copyings by 1
			AccessStatistics.TotalWriteRequestTime += (TIME_READ_PAGE + TIME_WRITE_PAGE); //#the time to copy a live page
		}
	}

	// update the erase count
	BlockEraseCnt[BlockID]++;
	// Update the max block erase count
	if (BlockEraseCnt[BlockID] > MaxBlockEraseCnt) {
		MaxBlockEraseCnt = BlockEraseCnt[BlockID];
	}

	// update total number of block erases
	AccessStatistics.TotalBlockEraseCount ++;
	// the time to erase a block
	AccessStatistics.TotalWriteRequestTime += TIME_ERASE_BLOCK;
	// Clean up the reclaimed block as a free block
	InvalidPageCnt[BlockID] = 0;
	ValidPageCnt[BlockID] = 0;
	FreePageCnt[BlockID] = MaxPage;
	// Set page free
	for (i = 0; i < MaxPage; i++)
		FlashMemory[BlockID][i] = FREE_PAGE;
	// Put the selected free block to the free block list
	PutFreeBlock(BlockID);
}

//# ****************************************************************
//# Do garbage collection to reclaim one more free block if there is no free block in the free block list
//# dynamic wear leveling: using cost-benefit strategy for its GC->
//# (free page=0, live page=-1, dead page = 1) while the sum over every page is larger than 0, erase the block.
//# Always reclaim the first block that has the maximal number of invalid pages
//# ****************************************************************
void DFTLGarbageCollection(void) {
	static flash_size_t currentScanningBlockID = 0;//* Record the Id of the block that is going to be scanned

	int currentMaxInvalidCnt = 0;
	int blockWithMaxInvalidCnt = -1;

	//* Keep reclaim blocks until there is one more free block in the free block list
	while (CheckFreeBlock() == -1) {

		currentScanningBlockID = 0;
		currentMaxInvalidCnt = 0;
		blockWithMaxInvalidCnt = -1;

		//* find out the block with the largest number of invalid pages
		do {
			// Rule out free block & bad Block
			if(FlashMemory[currentScanningBlockID][0] != FREE_PAGE &&
			   FlashMemory[currentScanningBlockID][0] != BAD_BLOCK) {
				// Stop one a block has zero vaild page
				if (InvalidPageCnt[currentScanningBlockID] == MaxPage) {
					currentMaxInvalidCnt = InvalidPageCnt[currentScanningBlockID];
					blockWithMaxInvalidCnt = currentScanningBlockID;
					break;
				}
				if(InvalidPageCnt[currentScanningBlockID] > currentMaxInvalidCnt) {
					currentMaxInvalidCnt = InvalidPageCnt[currentScanningBlockID];
					blockWithMaxInvalidCnt = currentScanningBlockID;
				}
			}
			currentScanningBlockID += 1;
		} while (currentScanningBlockID < MaxBlock);

		if(currentMaxInvalidCnt == 0)
			printf("DFTL, !!! \n");
		if (blockWithMaxInvalidCnt == -1) {
			printf("DFTL, cannot fin victim during GC :( \n");
			exit(1);
		} else {
			//* Erase one block and also update the BET and counters
			DFTLEraseOneBlock(blockWithMaxInvalidCnt);
		}

	} //* end of while()
}

//* ****************************************************************
//* Simulate the DFTL method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void DFTLSimulation(FILE *fp) {
	flash_size_t CurrentCluster;

	InitializeDFTL();

	// Randomly select where are the data stored in the flash memory initially
	// according the parameter "InitialPercentage" to determine what's the percentage of data in ths flash memory initially.

	// The max random number equals "InitialPercentage" percentage of data.
	int BlockMaxRandomValue = (int) ((float) InitialPercentage * (RAND_MAX) / 100);
	int i = 0;
	for (i = 0; i < MaxCluster; i++) {
		if(i % 500000 ==0)
			printf("i = %d, free blocks = %d\n", i, BlocksInFreeBlockList);

		if (rand() <= BlockMaxRandomValue) {
			DFTLWriteOneCluster(i);

			// Update the number of accessed logical pages
			AccessedLogicalPages++;	// Advanced the number of set flags in the map
			AccessedLogicalPageMap[i] = True; // Set the flag on
		}
	}
	//# Log information
	printf("Initial Percentage of data: %.02f%%\n", (float) AccessedLogicalPages / (MaxPage * (MaxBlock)) * 100);

	//# reset statictic variables
	ResetStatisticVariable(&AccessStatistics);

	//* ******************************* Start simulation ***************************
	int tmp = 0;
	while (GetOneOperation(fp, &CurrentRequest.AccessType, &CurrentRequest.StartCluster, &CurrentRequest.Length, False))//* fetch one write operation
	{
		if(tmp % 1000 == 0)
			printf("---- %d ---- \n", tmp);

		// Check each Write request is a new write or an update and then update the mapping information
		for (CurrentCluster = CurrentRequest.StartCluster;
				CurrentCluster < (CurrentRequest.StartCluster + CurrentRequest.Length);CurrentCluster++) {

			if (CurrentCluster >= MaxLBA) {
				printf("DFTL : LBA out of range\n");
				return ;
			}

			if (CurrentRequest.AccessType == WriteType) {
				// Write a cluster to the flash memory and also update the DFTL table.
				DFTLWriteOneCluster(CurrentCluster);

			} else if (CurrentRequest.AccessType == ReadType) {
				if (DFTLReadOneCluster(CurrentCluster) == False) {
					// Undo
					AccessStatistics.TotalReadRequestTime -= TIME_READ_PAGE * (CurrentCluster - CurrentRequest.StartCluster);
					AccessStatistics.FailedReadOperation += 1;
					AccessStatistics.TotalReadPageCount -= (CurrentCluster - CurrentRequest.StartCluster);
					break;
				}
			}
		}

		//# Statistics
		if (CurrentRequest.AccessType == WriteType) {
			if (CurrentCluster == (CurrentRequest.StartCluster + CurrentRequest.Length)) {
				AccessStatistics.TotalWriteRequestCount++;//# advance the number of total write requests
			} else {
				AccessStatistics.FailedWriteOperation += 1;
			}
		} else if (CurrentRequest.AccessType == ReadType) {
			if (CurrentCluster == (CurrentRequest.StartCluster + CurrentRequest.Length)) {
				AccessStatistics.TotalReadRequestCount++;//# advance the number of total read requests
			}
		}

		tmp += 1;
	}	//* end of while


	printf("FreeBlock %d. \n", BlocksInFreeBlockList);
	printf("Total Write %"PRINTF_LONGLONG" (Failed %"PRINTF_LONGLONG"). \n", AccessStatistics.TotalWriteRequestCount, AccessStatistics.FailedWriteOperation);
	printf("Total Read %"PRINTF_LONGLONG" (Failed %"PRINTF_LONGLONG"). \n", AccessStatistics.TotalReadRequestCount, AccessStatistics.FailedReadOperation);
	printf("Total Operations %"PRINTF_LONGLONG". \n", AccessStatistics.TotalWriteRequestCount + AccessStatistics.TotalReadRequestCount);
	printf("----------------------------------------- \n");
	printf("TotalBlockEraseCount %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalBlockEraseCount);
	printf("TotalLivePageCopyings %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalLivePageCopyings);
	printf("----------------------------------------- \n");
	printf("TotalWritePageCount %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalWritePageCount);
	printf("TotalReadPageCount %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalReadPageCount);

	FinalizeDFTL();

	//* ******************************* End of simulation *****************************
}

