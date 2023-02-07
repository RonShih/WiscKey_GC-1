 //# ****************************************************************
//# STRIPE_C.c : All the simulation for Li-Ping Chang's striping algorithm are defined in this file
//# dynamic wear leveling: using cost-benefit strategy for its GC
//# ****************************************************************

#ifndef _STRIPE_C
#define _STRIPE_C
#endif


#include "main.h"
#include "STRIPE.h"

//# ************************* Defines  *********************



//# **************************** MACRO ***********************
//# "clusters" is the array to cache clusters where it only caches at most one cluster for each chip.
//# "numclusters" is the number of cached clusters
//# "chips" is the number of chips
#define CLEAR_CACHEDCLUSTERS(clusters, numclusters, chips) do{int i; for(i=0;i<chips;i++) clusters[i] = NO_CLUSTER_CACHED; numclusters = 0;}while(0)


//# **************************** Global variables ***********************
static StripeElement_t *StripeTable;	//* Stripe table

static StripeFreeBlockElement_t *CurrentFreeBlock;	//# each chip has its own free block (for cold data of STRIPE_DYNAMIC)
static StripeFreeBlockElement_t *CurrentFreeBlockForHotData; //# each chip has its own free block for hot data of STRIPE_DYNAMIC

static flash_size_t *CachedClusters;	//# the array to store the cached clusters or live pages
static flash_size_t NumCachedClusters; //# the number of cached clusters or live pages
static DataType_t *CachedClustersDataType;	//# the type of data cached --> i.e., hot or cold data

static flash_huge_size_t *ChipEraseCnt;	//# the array to keep the total erase count of each chip
static flash_size_t *SortedOrder;	//# Chip ID with largest erase count is stored in the first element, and so on

static LRUList_t HotList = {0};		//# The hot LBA list
static LRUList_t CandidateList = {0};	//# The candidate LBA list



//* ******************************** functions *********************


//* ****************************************************************
//* Read a cluster from the flash memory
//* "request" is the write request that is going to be updated or written
//* Return value: If the clusters in the request[] exist, return True. Otherwise, return False.
//* *****************************************************************
static Boolean StripeReadOneRequest(OneRequest_t request)
{
	int i;
	Boolean *BusyChipFlag;	//# the flags to indicate whether a chip is busy
	int NumBusyChips;			//# the number of busy chips

	//# check whether the request clusters exsit
	for(i=request.StartCluster;i<request.StartCluster+request.Length;i++)
	{
		if (StripeTable[i].Chip == STRIPE_POINT_TO_NULL) return(False);
	}

	//# allocate the memory space and initialize it
	BusyChipFlag = (Boolean *)malloc(sizeof(Boolean)*ChipNumber);
	memset(BusyChipFlag,False,sizeof(Boolean)*ChipNumber);
	NumBusyChips=0;

	//# Stripe read
	for(i=request.StartCluster;i<request.StartCluster+request.Length;i++)
	{
		//The chip is busy, read the cluster from the chip
		if (BusyChipFlag[StripeTable[i].Chip] == True) //# stripe
		{
			//# Statistics
			AccessStatistics.TotalReadRequestTime += NumBusyChips * TIME_TRANSFER_PAGE + TIME_READ_PAGE_NO_TRANSFER; //# the time to read "NumBusyChips" pages concurrently
			//# reset the busy flags and busy counter
			memset(BusyChipFlag,False,sizeof(Boolean)*ChipNumber);
			NumBusyChips=0;
		}

		//# cache the cluster
		BusyChipFlag[StripeTable[i].Chip] = True;
		NumBusyChips++;
	}

	if(NumBusyChips > 0)
	{
		//# Statistics
		AccessStatistics.TotalReadRequestTime += NumBusyChips * TIME_TRANSFER_PAGE + TIME_READ_PAGE_NO_TRANSFER; //# the time to read "NumBusyChips" pages concurrently
	}

	free(BusyChipFlag); //# free memory space

	return(True);
}


