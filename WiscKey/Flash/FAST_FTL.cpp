//+ INCLUDE PROTECT +//
#ifndef _FAST_FTL_CPP
#define _FAST_FTL_CPP

#include "main.h"
#include "FAST_FTL.h"

//+ ============================================================================================================
//+ FAST_FTL
//+ ============================================================================================================
//+++++++++++++++++++++++++++++
//+ Constructor and Destructor
//+++++++++++++++++++++++++++++
FAST_FTL::FAST_FTL(flash_size_t MaxNumberOfPhysicalBlocks, flash_size_t MaxNumberOfRWLogBlocks)
{
	//+ Initial the FAST coarse-level mapping table (without allocate physical blocks)
	this->fastTable = new PhysicalBlock*[MaxNumberOfPhysicalBlocks];
	for(flash_size_t lba=0; lba<MaxBlock; lba++)
		this->fastTable[lba] = new PhysicalBlock();

	//+ Initial the swLogBlock (without allocate physical blocks)
	this->swLogBlock = new PhysicalBlock();
	
	//+ Initial the rwLogBLockQueue (without allocate physical blocks)
	this->rwLogBlockQueue = new FASTRWLogBlockQueue(MaxNumberOfRWLogBlocks, this);
}
FAST_FTL::~FAST_FTL()
{
	delete[] this->fastTable;
	delete this->swLogBlock;
	delete this->rwLogBlockQueue;
}

//+++++++++++++++++++++++++++++
//+ Read
//+++++++++++++++++++++++++++++
Boolean FAST_FTL::ReadOneCluster(flash_size_t ClusterID)
{
	//+ Convert a Cluster ID into its logical block ID and page offset in that logical block
	flash_size_t LogicalBlock = ClusterID / MaxPage;
	flash_size_t LogicalPage = ClusterID % MaxPage;

	//+ Check whether the corresponding data block exists
	if(this->fastTable[LogicalBlock]->getPBA() == FAST_POINT_TO_NULL)
	{
		return(False);
	}
	else
	{	//# **************************** for SLC ***********************************
		
		//+ Search in SW log block
		if((this->swLogBlock->getPBA()==FAST_POINT_TO_NULL) || (this->swLogBlock->getPBA()!=FAST_POINT_TO_NULL&&FlashMemory[this->swLogBlock->getPBA()][LogicalPage]!=ClusterID))
		{
			//+ Search from RW log block
			if(this->rwLogBlockQueue->ReadOneCluster(ClusterID) == False)
			{
				//+ Check the page at the corresponding offset in FAST table (Data block)
				//+ Staticstics
				AccessStatistics.TotalReadRequestTime += TIME_READ_SPARE_PAGE; //# the time to check the spare area of one page
	
				//+ for PerfectFlashMTD
				/*
				if(IsPerfectFlashMTDActivated==True)
					PerfectFlashMTDReadOnePageSpareArea(this->fastTable[LogicalBlock]->getPBA(), LogicalPage, 'R');
				 */
				//+ Error Check: The corresponding data page should not be an invalid page
				/*
				if(FlashMemory[this->fastTable[LogicalBlock]->getPBA()][LogicalPage]==INVALID_PAGE)
				{
					printf("Error: The corresponding data page of cluster=%d should not be an invalid page", ClusterID);
					system("PAUSE");
					exit(1);
				}
				//*/

				if(FlashMemory[this->fastTable[LogicalBlock]->getPBA()][LogicalPage]!=ClusterID)
				{
					return False;
				}
				else
				{
					//+ Read data block
					//+ for PerfectFlashMTD
					/*
					if(IsPerfectFlashMTDActivated==True)
						PerfectFlashMTDReadOnePage(this->fastTable[LogicalBlock]->getPBA(), LogicalPage, 'R');
					 */
				}
			}
		}
		else
		{
			//+ Read sw log block
			//+ for PerfectFlashMTD
			/*if(IsPerfectFlashMTDActivated==True)
				PerfectFlashMTDReadOnePage(this->swLogBlock->getPBA(), LogicalPage, 'R');
			*/
		}
		//# **************************** end of for SLC ***********************************
	}

	//# Staticstics
	AccessStatistics.TotalReadRequestTime += TIME_READ_PAGE; //# the time to read one page
	return(True);
}

