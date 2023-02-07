//* All the flash related opeartion are defined in this file

#ifndef _FLASH_C
#define _FLASH_C


#include "main.h"
#include "flash.h"

#ifndef _FLASH_GLOBAL_
#define _FLASH_GLOBAL_
//* *********************** Global variable *************************
flash_size_t FlashSize;		//# the size of the flash-memory storage system(unit:MB)
flash_size_t ChipNumber;	//# the number of flash-memory chips
flash_size_t ChipSize;		//# the size of one flash-memory chip(unit:MB)
flash_size_t BlockSize;		//# the size of a block(unit:KB)
flash_size_t BlocksPerChip;	//# the number of block per chip
flash_size_t PageSize;		//# the size of the data area of a page (unit:B)
flash_size_t SubPageSize;	//# the size of the data area of a subpage (unit:B)
flash_size_t PageSpareSize;	//# the size of the spare area of a page (unit:B)
flash_size_t PageToLBARatio;//# the number of sectors that can be stored in one page
flash_size_t MaxLBA;		//# the maximum LBA of the flash memory
flash_size_t MaxCluster;	//# Cluster size = Page size, so that MaxCluster = MaxLBA / PageToLBARatio
flash_size_t MaxBlock;		//# the maximum number of Block, total number of block
flash_size_t MaxPage;		//# the number of pages in a block
flash_size_t MaxSubPage;		//# the number of subpages in a page
flash_size_t BadBlocksPerChip;	//# the number of bad blocks in a chip
flash_size_t EraseBlockTime;	//# the time to erase a block (unit: ns)
flash_size_t WritePageTime;		//# the time to write a page (unit: ns)
flash_size_t ReadPageTime;		//# the time to read a page (unit: ns)
flash_size_t SerialAccessTime;	//# the time to transfer one-byte data between RAM and flash chip (unit: ns)
flash_size_t AccessSRAM;		//# the time to access one instruction from SRAM (unit: ns)
//flash_size_t VBTCacheLength;	//# the length of the LRU list to cache Virtual Block Tables, block remapping list, block remapping table, and list of free block sets: only for DISPOSABLE
//flash_size_t VPTCacheLength;	//# the length of the LRU list to cache Virtual Page Tables: only for DISPOSABLE
//flash_size_t ParityCheckCacheLength;	//# the length of the LRU list to cache parity-check information: only for DISPOSABLE
flash_size_t VirtualRegionNum; 	// (only for BEYONDADDR)
flash_size_t SramSize; 			// (only for BEYONDADDR) (unit: MB)
flash_size_t LRUListSramLength; // (only for BEYONDADDR)
flash_size_t OverProvisionAreaSize; // (only for FAST)
int debugMode; 					// (only for BEYONDADDR)

flash_size_t *FreeBlockList;	//* the free block list array
//static flash_size_t FreeBlockListHead;	//* The index of the first free block
flash_size_t FreeBlockListHead;	//* The index of the first free block
static flash_size_t FreeBlockListTail;	//* The index of the last free block
flash_size_t BlocksInFreeBlockList;	//* The number of free blocks in the free block list
flash_size_t *BlocksInFreeBlockListArray;	//# Keep track of the number of free block (i.e., the number of blocks in the free block list) of each chip
static flash_size_t LeastBlocksInFreeBlockList;	//* The minimum number of free blocks that should be reserved in the free block list

