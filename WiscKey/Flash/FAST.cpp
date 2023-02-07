//+ FAST.c : All the simulation for FAST are defined in this file
//+ Dynamic wear leveling: merge the sw log block or the front+1 block of the rw log block queue
//+ Static wear leveling: activated when the BlockErase_count/flag_count > SWLThreshold "T"


#ifndef _FAST_CPP
#define _FAST_CPP
#endif

#include "main.h"
#include "FAST.h"
#include "FAST_FTL.h"

//* ****************************************************************
//* Simulate the FAST method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void FASTSimulation(FILE *fp)
{
	
	//+ Create a FAST FTL
	static flash_size_t MaxNumberOfRWLogBlocks = 1024; //+ About 1GB capacity
	FAST_FTL fast_ftl(MaxBlock, MaxNumberOfRWLogBlocks);

	//+ Randomly select where are the data stored in the flash memory initially
	//+ according the parameter "InitialPercentage" to determine what's the percentage of data in ths flash memory initially.
	int BlockMaxRandomValue;	//* The maximal random value, which allows us to select a block storing initial data
	//int PageMaxRandomValue;	//* The maximal random value, which allows us to select a page of the selected block
	BlockMaxRandomValue = (int)((float)InitialPercentage * (RAND_MAX) / 100);	//* The max random number equals "InitialPercentage" percentage of data.
	//for(flash_size_t i=0; i<(MaxBlock-LeastFreeBlocksInFBL()-BadBlockCnt-MaxOverProvisionAreaBlock); i++) //+ We don't use the over-provision area
	for(flash_size_t i=0; i<MaxCluster; i++) //+ We don't use the over-provision area
	{
		if(rand() <= BlockMaxRandomValue)
		{
			//PageMaxRandomValue = rand();	//* The maximal number of pages storing initial data is from 0 to MaxPagd
			//for(flash_size_t j=0;j<MaxPage;j++)
			{
				//* Put the updated cluster to the array
				//if(rand() <= PageMaxRandomValue)
				{
					//+ Write one cluster
					//fast_ftl.WriteOneCluster(i*MaxPage+j);
					fast_ftl.WriteOneCluster(i);

					//# Update the number of accessed logical pages
					AccessedLogicalPages++;	//# advanced the number of set flags in the map
					//AccessedLogicalPageMap[i*MaxPage+j] = True; //# Set the flag on
					AccessedLogicalPageMap[i] = True; //# Set the flag on
				}
			}
		}
	}

	//# Log information
	//printf("Initial Percentage of data: %.02f%%\n",(float)AccessedLogicalPages/(MaxPage*MaxBlock)*100);
	printf("Initial Percentage of data: %.02f%%\n",(float)AccessedLogicalPages/(MaxCluster)*100);

	//# reset statictic variables
	ResetStatisticVariable(&AccessStatistics);

	//+ reset ChipStatistics (dedicated for PerfectFlash MTD)
	//PerfectFlashMTDResetChipStatistics();

	//* ******************************* Start simulation ***************************
	//* dynamic wear leveling: cost-benefit function
	flash_size_t CurrentCluster;
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
				//+ Write one cluster
				fast_ftl.WriteOneCluster(CurrentCluster);	//* Write a cluster to the flash memory and also update the FTL table. We suggest this function to write a cluster to the flash memory and also update the FTL table

				//+ Read after write Check!
				/*
				if(fast_ftl.ReadOneCluster(CurrentCluster) == False)
				{
					printf("ERROR: The cluster %d had not been written correctly!!!\n", CurrentCluster);
					system("PAUSE");
					exit(1);
				}
				//*/
			}
			else if(CurrentRequest.AccessType == ReadType)
			{
				//+ Read one cluster. If it doesn't exist, then simply exist.
				if(fast_ftl.ReadOneCluster(CurrentCluster) == False) break;
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
}