//* ****************************************************************
//* Proceed a cluster set that can be written concurrently and also update the address translation table.
//# Write one cluster to each chip concurrently
//* "clusters" is the array of clusters that are going to be striped
//# "num" is the number of clusters that are going to be written
//* ****************************************************************
static void StripeWriteOneClusterSet(flash_size_t *clusters, flash_size_t num)
{
	int i;

	//* if this is an update, invalidate this page, and advance the number of invalid pages by 1.
	for(i=0;i<ChipNumber;i++)
	{

		if(clusters[i] != NO_CLUSTER_CACHED) //# there is a cached cluster for writing to the corresponding chip
		{
			if(StripeTable[clusters[i]].Chip != STRIPE_POINT_TO_NULL)	//* an update
			{
				FlashMemory[StripeTable[clusters[i]].Chip*BlocksPerChip+StripeTable[clusters[i]].Block][StripeTable[clusters[i]].Page] = INVALID_PAGE;	//* invalidate this page
				InvalidPageCnt[StripeTable[clusters[i]].Chip*BlocksPerChip+StripeTable[clusters[i]].Block]++;	//* advance the number of invalid pages in the block
				ValidPageCnt[StripeTable[clusters[i]].Chip*BlocksPerChip+StripeTable[clusters[i]].Block]--;	//* decrease the number of valid pages in the block
				if(InvalidPageCnt[StripeTable[clusters[i]].Chip*BlocksPerChip+StripeTable[clusters[i]].Block]>MaxPage)
					printf("%d=%d\n",clusters[i],InvalidPageCnt[StripeTable[clusters[i]].Chip*BlocksPerChip+StripeTable[clusters[i]].Block]);
			}

			//# for STRIPE_STATIC:
			if(strcmp(FTLMethod, FTL_STRIPE_STATIC)==0)
			{
				//# Store the data of this LBA in current (Chip, Block, Page)
				StripeTable[clusters[i]].Chip = CurrentFreeBlock[clusters[i]%ChipNumber].Block / BlocksPerChip;
				StripeTable[clusters[i]].Block = CurrentFreeBlock[clusters[i]%ChipNumber].Block  % BlocksPerChip;
				StripeTable[clusters[i]].Page = CurrentFreeBlock[clusters[i]%ChipNumber].Page;
				//* advance the number of valid pages in the block
				ValidPageCnt[CurrentFreeBlock[clusters[i]%ChipNumber].Block]++;
				CurrentFreeBlock[clusters[i]%ChipNumber].Page++;	//# move to next free page
			}

			if(strcmp(FTLMethod, FTL_STRIPE_DYNAMIC)==0)
			{
				//# Store the cold-data cluster
				if(CachedClustersDataType[i] == ColdData)
				{
					StripeTable[clusters[i]].Chip = CurrentFreeBlock[i].Block / BlocksPerChip;
					StripeTable[clusters[i]].Block = CurrentFreeBlock[i].Block  % BlocksPerChip;
					StripeTable[clusters[i]].Page = CurrentFreeBlock[i].Page;
					//* advance the number of valid pages in the block
					ValidPageCnt[CurrentFreeBlock[i].Block]++;
					CurrentFreeBlock[i].Page++;	//# move to next free page
				}
				else //# store the hot-data cluster
				{
					StripeTable[clusters[i]].Chip = CurrentFreeBlockForHotData[i].Block / BlocksPerChip;
					StripeTable[clusters[i]].Block = CurrentFreeBlockForHotData[i].Block  % BlocksPerChip;
					StripeTable[clusters[i]].Page = CurrentFreeBlockForHotData[i].Page;
					//* advance the number of valid pages in the block
					ValidPageCnt[CurrentFreeBlockForHotData[i].Block]++;
					CurrentFreeBlockForHotData[i].Page++;	//# move to next free page
				}
			}

			//* cluster[i] is stored at current page
			FlashMemory[StripeTable[clusters[i]].Chip*BlocksPerChip+StripeTable[clusters[i]].Block][StripeTable[clusters[i]].Page] = clusters[i];

		}

	}

	//NumCachedClusters = 0;	//# initialize the number of clusters

	//# Statistics
	AccessStatistics.TotalWriteRequestTime += TIME_TRANSFER_PAGE * num + TIME_WRITE_PAGE_NO_TRANSFER; //# the time to transfer data to pages sequentially, and then program them concurrently

}