//+++++++++++++++++++++++++++++
//+ Write
//+++++++++++++++++++++++++++++
void FAST_FTL::WriteOneCluster(flash_size_t ClusterID)
{

	//+ Convert a Cluster ID into its logical block ID and page offset in that logical block
	//+ Note that 1 cluster = 1 page
	flash_size_t LogicalBlock = ClusterID / MaxPage;
	flash_size_t LogicalPage = ClusterID % MaxPage;
	flash_size_t DataBlockPBA = this->fastTable[LogicalBlock]->getPBA();

	//+ No data block for this Cluster
	if(DataBlockPBA == FAST_POINT_TO_NULL)
	{
		flash_size_t FreeBlock = this->AllocateFreeBlock(FAST_POINT_TO_NULL, "FAST_FTL WriteOneCluster() for Datablock");
		DataBlockPBA = FreeBlock;
		this->fastTable[LogicalBlock]->setPBA(DataBlockPBA);		//* assign the free block to current logical block
		this->fastTable[LogicalBlock]->setCurrentFreePageID(0);	//* Write data to the replacement block from its first page
	}

	//+ *** Write "ClusterID" into its corresponding page --> we consider MLC flash memory
	//+ Note: The condition in the if("expression") is negative to the Algorithm 1 write(lsn, data) in the paper
	//+ Case 1) Write to coarse-level block (data block)
	if((FlashMemory[DataBlockPBA][LogicalPage]==FREE_PAGE) && (this->fastTable[LogicalBlock]->getCurrentFreePageID()<=LogicalPage)) //* free page in primary block and the latest written page is above the to-be-writtn page
	{	
		//+ Write one cluster
		FlashMemory[DataBlockPBA][LogicalPage] = ClusterID;	//+ Write ClusterID into the spare area
		this->fastTable[LogicalBlock]->setCurrentFreePageID(LogicalPage+1);				//+ Update the CurrentFreePageIDinPB
		ValidPageCnt[DataBlockPBA]++;						//+ advance the number of valid pages in the primary block

		//+ for PerfectFlashMTD
		/*if(IsPerfectFlashMTDActivated==True)
			PerfectFlashMTDWriteOnePage(DataBlockPBA, LogicalPage, 'W');
*/
		//+ Statistics
		AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE; //# the time to write one page
	} 
	//+ Case 2) Write to log block
	else
	{
		//+ Write to log block
		this->WriteToLogBlock(ClusterID, LogicalBlock, LogicalPage);

		//+ ERROR CHECK
		/*
		DataBlockPBA = this->fastTable[LogicalBlock]->getPBA();	//+ Re-calculate the DataBlockPBA
		if(FlashMemory[DataBlockPBA][LogicalPage] != INVALID_PAGE)
		{
			printf("ERROR: The write to log block didn't set the data page as INVALID_PAGE\n");
			system("PAUSE");
			exit(1);
		}
		*/
	}
}
void FAST_FTL::WriteToLogBlock(flash_size_t ClusterID, flash_size_t LogicalBlock, flash_size_t LogicalPage)
{
	//+ Case 1) Reset/Allocate the SW log block and write to a new SW log block
	if(LogicalPage==0) // ( if offset is zero )
	{
		//+ Check whether the SW log block exists or not
		if(this->swLogBlock->getPBA() == FAST_POINT_TO_NULL)
		{
			flash_size_t FreeBlock = this->AllocateFreeBlock(FAST_POINT_TO_NULL, "FAST_FTL WriteToLogBlock() SW Check");
			this->swLogBlock->setPBA(FreeBlock);		//* assign the free block to current logical block
			this->swLogBlock->setCurrentFreePageID(0);	//* Write data to the replacement block from its first page
		}
		else
		{
			//+ Error condition
			if(this->swLogBlock->getCurrentFreePageID() > MaxPage) //+ Error free page ID
			{
				printf("ERROR: Current free page ID %d is over the number of pages in the SW log block\n", this->swLogBlock->getCurrentFreePageID());
				system("PAUSE");
				exit(1);
			}
			//+ Switch or Merge the SW log block
			else
			{
				//+ Check whether the SW log block is filled with sequentially written sectors
				flash_size_t i;
				for(i=0; i<MaxPage; i++)
				{
					if(FlashMemory[this->swLogBlock->getPBA()][i]==FREE_PAGE || FlashMemory[this->swLogBlock->getPBA()][i]==INVALID_PAGE)
						break;
				}

				if(i == MaxPage)
				{
					//+ Perform a switch operation between the SW log block and its corresponding data block
					this->SWLB_Switch();
				}
				else
				{
					//+ Perform a merge operation between the SW log block and its corresponding data block
					this->SWLB_Merge();
				}
			}
			
			//+ Check whether the SW log block exists or not
			if(this->swLogBlock->getPBA() == FAST_POINT_TO_NULL)
			{
				flash_size_t FreeBlock = this->AllocateFreeBlock(FAST_POINT_TO_NULL, "FAST_FTL WriteToLogBlock() SW Check (After SWLB_Switch() or SWLB_Merge())");
				this->swLogBlock->setPBA(FreeBlock);		//+ Assign the free block to current logical block
				this->swLogBlock->setCurrentFreePageID(0);	//+ Write data to the replacement block from its first page
			}
			else
			{
				printf("ERROR: The SWLB_Switch() or SWLB_Merge() operation doesn't reset the SW log block properly!\n");
				printf("The SW Log Block Info: PBA=%d, PageID=%d\n", this->swLogBlock->getPBA(), this->swLogBlock->getCurrentFreePageID());
				system("PAUSE");
				exit(1);
			}
		}

		//+ The new allocated SW log block is ready to use
		this->SWLB_Append(ClusterID, LogicalBlock, LogicalPage);	//+ Append data to the SW log block
	}
	//+ Case 2) Write to SW log block (case 2.1) or RW log block queue (case 2.2)
	else // ( if offset is NOT zero )
	{
		//+ Case 2.1) Write to the SW log block
		//+		The current ClusterID belongs to the SW log block
		if(this->swLogBlock->getPBA()!=FAST_POINT_TO_NULL && LogicalBlock==(FlashMemory[this->swLogBlock->getPBA()][this->swLogBlock->getCurrentFreePageID()-1]/MaxPage)) 
		{
			//+ The LogicalPage can be written into the SW log dirrectly
			if(LogicalPage == this->swLogBlock->getCurrentFreePageID()) 
			{
				//+ Perform an append operation between the SW log block and its corresponding data block
				this->SWLB_Append(ClusterID, LogicalBlock, LogicalPage);
			}
			//+ The LogicalPage cannot be written (doesn't follow the order to write)
			else
			{
				//+ Perform a merge operation between the SW log block and its corresponding data block
				this->SWLB_Merge();

				//+ Check whether the SW log block exists or not
				if(this->swLogBlock->getPBA() == FAST_POINT_TO_NULL)
				{
					flash_size_t FreeBlock = this->AllocateFreeBlock(FAST_POINT_TO_NULL, "WriteToLogBlock() SW Check After SWLB_Merge()");
					this->swLogBlock->setPBA(FreeBlock);		//* assign the free block to current logical block
					
					//+ To avoid the bad condition in SWLB_Append(), so we set the current free page ID with logical page
					this->swLogBlock->setCurrentFreePageID(LogicalPage);	//* Write data to the replacement block from its first page
				}
				else
				{
					printf("ERROR: The SWLB_Merge() operation desn't re-allocate the SW log block properly!");
					system("PAUSE");
					exit(1);
				}

				//+ Perform an append operation between the SW log block and its corresponding data block
				this->SWLB_Append(ClusterID, LogicalBlock, LogicalPage);
			}
		}
		//+ Case 2.2) Write to the RW log block queue
		else
		{
			//+ Write data into the RW log block queue
			this->rwLogBlockQueue->WriteOneCluster(ClusterID, this);
		}
	}
}