#ifdef BEYONDADDR
flash_size_t ***FlashMemorySub;	//# FlashMemory[Block_ID][Page_ID][SubPage] (only for BEYONDADDR)
flash_size_t **FlashMemory;	//* FlashMemory[Block_ID][Page_ID] (fake, not used during BEYONDADDR)
#else
flash_size_t **FlashMemory;	//* FlashMemory[Block_ID][Page_ID]
#endif
//flash_size_t ***FlashMemoryArray;	//# FlashMemory[Chip_ID][Block_ID][Page_ID] (only for DISPOSABLE management scheme) where Block_ID is the offset inside its own chip
flash_size_t BadBlockCnt=0;	//# the counter to count the number of bad blocks in the flash memory
flash_size_t *BadBlockCntArray;	//# the counters to count the number of bad blocks in each flash-memory chip (only for DISPOSABLE management scheme)
flash_size_t MaxBlockEraseCnt=0;	//# The max block erase count among blocks in the flash memory
flash_size_t *BlockEraseCnt;	//* BlockEraseCnt[Block_ID] restores the erase count of each block
flash_size_t *ChipEraseCnt;		//# Keep the total number of block erase in each chip
flash_size_t *InvalidPageCnt;	//* store the number of invalid pages in each block
flash_size_t *ValidPageCnt;	//* store the number of valid pages in each block
flash_size_t *FreePageCnt;	//* store the number of valid subpages in each block (only for BEYONDADDR)
flash_size_t *InvalidSubPageCnt;	//* store the number of invalid subpages in each block (only for BEYONDADDR)
flash_size_t *ValidSubPageCnt;	//* store the number of valid subpages in each block (only for BEYONDADDR)
flash_size_t *FreeSubPageCnt;	//* store the number of valid subpages in each block (only for BEYONDADDR)

flash_size_t AccessedLogicalPages;	//# Accumulate the total logical pages being accessed
Boolean *AccessedLogicalPageMap;	//# Point to the map array

// ReliableMTD
flash_size_t SubchipsPerChip;		//# the number of subchips in each chip: for ReliableMTD
flash_size_t PlanesPerSubchip;		//# the number of planes in each subchip: for ReliableMTD
flash_size_t BlocksPerPlane;		//# The number of blocks per plane
flash_size_t EncodingUnitsPerpage;	//# the number of encoding units per page
flash_size_t EccCapability;			//# the number of bits that could be corrected
flash_huge_float_t BitErrorRate;	//# the bit error rate
Boolean SystemFailFlg = False;

//# statistics
Statistics_t AccessStatistics = {0};
#endif


//* ******************************** Functions ******************************
void FreePageCount(int blockNum, flash_size_t num)
{
	FreePageCnt[blockNum] += num;
	if(FreePageCnt[blockNum] < 0) {
		printf("Error at Tracking free page num. \n");
	}
}

void VaildPageCount(int blockNum, flash_size_t num)
{
	ValidPageCnt[blockNum] += num;
	if(ValidPageCnt[blockNum] < 0) {
		printf("Error at Tracking vaild page num. \n");
	}
}

void InvaildPageCount(int blockNum, flash_size_t num)
{
	InvalidPageCnt[blockNum] += num;
	if(InvalidPageCnt[blockNum] < 0) {
		printf("Error at Tracking invaild page num. \n");
	}
}

void FreeSubPageCount(int blockNum, flash_size_t num)
{
	FreeSubPageCnt[blockNum] += num;
	if(FreeSubPageCnt[blockNum] < 0) {
		printf("Error at Tracking free subpage num. \n");
	}
}

void VaildSubPageCount(int blockNum, flash_size_t num)
{
	ValidSubPageCnt[blockNum] += num;

	if(ValidSubPageCnt[blockNum] < 0) {
		printf("Error at Tracking valid subpage num. \n");
	}
}

void InvaildSubPageCount(int blockNum, flash_size_t num)
{
	InvalidSubPageCnt[blockNum] += num;
	if(InvalidSubPageCnt[blockNum] < 0) {
		printf("Error at Tracking invalid subpage num. \n");
	}
}

//* ****************************************************************
//* Set up the least free blocks guaranteed to be in the free block list
//# "LeastFreeBlocks" is the least number of blocks in the free block list
//* ****************************************************************
static void SetLeastBlocksInFBL(int LeastFreeBlocks)
{
	LeastBlocksInFreeBlockList = LeastFreeBlocks;
}