//* ****************************************************************
//* Erase one block
//* ****************************************************************
void StripeEraseOneBlock(flash_size_t BlockID)
{
	flash_size_t i;
	int pos;				//# the position of the LBA in the LRU list
	int num;				//# the number of checked elements
	Boolean IsInLRUList;	//# whether the LBA in the list or not
	int CurrentChipOfHotData = ChipNumber-1;	//# hot data are stored from the chip with the least erase count
	int CurrentChipOfColdData = 0;			//# cold data are stored from the chip with the largest erase count


	//* Put the selected free block to the free block list
	PutFreeBlock(BlockID);

	//* if current free block is filled up, get a free block from the lsit
	EnsureFreeSpaceInFreeBlock();
	//# clear up cached clusters
	CLEAR_CACHEDCLUSTERS(CachedClusters, NumCachedClusters, ChipNumber);


	//* reclaim the selected block
	for(i=0; i<MaxPage;i++)
	{
 		if( (FlashMemory[BlockID][i] != INVALID_PAGE) &&  (FlashMemory[BlockID][i] != FREE_PAGE) )
		{
			//# for STRIPE_STATIC:
			if(strcmp(FTLMethod, FTL_STRIPE_STATIC)==0)
			{
				if(CachedClusters[FlashMemory[BlockID][i]%ChipNumber] != NO_CLUSTER_CACHED) //# cache the cluster whenever the corresponding chip is empty
				{
					//# flush cached clusters/live-pages out
					StripeWriteOneClusterSet(CachedClusters, NumCachedClusters);
					//* if current free block is filled up, get a free block from the lsit
					EnsureFreeSpaceInFreeBlock();
					//# clear up cached clusters
					CLEAR_CACHEDCLUSTERS(CachedClusters, NumCachedClusters, ChipNumber);
				}

				CachedClusters[FlashMemory[BlockID][i]%ChipNumber] = FlashMemory[BlockID][i]; //# cache the cluster/live-page
			}

			//# for STRIPE_DYNAMIC:
			if(strcmp(FTLMethod, FTL_STRIPE_DYNAMIC)==0)
			{
				if(NumCachedClusters == ChipNumber) //# if the cache is full, flush the cached clusters to the flash memory
				//if(CurrentChipOfHotData < CurrentChipOfColdData) //# if the cache is full, flush the cached clusters to the flash memory
				{
					//# flush cached clusters/live-pages out
					StripeWriteOneClusterSet(CachedClusters, NumCachedClusters);
					//# reset Current chip of hot and cold data
					CurrentChipOfHotData = ChipNumber-1;
					CurrentChipOfColdData = 0;
					//* if current free block is filled up, get a free block from the lsit
					EnsureFreeSpaceInFreeBlock();
					//# clear up cached clusters
					CLEAR_CACHEDCLUSTERS(CachedClusters, NumCachedClusters, ChipNumber);
				}



				IsInLRUList = IsElemenetInLRUList(&HotList, FlashMemory[BlockID][i], &pos, &num);  //# wehether the accessed cluster is a hot data
				//# Statistics
				AccessStatistics.TotalWriteRequestTime += TIME_ACCESS_ONE_LRU_ELEMENT * num;

				if(IsInLRUList) //# hot data
				{
					//# cache the cluster
					CachedClusters[SortedOrder[CurrentChipOfHotData]] = FlashMemory[BlockID][i]; //# put the cluster to cached cluster
					CachedClustersDataType[SortedOrder[CurrentChipOfHotData]] = HotData;
					CurrentChipOfHotData--;
				}
				else //# cold data
				{
					//# cache the cluster
					CachedClusters[SortedOrder[CurrentChipOfColdData]] = FlashMemory[BlockID][i]; //# put the cluster to cached cluster
					CachedClustersDataType[SortedOrder[CurrentChipOfColdData]] = ColdData;
					CurrentChipOfColdData++;
				}

			}


			NumCachedClusters++;	//# advance the number of cached clusters

			//# statistics
			AccessStatistics.TotalLivePageCopyings++;	//* advnace the total number of live-page copyings by 1
			AccessStatistics.TotalWriteRequestTime += (TIME_READ_PAGE); //#the time to read out the live page

		}
	}

	//# if there are some cached live pages
	if(NumCachedClusters > 0)
	{
		//# flush cached clusters/live-pages out
		StripeWriteOneClusterSet(CachedClusters, NumCachedClusters);
		//* if current free block is filled up, get a free block from the lsit
		EnsureFreeSpaceInFreeBlock();
		//# clear up cached clusters
		CLEAR_CACHEDCLUSTERS(CachedClusters, NumCachedClusters, ChipNumber);
	}

	//* Clean up the reclaimed block as a free block
	InvalidPageCnt[BlockID] = 0;
	ValidPageCnt[BlockID] = 0;
	for(i=0; i<MaxPage; i++) FlashMemory[BlockID][i] = FREE_PAGE;
	//* Put the selected free block to the free block list
	//PutFreeBlock(BlockID);

	BlockEraseCnt[BlockID]++;	//* update the erase count
	ChipEraseCnt[BlockID%ChipNumber]++; //# update the erase count of the corresponding chip: for dynamic wear leveling
	//# sort by the number of block erares in non-increasing order
	for(i=1;i<ChipNumber;i++)
	{
		flash_size_t tmp;
		if(ChipEraseCnt[SortedOrder[i]] > ChipEraseCnt[SortedOrder[i-1]])
		{
			//# Swap the order
			tmp = SortedOrder[i];
			SortedOrder[i] = SortedOrder[i-1];
			SortedOrder[i-1] = tmp;
		}
	}


	//# Update the max block erase count
	if(BlockEraseCnt[BlockID]>MaxBlockEraseCnt) MaxBlockEraseCnt = BlockEraseCnt[BlockID];

	//# statistics
	AccessStatistics.TotalBlockEraseCount++;		//* update total number of block erases
	AccessStatistics.TotalWriteRequestTime += TIME_ERASE_BLOCK; //# the time to erase a block

}