//+++++++++++++++++++++++++++++
//+ Sequential Log Block
//+++++++++++++++++++++++++++++
void FAST_FTL::SWLB_Append(flash_size_t ClusterID, flash_size_t LogicalBlock,flash_size_t LogicalPage)
{
	//+ Error Check
	if(LogicalPage != this->swLogBlock->getCurrentFreePageID() )
	{
		printf("ERROR: bad SWLB_Append condition!\n");
		system("PAUSE");
		exit(1);
	}

	//+ Append data to the SW log block & Update the SW log block part of the sector/page mapping table
	FlashMemory[this->swLogBlock->getPBA()][LogicalPage] = ClusterID;
	this->swLogBlock->setCurrentFreePageID(LogicalPage+1); //# Update the CurrentFreePageIDinPB
	//+ Advance the number of valid pages in the primary block
	ValidPageCnt[this->swLogBlock->getPBA()]++;

	//+ Mark the corresponding data page as INVALID_PAGE
	FlashMemory[this->fastTable[LogicalBlock]->getPBA()][LogicalPage] = INVALID_PAGE;
	ValidPageCnt[this->fastTable[LogicalBlock]->getPBA()]--; //+ decrease the number of valid pages
	InvalidPageCnt[this->fastTable[LogicalBlock]->getPBA()]++; //+ increase the number of invalid pages

	//+ Mark the corresponding page in the RW log queue as INVALID_PAGE
	this->rwLogBlockQueue->MarkTheCorrespondingPageInvalid(ClusterID);

	//+ for PerfectFlashMTD
	/*
	if(IsPerfectFlashMTDActivated==True)
		PerfectFlashMTDWriteOnePage(this->fastTable[LogicalBlock]->getPBA(), LogicalPage, 'W');
	 */
	//+ Statistics
	AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE; //+ the time to write one page
}