//* ****************************************************************
//* The least free blocks guaranteed to be in the free block list
//* return value is the minumum number of blocks that should be reserved in the free block list
//* ****************************************************************
flash_size_t LeastFreeBlocksInFBL(void)
{
	return (LeastBlocksInFreeBlockList);
}

#ifdef BEYONDADDR

void SetFlashSubPageStatus(int block, int page, int subpage, int status)
{
	FlashMemorySub[block][page][subpage] = status;
}

int GetFlashSubPageStatus(int block, int page, int subpage)
{
	return FlashMemorySub[block][page][subpage];
}

#endif

//* ****************************************************************
//* initialize the flash memory related information
//* ****************************************************************
void InitializeFlashMemory()
{
	flash_size_t i;
	flash_size_t *RandomFreeBlockOrder;	//* use to store the random order list of blocks
	ui SelectedItem;					//* The selected item to switch during the switching procedure
	flash_size_t RandomValue;			//* The value inside the selected item.
	int BadBlockThreshold;				//# The threshold value to mark a block as invalid
            
	SetLeastBlocksInFBL(64*ChipNumber);	//* Setup the least number of free blocks in the FBL
        
	SubPageSize = LBA_SIZE;
	
	//# physical address settings
	BlocksPerChip = ChipSize * 1024 / BlockSize; //# the number of blocks in a chip
	MaxBlock = FlashSize *1024 / BlockSize;	//* the number of blocks in the flash memory, total number of block
	MaxPage = BlockSize * 1024 / PageSize;	//* the number of pages in a block
	MaxSubPage = PageSize / SubPageSize; 	//* the number of subpages in a page

	//srand(time((time_t *)&i));	//* Set a random starting point
	srand(0); //* Set a random starting point

	//* Allocate the content of flash memory
#ifdef BEYONDADDR
	//* FlashMemorySub[Block_ID][Page_ID][SubPage] (only for BEYONDADDR)
	//* FlashMemory[Block_ID][Page_ID]
	//* Subpage implementation for beyond address mapping
	FlashMemorySub = (flash_size_t ***)malloc(sizeof(flash_size_t **)*MaxBlock);
	int j = 0;
	for(i=0; i < MaxBlock; i++)
	{
		FlashMemorySub[i] = (flash_size_t **)malloc(sizeof(flash_size_t *)*MaxPage);
		for(j=0; j < MaxPage; j++)
		{
			FlashMemorySub[i][j] = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxSubPage);
			memset(FlashMemorySub[i][j], FREE_PAGE, sizeof(flash_size_t)*MaxSubPage);	//* initialize the content of each page: means there is nothing in any page initially.
		}
	}
#else
	FlashMemory = (flash_size_t **)malloc(sizeof(flash_size_t *)*MaxBlock);
	for(i=0; i < MaxBlock; i++)
	{
		FlashMemory[i] = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxPage);
		memset(FlashMemory[i],FREE_PAGE,sizeof(flash_size_t)*MaxPage);	//* initialize the content of each page: means there is nothing in any page initially.
	}
