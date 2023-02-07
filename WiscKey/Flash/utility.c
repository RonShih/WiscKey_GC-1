//* All the utility functions and macros are collected in this file

#ifndef _UTILITY_C
#define _UTILITY_C
#endif


#include "main.h"
#include "utility.h"
#include "OutputResult.h"


#define TRACE_UNIT (372)	//# Define the unit of write operation
#define SHOW_REQUENT (1)	//# the number of rewinds to show the information

#define e	(2.71828183)

time_t TotalExecutionTime;	//* the time to run this simulation

OneRequest_t CurrentRequest;	//# to keep current request

static int	TotalAccessedLogicalPages=0;
static Boolean	*TotalAccessedLogicalPageMap;


//# ****************************************************************
//# This function is to calculate the probability of the occureecne >= x
//# u = lambda * t
//# p(X>=x) = 1 - sum_{y=0}^{y=x-1} { u^y * e^-u / y! }
//# return valu is the probability of the occurrence >= x
//# ****************************************************************
double possion(int x, double lambda, int t)
{
	int i;
	int factorial=1;
	double ExcludedPart = 0.0;
	double u = lambda * t;
	double u_power_x = 1;
	double e_power_u;

	e_power_u = pow(e, u * -1.0);

	for(i=0;i<x;i++)
	{

		ExcludedPart += (u_power_x / factorial) * e_power_u;
		//printf("%3d=%.30lf\n",i,(u_power_x / factorial) * e_power_u);
		factorial *= (i+1);
		u_power_x *= u;
	}


	return(1.0 - ExcludedPart);
}