void FAST_FTL::SWLB_Switch()
{
	//+ Find the ToBeSwitchedDataBlockLBA
	flash_size_t ToBeSwitchedDataBlockLBA = FlashMemory[this->swLogBlock->getPBA()][0]/MaxPage;
	
	//+ Find the ToBeErasedDataBlockPBA
	flash_size_t ToBeErasedDataBlockPBA = this->fastTable[ToBeSwitchedDataBlockLBA]->getPBA();

	//+ Exchange the victim log block with its data block
	this->fastTable[ToBeSwitchedDataBlockLBA]->setPBA(this->swLogBlock->getPBA());
	this->fastTable[ToBeSwitchedDataBlockLBA]->setCurrentFreePageID(this->swLogBlock->getCurrentFreePageID());

	//+ Reset the SW LogBlock
	this->swLogBlock->setPBA(FAST_POINT_TO_NULL);
	this->swLogBlock->setCurrentFreePageID(FAST_POINT_TO_NULL);

	//+ Free the original data block
	PutFreeBlock(ToBeErasedDataBlockPBA);	

	//+ Reset invalid and valid page count
	InvalidPageCnt[ToBeErasedDataBlockPBA] = 0;
	ValidPageCnt[ToBeErasedDataBlockPBA] = 0;
	
	//+ Clear pages as free pages
	for(flash_size_t i=0; i<MaxPage; i++)
	{
		FlashMemory[ToBeErasedDataBlockPBA][i] = FREE_PAGE;
	}

	//+ Statistics
	AccessStatistics.TotalWriteRequestTime += TIME_ERASE_BLOCK; //# the time to copy a live page

	//+ Update the erase count
	BlockEraseCnt[ToBeErasedDataBlockPBA]++;

	//+ Update the max block erase count
	if(BlockEraseCnt[ToBeErasedDataBlockPBA]>MaxBlockEraseCnt)
	{
		MaxBlockEraseCnt = BlockEraseCnt[ToBeErasedDataBlockPBA];
	}

	//+ Update total number of block erases
	AccessStatistics.TotalBlockEraseCount += 1;
}

