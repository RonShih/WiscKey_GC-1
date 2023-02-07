
#include "lab2_common.h"

#include "global.h"
#include "Flash/main.h"
#include "Flash/flash.h"
#include "Flash/flash.c"
#include "Flash/FTL.h"
#include "Flash/FTL.c"
#include "Flash/OutputResult.c"
#include <unistd.h>
unsigned int microsecond = 1000000;
#ifndef _FLASH_C
#define _FLASH_C
typedef struct Operation{
	AccessType_t AccessType;
	flash_size_t StartCluster;
	flash_size_t Length;
};
flash_size_t FlashSize;		//# the size of the flash-memory storage system
flash_size_t ChipNumber;		//* the number of flash-memory chips
flash_size_t ChipSize;		//* the size of one flash-memory chip(unit:MB)
flash_size_t BlockSize;		//* the size of a block(unit:KB)
flash_size_t BlocksPerChip;	//# the number of block per chip
flash_size_t PageSize;		//* the size of the data area of a page (unit:B)
flash_size_t PageSpareSize;	//# the size of the spare area of a page (unit:B)
flash_size_t PageToLBARatio;	//* the number of sectors that can be stored in one page
flash_size_t MaxLBA;			//* the maximum LBA of the flash memory
flash_size_t MaxCluster;		//* Cluster size = Page size, so that MaxCluster = MaxLBA / PageToLBARatio
flash_size_t MaxBlock;		//* the maximum number of Block
flash_size_t MaxPage;		//* the number of pages in one block
flash_size_t MaxSubPage;		//* the number of pages in one block (only for BEYONDADDR)
flash_size_t BadBlocksPerChip;	//# the number of bad blocks in a chip
flash_size_t EraseBlockTime;	//# the time to erase a block (unit: ns)
flash_size_t WritePageTime;		//# the time to write a page (unit: ns)
flash_size_t ReadPageTime;		//# the time to read a page (unit: ns)
flash_size_t SerialAccessTime;	//# the time to transfer one-byte data between RAM and flash chip (unit: ns)
flash_size_t AccessSRAM;		//# the time to access one instruction from SRAM (unit: ns)
//extern flash_size_t VBTCacheLength;	//# the length of the LRU list to cache Virtual Block Tables, block remapping list, block remapping table, and list of free block sets: only for DISPOSABLE
//extern flash_size_t VPTCacheLength;	//# the length of the LRU list to cache Virtual Page Tables: only for DISPOSABLE
//extern flash_size_t ParityCheckCacheLength;	//# the length of the LRU list to cache parity-check information: only for DISPOSABLE
flash_size_t VirtualRegionNum; // (only for BEYONDADDR)
flash_size_t SramSize; // (only for BEYONDADDR)
flash_size_t LRUListSramLength; // (only for BEYONDADDR)
flash_size_t OverProvisionAreaSize; // (only for FAST)
int debugMode; // (only for BEYONDADDR)

flash_size_t *FreeBlockList;	//* the free block list array

flash_size_t **FlashMemory;	//* FlashMemory[Block_ID][Page_ID]
flash_size_t ***FlashMemoryArray;	//# FlashMemory[Chip_ID][Block_ID][Page_ID] (only for DISPOSABLE management scheme) where Block_ID is the offset inside its own chip
//extern flash_size_t *FlashMemorySub;	//# FlashMemory[Chip_ID][Block_ID][Page_ID][SubPage_offset] (only for BEYONDADDR)
flash_size_t BadBlockCnt;	//# the counter to count the number of bad blocks in the flash memory
//extern flash_size_t *BadBlockCntArray;	//# the counters to count the number of bad blocks in each flash-memory chip (only for DISPOSABLE management scheme)

flash_size_t MaxBlockEraseCnt;	//# The max block erase count among blocks in the flash memory
flash_size_t *BlockEraseCnt;	//* BlockEraseCnt[Block_ID] restores the erase count of each block
flash_size_t *InvalidPageCnt;	//* store the number of invalide pages in each block
flash_size_t *ValidPageCnt;	//* store the number of valid pages in each block
flash_size_t *FreePageCnt;	//* store the number of free subpages in each block (only for BEYONDADDR)
flash_size_t *InvalidSubPageCnt;	//* store the number of invalid subpages in each block (only for BEYONDADDR)
flash_size_t *ValidSubPageCnt;	//* store the number of valid subpages in each block (only for BEYONDADDR)
flash_size_t *FreeSubPageCnt;	//* store the number of free subpages in each block (only for BEYONDADDR)

flash_size_t AccessedLogicalPages;	//# Accumulate the total logical pages being accessed
Boolean *AccessedLogicalPageMap;	//# Point to the map array

//# statistics
Statistics_t AccessStatistics;

flash_size_t FreeBlockListHead;	//* The index of the first free block
flash_size_t BlocksInFreeBlockList;	//* The minimum number of free blocks that should be reserved in the free block list
flash_size_t *BlocksInFreeBlockListArray;	//# Keep track of the number of free block (i.e., the number of blocks in the free block list) of each chip

// ReliableMTD
flash_size_t SubchipsPerChip;		//# the number of subchips in each chip: for ReliableMTD
flash_size_t PlanesPerSubchip;		//# the number of planes in each subchip: for ReliableMTD
flash_size_t BlocksPerPlane;		//# The number of blocks per plane
flash_size_t EncodingUnitsPerpage;	//# the number of encoding units per page
flash_size_t EccCapability;			//# the number of bits that could be corrected
flash_huge_float_t BitErrorRate;	//# the bit error rate
Boolean SystemFailFlg;
#endif

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