//# ****************************************************************
//# This function is to fetch one read/write operation from the trace file.
//# It returns the starting LBA and the number of the LBAs written whenever the access type (read or write) is going to be evaluated.
//# "fp" is the file descriptor pointing to the trace file
//# "AccessType" is the type of the request (i.e., ReadType or WriteType)
//# "StartCluster" returns the starting Cluster of the write operation -> 1 Cluster = 1 LBA * PageToLBARatio
//# "Length" is the number of Clusters written by the write opeartion
//# If finding a command successfully, return "TRUE"; otherwise, return "FALSE"
//# ****************************************************************
Boolean GetOneOperation(FILE *fp, AccessType_t *AccessType, flash_size_t *StartCluster, flash_size_t *Length, Boolean subPageLevel)
{
//	char AMPMString[BUFFER_SIZE];
//	char TimeString[BUFFER_SIZE];
	flash_size_t i;
	char ReadWriteString[BUFFER_SIZE];
	flash_size_t StartLBA;	//* start LBA
	flash_size_t EndLBA;	//* end LBA
	flash_size_t len;		//* the number of LBAs in each read/write opeartion
//	static int counter=0;
//	static int max_counter=0;
	static time_t CurrentRewindTimes=0;
	static time_t InitialTime;	//* Keep the starting time of the simluation
	static time_t LastTime=0;		//* keep the time of the time when we rewinded the input trace file.
	static int cnt=0;
	time_t CurrentTime;		//* keep current rewind time

	static Boolean IsParseTraceFile=False;	//# Keep track of whether the trace file has been parsed or not.
	static OperationRange_t *TraceOperation;
	static flash_size_t OperationCnt=0;		//# The number of opeartions in the "TraceOperation"
	static flash_size_t CurrentOperationCnt=0;	//# point to the opeartion that is going to be accessed.
	static int RandomIndex=0;


	//# Parse Trace file
	if(!IsParseTraceFile)
	{
		TotalAccessedLogicalPageMap=(Boolean *)malloc(sizeof(Boolean)*MaxPage*MaxBlock);
		memset(TotalAccessedLogicalPageMap,0,sizeof(Boolean)*MaxPage*MaxBlock);
		IsParseTraceFile = True;

		//# Find out the number of opeartions that fit in the range of "MaxLBA."
		while(!feof(fp))
		{
			fscanf(fp,"%s %d %d", ReadWriteString, &StartLBA, &len);	//* fetch one line acess

			if((ReadWriteString[0] == 'W' || ReadWriteString[0] == 'w') && (strcmp(EvaluatedAccessType,TYPE_WRITE)==0 || strcmp(EvaluatedAccessType,TYPE_READWRITE)==0))
			{
				EndLBA = StartLBA - 1 +len;	//* calculate the end of the accessed LBA
				if(EndLBA< MaxLBA) 	OperationCnt++; //# advance operation count
			}
			else if((ReadWriteString[0] == 'R' || ReadWriteString[0] == 'r') && (strcmp(EvaluatedAccessType,TYPE_READ)==0 || strcmp(EvaluatedAccessType,TYPE_READWRITE)==0))
			{
				EndLBA = StartLBA - 1 +len;	//* calculate the end of the accessed LBA
				if(EndLBA< MaxLBA) 	OperationCnt++; //# advance operation count
			}
		}

		//# Allocate the memory space for the operations
		TraceOperation = (OperationRange_t *)malloc(sizeof(OperationRange_t)*OperationCnt);
		rewind(fp);
		while(!feof(fp))
		{
			fscanf(fp,"%s %d %d", ReadWriteString, &StartLBA, &len);	//* fetch one line acess
			if((ReadWriteString[0] == 'W' || ReadWriteString[0] == 'w') && (strcmp(EvaluatedAccessType,TYPE_WRITE)==0 || strcmp(EvaluatedAccessType,TYPE_READWRITE)==0))
			{
				EndLBA = StartLBA - 1 +len;	//* calculate the end of the accessed LBA
				if(EndLBA< MaxLBA)
				{
					TraceOperation[CurrentOperationCnt].AccessType = WriteType;
					TraceOperation[CurrentOperationCnt].StartLBA = StartLBA;
					TraceOperation[CurrentOperationCnt].EndLBA = EndLBA;
					CurrentOperationCnt++;
				}
			}
			else if((ReadWriteString[0] == 'R' || ReadWriteString[0] == 'r') && (strcmp(EvaluatedAccessType,TYPE_READ)==0 || strcmp(EvaluatedAccessType,TYPE_READWRITE)==0))
			{
				EndLBA = StartLBA - 1 +len;	//* calculate the end of the accessed LBA
				if(EndLBA< MaxLBA)
				{
					TraceOperation[CurrentOperationCnt].AccessType = ReadType;
					TraceOperation[CurrentOperationCnt].StartLBA = StartLBA;
					TraceOperation[CurrentOperationCnt].EndLBA = EndLBA;
					CurrentOperationCnt++;
				}
			}
		}
		printf("Operation Count in This Trace = %d\n",OperationCnt);

		CurrentOperationCnt = 0;
		fclose(fp);	//* close the input trace file
	}


	if(LastTime==0)
	{
		time(&InitialTime);	//* get the starting time of the first round.
		LastTime = InitialTime;
	}

	//# get one read or write operation
	while(1)	//* We will rewind trace for "InputFileRewindTimes" times.
	{
		//# If do the simulation until the first block reaching the worn-out erase cycle, and the erase cycles terminate
		if(IsDoSimulationUntilFirstBlockWornOut && MaxBlockEraseCnt >= BlockEndurance)
		{
			time(&CurrentTime);	//* get current time
			printf("\nIt spent %ld seconds in total.\n",CurrentTime-InitialTime);
			TotalExecutionTime = CurrentTime-InitialTime;
			OutputRewindTimes((double)CurrentRewindTimes+(double)CurrentOperationCnt/(double)OperationCnt);
			break;
		}

		if(CurrentOperationCnt>=OperationCnt)	//* Reach the end of the trace file, rewind it if the number of times to rewind the input trace file has not reached the value set in "InputFileRewindTimes".
		{
			if(IsDoSimulationUntilFirstBlockWornOut || CurrentRewindTimes<InputFileRewindTimes)
			{
				CurrentOperationCnt=0;	//* rewind
				time(&CurrentTime);	//* get current time
				CurrentRewindTimes++;
				if(cnt++ % SHOW_REQUENT == 0)
				{
					printf("Rewind %d , %d sec last time, and %d sec to now, max erase=%d\n",(int)(CurrentRewindTimes),(int)(CurrentTime-LastTime), (int)(CurrentTime-InitialTime), MaxBlockEraseCnt);
					//printf("%5d%5d",(int)(CurrentRewindTimes), MaxBlockEraseCnt);
				}
				LastTime = CurrentTime;	//* update the rewind time
			}
			else
			{
				time(&CurrentTime);	//* get current time
				printf("\nIt spent %ld seconds in total.\n",CurrentTime-InitialTime);
				TotalExecutionTime = CurrentTime-InitialTime;
				break;	//* exit to finish the simulation
			}
		}

		if(subPageLevel)
		{
			*AccessType = TraceOperation[(RandomIndex+CurrentOperationCnt)%OperationCnt].AccessType;
			*StartCluster = TraceOperation[(RandomIndex+CurrentOperationCnt)%OperationCnt].StartLBA;	//* Translate from LBA number to Cluster number
			*Length = (TraceOperation[(RandomIndex+CurrentOperationCnt)%OperationCnt].EndLBA) - *StartCluster + 1;		//* Translate from the number of LBAs to the number of Clusters

			//# Update the number of accessed logical pages
			for(i = *StartCluster / MaxSubPage; i < *StartCluster / MaxSubPage + *Length; i ++)
			{
				if(!AccessedLogicalPageMap[i])
				{
					AccessedLogicalPages++;	//# advanced the number of set flags in the map
					AccessedLogicalPageMap[i] = True; //# Set the flag on
				}
				if(!TotalAccessedLogicalPageMap[i])
				{
					TotalAccessedLogicalPages++;	//# advanced the number of set flags in the map
					TotalAccessedLogicalPageMap[i] = True; //# Set the flag on
				}
			}
		}
		else
		{
			*AccessType = TraceOperation[(RandomIndex+CurrentOperationCnt)%OperationCnt].AccessType;
			*StartCluster = TraceOperation[(RandomIndex+CurrentOperationCnt)%OperationCnt].StartLBA / PageToLBARatio;	//* Translate from LBA number to Cluster number
			*Length = (TraceOperation[(RandomIndex+CurrentOperationCnt)%OperationCnt].EndLBA / PageToLBARatio) - *StartCluster + 1;		//* Translate from the number of LBAs to the number of Clusters

			//# Update the number of accessed logical pages
			for(i = *StartCluster; i < *StartCluster + *Length; i ++)
			{
				if(!AccessedLogicalPageMap[i])
				{
					AccessedLogicalPages++;	//# advanced the number of set flags in the map
					AccessedLogicalPageMap[i] = True; //# Set the flag on
				}
				if(!TotalAccessedLogicalPageMap[i])
				{
					TotalAccessedLogicalPages++;	//# advanced the number of set flags in the map
					TotalAccessedLogicalPageMap[i] = True; //# Set the flag on
				}
			}
		}

//		if(TraceOperation[(RandomIndex+CurrentOperationCnt)%OperationCnt].AccessType == WriteType) TotalWriteRequestCount++; //# advance the total number of write operations/requests
//		if(TraceOperation[(RandomIndex+CurrentOperationCnt)%OperationCnt].AccessType == ReadType) TotalReadRequestCount++; //# advance the total number of read operations/requests
		CurrentOperationCnt++; //# Advance to the next operation.

		if(CurrentOperationCnt%TRACE_UNIT==0)
		{
			RandomIndex = (int)(((float)rand()/RAND_MAX)*OperationCnt);
			//RandomIndex = 0; //# not to access every access segment randomly
		}

		return True;
	}


	//# Log information
	printf("Final Percentage of Data: %.02f%%, Total Percentage of Accessed Data: %.02f%%\n",
			(float)AccessedLogicalPages / (MaxPage * (MaxBlock - BadBlockCnt)) * 100,
			(float)TotalAccessedLogicalPages / (MaxPage * (MaxBlock - BadBlockCnt))*100);

	free(TraceOperation);
	free(TotalAccessedLogicalPageMap);

	return(False);	//* the end of the simulation for this input trace file

}




