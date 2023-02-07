//* BL.c : All the simulation for Block Level mapping are defined in this file
//* dynamic wear leveling: When a page is updated, the block is erased immediately


#ifndef _BL_C
#define _BL_C
#endif


#include "main.h"
#include "BL.h"


//* *************** global variabl *******************
static flash_size_t *BLTable;	//* Block Level Table: Each entry stores the block ID, which stores this logical block
static flash_size_t *ZeroedUpdatedClusters;



//* ****************************************************************
//* Read a cluster from the flash memory
//* "ClusterID" is the Cluster that is going to be updated or written
//* Return value: If the "ClusterID" exists, return True. Otherwise, return False.
//* *****************************************************************
static Boolean BLReadOneCluster(flash_size_t ClusterID)
{
	//# Check whether the accessed data exists
	if(BLTable[ClusterID/MaxPage] == BL_POINT_TO_NULL)
	{
		return(False);
	}
	else if(FlashMemory[BLTable[ClusterID/MaxPage]][ClusterID%MaxPage] != ClusterID)
	{
		return(False);
	}

	//# Staticstics
	AccessStatistics.TotalReadRequestTime += TIME_READ_PAGE; //# the time to read one page

	return(True);
}


//* ****************************************************************
//* Update data in current block
//* "LogicalBlock" is the logical block entry that that is going to be updated
//* "UpdatedClusters" is the array storing the clusters that are going to be updated or written
//* ****************************************************************
static void BLUpdateOneBlock(flash_size_t LogicalBlock, flash_size_t *UpdatedClusters)
{
	flash_size_t i;
	flash_size_t FreeBlock;

	//* Put the oringinal block to the free block list if it exists
	if(BLTable[LogicalBlock] != BL_POINT_TO_NULL) PutFreeBlock(BLTable[LogicalBlock]);


	if((FreeBlock=GetFreeBlock(False))==-1)	//* Get one free block
	{
		printf("ERROR: BL can't get a free block.\n");
		exit(1);
	}

	//* Write new write page data to the new block and then copy the valid pages in the original block to the new block
	for(i=0;i<MaxPage;i++)
	{
		if(UpdatedClusters[i] != BL_POINT_TO_NULL)
		{
			FlashMemory[FreeBlock][i] = UpdatedClusters[i];	//* an updated page
			ValidPageCnt[FreeBlock] ++; //# advance the number of valid pages in the block
			//# Statistics
			AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE; //#the time update a page
		}
		else if(BLTable[LogicalBlock] != BL_POINT_TO_NULL)	//* A physical block for this logical block exists
		{
			if(FlashMemory[BLTable[LogicalBlock]][i] != INVALID_PAGE && FlashMemory[BLTable[LogicalBlock]][i] != FREE_PAGE)
			{
				FlashMemory[FreeBlock][i] = FlashMemory[BLTable[LogicalBlock]][i];	//* live-page copying from original block to the new block
				ValidPageCnt[FreeBlock] ++; //# advance the number of valid pages in the block
				//# Statistics
				AccessStatistics.TotalLivePageCopyings++;	//* advnace the total number of live-page copyings by 1
				AccessStatistics.TotalWriteRequestTime += TIME_READ_PAGE+TIME_WRITE_PAGE; //#the time to copy a live page
			}
			else
			{
				//# Statistics
				AccessStatistics.TotalWriteRequestTime += TIME_READ_PAGE; //# the time to read the spare area of a page
			}
		}
	}

	if(BLTable[LogicalBlock] != BL_POINT_TO_NULL)
	{

		//* *** Clean up the reclaimed block as a free block ***
		//* reset invalid page count
		InvalidPageCnt[BLTable[LogicalBlock]] = 0;
		//* Clear pages as free pages
		for(i=0; i<MaxPage; i++)
		{
			FlashMemory[BLTable[LogicalBlock]][i] = FREE_PAGE;
		}
		//* update the erase count
		BlockEraseCnt[BLTable[LogicalBlock]]++;
		//# Update the max block erase count
		if(BlockEraseCnt[BLTable[LogicalBlock]]>MaxBlockEraseCnt) MaxBlockEraseCnt = BlockEraseCnt[BLTable[LogicalBlock]];

		//# Satistics
		AccessStatistics.TotalBlockEraseCount++; //* update total number of block erases
		AccessStatistics.TotalWriteRequestTime += TIME_ERASE_BLOCK;	//# the time to erase a block
	}

	//* Update the BL Table
	BLTable[LogicalBlock] = FreeBlock;


}