static void testing_compaction(DB *db,string key,string value,string &accvalue) 
{
/* Setting Value and Testing it */     
        for(int i=0;i<300000;i++){
        	//cout << i <<endl;
        	key = "key_v_"+std::to_string(i+1);
        	value = "value_v_"+std::to_string(i+1);
        	leveldb_vset(db,key,value);
        }

        
        //check is there something wrong
        bool found2;
	usleep(3 * microsecond);
        for(int i=0;i<30000;i++){
        	key = "key_v_"+std::to_string(i+1);
        	found2 = leveldb_vget(db,key,accvalue);
        	if (found2) {
		//cout << "Record found Matched :"<< key << endl;
		}
        }
	        key = "key_v_"+std::to_string(15000);
        	found2 = leveldb_get(db,key,accvalue);
        	if (found2) {
		cout << "Record found Matched :"<< key << endl;
		}
        //std::string input;
        //scanf("stop!!!!%s",input);
}
int main(int argc, char ** argv)
{
cout << "main of leveldb_test\n";
/*
  if (argc < 2) {
    cout << "Usage: " << argv[0] << " <value-size>" << endl;
    exit(0);
  }
  // value size is provided in bytes
  const size_t value_size = std::stoull(argv[1], NULL, 10);
  if (value_size < 1 || value_size > 100000) {
    cout << "  <value-size> must be positive and less then 100000" << endl;
    exit(0);
  }

  DB * db = open_leveldb("leveldb_test_dir");
  if (db == NULL) {
    cerr << "Open LevelDB failed!" << endl;
    exit(1);
  }
  char * vbuf = new char[value_size];
  for (size_t i = 0; i < value_size; i++) {
    vbuf[i] = rand();
  }
  string value = string(vbuf, value_size);

  size_t nfill = 1000000000 / (value_size + 8);
  clock_t t0 = clock();
  size_t p1 = nfill / 40;
  for (size_t j = 0; j < nfill; j++) {
    string key = std::to_string(((size_t)rand())*((size_t)rand()));
    leveldb_set(db, key, value);
    if (j >= p1) {
      clock_t dt = clock() - t0;
      cout << "progress: " << j+1 << "/" << nfill << " time elapsed: " << dt * 1.0e-6 << endl << std::flush;
      p1 += (nfill / 40);

    }
  }
  clock_t dt = clock() - t0;
  cout << "time elapsed: " << dt * 1.0e-6 << " seconds" << endl;
  	clock_t t1 = clock();
  	for (size_t k = 0; k < 100000; k++) {

                string testingkey = std::to_string(((size_t)rand())*((size_t)rand()));
                string testingvalue = "Abhishek";
                leveldb_set(db,testingkey,testingvalue);
                leveldb_get(db,testingkey,testingvalue);
        }
        clock_t dt1 = clock() - t1;
        cout << "set and read time elapsed: " << dt1 * 1.0e-6 << endl;
*/
	flash_size_t i;
	FILE *InFp; //* input file descriptor

	printf("Author: John Hsu. Built Date: %s, %s\n", __TIME__, __DATE__);

	//* ********************************** Start of reading argv[x] *************************************

	//* check the parameter
	if (argc != 32) {
		printf("ERROR: The number of parameters is not correct\n");
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
	InitializeFTL();
	printf("Info: FlashSize = %d MB, Chips = %d, ChipSize = %d MB, BlockSize = %d KB, PageSize = %d B \n",
			FlashSize, ChipNumber, ChipSize, BlockSize, PageSize);
	printf("MaxLBA: %d, BlocksPerChip: %d, PagesPerBlock %d, SubPagesPerPage %d \n",
			MaxLBA, BlocksPerChip, MaxPage, MaxSubPage);
	printf("Method : %s, Initial data= %d %%, Rewind times for \"%s\": %d \n\n",
			FTLMethod, InitialPercentage, argv[1], InputFileRewindTimes);
	//* leveldb open
	DB * db = open_leveldb("leveldb_test_dir");
	if (db == NULL) {
	  cerr << "Open LevelDB failed!" << endl;
	  exit(1);
	}
	//*testing function
	string key,value,accvalue;
	testing_compaction(db,key,value,accvalue);
	/*
	for(int i=0;i<Operations.size();i++){
		WritewithFTL(Operations[i].AccessType,Operations[i].StartCluster,Operations[i].Length);
	}*/
	
	FTL_mutex.lock();
	vector<Operation>::iterator iter = Operations.begin();
	FTL_mutex.unlock();
	while(true){
		FTL_mutex.lock();
		if(iter==Operations.end()){
			break;
		}else{
			WritewithFTL((*iter).AccessType,(*iter).StartCluster,(*iter).Length);
			iter++;
		}
		FTL_mutex.unlock();	
	}
	if (!IsDoSimulationUntilFirstBlockWornOut && debugMode == 0) {
		OutputResult(); //* Output the simulation results to log files.
	}
	//* release the data structure for flash memory
	FinalizeFlashMemory();
  destroy_leveldb("leveldb_test_dir");
  exit(0);
}
