// main.c : Defines the entry point for the console application.
// "distilled.txt" is a trace file of a 20GB hard disk.
#ifndef _MAIN_C
#define _MAIN_C
#endif

#include "main.h"
#include "FTL.h"
#include "NFTL.h"
#include "BL.h"
#include "STRIPE.h"
#include "BEYONDADDR.h"
#include "OutputResult.h"

#include "FAST.h"
#include "DFTL.h"

//* ********************* Global variable *********************

flash_size_t InitialPercentage; 	//* the percentage of the data stored in the flash memory initially (Unit: %)
flash_size_t InputFileRewindTimes;  //* the number of times to rewind the input trace file
char *InputFileName;      //* point to the input file name
char FTLMethod[20];   	  //* the method of Flash Translation Layer FTL/NFTL/BL/STRIPE_STATIC/STRIPE_STATIC/DISPOSABLE (BL=Block Level, STRIPE_STATIC/STRIPE_DYNAMIC: Li-Pin's paper, DISPOSABLE: proposed in this paper)
//Boolean IsDoSimulation; //* To check whether to do simulation or not.
Boolean IsDoSimulationUntilFirstBlockWornOut; //# To check whether to do simulation until the first block's worn-out
flash_size_t BlockEndurance; 				  //# the maximal erase cycle of a block
char *MetricType; 	 		//# the type of metric ART/TTT (ART=average response time, TTT=total transmission time)
char *EvaluatedAccessType;  //# the type of evaluated: R/W/RW (R=read only, W: write only, RW: both read and write requests)

//# the parameters for DISPOSABLE management scheme
flash_size_t SpareBlocksPerChip; 	  //# the number of spare blocks in a chip
flash_size_t BlocksPerPhysicalRegion; //# the number of blocks in a physical region
flash_size_t BlocksPerVirtualRegion;  //# the number of block in a virtual region
flash_size_t RegionSwappingThreshold; //# the number of new developed bad blocks to trigger region swapping

//# ****************************************************************
//# argv[0]: execution file:StaticWearLeveling.exe
//# argv[1]: the input file name (the trace file name)
//# argv[2]: the number of flash chips
//# argv[3]: the size of one flash memory chip (unit:MB)
//# argv[4]: the size of one block (unit:KB)
//# argv[5]: the size of data area of one page (unit: B)
//# argv[6]: the size of spare area of one page (unit: B)
//# argv[7]: the time to erase a block (unit: us)
//# argv[8]: the time to write a page (unit: us)
//# argv[9]: the time to read a page (unit: us)
//# argv[10]: the time to transfer one-byte data between RAM and flash chip (unit: ns) 
//# argv[11]: the time to access SRAM (unit:ns)
//# argv[12]: the type of management: FTL/NFTL/BL/STRIPE/DISPOSABLE/BEYONDADDR (BL=Block Level, STRIPE: Li-Pin's paper, DISPOSABLE: proposed in this paper)
//# argv[13]: the percentage of the data stored in the flash memory initially (Unit: %)
//# argv[14]: evaluation metric: ART/TTT (ART=average response time, TTT=total transmission time)
//# argv[15]: included access type: R/W/RW (R=read only, W: write only, RW: both read and write requests)
//# argv[16]: Do simulation until the first block worn out. Y/N
//# argv[17]: the number of times to rewind the input trace file. If argv[16]=Y, this argument is ignored.
//# argv[18]: the endurance of each block (MLC=10000, SLC=100000). If argv[16]=N, this argument is ignored.
//# argv[19]: Bad blocks in a chip
// Start of DISPOSABLE
//# argv[20]: spare block in a flash chip (only for DISPOSABLE management scheme) If the management is not DISPOSABLE, then this argument is discard.
//# argv[21]: the number of blocks in a physical region (only for DISPOSABLE management scheme)  If the management is not DISPOSABLE, then this argument is discard.
//# argv[22]: the number of blocks in a virtual region  (only for DISPOSABLE management scheme)  If the management is not DISPOSABLE, then this argument is discard.
//# argv[23]: the length of the LRU list to cache Virtual Block Tables, block remapping list, block remapping table, and list of free block sets: only for DISPOSABLE 
//# argv[24]: the length of the LRU list to cache Virtual Page Tables: only for DISPOSABLE 
//# argv[25]: the length of the LRU list to cahe parity-check information: only for DISPOSABLE 
//# argv[26]: the threshold: the number of new bad blocks in a region to trigger region swapping: only for DISPOSABLE
// End of DISPOSABLE
//# argv[27]: VirtualRegionNum (only for BEYONDADDR)
//# argv[28]: SramSize (MB)
//# argv[29]: LRUListSramLength (only for BEYONDADDR)
//# argv[30]: OverProvisionAreaSize (only for FAST)
//# argv[31]: Debug
//* ****************************************************************

