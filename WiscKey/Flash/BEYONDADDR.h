//* _DISPOSABLE_h : All the simulation for Disposable FTL are defined in this file
//* dynamic wear leveling: using cost-benefit function for its GC

#ifndef _BEYONDADDR_H
#define _BEYONDADDR_H
#endif

#include "typedefine.h"

#define BEYOND_POINT_TO_NULL	(-1)		//* point to NULL 
#define GC_COST_EFFECTIVE_LIMIT		(0.75)
#define BLOCK_RECYCLE_LIMIT		(0.25)

#define OVERLAP_IN_CACHE		(0)
#define OVERLAP_NOT_IN_CACHE	(1)

typedef struct treeNode
{
	int lba;
	int length;
	flash_size_t chipAddr;
	flash_size_t blockAddr;
	flash_size_t pageAddr;
	flash_size_t subPageOff;
	Boolean cacheStatus;

	struct treeNode *left;
	struct treeNode *right;

}treeNode;

typedef struct treeNodeLinkedList
{
	struct treeNode *node;
	struct treeNodeLinkedList *next;

}treeNodeLinkedList;

typedef struct blockLinkedList
{
	flash_size_t blockNum;
	struct blockLinkedList *next;

}blockLinkedList;

typedef struct virtualRegion
{
	struct blockLinkedList *allocatedBlockListHead;
	flash_size_t usedPageBuffer;
	struct treeNode *mrmTreeRoot;
	struct treeNodeLinkedList *cachedTreeNodeList;

	// Trece the cached number of tree nodes
	int cachedTreeNodeCount;
	// Record the last block in the linked list
	struct blockLinkedList *currBlock;
	// Track number of items in block list
	int blockCount;
	// Tack number of items in mrmTree
	int nodeCount;
	// Track the size of this virtual region
	int size;
	// Valid subpage count
	int validSubPage;

}virtualRegion;

extern int SramSizeLimit;		   // The length of this LRU list is limited by the SRAM size
extern flash_size_t basedUnitSize; // A virtual Region is composed by (a) a block linked list (b) a usedPageBuffer (c) a MrM Tree

//* ****************************************************************
//* Simulate the Disposable method with DWL
//* "fp" is the input file descriptor
//* ****************************************************************
void BeyondAddrMappingSimulation(FILE *fp);

virtualRegion *ConvertLbaToVirtualRegion(flash_size_t lba, flash_huge_size_t *totalRequestTime, int *lruPos, Boolean getFreeBlockMethod);

flash_size_t AllocateBlockToVirtualRegion(virtualRegion *currentRegionm, Boolean fromGcRequest);

void CreateMrMNode(struct virtualRegion *currentRegion, flash_size_t lba, flash_size_t len,
					flash_size_t chipAddr, flash_size_t blockAddr, flash_size_t pageAddr, flash_size_t subPageOff,
					Boolean skipChecking, int lruPos, Boolean allCached);

void RemoveOverlapped(struct virtualRegion *currentRegion, treeNode *node, flash_size_t lba, flash_size_t len, int pos);

void RemoveSubtree(struct virtualRegion *currentRegion, treeNode *node);

// Linked List Operation
struct blockLinkedList* AddToList(struct virtualRegion *addToRegion, int val, Boolean addToEnd);

struct blockLinkedList* SearchInList(struct virtualRegion *addToRegion, int val, struct blockLinkedList **prev);

int DeleteFromList(struct virtualRegion *addToRegion, int val);

struct blockLinkedList* CreateList(int val);

int SizeOfList(struct virtualRegion *addToRegion);

void PrintList(struct virtualRegion *addToRegion);

// binary search tree
treeNode* FindMin(treeNode *node);

treeNode* FindMax(treeNode *node);

treeNode *Insert(treeNode *node, flash_size_t lba, flash_size_t length,
		flash_size_t chipAddr, flash_size_t blockAddr, flash_size_t pageAddr,
		flash_size_t subPageOff, Boolean allCached);

treeNode * Delete(virtualRegion *currentRegion, treeNode *node, int data);

treeNode * Find(treeNode *node, int data);

void PrintTrees();

void WipeTreeCached();

#define pow2(n) (1 << (n))

void printGraph(treeNode *root);

void FlushVirtualRegion(flash_size_t virtualRegionNum, int pos, Boolean getFreeBlockMethod, flash_huge_size_t *totalRequestTime);

int Write(flash_size_t lba, int length, Boolean skipChecking, Boolean getFreeBlockMethod);

int Read(flash_size_t lba, int length, Boolean preCheck);
