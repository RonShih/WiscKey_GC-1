//* Output the simulation results to log files.

#ifndef _OUTPUT_RESULT_C
#define _OUTPUT_RESULT_C
#endif

#include "main.h"
#include "OutputResult.h"
#include "flash.h"
#include <math.h>
#include <errno.h>

//* ****************************** MACROs ***********************
#define TRACE_DIR "TraceFiles"	//* Define the directory to store the log files
#define PARSE_DIR "ParsedFiles"	//* Define the directory to store the parsed log files


#define BLOCK_ERASE_LIMIT		(1000000)	//* The limit of block erase cycles
#define ERASE_GROUP_INTERVAL	(10000)		//* The interval to group erase count
#define FIRST_ERASE_COUNT		(10000)		//* The first erase count



//* ****************************************************************
//* This function is to write simulation results to files
//* ****************************************************************
void OutputResult(void)
{
	//flash_size_t i;
	flash_huge_size_t AvgR, AvgW, AvgRW;
	char OutFilename[BUFFER_SIZE];	//* output file name
	FILE *OutFp;	//* point to the output file for erase count
	char str[BUFFER_SIZE];		//# the command string
	/*
	#ifdef LINUX
		sprintf(str,"mkdir -p %s/%s",TRACE_DIR,InputFileName);
		system(str);	//* Create directory
	#else
		sprintf(str,"mkdir %s\\%s",TRACE_DIR,InputFileName);
		system(str);	//* Create directory
	#endif
	 */
	//* *** Output the average response time or total transmission time ***
	//sprintf(OutFilename,TRACE_DIR"_%s_%s_%s_Performance.log",InputFileName, MetricType, FTLMethod);

#ifdef BEYONDADDR
	sprintf(OutFilename, "Result_%s_SRAM_%d_REGION_%d.log", FTLMethod, SramSize, VirtualRegionNum);
#else
	sprintf(OutFilename, "Result_%s_SRAM_%d.log", FTLMethod, SramSize);
#endif

	errno = 0;
	printf("%s \n", OutFilename);
	OutFp = fopen(OutFilename,"wb"); //# open output file
	if(OutFp == NULL) //if file does not exist, create it
	{
		printf("Error When creating file\n");
	}


	fprintf(OutFp,"%s		%s(R:us)		%s(W:us)		%s(RW:us)		(WT:ns)\n",FTLMethod, MetricType, MetricType, MetricType); //# output the title
	//fprintf(OutFp,"%s_%d_%d_%d_%d_%d_%d_%d_%d %s(R:us) %s(W:us) %s(RW:us) \n",FTLMethod,VBTCacheLength,VPTCacheLength,ParityCheckCacheLength, ChipNumber,BlocksPerVirtualRegion,BlocksPerPhysicalRegion,ChipSize,SpareBlocksPerChip, MetricType,MetricType,MetricType); //# output the title

	//# calculate the average response time
	if(strcmp(MetricType,"ART")==0)
	{
		if(AccessStatistics.TotalReadRequestCount != 0)
			AvgR = AccessStatistics.TotalReadRequestTime/AccessStatistics.TotalReadRequestCount/1000;
		else
			AvgR = 0;

		if(AccessStatistics.TotalWriteRequestCount != 0)
			AvgW = AccessStatistics.TotalWriteRequestTime/AccessStatistics.TotalWriteRequestCount/1000;
		else
			AvgW = 0;

		if(AccessStatistics.TotalReadRequestCount + AccessStatistics.TotalWriteRequestCount != 0)
			AvgRW = (AccessStatistics.TotalReadRequestTime + AccessStatistics.TotalWriteRequestTime) / (AccessStatistics.TotalReadRequestCount + AccessStatistics.TotalWriteRequestCount)/1000;
		else
			AvgRW = 0;
	}
	//# calculate the average response time
	if(strcmp(MetricType,"TTT")==0)
	{
		AvgR = AccessStatistics.TotalReadRequestTime/1000;
		AvgW = AccessStatistics.TotalWriteRequestTime/1000;
		AvgRW = (AccessStatistics.TotalReadRequestTime + AccessStatistics.TotalWriteRequestTime) / 1000;
	}

	fprintf(OutFp, "%s		%"PRINTF_LONGLONG"		%"PRINTF_LONGLONG"		%"PRINTF_LONGLONG" \n", FTLMethod, AvgR, AvgW, AvgRW); //# output the content
	//fprintf(OutFp, "%s_%d_%d_%d_%d_%d_%d_%d_%d %"PRINTF_LONGLONG" %"PRINTF_LONGLONG" %"PRINTF_LONGLONG"\n", FTLMethod,VBTCacheLength,VPTCacheLength,ParityCheckCacheLength, ChipNumber,BlocksPerVirtualRegion,BlocksPerPhysicalRegion,ChipSize,SpareBlocksPerChip,AvgR, AvgW, AvgRW); //# output the content

	//# Output the cache miss rate
	if(strcmp(FTLMethod,FTL_DISPOSABLE)==0)
	{
		fprintf(OutFp,"VBT hit rate: %2.2lf (%I64d,%I64d) %%\n", ((double)100*(AccessStatistics.TotalVBTReferenceCnt-AccessStatistics.TotalVBTMissCnt))/AccessStatistics.TotalVBTReferenceCnt,AccessStatistics.TotalVBTMissCnt,AccessStatistics.TotalVBTReferenceCnt);
		fprintf(OutFp,"VPT hit rate: %2.2lf (%I64d,%I64d) %%\n", ((double)100*(AccessStatistics.TotalVPTReferenceCnt-AccessStatistics.TotalVPTMissCnt))/AccessStatistics.TotalVPTReferenceCnt,AccessStatistics.TotalVPTMissCnt,AccessStatistics.TotalVPTReferenceCnt);
		fprintf(OutFp,"ParityCheck hit rate: %2.2lf (%I64d,%I64d) %%\n", ((double)100*(AccessStatistics.TotalParityCheckReferenceCnt-AccessStatistics.TotalParityCheckMissCnt))/AccessStatistics.TotalParityCheckReferenceCnt,AccessStatistics.TotalParityCheckMissCnt,AccessStatistics.TotalParityCheckReferenceCnt);
	}

	fprintf(OutFp, "----------------------------------------- \n");
	fprintf(OutFp, "FreeBlock %d. \n", BlocksInFreeBlockList);
	fprintf(OutFp, "Total Write %"PRINTF_LONGLONG" (Failed %"PRINTF_LONGLONG"). \n", AccessStatistics.TotalWriteRequestCount, AccessStatistics.FailedWriteOperation);
	fprintf(OutFp, "Total Read %"PRINTF_LONGLONG" (Failed %"PRINTF_LONGLONG"). \n", AccessStatistics.TotalReadRequestCount, AccessStatistics.FailedReadOperation);
	fprintf(OutFp, "Total Operations %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalWriteRequestCount + AccessStatistics.TotalReadRequestCount);
	fprintf(OutFp, "----------------------------------------- \n");
	fprintf(OutFp, "TotalBlockEraseCount %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalBlockEraseCount);
	fprintf(OutFp, "TotalLivePageCopyings %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalLivePageCopyings);
	fprintf(OutFp, "----------------------------------------- \n");
	fprintf(OutFp, "TotalWritePageCount %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalWritePageCount);
	fprintf(OutFp, "TotalReadPageCount %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalReadPageCount);
	fprintf(OutFp, "----------------------------------------- \n");
	fprintf(OutFp, "TotalWriteSubPageCount %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalWriteSubPageCount);
	fprintf(OutFp, "TotalReadSubPageCount %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalReadSubPageCount);
	fprintf(OutFp, "TotalLiveSubPageCopyings %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalLiveSubPageCopyings);
	fprintf(OutFp, "----------------------------------------- \n");
	fprintf(OutFp, "TotalWriteRequestTime %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalWriteRequestTime);
	fprintf(OutFp, "TotalReadRequestTime %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalReadRequestTime);

	fclose(OutFp);	//* close file descriptor

	//* *** Output the total number of block erases and total live-page copyings
	//sprintf(OutFilename,TRACE_DIR"/%s/EraseLiveCopy-%s_%s_%0d_%0d_%0d_%d_%d_%d_%d_%d.log", InputFileName, MetricType, FTLMethod,VBTCacheLength,VPTCacheLength,ParityCheckCacheLength,ChipNumber,BlocksPerVirtualRegion,BlocksPerPhysicalRegion,ChipSize,SpareBlocksPerChip);
	//OutFp = fopen(OutFilename,"w");

	//fprintf(OutFp,"%s_%d_%d_%d_%d_%d_%d_%d TotalBlockErases TotalLivePageCopyings\n", FTLMethod,VBTCacheLength,VPTCacheLength,ParityCheckCacheLength,BlocksPerVirtualRegion,BlocksPerPhysicalRegion,ChipSize,SpareBlocksPerChip); //# output the title
	//fprintf(OutFp, "%s_%d_%d_%d_%d_%d %"PRINTF_LONGLONG" %"PRINTF_LONGLONG"\n", FTLMethod,VBTCacheLength,VPTCacheLength,ParityCheckCacheLength,ChipSize, AccessStatistics.TotalBlockEraseCount,AccessStatistics.TotalLivePageCopyings,SpareBlocksPerChip); //# output the content

	//fprintf(OutFp,"The BET was reset for %d times\n",BETResetCnt);
	//fprintf(OutFp,"The FTL simulation spent %ld seconds (%.6f hours) in total.\n",TotalExecutionTime,(float)TotalExecutionTime/3600);	//* Show the total simulation time

	//fclose(OutFp);

}

//* ****************************************************************
//* This funciton is to analyze the simulation result
//* ****************************************************************
void AnalyzeResult(void)
{
	flash_size_t i;
	flash_size_t *EraseBlockCnt;	//* Point to the array that stores the number of blocks that are erased EraseBlockCnt[i] times
	flash_size_t BlockID;	//* the blokc ID
	flash_size_t EraseCnt;	//* the erases times of the block ID
	flash_size_t AccumulatedBlocks;	//* the number of blocks accumulated until now
	flash_size_t EraseCountMaximal;	//* the maximal erase cycles in a block
	flash_huge_size_t EraseCountTotal=0;	//* accumulate total block erase count
	double EraseCountAverage;		//* Average erase count of blocks
	double EraseCountVariance;		//* the variance of erase count of block
	

	char OutFilename[BUFFER_SIZE];	//* output file name
	FILE *OutFp, *InFp;	//* point to the output file for erase count

	char LineBuffer[BUFFER_SIZE];	//* The buffer to cache one line data
	flash_size_t FetchCnt;		//* count the number of integers that have encountered

	//* ******************* parse erase count **************************

	#ifdef LINUX
		system("mkdir -p "PARSE_DIR);	//* Create directory
	#else
		system("mkdir "PARSE_DIR);	//* Create directory
	#endif
	

	//* open the log file for input
	//sprintf(OutFilename,TRACE_DIR"/EachBlockErases_Trace-%s_FS%dMB_BS%dKB_PS%dB_%d%%_Re%d_%s_k%d_T%d_%s.log",InputFileName, FlashSize,BlockSize,PageSize,InitialPercentage,InputFileRewindTimes,FTLMethod ,MappingModeK,SWLThreshold,WLMethod);
	if((InFp = fopen(OutFilename,"r"))==NULL)
	{
		printf("ERROR: Can't open the log file:%s\n",OutFilename);
		exit(1);
	}
	//* open the output file
	//sprintf(OutFilename,PARSE_DIR"/EraseBlockCnt_Trace-%s_FS%dMB_BS%dKB_PS%dB_%d%%_Re%d_%s_k%d_T%04d_%s.log",InputFileName, FlashSize,BlockSize,PageSize,InitialPercentage,InputFileRewindTimes,FTLMethod ,MappingModeK,SWLThreshold,WLMethod);
	if((OutFp = fopen(OutFilename,"w"))==NULL)
	{
		printf("ERROR: Can't open the output file:%s\n",OutFilename);
		exit(1);
	}

	
	//* allocate memory
	EraseBlockCnt = (flash_size_t *)malloc(sizeof(flash_size_t)*(BLOCK_ERASE_LIMIT/ERASE_GROUP_INTERVAL+2));
	memset(EraseBlockCnt,0,sizeof(flash_size_t)*(BLOCK_ERASE_LIMIT/ERASE_GROUP_INTERVAL+2));

	EraseCountMaximal = 0;
	EraseCountTotal=0;
	//* parse each line of log file: first column: block ID, second column: the number of block erases
	do
	{
		if(fscanf(InFp,"%d %d", &BlockID, &EraseCnt)==2) //* get one line
		{
			EraseCountTotal += EraseCnt;	//* Total block erase count
			if(EraseCountMaximal < EraseCnt) EraseCountMaximal = EraseCnt; //* Keep the maximal erase count
			if(EraseCnt < FIRST_ERASE_COUNT) EraseBlockCnt[0]++;
			else
			{
				if(FIRST_ERASE_COUNT != 0)
					EraseBlockCnt[(EraseCnt-FIRST_ERASE_COUNT)/ERASE_GROUP_INTERVAL+1]++;
				else
					EraseBlockCnt[(EraseCnt-FIRST_ERASE_COUNT)/ERASE_GROUP_INTERVAL]++;
			}
		}
	}while(!feof(InFp));

	//# *** Calculate average and deviation ***
	EraseCountAverage = (float)EraseCountTotal / MaxBlock;
	EraseCountVariance = 0.0;
	rewind(InFp);	//* rewind
	//# parse each line of log file: first column: block ID, second column: the number of block erases
	//# calculate the variance  
	do
	{
		if(fscanf(InFp,"%d %d", &BlockID, &EraseCnt)==2) //* get one line
		{
			EraseCountVariance += ((double)EraseCnt-EraseCountAverage)*((double)EraseCnt-EraseCountAverage);
		}
	}while(!feof(InFp));
	EraseCountVariance /= MaxBlock;


	//* output the block cont
	AccumulatedBlocks = 0;
	//fprintf(OutFp,"EraseCnt\t%d_%s_%s_k%d_T%04d\n",InitialPercentage,FTLMethod,WLMethod,MappingModeK,SWLThreshold);
	for(i=0;i<=EraseCountMaximal/ERASE_GROUP_INTERVAL+1;i++)
	{	
		AccumulatedBlocks += EraseBlockCnt[i];	//* Accumulate the number of blocks until now
		fprintf(OutFp,"%d\t%.08lf\n",i*ERASE_GROUP_INTERVAL+FIRST_ERASE_COUNT, ((double)AccumulatedBlocks/MaxBlock)*100.0);
		//fprintf(OutFp,"%d\t%d\n",i*ERASE_GROUP_INTERVAL+FIRST_ERASE_COUNT, EraseBlockCnt[i]);
	}

	//* close files
	fclose(OutFp);
	fclose(InFp);

	free(EraseBlockCnt);	//* release memory space




	//* ******************* Parse live-page copying and block erases **************************

	//* open the log file for input
	//sprintf(OutFilename,TRACE_DIR"/TotalBlockErases_Trace-%s_FS%dMB_BS%dKB_PS%dB_%d%%_Re%d_%s_k%d_T%d_%s.log",InputFileName, FlashSize,BlockSize,PageSize,InitialPercentage,InputFileRewindTimes,FTLMethod ,MappingModeK,SWLThreshold,WLMethod);
	if((InFp = fopen(OutFilename,"r"))==NULL)
	{
		printf("ERROR: Can't open the log file:%s\n",OutFilename);
		exit(1);
	}
	//* open the output file
	//sprintf(OutFilename,PARSE_DIR"/TotalBlockErases_Trace-%s_FS%dMB_BS%dKB_PS%dB_%d%%_Re%d_%s_k%d_T%d_%s.log",InputFileName, FlashSize,BlockSize,PageSize,InitialPercentage,InputFileRewindTimes,FTLMethod ,MappingModeK,SWLThreshold,WLMethod);
	if((OutFp = fopen(OutFilename,"w"))==NULL)
	{
		printf("ERROR: Can't open the output file:%s\n",OutFilename);
		exit(1);
	}

	//## File header
	fprintf(OutFp,"Item:\tBlockEraseCount\tLivePageCopying\tBEAverage\tBEDeviation\tBEMaximal\n");
	//fprintf(OutFp,"%d_%s_%s_k%d_T%04d",InitialPercentage,FTLMethod,WLMethod,MappingModeK,SWLThreshold);

	//## First the first two integers in this file. The first one is block erase count and the second one is live-page copyings
	FetchCnt = 0;
	do
	{
		if(fscanf(InFp,"%s",LineBuffer)==1)
		{
			if(LineBuffer[0]>='0' && LineBuffer[0]<='9')
			{
				fprintf(OutFp,"\t%s",LineBuffer);
				FetchCnt++;
				if(FetchCnt==2) break;	//## We found the two integers that we want
			}
		}
	}while(!feof(InFp));

	//# Output average, variance and maximum
	fprintf(OutFp, "\t%.08lf\t%.08lf\t%d\n",EraseCountAverage, sqrt(EraseCountVariance),EraseCountMaximal);



	//* close files
	fclose(OutFp);
	fclose(InFp);
}

//# Output the number of time the trace has been rewinded.
void OutputRewindTimes(double RewindTimes)
{
	char OutFilename[BUFFER_SIZE];	//* output file name
	FILE *OutFp;	//* point to the output file for erase count
	char str[80];

	#ifdef LINUX
		sprintf(str,"mkdir -p %s/%s",TRACE_DIR,InputFileName);
		system(str);	//* Create directory
	#else
		sprintf(str,"mkdir %s\\%s",TRACE_DIR,InputFileName);
		system(str);	//* Create directory
	#endif

	//* *** Output the erase times of each block ***
	sprintf(OutFilename,TRACE_DIR"/%s/%s_%s_%d_%d_%d_%d_%d_FailureTime.log",InputFileName, MetricType, FTLMethod,ChipNumber,BlocksPerVirtualRegion,BlocksPerPhysicalRegion,ChipSize,SpareBlocksPerChip);
	OutFp = fopen(OutFilename,"w");

	//## File header
	//fprintf(OutFp,"%d_%s_%s_k%d_T%04d",,FTLMethod,WLMethod,MappingModeK,SWLThreshold);
	fprintf(OutFp,"%d_%s_%d_%d_%d_%d_%d_FailureTime: ",InitialPercentage, FTLMethod,ChipNumber,BlocksPerVirtualRegion,BlocksPerPhysicalRegion,ChipSize,SpareBlocksPerChip);
	fprintf(OutFp,"\t%.08lf\n",RewindTimes);
	printf("Rewind times: \t%.08lf\n",RewindTimes);


	fclose(OutFp);
}