//* ****************************************************************
//* Simulate the Block Level mapping method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void BLSimulation(FILE *fp)
{
	flash_size_t	CurrentAccessedBlock;	//* Point to current accessed block
	flash_size_t 	*UpdatedClusters;	//* Point to the array storing updated clusters

	flash_size_t i,j;
	flash_size_t CurrentCluster;	//* Current cluster that we are handling
	//flash_size_t StartCluster;	//* the starting Cluster to be written
	//flash_size_t Length;		//* the number of Clusters to be written


	int BlockMaxRandomValue;	//* The maximal random value, which allows us to select a block storing initial data
	//int PageMaxRandomValue;		//* The maximal random value, which allows us to select a page of thte selected block


	//* Initialize the zeroed cluster array for the EraseBlockSet()
	ZeroedUpdatedClusters = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxPage);	//* allocate memory space
	memset(ZeroedUpdatedClusters, BL_POINT_TO_NULL,sizeof(flash_size_t)*MaxPage);	//* initialize the content of this array


	//* Initialize Block Level table
	BLTable = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxBlock);	//* allocate memory space for the Block Level table
	memset(BLTable,BL_POINT_TO_NULL,sizeof(flash_size_t)*MaxBlock);	//* initialize the content of this table

	//* Initialize the cluster array
	UpdatedClusters = (flash_size_t *)malloc(sizeof(flash_size_t)*MaxPage);	//* allocate memory space
	memset(UpdatedClusters, BL_POINT_TO_NULL,sizeof(flash_size_t)*MaxPage);	//* initialize the content of this array

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
				//* Put the updated cluster to the array
				//if(rand() <= PageMaxRandomValue)
				{
					UpdatedClusters[j] = i*MaxPage+j;

					//# Update the number of accessed logical pages
					AccessedLogicalPages++;	//# advanced the number of set flags in the map
					AccessedLogicalPageMap[i*MaxPage+j] = True; //# Set the flag on
				}
			}

			BLUpdateOneBlock(i, UpdatedClusters);	//* Update data in current block
			memset(UpdatedClusters, BL_POINT_TO_NULL,sizeof(flash_size_t)*MaxPage);	//* reset the array storing the updated cluster
		}
	}
	//# Log information
	printf("Initial Percentage of data: %.02f%%\n",(float)AccessedLogicalPages/(MaxPage*MaxBlock)*100);
	//AccessedLogicalPages=0;
	//memset(AccessedLogicalPageMap,0,sizeof(Boolean)*MaxPage*MaxBlock);


	//# reset statictic variables
	ResetStatisticVariable(&AccessStatistics);

	//* ******************************* Start simulation ***************************
	//* dynamic wear leveling: cost-benefit function
	//* static wear leveling: activated when the BlockErase_count/flag_count > SWLThreshold "T"

	//GetOneOperation(fp, &CurrentRequest.AccessType, &CurrentRequest.StartCluster, &CurrentRequest.Length);
	//CurrentAccessedBlock = CurrentRequest.StartCluster/MaxPage;	//* Locate the logical block that the first write operation is going to write to.

	CurrentAccessedBlock = -1; //# initialize -> there is no data cached in the UpdatedClusters[]

	while( GetOneOperation(fp, &CurrentRequest.AccessType, &CurrentRequest.StartCluster, &CurrentRequest.Length, False) )	//* fetch one write operation
	{
		//* Accumulate pages that is going to be written to the same block until a page which belongs to another block is requested to write or update
		for(CurrentCluster=CurrentRequest.StartCluster; CurrentCluster<(CurrentRequest.StartCluster+CurrentRequest.Length); CurrentCluster++)
		{
			//# access another virtual block or encounter a read request, so that we have to flush the cached data in the current virtual block
			if( (CurrentAccessedBlock != -1) && (CurrentAccessedBlock != (CurrentCluster/MaxPage) || CurrentRequest.AccessType !=WriteType) )
			{
				BLUpdateOneBlock(CurrentAccessedBlock, UpdatedClusters);	//* Update data in current block
				memset(UpdatedClusters, BL_POINT_TO_NULL, sizeof(flash_size_t)*MaxPage);	//* reset the array storing the updated cluster
				CurrentAccessedBlock = -1;	//# there is no data cached in the UpdatedClusters[]
			}
			//# Put the updated cluster to the array, whenever this is a write request
			if(CurrentRequest.AccessType == WriteType)
			{
				UpdatedClusters[CurrentCluster%MaxPage] = CurrentCluster; //# cache the written cluster in the UpdatedClusters[]
				CurrentAccessedBlock = CurrentCluster/MaxPage; //# set to current accessed virtual block
			}

			//# a read request
			if(CurrentRequest.AccessType == ReadType)
			{
				//# read one cluster. If it doesn't exist, then simply exist.
				if(BLReadOneCluster(CurrentCluster) == False) break;
			}

		}	//* end of for

		//# Statistics
		if(CurrentCluster == (CurrentRequest.StartCluster+CurrentRequest.Length)) //# if the request is a legal command
		{
			if(CurrentRequest.AccessType == WriteType) AccessStatistics.TotalWriteRequestCount ++;	//# advance the number of total write requests
			if(CurrentRequest.AccessType == ReadType) AccessStatistics.TotalReadRequestCount ++;	//# advance the number of total read requests
		}

	}	//# end of while

	if(CurrentAccessedBlock != -1) BLUpdateOneBlock(CurrentAccessedBlock, UpdatedClusters);	//* Update the latest updated block before the trace simulation
	//* ******************************* End of simulation *****************************


	free(UpdatedClusters);	//* release memory space
	free(BLTable);	//* release memory space
}