#endif

	//# Allocate the flash memory array (only for DISPOSABLE management scheme)
	/*FlashMemoryArray = (flash_size_t ***)malloc(sizeof(flash_size_t **)*ChipNumber);
	for(i=0; i < ChipNumber; i++)
	{
		FlashMemoryArray[i] = &FlashMemory[i*BlocksPerChip]; //# point to the starting block of each chip
	}*/

	//# Allocate the flash memory with subpage implementation (only for BEYONDADDR)
	//FlashMemorySub = (flash_size_t *)malloc(sizeof(flash_size_t) * MaxBlock * MaxPage * MaxSubPage);
	//memset(FlashMemorySub, FREE_PAGE, sizeof(flash_size_t)* MaxBlock * MaxPage * MaxSubPage); //* initialize the content of each page: means there is nothing in any page initially

	RandomFreeBlockOrder = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate the memory space for the random block order
	for(i=0; i<MaxBlock; i++)
		RandomFreeBlockOrder[i] = i;	//* Assign an integer for each block in an increasing order
	//* Generate random sequence according to the increasing integer list
	for(i=MaxBlock; i>0; i--)
	{
		SelectedItem = ((ui)rand()<<16 | (ui)rand()) % (ui)i;
		//* switch values in the selected two blocks
		RandomValue = RandomFreeBlockOrder[SelectedItem];
		RandomFreeBlockOrder[SelectedItem] = RandomFreeBlockOrder[i-1];
		RandomFreeBlockOrder[i-1] = RandomValue;
	}

	//# (only for DISPOSABLE management scheme)
	BadBlockCntArray = (flash_size_t *)malloc(sizeof(flash_size_t)*ChipNumber);
	memset(BadBlockCntArray, 0, sizeof(flash_size_t)*ChipNumber);

	//# randomly mark some blocks as invalid blocks
	BadBlockThreshold = ((int)BadBlocksPerChip*RAND_MAX) / BlocksPerChip;

	//# Mark some blocks as bad blocks
	for(i=1; i<(MaxBlock-1); i++) //# the first and last block in the RandomFreeBlockOrder[] should be a good block
	{
		if(rand() < BadBlockThreshold)
		//if((RandomFreeBlockOrder[i] % BlocksPerChip) < BadBlocksPerChip)
		{
#ifdef BEYONDADDR
			int x = RandomFreeBlockOrder[i];
			FlashMemorySub[x][0][0] = BAD_BLOCK; //# mark an invalid block
#else
			FlashMemory[RandomFreeBlockOrder[i]][0] = BAD_BLOCK; //# mark an invalid block
#endif
			//BadBlockCntArray[RandomFreeBlockOrder[i] / BlocksPerChip]++;	//# advance the number of bad blocks in the corresponding chip
			BadBlockCnt++;	//# advance the number of bad blocks in total
		}
	}

	//* initialize the free block list
	FreeBlockList = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate the memory space for the free block list
	FreeBlockListHead = RandomFreeBlockOrder[0];	//* Initialize the index of the first free block
	//# set the order of free blocks in the list
	for(i=0; i < (MaxBlock-1) ; i++)
	{
		int current_i;
		current_i = i; //# keep current i
#ifdef BEYONDADDR
		while(FlashMemorySub[RandomFreeBlockOrder[i+1]][0][0] == BAD_BLOCK && i<(MaxBlock-1)) i++;	//# skip invalid block
#else
		while(FlashMemory[RandomFreeBlockOrder[i+1]][0] == BAD_BLOCK && i<(MaxBlock-1)) i++;	//# skip invalid block
#endif
		FreeBlockList[RandomFreeBlockOrder[current_i]] = RandomFreeBlockOrder[i+1];	//* the initial free block list: Block 0 points to Block 1, Block 1 points to Block 2, and so on.
	}
	FreeBlockList[RandomFreeBlockOrder[i]] = FBL_POINT_TO_NULL;	//* the last free block points to NULL.
	FreeBlockListTail = RandomFreeBlockOrder[i];		//* the index of the last free block
	//* free the memory space of random block list
	free(RandomFreeBlockOrder);
	//* set the initial number of free blocks in the free block list
	BlocksInFreeBlockList = MaxBlock - BadBlockCnt;

	//# for DISPOSABLE and STRIPE
	BlocksInFreeBlockListArray = (flash_size_t *)malloc(sizeof(flash_size_t)*ChipNumber);
	for(i=0;i<ChipNumber;i++)
		BlocksInFreeBlockListArray[i] = BlocksPerChip - BadBlockCntArray[i];

	//# logical address settings
	PageToLBARatio = PageSize / LBA_SIZE;	//* the number of sectors can be stored in a page
	SetMaxLBA((FlashSize * 1024 - (LeastFreeBlocksInFBL() + BadBlockCnt) * BlockSize) * (1024 / LBA_SIZE)); //* the number of sectors in the flash memory
	// flashSize -> MB, FlashSize * 1024 -> KB
	// BlockSize -> KB,

	MaxCluster = MaxLBA / PageToLBARatio;	//* the number of Clusters in the flash memory, 1 Cluster size = PageToLBARatio * 1 cluster size

	//* Allocate memory space for the erase count of each block
	BlockEraseCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate memory space
	memset(BlockEraseCnt,0,sizeof(flash_size_t)*MaxBlock);					//* set initial erase count as 0
	//* Allocate memory space for the erase count of each block (for STRIPE)
	ChipEraseCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*ChipNumber);	//* allocate memory space
	memset(ChipEraseCnt,0,sizeof(flash_size_t)*ChipNumber);					//* set initial erase count of each chip as 0

	//* Allocate memory space to keep the number of invalid pages in each block
	InvalidPageCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate memory space for InvalidPageCnt
	memset(InvalidPageCnt,0, sizeof(flash_size_t)*MaxBlock);				//* clear counters in the InvalidPageCnt
	//* Allocate memory space to keep the number of valid pages in each block
	ValidPageCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate memory space for ValidPageCnt
	memset(ValidPageCnt,0, sizeof(flash_size_t)*MaxBlock);					//* clear counters in the ValidPageCnt
	//* Allocate memory space to keep the number of valid pages in each block
	FreePageCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate memory space for FreePageCnt
	memset(FreePageCnt, 0, sizeof(flash_size_t)*MaxBlock);					//* clear counters in the FreePageCnt

	//* Allocate memory space to keep the number of invalid subpages in each page
	InvalidSubPageCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate memory space for InvalidSubPageCnt
	memset(InvalidSubPageCnt, 0, sizeof(flash_size_t)*MaxBlock);				//* clear counters in the InvalidSubPageCnt
	//* Allocate memory space to keep the number of valid subpages in each page
	ValidSubPageCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate memory space for ValidSubPageCnt
	memset(ValidSubPageCnt, 0, sizeof(flash_size_t)*MaxBlock);					//* clear counters in the ValidSubPageCnt
	//* Allocate memory space to keep the number of valid subpages in each page
	FreeSubPageCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);		//* allocate memory space for ValidSubPageCnt
	memset(FreeSubPageCnt, 0, sizeof(flash_size_t)*MaxBlock);					//* clear counters in the ValidSubPageCnt

	//* reset statistic variables
	ResetStatisticVariable(&AccessStatistics);

	//# Initialize total number of accessed pages
	AccessedLogicalPages = 0;

	//# Initialize the valid/invalid logical page list
	AccessedLogicalPageMap = (Boolean *)malloc(sizeof(Boolean)*MaxPage*MaxBlock);
	memset(AccessedLogicalPageMap,0,sizeof(Boolean)*MaxPage*MaxBlock);
}

