//* NFTL.c : All the simulation for NFTL type 2 are defined in this file
//* dynamic wear leveling: using cost-benefit function for its GC


#ifndef _NFTL_C
#define _NFTL_C
#endif


#include "main.h"
#include "NFTL.h"


//* *************** global variabl *******************
static NFTLElement_t *NFTLTable;	//* NFTL table



//* ****************************************************************
//* Read a cluster from the flash memory
//* "ClusterID" is the Cluster that is going to be updated or written
//* Return value: If the "ClusterID" exists, return True. Otherwise, return False.
//* *****************************************************************
static Boolean NFTLReadOneCluster(flash_size_t ClusterID)
{
	flash_size_t i;
	flash_size_t LogicalBlock;	//* the logical block ID of one cluster
	flash_size_t LogicalPage;	//* The logical page offset in a logical block of one cluster
	flash_size_t TimeToSearchAccessedCluster = 0;	//# the time to read a cluster

	//* Convert a Cluster ID into its logical block ID and page offset in that logical block
	LogicalBlock = ClusterID / MaxPage;
	LogicalPage = ClusterID % MaxPage;

	//# check whether the corresponding primary block exists
	if(NFTLTable[LogicalBlock].PrimaryBlcok == NFTL_POINT_TO_NULL)
	{
		return(False);
	}
	else
	{
		//# **************************** for MLC ***********************************
		//# First, search the replacement block from the its last page. If failed, check the page at the corresponding offset of primary block
		i=-1; //# initialize to the status of not found
		//# scan the replacement block
		if(NFTLTable[LogicalBlock].ReplacementBlock != NFTL_POINT_TO_NULL)
		{
			//# search the repmacelment block
			for(i=NFTLTable[LogicalBlock].CurrentFreePageIDinRB-1; i>=0; i--)
			{
				//# Staticstics
				TimeToSearchAccessedCluster += TIME_READ_SPARE_PAGE; //# the time to check the spare area of one page

				if(FlashMemory[NFTLTable[LogicalBlock].ReplacementBlock][i]==ClusterID)
					break;	//* exit when found
			}
		}
		//# not found in the replacement block, so that we check the primary block
		if(i<0)	//* find the data of ClusterID in the replacement block
		{
			//# Staticstics
			TimeToSearchAccessedCluster += TIME_READ_SPARE_PAGE; //# the time to check the spare area of one page

			if(FlashMemory[NFTLTable[LogicalBlock].PrimaryBlcok][LogicalPage] != ClusterID)
			{
				return False; //# the to-be-accessed "ClusterID" (LBA) is not found in the primary block
			}

		}
		//# **************************** end of for MLC ***********************************

		//# **************************** for SLC ***********************************
		//# First, check the page at the corresponding offset of primary block. If failed, search the replacement block from the its last page
/*		TimeToSearchAccessedCluster += TIME_READ_SPARE_PAGE; //# the time to check the spare area of one page

		if(FlashMemory[NFTLTable[LogicalBlock].PrimaryBlcok][LogicalPage] != ClusterID)
		{
			//# search the repmacelment block
			for(i=NFTLTable[LogicalBlock].CurrentFreePageIDinRB-1; i>=0; i--)
			{
				//# Staticstics
				TimeToSearchAccessedCluster += TIME_READ_SPARE_PAGE; //# the time to check the spare area of one page

				if(FlashMemory[NFTLTable[LogicalBlock].ReplacementBlock][i]==ClusterID)
				{
					break;	// exit when found
				}
			}
			if(i<0) return(False);
		}
		//# **************************** end of for SLC ***********************************
*/

	} //# end of if

	//# Staticstics
	AccessStatistics.TotalReadRequestTime += TimeToSearchAccessedCluster; //# the time to search the pages, so as to find the to-be-accessed cluster
	AccessStatistics.TotalReadRequestTime += TIME_READ_PAGE; //# the time to read one page

	return True;
}



