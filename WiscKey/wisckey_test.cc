#include "lab2_common.h"
#include <fstream>
#include <algorithm> 
#include <vector>      
#include <ctime>       
#include <cstdlib>    
#include <signal.h>

#include "Flash/typedefine.h"
#include "Flash/main.h"
#include "Flash/flash.h"
#include "Flash/flash.c"
#include "Flash/FTL.h"
#include "Flash/FTL.c"
#include "Flash/OutputResult.c"
#ifndef _FLASH_C
#define _FLASH_C
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



size_t value_size;
unsigned long GC = 0;
// Author: John Hsu
// Program: WiscKey Key Value Store with GC

typedef struct WiscKey {
  string dir;
  DB * leveldb;
  FILE * logfile;
} WK;

std::string headkey = "head";
size_t headkey_size = 32;
std::string tailkey = "tail";
std::string GCkey = "GC";
void logfile_GC(WK * wk,std::string &accvalue,std::string &key,std::string &value){
	int goal = 1*1024*1024;// a chunk of logfile too big will crash byte
	bool found;
	std::string::size_type sz;
  	unsigned long head,tail;
  	key = headkey;
  	bool foundhead = leveldb_get(wk->leveldb,key,accvalue);
  	if(foundhead){
  		head = stol(accvalue,&sz);
  	}else{
  		head = 0;
  	}
  	key = GCkey;
  	bool foundtail = leveldb_get(wk->leveldb,key,accvalue);
  	if(foundtail){
  		GC = stol(accvalue,&sz);
  	}else{
  		GC = 0;
  	}
  	long offset = tail;//find the last pos in the logfile
  	long length = std::stol ("32",&sz);
  	long size = length;
	long i =  std::stol ("0",&sz);
	while(i<=goal&&tail+length*2<head)
	{
		
		GC++;
		//access value and key from logfile
		//access key
	  	{	
	  		fseek(wk->logfile,SEEK_SET,tail+i);
	  		fread(&accvalue,length,1,wk->logfile);
	  	}
	  	key = accvalue;
	  	i+=sizeof(key);
	  	//access value
	  	{
	  		fseek(wk->logfile,SEEK_SET,tail+i);
	  		fread(&accvalue,length,1,wk->logfile);
	  	}
	  	value = accvalue;
	  	i+=sizeof(value);
	  	//value = std::to_string(GC);
	  	key = GCkey;
	  	found = leveldb_get(wk->leveldb,GCkey,accvalue);
	  	if(found){
	  		
	  		//append to logfile
	  		size = sizeof(GCkey);
		  	{	
		  		fwrite(&GCkey,size,1,wk->logfile);
		  	}
	  		//insert to LSM tree
	  		offset = ftell(wk->logfile);
	  		value = std::to_string(GC);
	  		size = sizeof(value);
			leveldb_set(wk->leveldb,GCkey,value);//write to LSMTree
			{	
				fwrite(&value, size,1,wk->logfile);//寫到vlog裡面
			}
	  		//head tail update
	  		offset = ftell(wk->logfile);
	  		tail += sizeof(key)+sizeof(value);
	  		value = std::to_string(offset);
	  		leveldb_set(wk->leveldb,headkey,value);
	  		value = std::to_string(GC);
	  		leveldb_set(wk->leveldb,GCkey,value);
	  	}else{
	  		//do nothing
	  	}
	  	
	}
}
void do_logfileGC(WK *wk,std::string &accvalue,std::string &key,std::string &value){
	if(compacted){
		logfile_GC(wk,accvalue,key,value);
		compacted = false;
	}
}

/* Commands for execution with gdb
1. gdb ./wisckey_test
2. r Example.log 4 16384 256 2048 64 1500 800 60 25 5 FTL 10 ART RW N 0 10000 200 512 512 500 4 8 8 10 2 2 8 4 0.0001 N Y 4 4 4 4 4 256 32 800 N
*/
static void wisckey_set(WK * wk, string &key, string &value,string &accvalue)//main wiskcey_set
{
	string tmpkey = key;
	string tmpvalue = value;
	do_logfileGC(wk,accvalue,key,value);//**Future trace
	key = tmpkey;
	value = tmpvalue;
	long offset = ftell(wk->logfile);//Get the start location of file to write
	long size = sizeof(key);//Get the size of key

	fwrite (key.c_str(), size, 1, wk->logfile);//Write "key" into Value Log (logfile)

	string vlog_offset = std::to_string(offset), vlog_size = std::to_string(size);//Convert to string
	stringstream vlog_value;
	vlog_value << vlog_offset << "&&" << vlog_size;
	string s = vlog_value.str();//offset + size to help retrieve value

	fwrite (s.c_str(), size, 1, wk->logfile);//Write offset+size into Value Log (logfile)

	leveldb_set(wk->leveldb, key, s);//Put current key/value into leveldb (LSM_tree)
	cout << "key: " << key << ", value: " << s.c_str() << endl;
	std::string headvalue = std::to_string(offset + size);
	leveldb_set(wk->leveldb, headkey, headvalue);//Put headkey/headvalue into leveldb (LSM_tree)
	cout << "key: " << key << ", value: " << headvalue << endl;
}