//* ****************************************************************
//* release the data structure for flash memory
//* ****************************************************************
void FinalizeFlashMemory()
{
	flash_size_t i;

	//* Free the content of flash memory
#ifdef BEYONDADDR
	int j = 0;
	for (i = 0; i < MaxBlock; i++) {
		for (j = 0; j < MaxPage; j++) {
			free(FlashMemorySub[i][j]);
		}
		free(FlashMemorySub[i]);
	}
	free(FlashMemorySub);
#else
	for(i=0; i<MaxBlock; i++)
		free(FlashMemory[i]);
	free(FlashMemory);
#endif
	//# free the pointers to flash-memory chips (only for DISPOSABLE management scheme)
	//free(FlashMemoryArray);

	//# Allocate the flash memory with subpage implementation (only for BEYONDADDR)
	//free(FlashMemorySub);
	
	//* free the free block list
	free(FreeBlockList);	

	//# free bad block counters(only for DISPOSABLE management scheme)
	free(BadBlockCntArray);
	//# free the array that records the number of free blocks of each chip
	free(BlocksInFreeBlockListArray);

	//* Free the block erase count
	free(BlockEraseCnt);
	//# Free the total block erase count of each chip
	free(ChipEraseCnt);

	//* Free the InvalidPageCnt
	free(InvalidPageCnt);
	//* Free the ValidPageCnt
	free(ValidPageCnt);

	free(AccessedLogicalPageMap);

}