//# ****************************************************************
//# Do garbage collection to reclaim one more free blocks if there is no free block in the free block list
//# dynamic wear leveling: using cost-benefit strategy for its GC->
//# (free page=0, live page=-1, dead page = 1) while the sum over every page is larger than 0, erase the block.
//# "chip" is the chip ID where we want to reclaim invalid space
//# ****************************************************************
static void StripeGarbageCollection(flash_size_t chip)
{
	flash_size_t i;
	static Boolean IsCurrentScanningBlockidInitialized = False;	//# to memorize whether the CurrentScanningBlockid has been initialied or not
	static flash_size_t *CurrentScanningBlockID;		//* Record the Id of the to-be-scanned block where the ID is the offset to the first block of its chip
	static flash_size_t *InvalidPage2ValidPageDivider;	//# The ratio of invalid pages to valid pages

	//# for the first time, we allocate memorpy space for the "CurrentScanningBlockID"
	if(IsCurrentScanningBlockidInitialized == False)
	{
		IsCurrentScanningBlockidInitialized = True;
		CurrentScanningBlockID = (flash_size_t *)malloc(sizeof(flash_size_t)*ChipNumber);
		memset(CurrentScanningBlockID,-1,sizeof(flash_size_t)*ChipNumber);
		InvalidPage2ValidPageDivider = (flash_size_t *)malloc(sizeof(flash_size_t)*ChipNumber);
		for(i=0;i<ChipNumber;i++) InvalidPage2ValidPageDivider[i] = 1;
	}


	//* Keep reclaim blocks until there is one more free block in the free block list
	while(CheckFreeBlockFromChip(chip)==-1)
	{
		//* find out the block with the largest number of invalid pages
 		for(i=0; i<BlocksPerChip; i++)
		{
			CurrentScanningBlockID[chip] = (CurrentScanningBlockID[chip] + 1) % BlocksPerChip;	//# advance to the next block

			//# statistics
			AccessStatistics.TotalWriteRequestTime += TIME_READ_SPARE_PAGE * MaxPage; //# the time to check the spare area of pages in one block

			//# a victim is found
			if( (InvalidPageCnt[chip*BlocksPerChip+CurrentScanningBlockID[chip]] > ValidPageCnt[chip*BlocksPerChip+CurrentScanningBlockID[chip]]/InvalidPage2ValidPageDivider[chip]) && \
				(FreeBlockList[chip*BlocksPerChip+CurrentScanningBlockID[chip]] == FBL_NOT_IN_LIST) && \
				((chip*BlocksPerChip+CurrentScanningBlockID[chip])/BlocksPerChip == chip) && \
				((chip*BlocksPerChip+CurrentScanningBlockID[chip]) != CurrentFreeBlock[chip].Block) ) //* encounter a block that has more invalid pages than valid pages
			{

				//# for STRIPE_STATIC:
				if(strcmp(FTLMethod, FTL_STRIPE_STATIC)==0)
					break;	//# break when the victim block is found

				//# for STRIPE_DYNAMIC:
				if(strcmp(FTLMethod, FTL_STRIPE_DYNAMIC)==0)
					if((chip*BlocksPerChip+CurrentScanningBlockID[chip]) != CurrentFreeBlockForHotData[chip].Block)
						break;
			}

		} //# end of for

		if(i==BlocksPerChip)
		{
			InvalidPage2ValidPageDivider[chip] *= 2;	//# lower the ratio whenever we can't find a victim (due to the cost-benefit function)
			printf("chip=%d, InvalidPage2ValidPageDivider=%d\n", chip, InvalidPage2ValidPageDivider[chip]);
		}
		else
			StripeEraseOneBlock(chip*BlocksPerChip+CurrentScanningBlockID[chip]);	//* Erase one block and also update the BET and counters


	} //* end of while()
}


