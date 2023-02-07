//+ FAST.h : All the simulation for FAST are defined in this file
//+ Dynamic wear leveling: using round-robin strategy for its GC
//+ Static wear leveling: activated when the BlockErase_count/flag_count > SWLThreshold "T"

//+ CPP SUPPORT +//
#ifdef __cplusplus
extern "C" {
#endif

//+ INCLUDE PROTECT +//
#ifndef _FAST_FTL_H
#define _FAST_FTL_H

//+ Used in FASTTable
#define FAST_POINT_TO_NULL (-1) //* point to NULL 

#include "typedefine.h"

class FAST_FTL;
class FASTRWLogBlockQueue;
class PhysicalBlock;

//+ ============================================================================================================
//+ PhysicalBlock: Define the pair of block addresses (LBA to PBA)
//+ ============================================================================================================
class PhysicalBlock
{
public:
	PhysicalBlock()
	{
		this->pba = FAST_POINT_TO_NULL;
		this->currentFreePageID = FAST_POINT_TO_NULL;
	}
	void setPBA(flash_size_t pba){ this->pba = pba; };
	flash_size_t getPBA(){ return this->pba; };
	void setCurrentFreePageID(flash_size_t currentFreePageID){ this->currentFreePageID = currentFreePageID; };
	flash_size_t getCurrentFreePageID(){ return this->currentFreePageID; };
private:
	flash_size_t pba;			//+ the physical block address
	short currentFreePageID;	//+ the next free page index that can be written
};

//+ ============================================================================================================
//+ FAST_FTL
//+ ============================================================================================================
class FAST_FTL
{
private:
	//+ The table stored the coarse-level mapping information
	PhysicalBlock** fastTable;
	PhysicalBlock* swLogBlock;
	FASTRWLogBlockQueue* rwLogBlockQueue;
	
	//+ Write to log block functions
	void WriteToLogBlock(flash_size_t ClusterID, flash_size_t LogicalBlock, flash_size_t LogicalPage);
	//+ Sequential Log Block
	void SWLB_Append(flash_size_t ClusterID, flash_size_t LogicalBlock,flash_size_t LogicalPage);
	void SWLB_Switch();
	void SWLB_Merge();
	//+ Random Log Block Queue
	//+ NOTE: ALL FUNCTIONS of RW QUEUE will be implemented in the class FASTRWLogBlockQueue

public:
	//+ Constructor and Destructor
	FAST_FTL(flash_size_t MaxNumberOfPhysicalBlocks, flash_size_t MaxNumberOfRWLogBlocks);
	~FAST_FTL();

	//+ Set/Get fastTable
	void setFastTable(flash_size_t LBA, flash_size_t PBA, flash_size_t CurrentFreePageID);
	flash_size_t getFastTable(flash_size_t LBA);

	//+ Read
	Boolean ReadOneCluster(flash_size_t ClusterID);
	//+ Write
	void WriteOneCluster(flash_size_t ClusterID);
	//+ Garbage Collection
	void GarbageCollection(flash_size_t NotToReclaimPBA);
	//+ Free Block Allocation
	flash_size_t AllocateFreeBlock(flash_size_t NotToReclaimPBA, char* CallerName);
};

//+ ============================================================================================================
//+ FASTRWLogBlockQueue
//+ ============================================================================================================
class FASTRWLogBlockQueue
{
private:
	int size;
	int front,rear;
	PhysicalBlock** blocks;
	FAST_FTL* fast_ftl;

	void EnQueue(PhysicalBlock* b)
	{
		if(isFull()!=true)
		{
			rear = (rear+1)%size;
			blocks[rear] = b;
		}
		else
		{
			//+ The queue is full
			printf("ERROR: The RW queue is full\n");
			system("PAUSE");
			exit(1);
		}
	}

	void DeQueue()
	{
		PhysicalBlock* b;
		if(isEmpty()!=true)
		{
			b = blocks[(front+1)%size];
			front = (front+1)%size;
			delete b;
		}
		else
		{
			//+ The queue is empty
			//+ ERROR
			printf("ERROR: The queue is empty, cannot be dequeue anymore");
			exit(1);
		}
	}

	bool isFull() { return (((rear+1)%size)==front); }

	bool isEmpty() { return (front==rear); }

	//+ Provide the logical to physical page-level mapping information (in rw log block)
	flash_size_t** table;
	flash_size_t getPPAinRWLogBlock(flash_size_t LPA) { return table[LPA/MaxPage][LPA%MaxPage];	}
	void setPPAinRWLogBlock(flash_size_t LPA, flash_size_t PPA) { table[LPA/MaxPage][LPA%MaxPage] = PPA;	}

public:
	FASTRWLogBlockQueue(flash_size_t size, FAST_FTL* fast_ftl)
	{
		this->size = size;
		blocks = new PhysicalBlock*[size];
		front = 0; //+ Always points to an NULL block
		rear = 0;
		this->fast_ftl = fast_ftl;

		//+ Initial the page-level table
		this->table = new flash_size_t*[MaxBlock];
		for(flash_size_t i=0; i<MaxBlock; i++)
		{
			this->table[i] = new flash_size_t[MaxPage];
			for(flash_size_t j=0; j<MaxPage; j++)
			{
				this->table[i][j] = FAST_POINT_TO_NULL;
			}
		}
	}
	
	~FASTRWLogBlockQueue ()
	{
		delete[] blocks;
		delete[] table;
	}
	
	flash_size_t getFrontAddOnePBA() { return this->blocks[(front+1)%size]->getPBA(); }

	Boolean ReadOneCluster(flash_size_t ClusterID)
	{
		if(isEmpty())
			return False;

		//+ Ver.1) Brute force search
		/*
		for(flash_size_t i=(front+1)%size; i!=(rear+1)%size; i=(i+1)%size)
		{
			for(flash_page_size_t j=0; j<MaxPage; j++)
				if(FlashMemory[this->blocks[i]->getPBA()][j] == ClusterID)
					return True;
		}
		//*/

		//+ Ver.2) Page-level table
		//*
		flash_size_t PPA = this->getPPAinRWLogBlock(ClusterID);
		if(PPA!=FAST_POINT_TO_NULL && FlashMemory[PPA/MaxPage][PPA%MaxPage] == ClusterID)
		{
			//+ Read rw log block
			//+ for PerfectFlashMTD
			/*if(IsPerfectFlashMTDActivated==True)
				PerfectFlashMTDReadOnePage(PPA/MaxPage, PPA%MaxPage, 'R');
			*/
			return True;
		}
		//*/

		return False;
	}

	void WriteOneCluster(flash_size_t ClusterID, FAST_FTL* fast_ftl)
	{
		//+++ Step 0:
		if(this->isEmpty())
		{
			//+ Get a block from the free-block list and add it to the end of the RW log block list
			flash_size_t FreeBlock = fast_ftl->AllocateFreeBlock(FAST_POINT_TO_NULL, "RW WriteOneCluster() Step 0");
			
			//+ Enqueue the free block to into the RW queue
			PhysicalBlock* b = new PhysicalBlock();
			b->setPBA(FreeBlock);		
			b->setCurrentFreePageID(0); //* Write data to the replacement block from its first page
			this->EnQueue(b);
		}

		//+++ Step 1: Check there're enough room
		//+ There're no rooms in the rear log block of the RW log block queue to store data
		if(this->blocks[rear]->getCurrentFreePageID()>=MaxPage) 
		{
			//+ The queue is full
			if(this->isFull())
			{
				//+ Merge/Delete the victim (the first block of the RW log queue) with its corresponding data block
				this->MergeTheFrontAddOne();
			}
			
			//+ Get a block from the free-block list and add it to the end of the RW log block list
			flash_size_t FreeBlock = fast_ftl->AllocateFreeBlock(FAST_POINT_TO_NULL, "RW WriteOneCluster() Step 1");
			
			//+ Enqueue the free block to into the RW queue
			PhysicalBlock* b = new PhysicalBlock();
			b->setPBA(FreeBlock);		
			b->setCurrentFreePageID(0); //* Write data to the replacement block from its first page
			this->EnQueue(b);
		}

		//+++ Step 2: Append data
		//+ Mark the existent page in the RW log queue as INVALID
		this->MarkTheCorrespondingPageInvalid(ClusterID);

		//+ Mark the corresponding data page as INVALID_PAGE
		FlashMemory[fast_ftl->getFastTable(ClusterID/MaxPage)][ClusterID%MaxPage] = INVALID_PAGE;
		ValidPageCnt[fast_ftl->getFastTable(ClusterID/MaxPage)]--; //+ decrease the number of valid pages
		InvalidPageCnt[fast_ftl->getFastTable(ClusterID/MaxPage)]++; //+ increase the number of invalid pages

		//+ Write data into the RW log block queue
		FlashMemory[this->blocks[rear]->getPBA()][this->blocks[rear]->getCurrentFreePageID()] = ClusterID;
		
		//+ for PerfectFlashMTD
		/*if(IsPerfectFlashMTDActivated==True)
			PerfectFlashMTDWriteOnePage(this->blocks[rear]->getPBA(), this->blocks[rear]->getCurrentFreePageID(), 'W');
		*/
		//+ Update the page-level table
		this->setPPAinRWLogBlock(ClusterID, this->blocks[rear]->getPBA()*MaxPage + this->blocks[rear]->getCurrentFreePageID());
		
		//+ Update the current free page id
		this->blocks[rear]->setCurrentFreePageID(this->blocks[rear]->getCurrentFreePageID()+1);

		//+ Update the ValidPageCnt
		ValidPageCnt[this->blocks[rear]->getPBA()]++; //+ advance the number of valid pages
				
		//+ Statistics
		AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE; //+ the time to write one page
	}

	void MarkTheCorrespondingPageInvalid(flash_size_t ClusterID)
	{
		
		//+ Find all pages belonged to the same logical block (CorrespondingLogicalBlock) by scanning the page-level mapping table for the RW log blocks
		
		//+ Ver.1) Brute force search
		/*
		flash_size_t FrontAddOne = (front+1)%size;
		for(flash_size_t RWLBIndex=FrontAddOne; RWLBIndex!=(rear+1)%size; RWLBIndex=(RWLBIndex+1)%size)
		{
			for(flash_size_t j=0; j<MaxPage; j++)
			{
				if( FlashMemory[this->blocks[RWLBIndex]->getPBA()][j] == ClusterID)
				{
					//+ Mark all found pages in the page-level mapping table as invalid state (-1)
					FlashMemory[this->blocks[RWLBIndex]->getPBA()][j] = INVALID_PAGE;
					ValidPageCnt[this->blocks[RWLBIndex]->getPBA()]--; //+ decrease the number of valid pages
					InvalidPageCnt[this->blocks[RWLBIndex]->getPBA()]++; //+ increase the number of invalid pages
					break;
				}
			}
		}
		//*/

		//+ Ver.2) Page-level table
		//*
		flash_size_t PPA = this->getPPAinRWLogBlock(ClusterID);
		if(PPA!=FAST_POINT_TO_NULL && FlashMemory[PPA/MaxPage][PPA%MaxPage] == ClusterID)
		{
			FlashMemory[PPA/MaxPage][PPA%MaxPage] = INVALID_PAGE;
			ValidPageCnt[PPA/MaxPage]--; //+ decrease the number of valid pages
			InvalidPageCnt[PPA/MaxPage]++; //+ increase the number of invalid pages
		}
		this->setPPAinRWLogBlock(ClusterID, FAST_POINT_TO_NULL);
		//*/
	}

	void MergeTheFrontAddOne()
	{
		//+ Check whether the queue is empty
		if (this->isEmpty())
		{
			printf("ERROR: The FASTRWLogBlockQueue log block queue is empty\n");
			exit(1);
		}
		//+ Merge the front RW log block
		else
		{
			flash_size_t FrontAddOne = (front+1)%size;
			flash_size_t CorrespondingPBA = this->blocks[FrontAddOne]->getPBA();

			//+ Scan all pages in the first/front RW log block
			flash_size_t CorrespondingLBA, FreeBlock;
			for(flash_size_t i=0; i<MaxPage; i++)
			{
				//+ Calculate CorrespondingLogicalBlock
				CorrespondingLBA = FlashMemory[CorrespondingPBA][i]/MaxPage;

				//+ If the current page is a valid page
				if((FlashMemory[CorrespondingPBA][i]!=INVALID_PAGE) && (FlashMemory[CorrespondingPBA][i]!=FREE_PAGE))
				{
					//+ Get a block from the free-block list and add it to the end of the RW log block list
					FreeBlock = fast_ftl->AllocateFreeBlock(CorrespondingPBA, "RW MergeTheFrontAddOne()");
			
					//+ Find all pages belonged to the same logical block (CorrespondingLogicalBlock) by scanning the page-level mapping table for the RW log blocks
					
					//+ Ver.1) Brute force search
					/*
					for(flash_size_t RWLBIndex=FrontAddOne; RWLBIndex!=(rear+1)%size; RWLBIndex=(RWLBIndex+1)%size)
					{
						for(flash_size_t j=0; j<MaxPage; j++)
						{
							//+ Check whether the page has the same LogicalBlock number
							if(FlashMemory[this->blocks[RWLBIndex]->getPBA()][j]!=FREE_PAGE && FlashMemory[this->blocks[RWLBIndex]->getPBA()][j]!=INVALID_PAGE && (FlashMemory[this->blocks[RWLBIndex]->getPBA()][j]/MaxPage)==CorrespondingLBA)
							{
								if(FlashMemory[FreeBlock][FlashMemory[this->blocks[RWLBIndex]->getPBA()][j]%MaxPage] != FREE_PAGE)
								{
									printf("ERROR: The new data page had been occupied!\n");
									system("PAUSE");
									exit(1);
								}

								//+ Copy the most up-to-date version from the RW log blocks to the free data block
								FlashMemory[FreeBlock][FlashMemory[this->blocks[RWLBIndex]->getPBA()][j]%MaxPage] = FlashMemory[this->blocks[RWLBIndex]->getPBA()][j];
								ValidPageCnt[FreeBlock]++; //+ advance the number of valid pages

								//+ Mark all found pages in the page-level mapping table as invalid state (-1)
								MarkTheCorrespondingPageInvalid(FlashMemory[this->blocks[RWLBIndex]->getPBA()][j]);
								
								//+ Statistics;
								AccessStatistics.TotalLivePageCopyings++;	//+ adance the total live-page copyings by 1.
								AccessStatistics.TotalWriteRequestTime += TIME_READ_PAGE+TIME_WRITE_PAGE; //+ the time to copy a live page
							}
						}
					}
					//*/
					//+ Ver.2) Page-level table
					//+ Find all pages had the same LogicalBlock number
					//*
					flash_size_t PPA, offset;
					for(flash_size_t j=0; j<MaxPage; j++)
					{
						PPA = this->getPPAinRWLogBlock(CorrespondingLBA*MaxPage+j);

						if(PPA!=FAST_POINT_TO_NULL && FlashMemory[PPA/MaxPage][PPA%MaxPage]!=FREE_PAGE && FlashMemory[PPA/MaxPage][PPA%MaxPage]!=INVALID_PAGE && (FlashMemory[PPA/MaxPage][PPA%MaxPage]/MaxPage)==CorrespondingLBA)
						{
							offset = FlashMemory[PPA/MaxPage][PPA%MaxPage]%MaxPage;

							if(FlashMemory[FreeBlock][offset] != FREE_PAGE)
							{
								printf("ERROR: The new data page had been occupied!\n");
								system("PAUSE");
								exit(1);
							}
	
							//+ Copy the most up-to-date version from the RW log blocks to the free data block
							FlashMemory[FreeBlock][offset] = FlashMemory[PPA/MaxPage][PPA%MaxPage];
							ValidPageCnt[FreeBlock]++; //+ advance the number of valid pages

							//+ Mark all found pages in the page-level mapping table as invalid state (-1)
							MarkTheCorrespondingPageInvalid(FlashMemory[PPA/MaxPage][PPA%MaxPage]);
							
							//+ for PerfectFlashMTD
							/*
							if(IsPerfectFlashMTDActivated==True)
							{
								PerfectFlashMTDReadOnePage(PPA/MaxPage, PPA%MaxPage, 'L');
								PerfectFlashMTDWriteOnePage(FreeBlock, offset, 'L');
							}
							 */
							//+ Statistics;
							AccessStatistics.TotalLivePageCopyings++;	//+ adance the total live-page copyings by 1.
							AccessStatistics.TotalWriteRequestTime += TIME_READ_PAGE+TIME_WRITE_PAGE; //+ the time to copy a live page
						}
					}
					//*/

					//+ Copy the live page in the data block
					flash_size_t DataBlockPBA = this->fast_ftl->getFastTable(CorrespondingLBA);
					for(flash_size_t k=0; k<MaxPage; k++)
					{
						//+ If the corresponding page of the data block is not free, we should do at least one time live-page-copying
						if(FlashMemory[DataBlockPBA][k]!=FREE_PAGE && FlashMemory[DataBlockPBA][k]!=INVALID_PAGE)
						{
							//+ Copy the valid data from the data block to the free block
							if(FlashMemory[FreeBlock][k]==FREE_PAGE)
							{
								FlashMemory[FreeBlock][k] = FlashMemory[DataBlockPBA][k];
								ValidPageCnt[FreeBlock]++; //+ advance the number of valid pages
								
								//+ for PerfectFlashMTD
								/*if(IsPerfectFlashMTDActivated==True)
								{
									PerfectFlashMTDReadOnePage(DataBlockPBA, k, 'L');
									PerfectFlashMTDWriteOnePage(FreeBlock, k, 'L');
								}*/

								//+ Statistics;
								AccessStatistics.TotalLivePageCopyings++;	//+ adance the total live-page copyings by 1.
								AccessStatistics.TotalWriteRequestTime += TIME_READ_PAGE+TIME_WRITE_PAGE; //+ the time to copy a live page
							}
							else
							{
								printf("Error: The RW log block and the data block both contain a valid data (ClusterID=%d)\n", FlashMemory[DataBlockPBA][k]);
								system("PAUSE");
								exit(1);
							}
						}
					}

					//+ Free the original data block
					//+ Erase the data block block itself and returns it to the free-block list
					PutFreeBlock(DataBlockPBA);	

					//+ Reset
					//+ *** Clean up the reclaimed block as a free block ***
					//+ reset invalid page count
					InvalidPageCnt[DataBlockPBA] = 0;
					ValidPageCnt[DataBlockPBA] = 0;

					//+ Clear pages as free pages
					for(flash_size_t k=0; k<MaxPage; k++)
					{
						FlashMemory[DataBlockPBA][k] = FREE_PAGE;
					}
					
					//+ for PerfectFlashMTD
					/*if(IsPerfectFlashMTDActivated==True)
						PerfectFlashMTDEraseOneBlock(DataBlockPBA, 'L');
					*/
					//+ Statistics
					AccessStatistics.TotalWriteRequestTime += TIME_ERASE_BLOCK * 1; //+ the time to copy a live page

					//+ Update the erase count
					BlockEraseCnt[DataBlockPBA]++;
					
					//+ Update the MaxBlockEraseCnt
					if(BlockEraseCnt[DataBlockPBA]>MaxBlockEraseCnt)
					{
						MaxBlockEraseCnt = BlockEraseCnt[DataBlockPBA];
					}

					//+ Update total number of block erases
					AccessStatistics.TotalBlockEraseCount += 1;
					
					//* Update the FAST Table
					//+ Exchange the free-block list and erase the data block
					this->fast_ftl->setFastTable(CorrespondingLBA, FreeBlock, MaxPage);
				}
			}

			//+ Erase the front+1 RW log block
			PutFreeBlock(CorrespondingPBA);	

			//+ Reset
			//* *** Clean up the reclaimed block as a free block ***
			//* reset invalid page count
			InvalidPageCnt[CorrespondingPBA] = 0;
			ValidPageCnt[CorrespondingPBA] = 0;
			//* Clear pages as free pages
			for(flash_size_t i=0; i<MaxPage; i++)
			{
				FlashMemory[CorrespondingPBA][i] = FREE_PAGE;
			}


			//+ Statistics
			AccessStatistics.TotalWriteRequestTime += TIME_ERASE_BLOCK; //#the time to copy a live page

			//+ update the erase count
			BlockEraseCnt[CorrespondingPBA]++;
			
			//+ Update the MaxBlockEraseCnt
			if(BlockEraseCnt[CorrespondingPBA]>MaxBlockEraseCnt)
			{
				MaxBlockEraseCnt = BlockEraseCnt[CorrespondingPBA];
			}

			//+ update total number of block erases
			AccessStatistics.TotalBlockEraseCount ++;
			
			//+ Update the FAST Table
			//+ Dequeue
			this->DeQueue();
		}
	}
};

//+ INCLUDE PROTECT +//
#endif

//+ CPP SUPPORT +//
#ifdef __cplusplus
}
#endif