//* ****************************************************************
//* get one free block from the free block list
//* There is always some free blocks in the free block list
//* return the free block id on success. Return -1 when failed to find a free block
//* ****************************************************************
flash_size_t GetFreeBlock(Boolean fromGcRequest)
{
	flash_size_t block;

	if(fromGcRequest == True) {
		block = FreeBlockListHead;	//* fetch the first free block in the free block list
		FreeBlockListHead = FreeBlockList[block];	//* move head to next free block
		FreeBlockList[block] = FBL_NOT_IN_LIST;	//* Remove this block from the free block list and mark is as a block not in the free block list

		if(block > MaxBlock || block < 0)
		{
			printf("ERROR: A block ID %d returned from the free blocks list\n",block);
			exit(1);
		}

		BlocksInFreeBlockList--;	//* Decrease the number of free blocks in the free block list
		BlocksInFreeBlockListArray[block/BlocksPerChip]--;	//* Decrease the number of free blocks in the chip

		FreePageCnt[block] = MaxPage; //* Initialize the free page count for each block
		InvalidPageCnt[block] = 0;
		ValidPageCnt[block] = 0;

		FreeSubPageCnt[block] = MaxSubPage * MaxPage; //* Initialize the free subpage count for each block
		InvalidSubPageCnt[block] = 0;
		ValidSubPageCnt[block] = 0;

		return block;

	} else {
		if(BlocksInFreeBlockList > LeastFreeBlocksInFBL())	//* There is always some free blocks in the free block list
		{
			block = FreeBlockListHead;	//* fetch the first free block in the free block list
			FreeBlockListHead = FreeBlockList[block];	//* move head to next free block
			FreeBlockList[block] = FBL_NOT_IN_LIST;	//* Remove this block from the free block list and mark is as a block not in the free block list

			if(block>MaxBlock || block<0)
			{
				printf("ERROR: A block ID %d returned from the free blocks list\n",block);
				exit(1);
			}

			BlocksInFreeBlockList--;	//* Decrease the number of free blocks in the free block list
			BlocksInFreeBlockListArray[block/BlocksPerChip]--;	//* Decrease the number of free blocks in the chip

			FreePageCnt[block] = MaxPage; //* Initialize the free page count for each block
			InvalidPageCnt[block] = 0;
			ValidPageCnt[block] = 0;

			FreeSubPageCnt[block] = MaxSubPage * MaxPage; //* Initialize the free subpage count for each block
			InvalidSubPageCnt[block] = 0;
			ValidSubPageCnt[block] = 0;

			return block;
		}
	}
	return(-1);		//* Failed to find a free block
}


//* ****************************************************************
//* get one free block from one specific chip without checking whether there is enough free blocks in the list
//* "chip" indicates the ID of the chip, from which we want to get a free block.
//* return the free block id
//* ****************************************************************
flash_size_t GetFreeBlockFromChipWithoutChecking(int chip)
{
	flash_size_t block;
	int cnt=0;

	while(1) //# loop until we find a free block in the assigned "chip"
	{
		cnt++;

		block = FreeBlockListHead;	//* fetch the first free block in the free block list
		FreeBlockListHead = FreeBlockList[block];	//* move head to next free block
		FreeBlockList[block] = FBL_NOT_IN_LIST;	//* Remove this block from the free block list and mark is as a block not in the free block list

		if(block>MaxBlock || block<0 || cnt > MaxBlock)
		{
			printf("ERROR: A block ID %d returned from the free blocks list\n",block);
			exit(1);
		}

		BlocksInFreeBlockList--;	//* Decrease the number of free blocks in the free block list
		BlocksInFreeBlockListArray[block/BlocksPerChip]--;	//* Decrease the number of free blocks in the chip

		FreePageCnt[block] = MaxPage; //* Initialize the free page count for each block
		InvalidPageCnt[block] = 0;
		ValidPageCnt[block] = 0;

		FreeSubPageCnt[block] = MaxSubPage * MaxPage; //* Initialize the free subpage count for each block 
		InvalidSubPageCnt[block] = 0;
		ValidSubPageCnt[block] = 0;

		//# if the allocated free block "block" is not in the designated chip, put it back; othereise, return it.
		if(block/BlocksPerChip != chip)
			PutFreeBlock(block);
		else
			break;
	}

	return block;
}


