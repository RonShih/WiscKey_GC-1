//* All the utility functions and macros are collected in this file


#ifndef _UTILITY_H
#define _UTILITY_H

#include "typedefine.h"

#define BUFFER_SIZE	(256)	//* define the size of string buffer

typedef enum 
{
	ReadType,
	WriteType
} AccessType_t;


typedef struct
{
	AccessType_t AccessType;	//# access type: Read or Write
	flash_size_t StartLBA;	//# start LBA of this access
	flash_size_t EndLBA;	//# end LBA of this access
} OperationRange_t;

typedef struct
{
	AccessType_t AccessType;	//# access type: Read or Write
	flash_size_t StartCluster;	//# start LBA of this access
	flash_size_t Length;	//# the number of accessed clusters
} OneRequest_t;

//* ******************** Global variables *****************
extern time_t TotalExecutionTime;	//* the time to run this simulation
extern OneRequest_t CurrentRequest;	//# to keep current request


//# ****************************************************************
//# This function is to calculate the probability of the occureecne >= x
//# u = lambda * t
//# p(X>=x) = 1 - sum_{y=0}^{y=x-1} { u^y * e^-u / y! }
//# return valu is the probability of the occurrence >= x
//# ****************************************************************
double possion(int x, double lambda, int t);



//# ****************************************************************
//# This function is to fetch one read/write operation from the trace file.
//# It returns the starting LBA and the number of the LBAs written whenever the access type (read or write) is going to be evaluated.
//# "fp" is the file descriptor pointing to the trace file
//# "AccessType" is the type of the request (i.e., ReadType or WriteType)
//# "StartCluster" returns the starting Cluster of the write operation -> 1 Cluster = 1 LBA * PageToLBARatio
//# "Length" is the number of Clusters written by the write opeartion
//# If finding a command successfully, return "TRUE"; otherwise, return "FALSE"
//# ****************************************************************
Boolean GetOneOperation(FILE *fp, AccessType_t *AccessType, flash_size_t *StartCluster, flash_size_t *Length, Boolean subPageLevel);

















#endif