int main(int argc, char* argv[]) {
	flash_size_t i;
	FILE *InFp; //* input file descriptor

	printf("Author: Shuo-Han Chen. Built Date: %s, %s\n", __TIME__, __DATE__);

	//* ********************************** Start of reading argv[x] *************************************

	//* check the parameter
	if (argc != 32) {
		printf("ERROR: The number of parameters is not correct\n");
	}

	//* open the input file, the trace file
	InputFileName = argv[1];
	//while (*InputFileName == '.' || *InputFileName == '\\' || *InputFileName == '/')
	//  InputFileName++; //* Locate the starting position of the input file name.

	if ((InFp = fopen(InputFileName, "r")) == NULL) {
		printf("ERROR: Failed to open the trace file\n");
		return -1;
		//PrintInfoAndExit();
	}

	//* retrieve the parameters
	ChipNumber = atol(argv[2]);
	ChipSize = atol(argv[3]);
	BlockSize = atol(argv[4]);
	FlashSize = ChipNumber * ChipSize;
	PageSize = atol(argv[5]); //# size of the data area of a page
	PageSpareSize = atol(argv[6]); //# size of the spare of a page

	//# the time to erase a block (unit: ns)
	EraseBlockTime = atol(argv[7]) * 1000;
	//# the time to write a page (unit: ns)
	WritePageTime = atol(argv[8]) * 1000;
	//# the time to read a page (unit: ns)
	ReadPageTime = atol(argv[9]) * 1000;
	//# the time to transfer one-byte data between RAM and flash chip (unit: ns)
	SerialAccessTime = atol(argv[10]);
	//# the time to access SRAM (unit: ns)
	AccessSRAM = atol(argv[11]);

	//# FTL method
	for (i = 0; i < (int) strlen(argv[12]); i++) { //* convert lower case to upper case
		if (argv[12][i] >= 'a' && argv[12][i] <= 'z')
			FTLMethod[i] = argv[12][i] - 'a' + 'A';
		else
			FTLMethod[i] = argv[12][i];
	}
	FTLMethod[i] = '\0'; //* FTL method

	//* Read parameter for the initial percentage data stored in the flash memory
	if ((InitialPercentage = atoi(argv[13])) < 0 || InitialPercentage > 100) {
		printf("The range of Initial percentatge of data should be in (0, 100).\n");
	}

	//# metric type
	MetricType = argv[14];
	if (strcmp(MetricType, METRIC_AVERAGE_RESPONSE_TIME) != 0
			&& strcmp(MetricType, METRIC_TOAL_TRANSMISSION_TIME) != 0) {
		printf("The evaluation metric should be ART or TTT.\n");
	}

	//# access type
	EvaluatedAccessType = argv[15];
	if (strcmp(EvaluatedAccessType, TYPE_READ) != 0
			&& strcmp(EvaluatedAccessType, TYPE_WRITE) != 0
			&& strcmp(EvaluatedAccessType, TYPE_READWRITE) != 0) {
		printf("The included access type should be R, W, or RW.\n");
	}

	//* Check whether to do simulation until the first block worn out
	if (argv[16][0] == 'y' || argv[16][0] == 'Y')
		IsDoSimulationUntilFirstBlockWornOut = True;
	else if (argv[16][0] == 'n' || argv[16][0] == 'N')
		IsDoSimulationUntilFirstBlockWornOut = False;
	else {
		printf("ERROR: The IsDoSimulationUntilFirstBlockWornOut parameter should be Y or N.\n");
	}

	//* check the number of times to rewind the input trace file.
	if ((InputFileRewindTimes = atoi(argv[17])) < 0) {
		printf("The number of times to rewind the input trace file can't be equal or smaller than 0.\n");
	}

	//* the erase cycle of every block
	BlockEndurance = atoi(argv[18]);
	//# the number of bad blocks in a chip
	BadBlocksPerChip = atoi(argv[19]);
	//# the number of spare blocks in a chip
	SpareBlocksPerChip = atoi(argv[20]);
	//# the number of blocks in a physical region
	BlocksPerPhysicalRegion = atoi(argv[21]);
	//# the number of blocks in a virtual region
	BlocksPerVirtualRegion = atoi(argv[22]);
	//# the length of the LRU list to cache virtual block tables (only for DISPOSABLE)
	//VBTCacheLength = atoi (argv[23]);
	//# the length of the LRU list to cache virtual block tables (onlye for DISPOSABLE)
	//VPTCacheLength = atoi (argv[24]);
	//# the length of the LRU list to cache parity-check information (onlye for DISPOSABLE)
	//ParityCheckCacheLength = atoi (argv[25]);
	//# argv[26]: the threshold: the number of new bad blocks in a region to trigger region swapping: only for DISPOSABLE
	//RegionSwappingThreshold = atoi (argv[26]);
	//#
	VirtualRegionNum = atoi(argv[27]);
	//#
	SramSize = atoi(argv[28]);
	//#
	LRUListSramLength = atoi(argv[29]);
	//#
	OverProvisionAreaSize = atoi(argv[30]);
	//#
	debugMode = atoi(argv[31]);

	//* ****************************** end of reading argv[x] **********************************

	//* exclude devided-by-error
	if (ChipNumber == 0 || ChipSize == 0 || BlockSize == 0 || PageSize == 0) {
		printf("ERROR: Flash size, block size, or page size should not be 0.\n");
	}
	//* chech whether flash size, block size, and page size are reasonable.
	if (((FlashSize * 1024) % BlockSize != 0) || ((BlockSize * 1024) % PageSize != 0)) {
		printf("ERROR: Flash size, block size, or page size are not a reasonable set of parameters.\n");
	}

	//* Check FTL method
	if ((strcmp(FTLMethod, FTL_FTL) != 0) && (strcmp(FTLMethod, FTL_NFTL) != 0)
			&& (strcmp(FTLMethod, FTL_BL) != 0)
			&& (strcmp(FTLMethod, FTL_STRIPE_STATIC) != 0)
			&& (strcmp(FTLMethod, FTL_STRIPE_DYNAMIC) != 0)
			&& (strcmp(FTLMethod, FTL_DISPOSABLE) != 0)
			&& (strcmp(FTLMethod, FTL_BEYONDADDR) != 0)
			&& (strcmp(FTLMethod, FTL_DFTL) != 0)
			&& (strcmp(FTLMethod, FTL_FAST) != 0)) {
		printf("ERROR: The input FTL_method doesn't exist.\n");
	}

	//* Initialize flash memory
	InitializeFlashMemory();

	//* Show the simluation information
	//printf("Info: Flash size=%dMB, Block size=%dKB, Page size=%d\n%s %s, T=%d, k=%d, Initial data=%d, Rewind times for \"%s\": %d\n\n",FlashSize,BlockSize,PageSize, WLMethod,FTLMethod, SWLThreshold, MappingModeK, InitialPercentage, argv[1],InputFileRewindTimes);
	printf("Info: FlashSize = %d MB, Chips = %d, ChipSize = %d MB, BlockSize = %d KB, PageSize = %d B \n",
			FlashSize, ChipNumber, ChipSize, BlockSize, PageSize);
	printf("MaxLBA: %d, BlocksPerChip: %d, PagesPerBlock %d, SubPagesPerPage %d \n",
			MaxLBA, BlocksPerChip, MaxPage, MaxSubPage);
	printf("Method : %s, Initial data= %d %%, Rewind times for \"%s\": %d \n\n",
			FTLMethod, InitialPercentage, argv[1], InputFileRewindTimes);

	if (strcmp(FTLMethod, FTL_FTL) == 0) //* FTL is selected
		FTLSimulation(InFp);
	else if (strcmp(FTLMethod, FTL_NFTL) == 0) //* NFTL is selected
		NFTLSimulation(InFp);
	else if (strcmp(FTLMethod, FTL_BL) == 0) //* Block Level Mapping is selected
		BLSimulation(InFp);
	else if (strcmp(FTLMethod, FTL_STRIPE_STATIC) == 0 || strcmp(FTLMethod, FTL_STRIPE_DYNAMIC) == 0) //* Li-Pin Chang's striping algorithm is selected
		StripeSimulation(InFp);
	else if (strcmp(FTLMethod, FTL_BEYONDADDR) == 0)
		BeyondAddrMappingSimulation(InFp);
	//else if (strcmp(FTLMethod, FTL_FAST) == 0)		//* FAST is selected
	//	FASTSimulation(InFp);
	else if (strcmp(FTLMethod, FTL_DFTL) == 0)		//* DFTL is selected
		DFTLSimulation(InFp);

	if (!IsDoSimulationUntilFirstBlockWornOut && debugMode == 0) {
		OutputResult(); //* Output the simulation results to log files.
		//* Analize the simulation result
		//AnalyzeResult();
	}

	//* release the data structure for flash memory
	FinalizeFlashMemory();

	//fclose(InFp);	//* clsoe the opened file

	exit(0);
}