//* ****************************************************************
//* Merge the primary block and the replacement block together
//* "MergeBlockSet" is the logical block entry that we are going to merge
//* ****************************************************************
static void NFTLMergeBlocks(flash_size_t MergeBlockSet)
{
	flash_size_t i;
	flash_size_t FreeBlock;	//* the free block just retrieved from the free block list
	flash_size_t CurrentFreePageIDinPB = 0;	//# to keep the first free block in the primary block

	//* *** Add the primary block and the replacement block into the free block list ***
	PutFreeBlock(NFTLTable[MergeBlockSet].PrimaryBlcok);
	PutFreeBlock(NFTLTable[MergeBlockSet].ReplacementBlock);

	//* Retrieve one free block from the free block list
	FreeBlock = GetFreeBlock(False);

	//* Move the live-pages from the primary and replacement blocks to the new free block
	for(i=0; i<MaxPage; i++)
	{
		//* Move one page from the primary block to the new block
		if((FlashMemory[NFTLTable[MergeBlockSet].PrimaryBlcok][i] != INVALID_PAGE) && (FlashMemory[NFTLTable[MergeBlockSet].PrimaryBlcok][i] != FREE_PAGE))
		{
			FlashMemory[FreeBlock][i] = FlashMemory[NFTLTable[MergeBlockSet].PrimaryBlcok][i];
			ValidPageCnt[FreeBlock]++; //# advance the number of valid pages

			ValidPageCnt[NFTLTable[MergeBlockSet].PrimaryBlcok]--; //# decrease the number of valid pages
			InvalidPageCnt[NFTLTable[MergeBlockSet].PrimaryBlcok]++; //# increase the number of invalid pages

			if(CurrentFreePageIDinPB <= i) CurrentFreePageIDinPB = i+1; //# update the first free page in the primary block
			//Statistics;
			AccessStatistics.TotalLivePageCopyings++;	//* Adance the total live-page copyings by 1.
			AccessStatistics.TotalWriteRequestTime += TIME_READ_PAGE+TIME_WRITE_PAGE; //#the time to copy a live page
		}
		//* Move one page from the replacement block to the new block
		if((FlashMemory[NFTLTable[MergeBlockSet].ReplacementBlock][i] != INVALID_PAGE) && (FlashMemory[NFTLTable[MergeBlockSet].ReplacementBlock][i] != FREE_PAGE))
		{
			FlashMemory[FreeBlock][FlashMemory[NFTLTable[MergeBlockSet].ReplacementBlock][i]%MaxPage] = FlashMemory[NFTLTable[MergeBlockSet].ReplacementBlock][i];
			ValidPageCnt[FreeBlock]++; //# advance the number of valid pages

			ValidPageCnt[NFTLTable[MergeBlockSet].ReplacementBlock]--; //# decrease the number of valid pages
			InvalidPageCnt[NFTLTable[MergeBlockSet].ReplacementBlock]++; //# increase the number of invalid pages

			if(CurrentFreePageIDinPB <= (FlashMemory[NFTLTable[MergeBlockSet].ReplacementBlock][i]%MaxPage)) CurrentFreePageIDinPB = (FlashMemory[NFTLTable[MergeBlockSet].ReplacementBlock][i]%MaxPage) + 1; //# update the first free page in the primary block
			//Statistics;
			AccessStatistics.TotalLivePageCopyings++;	//* Adance the total live-page copyings by 1.
			AccessStatistics.TotalWriteRequestTime += TIME_READ_PAGE+TIME_WRITE_PAGE; //#the time to copy a live page
		}
	}



	//* *** Clean up the reclaimed block as a free block ***
	//* reset invalid page count
	InvalidPageCnt[NFTLTable[MergeBlockSet].PrimaryBlcok] = 0;
	InvalidPageCnt[NFTLTable[MergeBlockSet].ReplacementBlock] = 0;
	ValidPageCnt[NFTLTable[MergeBlockSet].PrimaryBlcok] = 0;
	ValidPageCnt[NFTLTable[MergeBlockSet].ReplacementBlock] = 0;
	//* Clear pages as free pages
	for(i=0; i<MaxPage; i++)
	{
		FlashMemory[NFTLTable[MergeBlockSet].PrimaryBlcok][i] = FREE_PAGE;
		FlashMemory[NFTLTable[MergeBlockSet].ReplacementBlock][i] = FREE_PAGE;
	}

	//# Statistics
	AccessStatistics.TotalBlockEraseCount += 2;
	AccessStatistics.TotalWriteRequestTime += TIME_ERASE_BLOCK * 2; //#the time to copy a live page

	//* update the erase count
	BlockEraseCnt[NFTLTable[MergeBlockSet].PrimaryBlcok]++;
	//# Update the max block erase count
	if(BlockEraseCnt[NFTLTable[MergeBlockSet].PrimaryBlcok]>MaxBlockEraseCnt) MaxBlockEraseCnt = BlockEraseCnt[NFTLTable[MergeBlockSet].PrimaryBlcok];

	BlockEraseCnt[NFTLTable[MergeBlockSet].ReplacementBlock]++;
	//# Update the max block erase count
	if(BlockEraseCnt[NFTLTable[MergeBlockSet].ReplacementBlock]>MaxBlockEraseCnt) MaxBlockEraseCnt = BlockEraseCnt[NFTLTable[MergeBlockSet].ReplacementBlock];
	//* update total number of block erases


	//* Update the NFTL Table
	NFTLTable[MergeBlockSet].PrimaryBlcok = FreeBlock;
	NFTLTable[MergeBlockSet].ReplacementBlock = NFTL_POINT_TO_NULL;
	NFTLTable[MergeBlockSet].CurrentFreePageIDinPB = CurrentFreePageIDinPB;
	NFTLTable[MergeBlockSet].CurrentFreePageIDinRB = NFTL_POINT_TO_NULL;


}