//* ****************************************************************
//* Ensure there is free space in each free block. This might trigger the garbage collection
//* ****************************************************************
static void EnsureFreeSpaceInFreeBlock(void)
{
	flash_size_t i;

	//* Curret block is filled up. Therefore, we have to get another free block from the free block list.
	for(i=0;i<ChipNumber;i++)
	{
		while(CurrentFreeBlock[i].Page >= MaxPage)
		{
			if(CheckFreeBlockFromChip(i)== -1)	//* get one free block, if the block is not free, do garbage collection to reclaim one more free block
			{
				StripeGarbageCollection(i);	//* Do garbage collection to reclaim one more free block if there is no free block in the free block list
			}
			else
			{
				CurrentFreeBlock[i].Block=GetFreeBlockFromChip(i); //# Get one free block form its chip
				CurrentFreeBlock[i].Page =  0;	//* the free block should start from page 0 to store data

//				//# DEBUG -> this occurs whenever there is not enough space to store more LBAs in this chip, so that we have to adjust the errors
//
//				{
//					int j;
//					for(j=0;j<MaxPage;j++)
//					{
//						if(FlashMemory[CurrentFreeBlock[i].Block][j] != FREE_PAGE)
//						{
//							FlashMemory[CurrentFreeBlock[i].Block][j] = FREE_PAGE;
//							//printf("(%d, %d) not free\n",CurrentFreeBlock[i].Block,j);
//						}
//					}
//					ValidPageCnt[CurrentFreeBlock[i].Block] = 0;
//					InvalidPageCnt[CurrentFreeBlock[i].Block] = 0;
//				}

			}
		}
	}

	//# for STRIPE_DYNAMIC:
	if(strcmp(FTLMethod, FTL_STRIPE_DYNAMIC)==0)
	{
		for(i=0;i<ChipNumber;i++)
		{
			while(CurrentFreeBlockForHotData[i].Page >= MaxPage)
			{
				if(CheckFreeBlockFromChip(i)== -1)	//* get one free block, if the block is not free, do garbage collection to reclaim one more free block
				{
					StripeGarbageCollection(i);	//* Do garbage collection to reclaim one more free block if there is no free block in the free block list
				}
				else
				{
					CurrentFreeBlockForHotData[i].Block=GetFreeBlockFromChip(i); //# Get one free block form its chip
					CurrentFreeBlockForHotData[i].Page =  0;	//* the free block should start from page 0 to store data

//					//# DEBUG -> this occurs whenever there is not enough space to store more LBAs in this chip, so that we have to adjust the errors
//					{
//						int j;
//						for(j=0;j<MaxPage;j++)
//						{
//							if(FlashMemory[CurrentFreeBlock[i].Block][j] != FREE_PAGE)
//							{
//								FlashMemory[CurrentFreeBlock[i].Block][j] = FREE_PAGE;
//								//printf("(%d, %d) not free\n",CurrentFreeBlock[i].Block,j);
//							}
//						}
//						ValidPageCnt[CurrentFreeBlock[i].Block] = 0;
//						InvalidPageCnt[CurrentFreeBlock[i].Block] = 0;
//					}

				}

			}
		}
	}
}