/* Process of wisckey_get(): 
* 1. key --> db --> VTable entry addr.
* 2. VTable entry addr --> target offset&&length to Vlog.
* 3. offset&&length --> read offset+length of Vlog --> get real value.
*/
static bool wisckey_get(WK * wk, string &key, string &value,string &accvalue)
{	
	string::size_type sz;
	string tmpkey = key;
	//compaction
	do_logfileGC(wk,accvalue,key,value);
	key = tmpkey;
	cout << "\n*In Get Function, Key Received: " << key << "\n";

	/* 1. Get: key --> db --> VTable entry addr. */
	string Vtable_str_addr;
	bool found = leveldb_get(wk->leveldb, key, Vtable_str_addr);
	if (found) {
		cout << "VTable address of this key: " << Vtable_str_addr << endl;
	}
	else {
		cout << "Record: Not Found" << endl;
		return false;
	}
	
	/* 2. VTable entry addr --> target offset&&length to Vlog */
	/* Convert str_type address into ptr_type address. */
	stringstream str_to_addr;
	uintptr_t target_addr;
	str_to_addr << Vtable_str_addr;
	str_to_addr >> std::hex >> target_addr;
	valueinfo *value_addr = reinterpret_cast<valueinfo*>(target_addr);
	string s = value_addr->value;

	/* Retrieve the value of this address, get value: "offset&&length", and disassemble it. */
	string value_offset, value_length, delimiter = "&&", token;
	size_t pos = 0;
	while ((pos = s.find(delimiter)) != std::string::npos) {
		token = s.substr(0, pos);
		value_offset = token;
		s.erase(0, pos + delimiter.length());
	}
	value_length = s;
	
	long offset = std::stol (value_offset, &sz);
	long length = std::stol (value_length, &sz);
	cout << "Retrieve value from VTable address: " << "key: " << value_addr->key << ", offset: " << offset << ", length: " << length << endl;	

	/* 3. offset&&length --> read offset+length of Vlog --> get real value. */
	fseek(wk->logfile, offset, SEEK_SET);//Start from offset
	fread(&key[0], length, 1, wk->logfile);//Read length bytes
	fseek(wk->logfile, offset+length, SEEK_SET);//Start from offset+length
	fread(&value[0], length, 1, wk->logfile);//Read length bytes
	cout << "Read Value Log,"  << " key: " << key << ", value: " << value << endl;
}	


static void wisckey_del(WK * wk, string &key,string &value,string &accvalue)
{	
	std::string tmpkey = key;
	do_logfileGC(wk,accvalue,key,value);
	key = tmpkey;
	//純粹從level DB刪掉資料  gc？？？
 	cout << "Key: " << key << endl; 
	leveldb_del(wk->leveldb,key);
}

static WK * open_wisckey(const string& dirname)
{
	WK * wk = new WK;
	wk->logfile = fopen("logfile","w+");
	
	wk->leveldb = open_leveldb(dirname,wk->logfile);
  	wk->dir = dirname;
  	return wk;
}
static void head_tail_insert(WK *wk,std::string &key,std::string &value){
	value = "0";
	key = headkey;
	leveldb_set(wk->leveldb,key,value);
	key = tailkey;
	leveldb_set(wk->leveldb,key,value);
	key = GCkey;
	leveldb_set(wk->leveldb,key,value);
}
static void close_wisckey(WK * wk)
{
	fclose(wk->logfile);
  	delete wk->leveldb;
  	delete wk;
}


static void testing_function(WK *wk,string key,string &value,string &accvalue){
	
}


void testing_compaction(WK *wk,string key,string &value,string &accvalue) 
{
/* Setting Value and Testing it */     
	//raise(SIGINT);
	int test_case = 1000000;
	for(int i=0;i<test_case;i++){
		key = "key_v_"+std::to_string(i);
		value = "value_v_"+std::to_string(i);
		wisckey_set(wk,key,value,accvalue);//set key key_v_i and value value_v_i for each i
	}

	//check is there something wrong
	bool found = false;
	key = "key_v_"+std::to_string(0);
	found = wisckey_get(wk,key,value,accvalue);
	if (found) cout << "Record found Matched: "<< key << endl;
	else cout << "No record\n" << endl;

	key = "key_v_"+std::to_string(1);
	found = wisckey_get(wk,key,value,accvalue);
	if (found) cout << "Record found Matched: "<< key << endl;
	else cout << "No record\n" << endl;

	key = "key_v_"+std::to_string(2000000);
	found = wisckey_get(wk,key,value,accvalue);
	if (found) cout << "Record found Matched: "<< key << endl;
	else cout << "No record\n" << endl;

	key = "key_v_"+std::to_string(2);
	found = wisckey_get(wk,key,value,accvalue);
	if (found) cout << "Record found Matched: "<< key << endl;
	else cout << "No record\n" << endl;

	//std::string input;
	//scanf("stop!!!!%s",input); 
}