void FAST_FTL::SWLB_Merge()
{
	//+ Retrieve one free block from the free block list
	flash_size_t FreeBlock = this->AllocateFreeBlock(this->swLogBlock->getPBA(), "SWLB_Merge()");

	//+ Define the shortcuts of data block and SW log block
	flash_size_t DataBlockLBA = FlashMemory[this->swLogBlock->getPBA()][this->swLogBlock->getCurrentFreePageID()-1]/MaxPage;
	flash_size_t DataBlockPBA = this->fastTable[DataBlockLBA]->getPBA();
	flash_size_t SWBlockPBA = this->swLogBlock->getPBA();

	bool copy;
	//+ Migrate the live-pages from the data block and the SW log block to the new free block
	for(flash_size_t i=0; i<MaxPage; i++)
	{
		copy = false;

		//+ Move one page from the SW log block to the new block
		if((FlashMemory[SWBlockPBA][i]!=INVALID_PAGE) && (FlashMemory[SWBlockPBA][i]!=FREE_PAGE))
		{
			copy = true;

			FlashMemory[FreeBlock][i] = FlashMemory[SWBlockPBA][i];
			ValidPageCnt[FreeBlock]++; //+ Advance the number of valid pages

			ValidPageCnt[SWBlockPBA]--; //+ Decrease the number of valid pages
			InvalidPageCnt[SWBlockPBA]++; //+ Increase the number of invalid pages

			//+ for PerfectFlashMTD
			/*
			if(IsPerfectFlashMTDActivated==True)
			{
				PerfectFlashMTDReadOnePage(SWBlockPBA, i, 'L');
				PerfectFlashMTDWriteOnePage(FreeBlock, i, 'L');
			}
			 */
			//+ Statistics
			AccessStatistics.TotalLivePageCopyings++;	//+ Adance the total live-page copyings by 1.
			AccessStatistics.TotalWriteRequestTime += TIME_READ_PAGE+TIME_WRITE_PAGE; //+ The time to copy a live page
		}
		
		//+ Move one page from the data block to the new block
		if((FlashMemory[DataBlockPBA][i]!=INVALID_PAGE) && (FlashMemory[DataBlockPBA][i]!=FREE_PAGE))
		{
			if(copy == true)
			{
				printf("ERROR: SWLB_Merge() try to copy a page from sw log block and data block\n");
				system("PAUSE");
				exit(1);
			}

			copy = true;

			FlashMemory[FreeBlock][i] = FlashMemory[DataBlockPBA][i];
			ValidPageCnt[FreeBlock]++; //+ advance the number of valid pages

			ValidPageCnt[DataBlockPBA]--; //+ decrease the number of valid pages
			InvalidPageCnt[DataBlockPBA]++; //+ increase the number of invalid pages

			//+ for PerfectFlashMTD
			/*
			if(IsPerfectFlashMTDActivated==True)
			{
				PerfectFlashMTDReadOnePage(DataBlockPBA, i, 'L');
				PerfectFlashMTDWriteOnePage(FreeBlock, i, 'L');
			}
			 */
			//Statistics;
			AccessStatistics.TotalLivePageCopyings++;	//+ Adance the total live-page copyings by 1.
			AccessStatistics.TotalWriteRequestTime += TIME_READ_PAGE+TIME_WRITE_PAGE; //#the time to copy a live page
		}
	}

	//+ *** Clean up the data block and the SW log block as a free block ***
	//+ Free the original data block and the SW log block
	PutFreeBlock(DataBlockPBA);
	PutFreeBlock(SWBlockPBA);

	//+ Reset invalid and valid page count
	InvalidPageCnt[DataBlockPBA] = 0;
	InvalidPageCnt[SWBlockPBA] = 0;
	ValidPageCnt[DataBlockPBA] = 0;
	ValidPageCnt[SWBlockPBA] = 0;

	//+ Clear pages as free pages
	for(flash_size_t i=0; i<MaxPage; i++)
	{
		FlashMemory[DataBlockPBA][i] = FREE_PAGE;
		FlashMemory[SWBlockPBA][i] = FREE_PAGE;
	}

	//+ for PerfectFlashMTD
	/*
	if(IsPerfectFlashMTDActivated==True)
	{
		PerfectFlashMTDEraseOneBlock(DataBlockPBA, 'L');
		PerfectFlashMTDEraseOneBlock(SWBlockPBA, 'L');
	}
	 */
	//+ Statistics
	AccessStatistics.TotalWriteRequestTime += TIME_ERASE_BLOCK * 2; //# the time to copy a live page

	//+ Update the erase count
	BlockEraseCnt[DataBlockPBA]++;
	BlockEraseCnt[SWBlockPBA]++;
	
	//+ Update the max block erase count
	if(BlockEraseCnt[DataBlockPBA]>MaxBlockEraseCnt)
	{
		MaxBlockEraseCnt = BlockEraseCnt[DataBlockPBA];
	}

	if(BlockEraseCnt[SWBlockPBA]>MaxBlockEraseCnt)
	{
		MaxBlockEraseCnt = BlockEraseCnt[SWBlockPBA];
	}

	//+ Update total number of block erases
	AccessStatistics.TotalBlockEraseCount += 2;

	//+ Update the FAST Table
	this->fastTable[DataBlockLBA]->setPBA(FreeBlock);
	this->fastTable[DataBlockLBA]->setCurrentFreePageID(MaxPage);
	this->swLogBlock->setPBA(FAST_POINT_TO_NULL);
	this->swLogBlock->setCurrentFreePageID(FAST_POINT_TO_NULL);
}