//* ****************************************************************
//* Proceed a write request and also update the FTL table.
//* "request" is the write request that is going to be updated or written
//* ****************************************************************
static void SecureStripeWriteOneRequest(OneRequest_t request)
{
	int i;
	int pos;				//# the position of the LBA in the LRU list
	int num;				//# the number of checked elements
	Boolean IsInLRUList;	//# whether the LBA in the list or not
	int CurrentChipOfHotData = ChipNumber-1;	//# hot data are stored from the chip with the least erase count
	int CurrentChipOfColdData = 0;			//# cold data are stored from the chip with the largest erase count

	//# Ensure there is free space in each free block
	EnsureFreeSpaceInFreeBlock();

	//# clear up cached clusters
	CLEAR_CACHEDCLUSTERS(CachedClusters, NumCachedClusters, ChipNumber);

	while(request.Length > 0) //# write one cluster set in each iteration
	{
		//# Ensure there is free space in each free block
		EnsureFreeSpaceInFreeBlock();

		//# for STRIPE_STATIC:
		if(strcmp(FTLMethod, FTL_STRIPE_STATIC)==0)
		{
			if(request.Length>=ChipNumber)
				NumCachedClusters = ChipNumber;
			else
				NumCachedClusters = request.Length;
			//# write one cluster set
			for(i=0;i<NumCachedClusters;i++) CachedClusters[(request.StartCluster + i)%ChipNumber] = request.StartCluster + i;
			request.StartCluster += NumCachedClusters;	//# advance the start cluster
			request.Length -= NumCachedClusters;		//# decrase the remaning cluster
			StripeWriteOneClusterSet(CachedClusters, NumCachedClusters);	//* update the address translation information and write current cluster set
			//# Ensure there is free space in each free block
			EnsureFreeSpaceInFreeBlock();
			//# clear up cached clusters
			CLEAR_CACHEDCLUSTERS(CachedClusters, NumCachedClusters, ChipNumber);
		}

		//# for STRIPE_DYNAMIC:
		if(strcmp(FTLMethod, FTL_STRIPE_DYNAMIC)==0)
		{
			IsInLRUList = IsElemenetInLRUList(&HotList, request.StartCluster, &pos, &num);
			//# Statistics
			AccessStatistics.TotalWriteRequestTime += TIME_ACCESS_ONE_LRU_ELEMENT * num;

			if(IsInLRUList == True) //# hot data
			{
				RemoveElemenetFromLRUList(&HotList, request.StartCluster, &pos);
				//# update the HotList
				PutElemenetToLRUList(&HotList, request.StartCluster, &pos);
				//# cache the cluster
				CachedClusters[SortedOrder[CurrentChipOfHotData]] = request.StartCluster; //# put the cluster to cached cluster
				CachedClustersDataType[SortedOrder[CurrentChipOfHotData]] = HotData;		//# Set the type of data of cached cluster
				CurrentChipOfHotData--;
			}
			else //# not hot data -> cold data
			{
				IsInLRUList = IsElemenetInLRUList(&CandidateList, request.StartCluster, &pos, &num);
				//# Statistics
				AccessStatistics.TotalWriteRequestTime += TIME_ACCESS_ONE_LRU_ELEMENT * num;

				if(IsInLRUList == True) //# in the candidate list, but still cold data
				{
					//# Move the LBA from candidate data list to the hot data list
					RemoveElemenetFromLRUList(&CandidateList, request.StartCluster, &pos);
					PutElemenetToLRUList(&HotList, request.StartCluster, &pos);
				}
				else //# cold data: in neither hot data list nor candidate data list
				{
					PutElemenetToLRUList(&CandidateList, request.StartCluster, &pos);
				}
				//# cache the cluster
				CachedClusters[SortedOrder[CurrentChipOfColdData]] = request.StartCluster; //# put the cluster to cached cluster
				CachedClustersDataType[SortedOrder[CurrentChipOfColdData]] = ColdData;		//# Set the type of data of cached cluster
				CurrentChipOfColdData++;
			}

			NumCachedClusters++;	//# advance the number of cached data
			//# advance to the next cluster of the request
			request.StartCluster++;
			request.Length--;

			if(NumCachedClusters == ChipNumber || request.Length==0)
			{
				StripeWriteOneClusterSet(CachedClusters, NumCachedClusters);	//* update the address translation information and write current cluster set
				//# reset Current chip of hot and cold data
				CurrentChipOfHotData = ChipNumber-1;
				CurrentChipOfColdData = 0;
				//# Ensure there is free space in each free block
				EnsureFreeSpaceInFreeBlock();
				//# clear up cached clusters
				CLEAR_CACHEDCLUSTERS(CachedClusters, NumCachedClusters, ChipNumber);
			}
		}


		//# Ensure there is free space in each free block
		EnsureFreeSpaceInFreeBlock();
	}


}