static void datainsert(WK *wk,std::string &key,std::string &value,std::string &accvalue)
{
	char * vbuf = new char[value_size];
  	for (size_t i = 0; i < value_size; i++) {
    		vbuf[i] = rand();
  	}
  	value = string(vbuf, value_size);
  	
 	size_t nfill = 1000000000 / (value_size + 8);
  	clock_t t0 = clock();
  	size_t p1 = nfill / 40;
  	for (size_t j = 0; j < nfill; j++) {
    		key = std::to_string(((size_t)rand())*((size_t)rand()));
    		wisckey_set(wk, key, value,accvalue);
   		if (j >= p1) {
      			clock_t dt = clock() - t0;
      			cout << "progress: " << j+1 << "/" << nfill << " time elapsed: " << dt * 1.0e-6 << endl << std::flush;
      			p1 += (nfill / 40);
    		}    
  	}
  	
  	clock_t dt = clock() - t0;
  	cout << "time elapsed: " << dt * 1.0e-6 << " seconds" << endl;
}
void reopenlogfile(WK * wk){
	fclose(wk->logfile);
  	wk->logfile = fopen("logfile","w+");
}

int main(int argc, char ** argv)
{
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
  	
  	std::string testkey;
  	std::string testvalue;
  	std::string accvalue;

  	WK * wk = open_wisckey("wisckey_test_dir");
  	if (wk == NULL) {
    		cerr << "Open WiscKey failed!" << endl;
    		exit(1);
  	}
  	
  	/*
  	這裡是測試get set delete 都OK
  	*/
  	testing_compaction(wk,testkey,testvalue,accvalue);
	cout << "here\n";
  	/*
  	這裡是測試
  	*/
  	/*Without reopen, it will cause segamentation fault*/
  	//reopenlogfile(wk);
  	//datainsert(wk,testkey,testvalue,accvalue);
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
	close_wisckey(wk);
	destroy_leveldb("wisckey_test_dir");       
	remove("logfile");
	exit(0);

}

/*
static void wisckey_set_v2(WK * wk, string &key, string value,string &accvalue)
{	
	//size insert
	long offset = ftell(wk->logfile);
	long size = sizeof(key);

	{	
		fwrite (&key, size,1,wk->logfile);
	}
	
	offset = ftell(wk->logfile);
	size = sizeof(value);
	
	std::string vlog_offset = std::to_string(offset);
	std::string vlog_size = std::to_string(size);
	std::stringstream vlog_value;
	vlog_value << vlog_offset << "&&" << vlog_size;//紀錄偏移量還有size等等就知道要從哪裡取value值(get會用到)
	std::string s = vlog_value.str();
	{	
		fwrite (&value, size,1,wk->logfile);//寫到vlog裡面
	}
	
	//更新head
	leveldb_vset(wk->leveldb,key,s);
	std::string headvalue = std::to_string(offset+size);
	leveldb_vset(wk->leveldb,headkey,headvalue);
}
*/

/*
static bool wisckey_get(WK * wk, string &key, string &value,string &accvalue)
{	std::string tmpkey = key;
	//compaction
	do_logfileGC(wk,accvalue,key,value);
	key = tmpkey;
	cout << "\n\t\tGet Function\n\n";
	cout << "Key Received: " << key << endl;

	string offsetinfo;
	bool found = leveldb_get(wk->leveldb, key, offsetinfo);
	if (found) {
		cout << "Offset and Length: " << offsetinfo << endl;//call by address直接修改成可以用來取值的offset以及value size
	}
	else {
			cout << "Record:Not Found" << endl;
	return false;
	}

	//這裡做拆解offset資訊
	std::string value_offset;
	std::string value_length;
	std::string s = offsetinfo;
	std::string delimiter = "&&";
	size_t pos = 0;
	std::string token;
	while ((pos = s.find(delimiter)) != std::string::npos) {
    		token = s.substr(0, pos);
		value_offset = token;
    		s.erase(0, pos + delimiter.length());
	}
	value_length = s;
	//這裡做拆解offset資訊
	//cout << "Value Offset: " << value_offset << endl;
	//cout << "Value Length: " << value_length << endl;

  	std::string::size_type sz;
  	long offset = std::stol (value_offset,&sz);
	long length = std::stol (value_length,&sz);
	
	//rewind(wk->logfile);
	//cout << offset << length << endl;
	std::string value_record;
	//cout << ftell(wk->logread) << endl;
	
	fseek(wk->logfile,offset,SEEK_SET);
	fread(&value,length,1,wk->logfile);
	fseek(wk->logfile,offset-32,SEEK_SET);
	fread(&key,length,1,wk->logfile);
	
	//rewind(wk->logfile);
	cout << "Value Key: " <<key<<endl;
	cout << "LogFile Value: " << value << endl;
	return true;
}	
*/