//* ****************************************************************
//* Do garbage collection to reclaim one more free block if there is no free block in the free block list
//# dynamic wear leveling: using cost-benefit strategy for its GC-> 
//# (free page=0, live page=-1, dead page = 1) while the sum over every page is larger than 0, erase the block.
//* "NotToReclaimBlockSet" is the block set that we can not reclaim
//* ****************************************************************
static void NFTLGarbageCollection(flash_size_t NotToReclaimBlockSet)
{
	flash_size_t i;
	static flash_size_t CurrentScanningBlockSetID=-1;	//* Keep current block that is going to be scaneed.
	static flash_size_t InvalidPage2ValidPageDivider=1;	//# The ratio of invalid pages to valid pages


#ifdef _JOHNSON_DEBUG
	printf("\nEntering NFTLGarbageCollection()");
#endif


	//MaxInvalidPages = 0;	//* reset the max number of invalid pages

	//StartBlockSetID = (MaxInvalidBlockSetID+1) % MaxBlock;	//* The starting block that is going to be scanned is the one next to the block selected last time

	//* find out the block with the largest number of invalid pages
	for(i=0; i<MaxBlock; i++)
	{
		CurrentScanningBlockSetID = (CurrentScanningBlockSetID + 1) % MaxBlock;	//# advance to the next block

		//# statistics
		AccessStatistics.TotalWriteRequestTime += TIME_READ_SPARE_PAGE * MaxPage * 2; //# the time to check the spare area of pages in one block

		if( ( (InvalidPageCnt[NFTLTable[CurrentScanningBlockSetID].PrimaryBlcok] > ValidPageCnt[NFTLTable[CurrentScanningBlockSetID].PrimaryBlcok]/InvalidPage2ValidPageDivider) || \
			(InvalidPageCnt[NFTLTable[CurrentScanningBlockSetID].ReplacementBlock] > ValidPageCnt[NFTLTable[CurrentScanningBlockSetID].ReplacementBlock]/InvalidPage2ValidPageDivider) )  && \
			(NFTLTable[CurrentScanningBlockSetID].PrimaryBlcok != NFTL_POINT_TO_NULL) && \
			(NFTLTable[CurrentScanningBlockSetID].ReplacementBlock != NFTL_POINT_TO_NULL) && \
			(CurrentScanningBlockSetID != NotToReclaimBlockSet) )	//* encounter a block that has more invalid pages than valid pages
		{
			break;	//# victim block is found
		}

	}

//	printf("NFTLGarbageCollection\n");
	if(i==MaxBlock) //# the cost-benefit function can't find the victim block set, so that we use the block set that has the larget weight
	{
		InvalidPage2ValidPageDivider *= 2;	//# lower the ratio whenever we can't find a victim (due to the cost-benefit function)
		printf("InvalidPage2ValidPageDivider=%d\n", InvalidPage2ValidPageDivider); 

	}
	else
	{
		NFTLMergeBlocks(CurrentScanningBlockSetID);	//* Erase one block and also update the BET and counters
	}



#ifdef _JOHNSON_DEBUG
	printf("\nLeaving NFTLGarbageCollection()");
#endif
}