//+++++++++++++++++++++++++++++
//+ Set/Get fastTable
//+++++++++++++++++++++++++++++
void FAST_FTL::setFastTable(flash_size_t LBA, flash_size_t PBA, flash_size_t CurrentFreePageID)
{
	this->fastTable[LBA]->setPBA(PBA);
	this->fastTable[LBA]->setCurrentFreePageID(CurrentFreePageID);
}
flash_size_t FAST_FTL::getFastTable(flash_size_t LBA)
{
	return this->fastTable[LBA]->getPBA();
}


//+++++++++++++++++++++++++++++
//+ Free Block Allocation
//+++++++++++++++++++++++++++++
flash_size_t FAST_FTL::AllocateFreeBlock(flash_size_t NotToReclaimPBA, char* CallerName)
{
	flash_size_t FreeBlock;
	//+ Allocate a new free block
	while((FreeBlock=GetFreeBlock(False)) == -1)
	{
		//+ Do garbage collection to reclaim one more free block if there is no free block in the free block list
		this->GarbageCollection(NotToReclaimPBA);	
	}

	//+ Free block validation
	for(flash_size_t i=0; i<MaxPage; i++)
	{
		if(FlashMemory[FreeBlock][i]!=FREE_PAGE)
		{
			printf("ERROR: The free block is not composed of free pages!\n");
			printf("Info: PBA=%d\n",FreeBlock);
			system("PAUSE");
			exit(1);
		}
	}
	
	return FreeBlock;
}

//+++++++++++++++++++++++++++++
//+ Garbage Collection
//+++++++++++++++++++++++++++++
//+ Do garbage collection to reclaim one more free block if there is no free block in the free block list
//+ "NotToReclaimBlockSet" is the block set that we can not reclaim
void FAST_FTL::GarbageCollection(flash_size_t NotToReclaimPBA)
{
	if(this->rwLogBlockQueue->getFrontAddOnePBA()!=FAST_POINT_TO_NULL && this->rwLogBlockQueue->getFrontAddOnePBA()!=NotToReclaimPBA)
	{
		this->rwLogBlockQueue->MergeTheFrontAddOne();
	}
	else if(this->swLogBlock->getPBA()!=FAST_POINT_TO_NULL && this->swLogBlock->getPBA()!=NotToReclaimPBA)
	{
		this->SWLB_Merge();
	}
	else
	{
		printf("ERROR: No proper physical block can be reclaimed!\n");
		system("PAUSE");
		exit(1);
	}
}

//+ INCLUDE PROTECT +//
#endif