//* ****************************************************************
//* Simulate Li-Ping Chang's STRIPE method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void StripeSimulation(FILE *fp)
{
	flash_size_t i,j;
//	flash_size_t CurrentFreeBlockID;	//* Point to current free block ID
//	flash_size_t CurrentFreePageID;	//* Point to current free page ID
	int BlockMaxRandomValue;	//* The maximal random value, which allows us to select a block storing initial data
	//int PageMaxRandomValue;		//* The maximal random value, which allows us to select a page of thte selected block

	//# ************************** memory allocations ******************************
	//* Initialize Stripe table
	StripeTable = (StripeElement_t *)malloc(sizeof(StripeElement_t)*MaxCluster);	//* allocate memory space for the FTL table
	memset(StripeTable,STRIPE_POINT_TO_NULL,sizeof(StripeElement_t)*MaxCluster);	//* initialize the content of this table

	//# Initialize the free block list of each chip (for STRiPE_STATIC or cold data of STRIPE_DYNAMIC)
	CurrentFreeBlock = (StripeFreeBlockElement_t *)malloc(sizeof(StripeFreeBlockElement_t)*ChipNumber);	//* allocate memory space for the space to keep the free current free block and current free page of each chip
	memset(CurrentFreeBlock,STRIPE_POINT_TO_NULL,sizeof(StripeFreeBlockElement_t)*ChipNumber);	//* initialize the content of this table
	//# only for STRIPE_DYNAMIC: Initialize the free block list of each chip (for hot data in STRIPE_DYNAMIC)
	CurrentFreeBlockForHotData = (StripeFreeBlockElement_t *)malloc(sizeof(StripeFreeBlockElement_t)*ChipNumber);	//* allocate memory space for the space to keep the free current free block and current free page of each chip
	memset(CurrentFreeBlockForHotData,STRIPE_POINT_TO_NULL,sizeof(StripeFreeBlockElement_t)*ChipNumber);	//* initialize the content of this table


	//# Initialize the cluster/live-page cache
	CachedClusters = (flash_size_t *)malloc(sizeof(flash_size_t)*ChipNumber);
	CachedClustersDataType = (DataType_t *)malloc(sizeof(DataType_t)*ChipNumber);
	NumCachedClusters = 0;

	//# Initialize the number of block erases of each chip
	ChipEraseCnt = (flash_huge_size_t *)malloc(sizeof(flash_huge_size_t)*ChipNumber);
	memset(ChipEraseCnt,0,sizeof(flash_huge_size_t)*ChipNumber);	//* initialize the erase count of each chip
	//# Initialize the array to stored the order of the block erases
	SortedOrder = (flash_size_t *)malloc(sizeof(flash_size_t)*ChipNumber);
	for(i=0;i<ChipNumber;i++) SortedOrder[i]=i;	//* initialize the array to stored the order of the block erases


	//# only for STRIPE_DYNAMIC: initialize the LRULists (HotLRUList[] and CandidateLRUList[])
	CreateLRUList(&HotList, HOTLIST_LENGTH); //# create and initialize the LRU lsit for hot LBAs
	CreateLRUList(&CandidateList, CANDIDATELIST_LENGTH);	//# create and initialize the LRU lsit for candidate LBAs


	///# ************************** end of memory allocations ******************************


	//# Get the first free block
	for(i=0;i<ChipNumber;i++)
	{
		CurrentFreeBlock[i].Block = GetFreeBlockFromChip(i);
		CurrentFreeBlock[i].Page = 0;
	}

	//* Randomly select where are the data stored in the flash memory initially
	//* according the parameter "InitialPercentage" to determine what's the percentage of data in ths flash memory initially.
	BlockMaxRandomValue = (int)((float)InitialPercentage * (RAND_MAX) / 100);	//* The max random number equals "InitialPercentage" percentage of data.
	for(i=0; i<(MaxBlock-LeastFreeBlocksInFBL()-BadBlockCnt); i++)
	{
		//if(i%100==0) printf("%8d",i);
		if( rand() <= BlockMaxRandomValue)
		{
			//# setup the write request
			CurrentRequest.AccessType = WriteType;
			CurrentRequest.StartCluster = i*MaxPage;
			CurrentRequest.Length = MaxPage;
			//# perform the write request
			SecureStripeWriteOneRequest(CurrentRequest);
			//# Accumulate the total number of accessed pages
			for(j=0;j<MaxPage;j++)
			{
				//# Update the number of accessed logical pages
				AccessedLogicalPages++;	//# advanced the number of set flags in the map
				AccessedLogicalPageMap[i*MaxPage+j] = True; //# Set the flag on
			}
		} // end of if
	}
	//# Log information
	printf("Initial Percentage of data: %.02f%%\n",(float)AccessedLogicalPages/(MaxPage*(MaxBlock))*100);


	//# reset statictic variables
	ResetStatisticVariable(&AccessStatistics);

	//* ******************************* Start simulation ***************************

	//# only for STRIPE_DYNAMIC: Get the first free block for storing hot data
	if(strcmp(FTLMethod, FTL_STRIPE_DYNAMIC)==0) //# for STRIPE_DYNAMIC:
	{
		for(i=0;i<ChipNumber;i++)
		{
			CurrentFreeBlockForHotData[i].Block = GetFreeBlockFromChip(i);
			CurrentFreeBlockForHotData[i].Page = 0;
		}
	}

	//* fetch one operation in each iteration
	while(GetOneOperation(fp, &CurrentRequest.AccessType, &CurrentRequest.StartCluster, &CurrentRequest.Length, False))
	{
		if(CurrentRequest.AccessType == WriteType)
		{
			SecureStripeWriteOneRequest(CurrentRequest);	//* Write a request to the flash memory, and also update the FTL table.
			//# Statistics
			AccessStatistics.TotalWriteRequestCount ++;	//# advance the number of total write requests
		}
		else if(CurrentRequest.AccessType == ReadType)
		{
			//* proceed a read request and also update the FTL table.
			if(StripeReadOneRequest(CurrentRequest) != False)
			{
				AccessStatistics.TotalReadRequestCount ++;	//# advance the number of total read requests
			}
		}


	}	//* end of while
	//* ******************************* End of simulation *****************************

	free(StripeTable);	//* release memory space
	free(CurrentFreeBlock); //# release memory space
	free(CurrentFreeBlockForHotData);
	free(CachedClusters); //# release memory space
	free(CachedClustersDataType);
	free(ChipEraseCnt);
	free(SortedOrder);
	//# only for STRIPE_DYNAMIC: initialize the LRULists (HotLRUList[] and CandidateLRUList[])
	FreeLRUList(&HotList); //# create and initialize the LRU lsit for hot LBAs
	FreeLRUList(&CandidateList);	//# create and initialize the LRU lsit for candidate LBAs
}