//* ****************************************************************
//* Write a cluster to the flash memory and also update the NFTL table.
//* "ClusterID" is the Cluster that is going to be updated or written
//* ****************************************************************
static void NFTLWriteOneCluster(flash_size_t ClusterID)
{
	flash_size_t i;
	flash_size_t FreeBlock;
	flash_size_t LogicalBlock;	//* the logical block ID of one cluster
	flash_size_t LogicalPage;	//* The logical page offset in a logical block of one cluster


#ifdef _JOHNSON_DEBUG
	printf("\nEntering NFTLWriteOneCluster()");
#endif

	//* Convert a Cluster ID into its logical block ID and page offset in that logical block
	LogicalBlock = ClusterID / MaxPage;
	LogicalPage = ClusterID % MaxPage;

	//* ** => Note that 1 cluster = 1 page
	//* No primary block for this Cluster
	if(NFTLTable[LogicalBlock].PrimaryBlcok == NFTL_POINT_TO_NULL)
	{
		while((FreeBlock=GetFreeBlock(False)) == -1)
		{
			NFTLGarbageCollection(LogicalBlock);	//* Do garbage collection to reclaim one more free block if there is no free block in the free block list
		}
		NFTLTable[LogicalBlock].PrimaryBlcok = FreeBlock;	//* assign the free block to current logical block
		NFTLTable[LogicalBlock].CurrentFreePageIDinPB = 0;			//* Write data to the replacement block from its first page
	}

	//* *** Write "ClusterID" into its corresponding page --> we consider MLC flash memory
	//* There are three cases:	Case 1) Write to primary block
	//*							Case 2) An update for ClusterID, which is currently in the primary block
	//*							Case 3) An update for ClusterID, which is currently in the replacement block
	//#                         Case 4) An update for ClusterID, but it can't be written in the primary block (due to the write constraints of MLC), and we are not sure whether it is in the replacement block
	//* *** CASE 1
	if((FlashMemory[NFTLTable[LogicalBlock].PrimaryBlcok][LogicalPage] == FREE_PAGE) && (NFTLTable[LogicalBlock].CurrentFreePageIDinPB <= LogicalPage) ) //* free page in primary block and the latest written page is above the to-be-writtn page
	{
		FlashMemory[NFTLTable[LogicalBlock].PrimaryBlcok][LogicalPage] = ClusterID;
		NFTLTable[LogicalBlock].CurrentFreePageIDinPB = LogicalPage+1; //# Update the CurrentFreePageIDinPB
		ValidPageCnt[NFTLTable[LogicalBlock].PrimaryBlcok]++;	//* advance the number of valid pages in the primary block
		//# Statistics
		AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE; //# the time to write one page
	}
	else
	{
		//* this while loop is to make sure that there is free page in the replacement blcok to store the new cluster.
		while(1)
		{
			//* Check whether the replacement block exists or not
			if(NFTLTable[LogicalBlock].ReplacementBlock == NFTL_POINT_TO_NULL)
			{
				while((FreeBlock=GetFreeBlock(False)) == -1)
				{
					NFTLGarbageCollection(LogicalBlock);	//* Do garbage collection to reclaim one more free block if there is no free block in the free block list
				}
				NFTLTable[LogicalBlock].ReplacementBlock = FreeBlock;	//* assign the free block to current logical block
				NFTLTable[LogicalBlock].CurrentFreePageIDinRB = 0;			//* Write data to the replacement block from its first page
			}

			//* if the replacement block is full, merge them and also allocate one replacement block for it
			if(NFTLTable[LogicalBlock].CurrentFreePageIDinRB >= MaxPage)	//* The replacement block is full -> Merge the primary block and replacement block
			{
				if(NFTLTable[LogicalBlock].CurrentFreePageIDinRB>MaxPage)	//* Error free page ID
				{
					printf("ERROR: Current free page ID %d is over the number of pages in a block\n",NFTLTable[LogicalBlock].CurrentFreePageIDinRB);
					exit(1);
				}
				//# statistics
				AccessStatistics.TotalWriteRequestTime += TIME_READ_SPARE_PAGE * MaxPage * 2; //# the time to check the spare area of pages in one block
				//* Merge the primary block and replacement block
				//printf("NFTLWriteOneCluster\n");
				NFTLMergeBlocks(LogicalBlock);
			}
			else
			{
				break;	//* got a replacement block with free page
			}
		}	//* end of while()

		//* *** CASE 2
		if( FlashMemory[NFTLTable[LogicalBlock].PrimaryBlcok][LogicalPage] == ClusterID )	//* This Cluster is stored in the primary block
		{
			FlashMemory[NFTLTable[LogicalBlock].PrimaryBlcok][LogicalPage] = INVALID_PAGE;	//* Mark as an invalid page
			InvalidPageCnt[NFTLTable[LogicalBlock].PrimaryBlcok]++;	//* advance the number of invalid pages in the primary block
			ValidPageCnt[NFTLTable[LogicalBlock].PrimaryBlcok]--;	//* decrease the number of valid pages in the primary block
			FlashMemory[NFTLTable[LogicalBlock].ReplacementBlock][NFTLTable[LogicalBlock].CurrentFreePageIDinRB] = ClusterID; //# write data to current page
			ValidPageCnt[NFTLTable[LogicalBlock].ReplacementBlock]++;	//* increase the number of valid pages in the replacement block
			NFTLTable[LogicalBlock].CurrentFreePageIDinRB++;	//* Move to the first free page in the replacement block
			//# Statistics
			AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE; //# the time to write one page
		}

		//* *** CASE 3
		else if(FlashMemory[NFTLTable[LogicalBlock].PrimaryBlcok][LogicalPage] == INVALID_PAGE)	//* invalid page in primary block
		{
			//* scan where is the Cluster data stored
			for(i=NFTLTable[LogicalBlock].CurrentFreePageIDinRB-1; i>=0; i--)
			{
				if(FlashMemory[NFTLTable[LogicalBlock].ReplacementBlock][i]==ClusterID)
					break;	//* exit when founded
			}

			if(i<0)	//* error checking: Can't find the data of ClusterID in the replacement block
			{
				printf("ERROR: The data of Cluster %d should be stored in the replacement block, but actually not.\n",ClusterID);
				exit(1);
			}
			else	//* found
			{
				//# update data stored in the replacement block
				FlashMemory[NFTLTable[LogicalBlock].ReplacementBlock][i] = INVALID_PAGE;	//* change it as an invalid page
				InvalidPageCnt[NFTLTable[LogicalBlock].ReplacementBlock]++;	//* increase the invalid page count
				FlashMemory[NFTLTable[LogicalBlock].ReplacementBlock][NFTLTable[LogicalBlock].CurrentFreePageIDinRB] = ClusterID;	//* write the data of the ClusterID to this free page.
				NFTLTable[LogicalBlock].CurrentFreePageIDinRB++;	//* move to the next free page
				//# Statistics
				AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE; //# the time to write one page
			}
		}
		//# *** CASE 4
		else if ((FlashMemory[NFTLTable[LogicalBlock].PrimaryBlcok][LogicalPage] == FREE_PAGE) && (NFTLTable[LogicalBlock].CurrentFreePageIDinPB > LogicalPage))
		{
			//* scan where is the Cluster data stored
			for(i=NFTLTable[LogicalBlock].CurrentFreePageIDinRB-1; i>=0; i--)
			{
				if(FlashMemory[NFTLTable[LogicalBlock].ReplacementBlock][i]==ClusterID)
					break;	//* exit when founded
			}

			if(i>=0)	//* find the data of ClusterID in the replacement block
			{
				FlashMemory[NFTLTable[LogicalBlock].ReplacementBlock][i] = INVALID_PAGE;	//* change it as an invalid page
				InvalidPageCnt[NFTLTable[LogicalBlock].ReplacementBlock]++;	//* increase the invalid page count
				ValidPageCnt[NFTLTable[LogicalBlock].ReplacementBlock]--;	//* decrease the valid page count
			}
			//* found --> update the related information
			FlashMemory[NFTLTable[LogicalBlock].ReplacementBlock][NFTLTable[LogicalBlock].CurrentFreePageIDinRB] = ClusterID;	//* write the data of the ClusterID to this free page.
			ValidPageCnt[NFTLTable[LogicalBlock].ReplacementBlock]++;	//* increase the va;od page count
			NFTLTable[LogicalBlock].CurrentFreePageIDinRB++;	//* move to the next free page
			//# Statistics
			AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE; //# the time to write one page
		}
//		//* *** CASE ERROR
//		else if(FlashMemory[NFTLTable[LogicalBlock].PrimaryBlcok][LogicalPage] != ClusterID)	//* erro page in primary block
//		{
//			printf("ERROR: The primary block of logical block %d stores invalid page data in page %d.\n", LogicalBlock, LogicalPage);
//			exit(1);
//		}
	}


#ifdef _JOHNSON_DEBUG
	printf("\nLeaving NFTLWriteOneCluster()");
#endif
}