//* ****************************************************************
//* get one free block from one specific chip
//* There is always some free blocks in the free block list
//# "chip" indicates the ID of the chip, from which we want to get a free block.
//* return the free block id on suceess. Return -1 when failed to find a free block
//* ****************************************************************
flash_size_t GetFreeBlockFromChip(int chip)
{
	if(BlocksInFreeBlockListArray[chip] > LeastFreeBlocksInFBL()/ChipNumber)	//* There is always more some free blocks in the free block list in each chip
	{
		return(GetFreeBlockFromChipWithoutChecking(chip));
	}

	return(-1);		//* Failed to find a free block
}


//* ****************************************************************
//* Check the status of the free block list
//* There is some free blocks in the free block list
//* When there is more than the minimum number of free blocks, return the ID of the first block in the list.
//* Otherwise return -1.
//* ****************************************************************
flash_size_t CheckFreeBlock(void)
{
	if(BlocksInFreeBlockList > LeastFreeBlocksInFBL())	//* There is always more than 2^k blocks in the free block list
		return FreeBlockListHead;
	else
		return -1;
}


//* ****************************************************************
//* Check the status of the free block list
//* There is some free blocks in the free block list
//# "chip" indicates the ID of the chip, from which we want to get a free block.
//* When there is more than the minimum number of free blocks, return the ID of the first block in the list.
//* Otherwise return -1.
//* ****************************************************************
flash_size_t CheckFreeBlockFromChip(flash_size_t chip)
{
	if(BlocksInFreeBlockListArray[chip] > LeastFreeBlocksInFBL()/ChipNumber)	//* There is always more than the minimum number of free blocks
	{
		return FreeBlockListHead;
	}
	else
	{
		return -1;
	}
}


//* ****************************************************************
//* Put one block into the free block list
//* "BlockID" is block ID of the physical block that is going to be insert into the free block list
//* ****************************************************************
void PutFreeBlock(flash_size_t BlockID)
{
	//* Add the primary block into the free block list
	FreeBlockList[FreeBlockListTail] = BlockID;
	FreeBlockListTail = BlockID;
	FreeBlockList[FreeBlockListTail] = FBL_POINT_TO_NULL;

	BlocksInFreeBlockList++;	//* advance the counter of the number of free blocks
	BlocksInFreeBlockListArray[BlockID/BlocksPerChip]++;	//* Increase the number of free blocks in the chip
}

//# ****************************************************************
//# Set the maximal value of LBA
//# "max" is the value of MaxLBA
//# Return: no return value
//# ****************************************************************
void SetMaxLBA(int max)
{
	MaxLBA = max;
	MaxCluster = MaxLBA / PageToLBARatio;
}

//# ****************************************************************
//# return the maximal value of LBA
//# Return: no return value
//# ****************************************************************
flash_size_t GetMaxLBA(void)
{
	return MaxLBA;
}

//# ****************************************************************
//# reset statistic variables
//# "statistic" is the variable to store the statistic information
//# Return: no return value
//# ****************************************************************
void ResetStatisticVariable(Statistics_t *statistic)
{
	memset(statistic,0,sizeof(Statistics_t));
}

//# ****************************************************************
//# Set the system as fail
//# ****************************************************************
void SetSystemFail(void)
{
	SystemFailFlg = True;
}

//# ****************************************************************
//# Return the status of the system
//# ****************************************************************
Boolean IsSystemFail(void)
{
	return SystemFailFlg;
}
#endif