//* ****************************************************************
//* Simulate the NFTL type 2 method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void NFTLSimulation(FILE *fp)
{
	flash_size_t i,j;
	flash_size_t CurrentCluster;	//* Current cluster that we are handling

	int BlockMaxRandomValue;	//* The maximal random value, which allows us to select a block storing initial data
	//int PageMaxRandomValue;		//* The maximal random value, which allows us to select a page of thte selected block


	//* Initialize NFTL table
	NFTLTable = (NFTLElement_t *)malloc(sizeof(NFTLElement_t)*MaxBlock);	//* allocate memory space for the NFTL table
	memset(NFTLTable,NFTL_POINT_TO_NULL,sizeof(NFTLElement_t)*MaxBlock);	//* initialize the content of this table

	//* Randomly select where are the data stored in the flash memory initially
	//* according the parameter "InitialPercentage" to determine what's the percentage of data in ths flash memory initially.
	BlockMaxRandomValue = (int)((float)InitialPercentage * (RAND_MAX) / 100);	//* The max random number equals "InitialPercentage" percentage of data.
	for(i=0; i<(MaxBlock-LeastFreeBlocksInFBL()-BadBlockCnt); i++)
	{
		if( rand() <= BlockMaxRandomValue)
		{
			//PageMaxRandomValue = rand();	//* The maximal number of pages storing initial data is from 0 to MaxPagd
			for(j=0;j<MaxPage;j++)
			{
				//if(rand() <= PageMaxRandomValue)
				{
					NFTLWriteOneCluster(i*MaxPage+j);


					//# Update the number of accessed logical pages
					AccessedLogicalPages++;	//# advanced the number of set flags in the map
					AccessedLogicalPageMap[i*MaxPage+j] = True; //# Set the flag on
				}
			}
		}
	}
	//# Log information
	printf("Initial Percentage of data: %.02f%%\n",(float)AccessedLogicalPages/(MaxPage*MaxBlock)*100);

	//# reset statictic variables
	ResetStatisticVariable(&AccessStatistics);

	//* ******************************* Start simulation ***************************
	//* dynamic wear leveling: cost-benefit function
	while(GetOneOperation(fp, &CurrentRequest.AccessType, &CurrentRequest.StartCluster, &CurrentRequest.Length, False))	//* fetch one write operation
	{
		//* Check each Write request is a new write or an update and then update the mapping information
		for(CurrentCluster=CurrentRequest.StartCluster; CurrentCluster<(CurrentRequest.StartCluster+CurrentRequest.Length); CurrentCluster++)
		{
			//DEBUG
			//if(AccessStatistics.TotalWriteRequestCount%1000 ==0) 
			//	printf("%8d",AccessStatistics.TotalWriteRequestCount);
			
			if(CurrentRequest.AccessType == WriteType)
			{
				NFTLWriteOneCluster(CurrentCluster);	//* Write a cluster to the flash memory and also update the FTL table. We suggest this function to write a cluster to the flash memory and also update the FTL table
			}
			else if(CurrentRequest.AccessType == ReadType)
			{
				//# read one cluster. If it doesn't exist, then simply exist.
				if(NFTLReadOneCluster(CurrentCluster) == False) break;
			}

		}	//* end of for

		//# Statistics
		if(CurrentCluster == (CurrentRequest.StartCluster+CurrentRequest.Length)) //# if the request is a legal command
		{
			if(CurrentRequest.AccessType == WriteType) AccessStatistics.TotalWriteRequestCount ++;	//# advance the number of total write requests
			if(CurrentRequest.AccessType == ReadType) AccessStatistics.TotalReadRequestCount ++;	//# advance the number of total read requests
		}

	}	//* end of while
	//* ******************************* End of simulation *****************************

	free(NFTLTable);	//* release memory space
}

