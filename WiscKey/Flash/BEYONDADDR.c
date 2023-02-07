#ifndef _BEYONDADDR_H
#define _BEYONDADDR_H
#endif

#include "main.h"
#include "BEYONDADDR.h"

static virtualRegion *virtualRegionList;
static int virtualRegionAddrSize;
static flash_size_t virtualRegionBlockSize;
static flash_size_t virtualRegionPageSize;
static flash_size_t virtualRegionSubPageSize;
static LRUSRAMList_t virtualRegionLRUSRAM = { 0 }; // The LRU list for cached virtual region
int SramSizeLimit; // The length of this LRU list is limited by the SRAM size, 10 MB * 1024 * 1024 = B
flash_size_t basedUnitSize; // A virtual Region is composed by (a) a block linked list (b) a usedPageBuffer (c) a MrM Tree
static treeNode *insertedNode = NULL;

void ChangeCacheTreeNodeCnt(virtualRegion* currentRegion, int tmp) {
	currentRegion->cachedTreeNodeCount += tmp;
	if(currentRegion->cachedTreeNodeCount < 0)
		printf("Error at changing used cachedTreeNodeCount. \n");
}

//# ****************************************************************
//# Tree Node Linked List Operation
//# ****************************************************************
struct treeNodeLinkedList* CreateCachedNodeList(treeNode *node) {
	if (DEBUG_HANK)
		printf("INFO: Creating a tree node cached list with (lba %d, len %d) \n", node->lba, node->length);
	struct treeNodeLinkedList *ptr = (struct treeNodeLinkedList*) malloc(sizeof(struct treeNodeLinkedList));
	if (NULL == ptr) {
		printf("ERROR: Cached tree list node creation failed \n");
		return NULL;
	}
	ptr->node = node;
	ptr->next = NULL;

	return ptr;
}

void AddToCachedNodeList(struct virtualRegion *addToRegion, treeNode *node) {
	// Check if the list is initialized or not
	if (addToRegion->cachedTreeNodeList == NULL) {
		addToRegion->cachedTreeNodeList = CreateCachedNodeList(node);
		return ;
	}
	// If the list is already initialized, let's create a new one and append to the list
	struct treeNodeLinkedList *ptr = (struct treeNodeLinkedList*) malloc(sizeof(struct treeNodeLinkedList));
	if (NULL == ptr) {
		printf("ERROR: Cached tree list node creation failed \n");
		return ;
	}

	ptr->node = node;

	if (DEBUG_HANK)
		printf( "INFO : Add to the beginning of list with (lba %d, len %d) \n", node->lba, node->length);

	ptr->next = addToRegion->cachedTreeNodeList;
	addToRegion->cachedTreeNodeList = ptr;

	return ;

}

int DeleteCachedNodeList(struct virtualRegion *addToRegion) {
	struct treeNodeLinkedList *ptr = NULL;
	int tmp = 0;

	while(addToRegion->cachedTreeNodeList != NULL) {
		// Remove nodes from the head of the list
		ptr = addToRegion->cachedTreeNodeList;
		// Set cached status to false
		ptr->node->cacheStatus = False;
		// Point the head to next node
		addToRegion->cachedTreeNodeList = ptr->next;

		if (DEBUG_HANK)
			printf("INFO: Deleting (lba %d, len %d) from list \n", ptr->node->lba, ptr->node->length);

		//Free this node
		free(ptr);
		ptr = NULL;
		tmp += 1;
	}

	addToRegion->cachedTreeNodeList = NULL;

	return tmp;
}

struct treeNodeLinkedList* SearchInCachedNodeList(struct virtualRegion *addToRegion, treeNode *node,
		struct treeNodeLinkedList **prev) {
	struct treeNodeLinkedList *ptr = addToRegion->cachedTreeNodeList;
	struct treeNodeLinkedList *tmp = NULL;
	Boolean found = False;

	if (DEBUG_HANK)
		printf("INFO: Searching the list for (lba %d, len %d) \n", ptr->node->lba, ptr->node->length);
	while (ptr != NULL) {
		if (ptr->node->lba == node->lba && ptr->node->length == node->length) {
			found = True;
			break;
		} else {
			tmp = ptr;
			ptr = ptr->next;
		}
	}

	if (found == True) {
		if (prev)
			*prev = tmp;

		return ptr;
	} else {
		return NULL;
	}
}

int DeleteCachedNodeFromList(struct virtualRegion *addToRegion, treeNode *node) {
	struct treeNodeLinkedList *prev = NULL;
	struct treeNodeLinkedList *del = NULL;

	if (DEBUG_HANK)
		printf("INFO: Deleting (lba %d, len %d) from list \n", node->lba, node->length);
	// Try to find the element to delete
	del = SearchInCachedNodeList(addToRegion, node, &prev);

	if (del == NULL) {
		return -1;
	} else {
		// Set cached status to false
		del->node->cacheStatus = False;

		if (prev != NULL)
			prev->next = del->next;

		if (del == addToRegion->cachedTreeNodeList) {
			addToRegion->cachedTreeNodeList = del->next;
		}
	}

	free(del);
	del = NULL;

	return 0;
}

//# ****************************************************************
//# Block Linked List Operation
//# ****************************************************************
struct blockLinkedList* AddToList(struct virtualRegion *addToRegion, int val, Boolean addToEnd) {
	// Check if the list is initialized or not
	if (addToRegion->allocatedBlockListHead == NULL) {
		addToRegion->allocatedBlockListHead = CreateList(val);
		addToRegion->currBlock = addToRegion->allocatedBlockListHead;
		return addToRegion->currBlock;
	}
	// If the list is already initialized, let's create a new one and append to the list
	struct blockLinkedList *ptr = (struct blockLinkedList*) malloc(sizeof(struct blockLinkedList));
	if (NULL == ptr) {
		printf("ERROR: Node creation failed \n");
		return NULL;
	}

	ptr->blockNum = val;
	ptr->next = NULL;

	if (addToEnd) {
		if (DEBUG_HANK)
			printf("INFO : Add to the end of list with block number [%d] \n", val);
		addToRegion->currBlock->next = ptr;
		addToRegion->currBlock = ptr;
		return addToRegion->currBlock;
	} else {
		if (DEBUG_HANK)
			printf( "INFO : Add to the beginning of list with block number [%d] \n", val);
		ptr->next = addToRegion->allocatedBlockListHead;
		addToRegion->allocatedBlockListHead = ptr;
		return addToRegion->allocatedBlockListHead;
	}
}

struct blockLinkedList* SearchInList(struct virtualRegion *addToRegion, int val,
		struct blockLinkedList **prev) {
	struct blockLinkedList *ptr = addToRegion->allocatedBlockListHead;
	struct blockLinkedList *tmp = NULL;
	Boolean found = False;

	if (DEBUG_HANK)
		printf("INFO: Searching the list for block number[%d] \n", val);
	while (ptr != NULL) {
		if (ptr->blockNum == val) {
			found = True;
			break;
		} else {
			tmp = ptr;
			ptr = ptr->next;
		}
	}

	if (found == True) {
		if (prev)
			*prev = tmp;

		return ptr;
	} else {
		return NULL;
	}
}

int DeleteFromList(struct virtualRegion *addToRegion, int val) {
	struct blockLinkedList *prev = NULL;
	struct blockLinkedList *del = NULL;

	if (DEBUG_HANK)
		printf("INFO: Deleting value [%d] from list \n", val);

	del = SearchInList(addToRegion, val, &prev);
	if (del == NULL) {
		return -1;
	} else {
		if (prev != NULL)
			prev->next = del->next;

		if (del == addToRegion->currBlock) {
			addToRegion->currBlock = prev;
		} else if (del == addToRegion->allocatedBlockListHead) {
			addToRegion->allocatedBlockListHead = del->next;
		}
	}

	free(del);
	del = NULL;

	return 0;
}

struct blockLinkedList* CreateList(int val) {
	if (DEBUG_HANK)
		printf("INFO: Creating a list with head block number [%d] \n", val);
	struct blockLinkedList *ptr = (struct blockLinkedList*) malloc(
			sizeof(struct blockLinkedList));
	if (NULL == ptr) {
		printf("ERROR: Node creation failed \n");
		return NULL;
	}
	ptr->blockNum = val;
	ptr->next = NULL;

	return ptr;
}

int SizeOfList(struct virtualRegion *addToRegion) {
	int len = 0;
	struct blockLinkedList *ptr = addToRegion->allocatedBlockListHead;
	while (ptr != NULL) {
		len++;
		ptr = ptr->next;
	}
	return len;
}

void PrintList(struct virtualRegion *addToRegion) {
	struct blockLinkedList *ptr = addToRegion->allocatedBlockListHead;

	printf("\n -------Printing list Start \n");
	while (ptr != NULL) {
		printf("[%d] (FreePageCnt %d) \n", ptr->blockNum, FreePageCnt[ptr->blockNum]);
		ptr = ptr->next;
	}
	printf("\n -------Printing list End \n");

	return;
}

//# ****************************************************************
//# Binary Search Tree Operation
//# ****************************************************************
treeNode* FindMin(treeNode *node) {
	if (node == NULL) {
		/* There is no element in the tree */
		return NULL;
	}
	if (node->left) /* Go to the left sub tree to find the min element */
		return FindMin(node->left);
	else
		return node;
}

treeNode* FindMax(treeNode *node) {
	if (node == NULL) {
		/* There is no element in the tree */
		return NULL;
	}
	if (node->right) /* Go to the right sub tree to find the max element */
		FindMax(node->right);
	else
		return node;
}

treeNode *Insert(treeNode *node, flash_size_t lba, flash_size_t length,
		flash_size_t chipAddr, flash_size_t blockAddr, flash_size_t pageAddr,
		flash_size_t subPageOff, Boolean allCached) {
	if(length == 0) {
		printf("Error at inserting a new MrM node \n");
	} else {
		if (node == NULL) {

			insertedNode = (treeNode *) malloc(sizeof(treeNode));
			insertedNode->lba = lba;
			insertedNode->length = length;
			insertedNode->chipAddr = chipAddr;
			insertedNode->blockAddr = blockAddr;
			insertedNode->pageAddr = pageAddr;
			insertedNode->subPageOff = subPageOff;
			insertedNode->left = insertedNode->right = NULL;
			insertedNode->cacheStatus = allCached;

			return insertedNode;
		}

		if (lba > (node->lba)) {
			node->right = Insert(node->right, lba, length, chipAddr, blockAddr,
					pageAddr, subPageOff, allCached);
		} else if (lba < (node->lba)) {
			node->left = Insert(node->left, lba, length, chipAddr, blockAddr,
					pageAddr, subPageOff, allCached);
		} else if (lba == (node->lba)) {
			printf("Duplicate mrm node with lba %d \n", lba);
			insertedNode = NULL;
		}
	}
	/* Else there is nothing to do as the data is already in the tree. */
	return node;
}

treeNode * Delete(virtualRegion *currentRegion, treeNode *node, int data) {
	treeNode *temp;
	if (node == NULL) {
		printf("DELETE : Element Not Found");
	} else if (data < node->lba) {
		node->left = Delete(currentRegion, node->left, data);
	} else if (data > node->lba) {
		node->right = Delete(currentRegion, node->right, data);
	} else {
		/* Now We can delete this node and replace with either minimum element
		 in the right sub tree or maximum element in the left subtree */
		if (node->right && node->left) {
			/* Here we will replace with minimum element in the right sub tree */
			temp = FindMin(node->right);
			node->lba = temp->lba;
			node->length = temp->length;
			node->chipAddr = temp->chipAddr;
			node->blockAddr = temp->blockAddr;
			node->pageAddr = temp->pageAddr;
			node->subPageOff = temp->subPageOff;

			if(node->cacheStatus == False && temp->cacheStatus == True) {
				DeleteCachedNodeFromList(currentRegion, temp);
				AddToCachedNodeList(currentRegion, node);
			} else if(node->cacheStatus == True && temp->cacheStatus== False) {
				ChangeCacheTreeNodeCnt(currentRegion, -1);
				DeleteCachedNodeFromList(currentRegion, node);
			}
			node->cacheStatus = temp->cacheStatus;
			/* As we replaced it with some other node, we have to delete that node */
			node->right = Delete(currentRegion, node->right, temp->lba);
		} else {
			/* If there is only one or zero children then we can directly
			 remove it from the tree and connect its parent to its child */
			temp = node;
			if (node->left == NULL)
				node = node->right;
			else if (node->right == NULL)
				node = node->left;
			else
				node = NULL;
			free(temp); /* temp is longer required */
		}
	}
	return node;

}

treeNode * Find(treeNode *node, int data) {
	if (node == NULL) {
		/* Element is not found */
		return NULL;
	}
	if (data > node->lba) {
		/* Search in the right sub tree. */
		return Find(node->right, data);
	} else if (data < node->lba) {
		/* Search in the left sub tree. */
		return Find(node->left, data);
	} else {
		/* Element Found */
		return node;
	}
}

void PrintTrees() {
	int i = 0;
	for (i = 0; i < VirtualRegionNum; i++) {
		if(virtualRegionList[i].blockCount != 0 && virtualRegionList[i].validSubPage != 0) {
			printf("$$$$ Virtual Region %d $$$\n", i);
			printf("BlockCount %d \n", virtualRegionList[i].blockCount);
			printf("Valid SubPage Count %d \n", virtualRegionList[i].validSubPage);

			if (virtualRegionList[i].blockCount != 0)
				PrintList(&virtualRegionList[i]);
			if (virtualRegionList[i].mrmTreeRoot != NULL) {
				printf("MrM Tree \n");
				printGraph(virtualRegionList[i].mrmTreeRoot);
			}
		}
	}
}
/*
inline void WipeTreeCached(treeNode *node) {
	if (node->left != NULL)
		WipeTreeCached(node->left);

	if (node->right != NULL)
		WipeTreeCached(node->right);

	node->cacheStatus = False;
}
*/
int getNodeMaxDepth(treeNode *node, int depth) {
	int ld, rd;
	if (node == NULL) {
		return depth - 1;
	}
	ld = getNodeMaxDepth(node->left, depth + 1);
	rd = getNodeMaxDepth(node->right, depth + 1);
	return (ld < rd) ? rd : ld;
}

void printGraph(treeNode *root) {
	char fmt[9];
	treeNode **row1, **row2, **rowTemp;
	int rows, row, col;

	if (root == NULL) {
		return;
	}

	rows = getNodeMaxDepth(root, 1);
	row1 = (treeNode **) malloc(sizeof(treeNode*) * pow2(rows));
	row2 = (treeNode **) malloc(sizeof(treeNode*) * pow2(rows));
	row1[0] = root;
	for (row = 0; row < rows; row++) {
		int col2 = 0, cols = pow2(row);
		sprintf(fmt, "%%%ds", pow2(rows - (row + 1)));
		for (col = 0; col < cols; col++) {
			treeNode *node = row1[col];
			if (node != NULL) {
				printf(fmt, "  ");
				printf("(%d, %d)", node->lba, node->length);
				row2[col2++] = node->left;
				row2[col2++] = node->right;
			} else {
				printf(fmt, "  ");
				printf(" ");
				row2[col2++] = NULL;
				row2[col2++] = NULL;
			}
			if (col == 0) {
				sprintf(fmt, "%%%ds", pow2(rows - (row + 0)));
			}
		}
		printf("\n");
		rowTemp = row1;
		row1 = row2;
		row2 = rowTemp;
	}
	free(row1);
	free(row2);
}

//# ****************************************************************
//# Initialize the settings for flash memory in Beyond Address Mapping
//# ****************************************************************
static void InitializeBeyondMapping(void) {

}

static void InitializeVirtualRegions(void) {
	// Initialize virtual Region
	virtualRegionList = (virtualRegion *) malloc(sizeof(virtualRegion) * VirtualRegionNum);
	memset(virtualRegionList, BEYOND_POINT_TO_NULL, sizeof(virtualRegion) * VirtualRegionNum);
	int i = 0;
	// Initialize each virtual region
	for (i = 0; i < VirtualRegionNum; i++) {
		virtualRegionList[i].usedPageBuffer = 0;
		virtualRegionList[i].blockCount = 0;
		virtualRegionList[i].nodeCount = 0;
		virtualRegionList[i].size = 0;
		virtualRegionList[i].validSubPage = 0;
		virtualRegionList[i].allocatedBlockListHead = NULL;
		virtualRegionList[i].currBlock = NULL;
		virtualRegionList[i].mrmTreeRoot = NULL;
		virtualRegionList[i].cachedTreeNodeCount = 0;
		virtualRegionList[i].cachedTreeNodeList = NULL;
	}
	// Calculate the covering address space size of a virtual region
	virtualRegionAddrSize = MaxLBA / VirtualRegionNum;
	virtualRegionBlockSize = MaxBlock / VirtualRegionNum;
	virtualRegionPageSize = MaxPage * MaxBlock / VirtualRegionNum;
	virtualRegionSubPageSize = MaxSubPage * MaxPage * MaxBlock / VirtualRegionNum;

	printf("VirtualRegion, AddrSize = %d, Block count = %d, Page count = %d, SubPag count = %d \n",
			virtualRegionAddrSize, virtualRegionBlockSize,
			virtualRegionPageSize, virtualRegionSubPageSize);
}

static void InitializeLRUSRAMList(void) {
	// Set SRAM size limit (SramSize -> MB, SramSizeLimit -> B)
	SramSizeLimit = SramSize * 1024 * 1024;
	basedUnitSize = sizeof(blockLinkedList) * 1000 + sizeof(treeNode) * 1000 + PageSize + sizeof(int) * 5;
	// Allocate LRU list
	CreateLRUListS(&virtualRegionLRUSRAM, LRUListSramLength);
}

//# ****************************************************************
//# Release the memory space allocated for translation tables
//# ****************************************************************
static void FinalizeTranslationTables(void) {
	int i = 0;
	for (i = 0; i < VirtualRegionNum; i++) {
		free(virtualRegionList[i].allocatedBlockListHead);
		free(virtualRegionList[i].cachedTreeNodeList);
		free(virtualRegionList[i].currBlock);
	}

	FreeLRUListS(&virtualRegionLRUSRAM);
	free(virtualRegionList);
}

//# ****************************************************************
//# Find the correct virtual region for the requested LBA
//# ****************************************************************
virtualRegion *ConvertLbaToVirtualRegion(flash_size_t lba, flash_huge_size_t *totalRequestTime, int *lruPos, Boolean getFreeBlockMethod) {
	if (lba > MaxLBA) {
		printf("ERROR: The request LBA (%d) is larger the the max LBA (%d) \n", lba, MaxLBA);
		return NULL;
	}
	// calculate virtual region number
	int virtualRegionNum = (int) (lba / virtualRegionAddrSize);
	if(virtualRegionNum >= VirtualRegionNum) {
		printf("ERROR: The region (%d) of request LBA (%d) exceed limit (%d) \n", lba, (int) (lba / virtualRegionAddrSize), VirtualRegionNum);
		return NULL;
	}
	if (DEBUG_HANK)
		printf("INFO : The request LBA (%d) is in region (%d), ", lba, virtualRegionNum);

	int num;	// the number of searched elements in the LRU list
	// check if the virtual region is cached in the LRU SRAM list
	if (virtualRegionLRUSRAM.Len != 0 && IsElemenetInLRUListS(&virtualRegionLRUSRAM, virtualRegionNum, lruPos, &num)) {
		if (DEBUG_HANK)
			printf(" 'FETCH' from LRU list. \n");

		MoveElemenetToTheHeadofLRUListS(&virtualRegionLRUSRAM, virtualRegionNum, lruPos);
	} else {
		// it's not in the LRU SRAM, let move it into the SRAM list
		if (DEBUG_HANK)
			printf(" 'MOVE' into LRU list. \n");

		AddElemenetToTheHeadOfLRUListS(&virtualRegionLRUSRAM,
										virtualRegionNum,
										lruPos,
										virtualRegionList[virtualRegionNum].size,
										totalRequestTime,
										getFreeBlockMethod);
	}

	if (CurrentRequest.AccessType == WriteType)
		virtualRegionLRUSRAM.Element[*lruPos].dirty = True;

	return &virtualRegionList[virtualRegionNum];
}

// Find the index of the minimum element in the list
int find_minimum(double a[], int n) {
	double min;
	int c, index;

	min = a[0];
	index = 0;

	for (c = 1; c < n; c++) {
		if(a[c] == 0) {
			index = c;
			min = a[c];
			break;
		}
		if (a[c] < min) {
			index = c;
			min = a[c];
		}
	}

	return index;
}

int find_minimum_int(int a[], int n) {
	int min, c, index;

	min = a[0];
	index = 0;

	for (c = 1; c < n; c++) {
		if(a[c] == 0) {
			index = c;
			min = a[c];
			break;
		}
		if (a[c] < min) {
			index = c;
			min = a[c];
		}
	}

	return index;
}

inline Boolean ConvertBlkNumToIndex(blockLinkedList *block, flash_size_t blockAddr, int *res) {
	blockLinkedList *tmp;
	tmp = block;
	do {
		if (tmp->blockNum == blockAddr) {
			return True;
		} else {
			*res += 1;
			tmp = tmp->next;
		}
	} while (tmp != NULL);

	return False;
}

int ConvertIndexToBlkNum(blockLinkedList *block, int index) {
	int at = 0;

	blockLinkedList *tmp;
	tmp = block;
	do {
		if (at == index)
			break;
		else {
			at += 1;
			tmp = tmp->next;
		}
	} while (tmp->next != NULL);

	return tmp->blockNum;
}

Boolean CheckOverlapped (int A, int lenA, int B, int lenB) {

	if(lenA == 0 || lenB == 0)
		return False;

	if(A == B && lenB <= lenA) // Start from same point, different length
		return True;
	if(A < B && B + lenB <= A + lenA) // B is wrapped by A
		return True;
	if(B < A + lenA && A + lenA < B + lenB) // B overlaps A on at right end
		return True;
	if(A < B + lenB && B + lenB < A + lenA) // B overlaps A on at left end
		return True;

	return False;
}

Boolean DoubleCheck(int A, Boolean B){
	if(A == True && B == True)
		return True;

	return False;
}

void ChangeVaildSubPageCnt(virtualRegion *currentRegion, int num) {
	currentRegion->validSubPage += num;
	if(currentRegion->validSubPage < 0 )
		printf("Error at changing valid subpage count. \n");
}

void ChangeUsedBuffer(virtualRegion *currentRegion, int length) {
	currentRegion->usedPageBuffer += length;
	if(currentRegion->usedPageBuffer < 0 || currentRegion->usedPageBuffer > 32)
		printf("Error at changing used page buffer subpage num. \n");
}



void SetSubPageStatusOnFlash(flash_size_t len, treeNode* res, int overlappedOffset, int status) {
	// subpages of mrm node is always in a single block

	// 1. Find the starting pageAddr and subPageOffset
	int pageAddr = 0;
	int subPageOff = 0;
	if (overlappedOffset >= MaxSubPage) {
		pageAddr = res->pageAddr + ((overlappedOffset - (MaxSubPage - res->subPageOff)) / MaxSubPage);
		subPageOff = (overlappedOffset - (MaxSubPage - res->subPageOff)) % MaxSubPage;
	} else if (overlappedOffset < MaxSubPage) {
		pageAddr = res->pageAddr;
		subPageOff = overlappedOffset + res->subPageOff;
	} else {
		pageAddr = res->pageAddr;
		subPageOff = res->subPageOff;
	}
	// 2. Re-correct the starting point
	if(subPageOff > MaxSubPage) {
		pageAddr += (subPageOff / MaxSubPage);
		subPageOff = (subPageOff % MaxSubPage);
	}
	// 3. Set the page invalid in flash
	if (subPageOff + len < MaxSubPage) {
		// Within a single page
		int z = 0;
		// Debug
		if (DEBUG_HANK) {
			printf("# Invalidating at Block %d, Page %d, Start SubPage %d, Total SubPages %d \n",
					res->blockAddr, pageAddr, subPageOff, len);
		}
		for (z = subPageOff; z < subPageOff + len; z++) {
#ifdef BEYONDADDR
			SetFlashSubPageStatus(res->blockAddr, pageAddr, z, status);
#endif
		}
	} else {
		// Spanning over multiple page
		int spanedPageNum = 0, tail = 0;
		if(subPageOff == 0) {
			spanedPageNum = len / MaxSubPage;
			tail = len % MaxSubPage;
			// Debug
			if (DEBUG_HANK) {
				printf("# Invalidating at Block %d, Start Page %d, Start SubPage %d, Total Page Num %d, Tail %d \n",
						res->blockAddr, pageAddr, subPageOff, spanedPageNum, tail);
			}
			// Invalidate pages
			int i, j;
			for (i = pageAddr; i < spanedPageNum; i++) {
				for (j = 0; j < MaxSubPage; j++) {
#ifdef BEYONDADDR
					SetFlashSubPageStatus(res->blockAddr, i, j, status);
#endif
				}
			}
			// Invalidate tail pages
			for (j = 0; j < tail; j++) {
#ifdef BEYONDADDR
				SetFlashSubPageStatus(res->blockAddr, pageAddr + spanedPageNum, j, status);
#endif
			}
		} else {
			spanedPageNum = ((len - (MaxSubPage - subPageOff)) / MaxSubPage);
			tail = (len - (MaxSubPage - subPageOff)) % MaxSubPage;
			// Debug
			if (DEBUG_HANK) {
				printf("# Invalidating at Block %d, Start Page %d, Start SubPage %d, Tail part of 1st page %d, Total Page Num %d, Tail %d \n",
						res->blockAddr, pageAddr, subPageOff, (MaxSubPage - subPageOff), spanedPageNum, tail);
			}
			// Invalidate the first page
			int i, j;
			for (j = subPageOff; j < MaxSubPage; j++) {
#ifdef BEYONDADDR
				SetFlashSubPageStatus(res->blockAddr, pageAddr, j, status);
#endif
			}
			// Invalidate pages
			for (i = pageAddr + 1; i < pageAddr + 1 + spanedPageNum; i++) {
				for (j = 0; j < MaxSubPage; j++) {
#ifdef BEYONDADDR
					SetFlashSubPageStatus(res->blockAddr, i, j, status);
#endif
				}
			}
			if(tail > 0){
				// Invalidate tail pages
				for (j = 0; j < tail; j++) {
#ifdef BEYONDADDR
					SetFlashSubPageStatus(res->blockAddr, pageAddr + 1 + spanedPageNum, j, status);
#endif
				}
			}
		}

	}
}

void VirtualRegionSizeUpdate(struct virtualRegion *currentRegion) {
	currentRegion->size = (currentRegion->usedPageBuffer * LBA_SIZE)
			+ (sizeof(blockLinkedList) * (currentRegion->blockCount))
			+ (sizeof(treeNode) * (currentRegion->nodeCount))
			+ (sizeof(int) * 5);
}

int InvalidateSupPage(struct virtualRegion *currentRegion, flash_size_t len, treeNode* res, int lruPos, Boolean fromGC) {
	// That's great, the length is identical. We just need to update the mrm info
	// Invalidate those subpage belongs to the original block
	if ((fromGC == False)
			&& (currentRegion->currBlock->blockNum == res->blockAddr)
			&& (currentRegion->usedPageBuffer != 0)
			&& (res->cacheStatus == True)) {
		// The overlapped part is still in the cache
		if(DEBUG_HANK) {
			printf("# Remove overlapped part : FreeSub %d Valid %d Invalid %d Cached %d. \n", FreeSubPageCnt[res->blockAddr], ValidSubPageCnt[res->blockAddr], InvalidSubPageCnt[res->blockAddr], currentRegion->usedPageBuffer);
			printf("# Remove overlapped part (%d) from cache (%d). \n", len, currentRegion->usedPageBuffer);
		}

		ChangeUsedBuffer(currentRegion, len * -1);
		// Update virtual region size
		VirtualRegionSizeUpdate(currentRegion);
		// Track the SRAM usage
		UpdateLruItemSize(&virtualRegionLRUSRAM, lruPos, currentRegion->size, &AccessStatistics.TotalWriteRequestTime, False);
		// Update free subpage count
		FreeSubPageCount(res->blockAddr, len);

		return OVERLAP_IN_CACHE;
	} else {
		// The overlapped part is already in flash
		if(DEBUG_HANK)
			printf("# Remove overlapped part (%d) from flash (%d). \n", len, ValidSubPageCnt[res->blockAddr]);

		VaildSubPageCount(res->blockAddr, len * -1);
		InvaildSubPageCount(res->blockAddr, len);

		return OVERLAP_NOT_IN_CACHE;
	}
}

void MrMTreeCheck(virtualRegion *currentRegion, treeNode *node, int blkNum, int *res) {

	// Let's get started
	if(currentRegion != NULL && node->length > 0) {
		if(node->blockAddr == blkNum) {
			*res += node->length;
		}
	}

	if (currentRegion != NULL && node->right != NULL)
		MrMTreeCheck(currentRegion, node->right, blkNum, res);

	if (currentRegion != NULL && node->left != NULL)
		MrMTreeCheck(currentRegion, node->left, blkNum, res);
}


void MrMTreeTravse(virtualRegion *currentRegion, treeNode *node, int *vaildSubPageCount, Boolean skipCurBlk) {
	// the block list => currentRegion->allocatedBlockListHead
	// the subpage count array correspond => vaildSubPageCount
	
	// Here, we are going to calculate the valid subpage count of each block
	// The result can be compared with ValidSubPageCnt(of each page) for verifying purpose
	// BTW, Dont worry about a node spanning over two blocks, it's prevented already when conducting writie operations
	
	// Let's get started
	if(currentRegion != NULL && node->length > 0) {
		int index = 0;
		if(ConvertBlkNumToIndex(currentRegion->allocatedBlockListHead, node->blockAddr, &index) == True
				&& !(skipCurBlk == True && node->blockAddr == currentRegion->currBlock->blockNum)) {
			vaildSubPageCount[index] += node->length;
		}
	}

	if (currentRegion != NULL && node->right != NULL)
		MrMTreeTravse(currentRegion, node->right, vaildSubPageCount, skipCurBlk);

	if (currentRegion != NULL && node->left != NULL)
		MrMTreeTravse(currentRegion, node->left, vaildSubPageCount, skipCurBlk);
}



void MrMTreeMove(int selectedVirtualRegion, treeNode *node, flash_size_t Addr, int *count) {
	virtualRegion *currentRegion = &virtualRegionList[selectedVirtualRegion];

	if (node != NULL && node->right != NULL)
		MrMTreeMove(selectedVirtualRegion, node->right, Addr, count);

	if (node != NULL && node->left != NULL)
		MrMTreeMove(selectedVirtualRegion, node->left, Addr, count);

	if (node != NULL && node->blockAddr == Addr) {
		flash_size_t lba = node->lba;
		flash_size_t len = node->length;

		if (DEBUG_HANK)
			printf("TreeMove : Remove %d from chip %d block %d Page %d SubAddr %d \n",
					node->lba, node->chipAddr, node->blockAddr, node->pageAddr, node->subPageOff);

		// Invalidate those subpage belongs to the original block
		if(InvalidateSupPage(currentRegion, node->length, node, 0, True) == OVERLAP_NOT_IN_CACHE) {
			// Set the page invalid in flash
			SetSubPageStatusOnFlash(node->length, node, 0, INVALID_PAGE);
		}

		// Delete original node
		if(node->cacheStatus == True) {
			ChangeCacheTreeNodeCnt(currentRegion, -1);
			DeleteCachedNodeFromList(currentRegion, node);
		}
		currentRegion->mrmTreeRoot = Delete(currentRegion, currentRegion->mrmTreeRoot, lba);
		currentRegion->nodeCount -= 1;
		// Update virtual region info
		ChangeVaildSubPageCnt(currentRegion, -len);

		AccessStatistics.TotalWriteRequestTime += TIME_READ_PAGE;
		AccessStatistics.TotalReadPageCount += 1;
		AccessStatistics.TotalReadSubPageCount += len;

		if(Write(lba, len, False, True) == 0) {
			// Update output
			*count += len;
		} else {
			printf("TreeMove : Error when re-writing \n");
			return;
		}
	}
}

void GarbageCollection(void) {
	//printf("GC Start \n");
#ifdef BEYONDADDR_GC_FIND_MINIMUM
	// 1. Try to find a block with maximum
#else
	// 1. Select virtual region by calculating the cost-effective ratio
	// cost-effective ratio = valid subpage count / number of blocks
	// Choose the smallest one for gc
	double *costEff = (double *) malloc(sizeof(double) * VirtualRegionNum);
	memset(costEff, 0, sizeof(double) * VirtualRegionNum);

	int i = 0;
	do {
		costEff[i] = (double)((double)virtualRegionList[i].validSubPage / (double)virtualRegionList[i].blockCount);
		if (DEBUG_HANK)
			printf("**** VR %d, valid SubPgae %d, block count %d, ratio %f \n",
						i,
						virtualRegionList[i].validSubPage,
						virtualRegionList[i].blockCount,
						costEff[i]);
		i++;
	} while (i < VirtualRegionNum);
	
	// Find the index of the minimum
	int index = find_minimum(costEff, VirtualRegionNum);
	struct virtualRegion *gcVirtualRegion = &virtualRegionList[index];

	//printf("GC : Select Region %d with cost-effect ratio %d, total blk count %d\n", index, costEff[index], gcVirtualRegion->blockCount);

	// 2. Scanning MrM tree to calculate live subpage count for each block
	// Select the victim block by the live supage count, the lower
	int *vaildSubPageCount = (int *) malloc(sizeof(int) * (gcVirtualRegion->blockCount - 1));
	memset(vaildSubPageCount, 0, sizeof(int) * (gcVirtualRegion->blockCount - 1));
	
	MrMTreeTravse(gcVirtualRegion,
			gcVirtualRegion->mrmTreeRoot,
			vaildSubPageCount,
			True);

	if (DEBUG_HANK) {
		int z = 0;
		for(z = 0;z < gcVirtualRegion->blockCount - 1;z++) {
			printf("** Block %d, valid SubPgae %d (%d), \n",
						ConvertIndexToBlkNum(gcVirtualRegion->allocatedBlockListHead, z),
						vaildSubPageCount[z],
						ValidSubPageCnt[ConvertIndexToBlkNum(gcVirtualRegion->allocatedBlockListHead, z)]);
		}
	}
	// 3. Select a victim block
	// Skip current block :)
	int selectedBlockIndex = find_minimum_int(vaildSubPageCount, gcVirtualRegion->blockCount - 1);
	int selectedBlockNum = ConvertIndexToBlkNum(gcVirtualRegion->allocatedBlockListHead, selectedBlockIndex);
	int validSubpageCountTree = vaildSubPageCount[selectedBlockIndex];
#endif

	if(selectedBlockNum == gcVirtualRegion->currBlock->blockNum)
		printf("GC : Error \n");

	if(ValidSubPageCnt[selectedBlockNum] != validSubpageCountTree)
		printf("GC : Stop & Debug, :) \n");

	//printf("GC : Select Block %d with valid subpage count %d \n", selectedBlockNum, vailSubpageCount);

	// 4. live sup-page copys
	if(validSubpageCountTree == 0) {
		// Skip live page-copy if live page count = 0
	} else {
		int res = 0;

		MrMTreeMove(index, gcVirtualRegion->mrmTreeRoot, selectedBlockNum, &res);

		if (res == validSubpageCountTree) {
			//# Statistics
			AccessStatistics.TotalLiveSubPageCopyings += validSubpageCountTree;
			AccessStatistics.TotalLivePageCopyings += ceil((double)((double)validSubpageCountTree / (double)MaxSubPage));
		} else {
			printf("GC : Error, mismatch number of live subpages. \n");
			return;
		}

	}

	int res = 0;
	MrMTreeCheck(gcVirtualRegion, gcVirtualRegion->mrmTreeRoot, selectedBlockNum, &res);
	if(res != 0) {
		printf("GC : Error \n");
	}

	if(ValidSubPageCnt[selectedBlockNum] != 0 && InvalidSubPageCnt[selectedBlockNum] == MaxPage * MaxSubPage)
		printf("GC : Stop & Debug :( \n");

	// 5. Free block
	DeleteFromList(gcVirtualRegion, selectedBlockNum);
	gcVirtualRegion->blockCount -= 1;

	FreePageCnt[selectedBlockNum] = MaxPage;
	ValidPageCnt[selectedBlockNum] = 0;
	InvalidPageCnt[selectedBlockNum] = 0;

	FreeSubPageCnt[selectedBlockNum] = MaxSubPage * MaxPage;
	ValidSubPageCnt[selectedBlockNum] = 0;
	InvalidPageCnt[selectedBlockNum] = 0;

	// Set the subpage free in flash
	int w , j;
	for(w = 0;w < MaxPage;w++) {
		for(j = 0;j < MaxSubPage;j++) {
#ifdef BEYONDADDR
			SetFlashSubPageStatus(selectedBlockNum, w, j, FREE_PAGE);
#endif
		}
	}

	PutFreeBlock(selectedBlockNum);
	if(DEBUG_HANK)
		printf("GC : Select Block %d with valid subpage count %d, Free blk num %d (threshold %d) \n", selectedBlockNum, validSubpageCountTree, BlocksInFreeBlockList, LeastFreeBlocksInFBL());

	//# Satistics
	AccessStatistics.TotalBlockEraseCount++; //* update total number of block erases
	AccessStatistics.TotalWriteRequestTime += TIME_ERASE_BLOCK;	//# the time to erase a block

	// Finalize
	free(costEff);
	free(vaildSubPageCount);
}

//# ****************************************************************
//# Write some content to 'lba' with 'length'
//# ****************************************************************
flash_size_t AllocateBlockToVirtualRegion(virtualRegion *currentRegion, Boolean fromGcRequest) {
	// Get a new Block
	flash_size_t newBlockNum = -1;
	int oriBlkNum = -1;
	Boolean gcGetBlk = False;
	if(currentRegion->currBlock != NULL)
		oriBlkNum = currentRegion->currBlock->blockNum;

	if(fromGcRequest == True && BlocksInFreeBlockList > 1) {
		newBlockNum = GetFreeBlock(fromGcRequest); //* get one free block, if the block is not free, do garbage collection to reclaim one more free block
	} else {
		do {
			newBlockNum = GetFreeBlock(fromGcRequest); //* get one free block, if the block is not free, do garbage collection to reclaim one more free block
			//* Do garbage collection to reclaim one more free block if there is no free block in the free block list
			if(newBlockNum == -1) {
				GarbageCollection();
				// Check if gc gets a new block for us
				if (currentRegion->currBlock != NULL
						&& oriBlkNum != currentRegion->currBlock->blockNum
						&& FreeSubPageCnt[currentRegion->currBlock->blockNum] > 0) {
					newBlockNum = currentRegion->currBlock->blockNum;
					gcGetBlk = True;
				}
			}
		} while (newBlockNum == -1);
	}

	// Log the block info 
	if(gcGetBlk != True) {
		AddToList(currentRegion, newBlockNum, True);
		currentRegion->blockCount += 1;
	}

	return newBlockNum;
}

//* ****************************************************************
//* This function is used to find overlapped part or identical node :)
//* ****************************************************************
void FindOverlapped(virtualRegion *currentRegion, treeNode *node, int lba, int len, int *length) {
	if (lba < node->lba &&
			(lba + len - 1) >= node->lba &&
		    (lba + len - 1) <= (node->lba + node->length -1)) {
			// The new node is overlapped the original node on the left hand side

		// Update request length
		int satisfiedLength = lba + len - node->lba;
		*length -= satisfiedLength;

		if (DEBUG_HANK)
			printf("%d Read length %d, page count %f, Left length %d \n",
					lba,
					satisfiedLength,
					ceil((double) ((double) (satisfiedLength) / (double) (MaxSubPage))),
					*length);

		// Continue check left subtree
		if (node->left != NULL && *length > 0) {
			FindOverlapped(currentRegion, node->left, lba, (len - satisfiedLength), length);
		}

	} else if (lba > node->lba &&
				lba <= (node->lba + node->length - 1) &&
				(lba + len - 1) >= (node->lba + node->length - 1)) {
		// The new node is overlapped on the right hand side

		// Update request length
		int satisfiedLength = node->lba + node->length - lba;
		*length -= satisfiedLength;

		if (DEBUG_HANK)
			printf("%d Read length %d, page count %f, Left length %d \n",
					lba,
					satisfiedLength,
					ceil((double) ((double) (satisfiedLength) / (double) (MaxSubPage))),
					*length);

		int lbaN = lba + satisfiedLength;
		int lenN = len - satisfiedLength;
		// Continue check right subtree
		if (node->right != NULL && *length > 0) {
			FindOverlapped(currentRegion, node->right, lbaN, lenN, length);
		}

	} else if (lba >= node->lba &&
				(lba + len - 1) <= (node->lba + node->length - 1)) {
		// The new node is wrapped by the original node or identical node :)

		// Update request length
		*length = 0;
		if (DEBUG_HANK)
			printf("%d Read length %d, page count %f, Left length %d \n",
					lba,
					len,
					ceil((double) ((double) (len) / (double) (MaxSubPage))),
					*length);
	} else if (lba < node->lba
			&& (lba + len - 1) >= (node->lba + node->length - 1)) {
		// The original node is wrapped by the new node
		*length -= node->length;

		if (DEBUG_HANK)
			printf("%d Read length %d, page count %f, Left length %d \n",
					lba,
					node->length,
					ceil((double) ((double) (node->length) / (double) (MaxSubPage))),
					*length);

		int lenNL = 0;
		if(lba < node->lba) {
			// Continue read from left subtree
			int lenNL = node->lba - lba;
			if (node->left != NULL && *length > 0) {
				FindOverlapped(currentRegion, node->left, lba, lenNL, length);
			}
		}

		if((lba + len - 1) > (node->lba + node->length - 1)) {
			// Continue read from right subtree
			int lbaNR = node->lba + node->length;
			int lenNR = len - node->length - lenNL;
			if (node->right != NULL && *length > 0) {
				FindOverlapped(currentRegion, node->right, lbaNR, lenNR, length);
			}
		}

	} else if ((lba + len - 1) < node->lba) {
		// The node is not overlapped, check left subtree
		if (node->left != NULL && *length > 0)
			FindOverlapped(currentRegion, node->left, lba, len, length);
	} else if ((node->lba + node->length - 1) < lba) {
		// The node is not overlapped, check right subtree
		if (node->right != NULL && *length > 0)
			FindOverlapped(currentRegion, node->right, lba, len, length);
	} else {
		if(DEBUG_HANK)
			printf("Read not found at LBA %d, length %d \n", lba, len);
	}
}

int Read(flash_size_t lba, int length, Boolean preCheck) {
	// Read some content from 'lba' with 'length' 
	/*if(preCheck == True) {
		// calculate virtual region number
		int virtualRegionNum = (int) (lba / virtualRegionAddrSize);
		if(virtualRegionNum >= VirtualRegionNum) {
			printf("ERROR: The region (%d) of request LBA (%d) exceed limit (%d) \n", lba, (int) (lba / virtualRegionAddrSize), VirtualRegionNum);
			return length;
		}
		virtualRegion *regionReq = virtualRegionList[virtualRegionNum];

	}*/

	// Find the corresponding virtual region
	int pos;	// the position of the element in the LRU list
	virtualRegion *currentRegion = ConvertLbaToVirtualRegion(lba, &AccessStatistics.TotalReadRequestTime, &pos, False);

	if(currentRegion == NULL) {
		return length;
	}

	// The number of subpages to read
	int leftLengthToRead = length;

	if (DEBUG_HANK)
		printf("INFO : Read from lba %d with length %d. \n", lba, leftLengthToRead);

	// Look for the MrM node in the MrM tree
	// Check the items in the MrM Tree
	if (currentRegion->mrmTreeRoot != NULL && currentRegion->nodeCount != 0) {
		// the tree is initialized, check for overlapped region
		do {
			treeNode *res = Find(currentRegion->mrmTreeRoot, lba);
			if (res != NULL) {
				// ok, we find a tree node start with the same LBA
				if (res->length > 0) {
					// Check the length of MrM node and the new request
					if (leftLengthToRead == res->length || leftLengthToRead < res->length) {
						// That's great, the length is identical.
						// Update left length
						leftLengthToRead -= leftLengthToRead;

						if (DEBUG_HANK)
							printf("%d Read length %d, page count %f, Left length %d \n",
									lba, leftLengthToRead,
									ceil((double) ((double) (leftLengthToRead) / (double) (MaxSubPage))),
									leftLengthToRead);
					} else if (leftLengthToRead > res->length) {
						// fuck, this is quite tricky. We need to check to right sub tree for more lba
						// Update left length
						leftLengthToRead -= res->length;

						if (DEBUG_HANK)
							printf("%d Read length %d, page count %f, Left length %d \n",
									lba, res->length,
									ceil((double) ((double) (res->length) / (double) (MaxSubPage))),
									leftLengthToRead);
						// Continue read from right subtree
						lba = lba + res->length;
						if (res->right != NULL && leftLengthToRead > 0)
							FindOverlapped(currentRegion, res->right, lba, leftLengthToRead, &leftLengthToRead);
					}
				} else {
					printf("length < 0 in Read() \n");
				}
			} else {
				// We cannot find a MrM node start with the LBA
				FindOverlapped(currentRegion, currentRegion->mrmTreeRoot, lba, length, &leftLengthToRead);
				break;
			}
		} while (leftLengthToRead > 0);

	} else {	// the tree is not initialized
				// do nothing, we just cant find the mapping info
	}

	//# Statistics - read a page
	AccessStatistics.TotalReadRequestTime += ceil((double) ((double) (length - leftLengthToRead) / (double) (MaxSubPage))) * TIME_READ_PAGE;
	AccessStatistics.TotalReadPageCount += ceil((double) ((double) (length - leftLengthToRead) / (double) (MaxSubPage)));
	AccessStatistics.TotalReadSubPageCount += (length - leftLengthToRead);
	// Partially read
	if(leftLengthToRead < length && preCheck == True) {
		// Just recognize it as a fulfilled request is preCheck is enabled
		leftLengthToRead = 0;
		if(DEBUG_HANK)
			printf("Cheat re-correction, XD (Diff %d) \n", length - leftLengthToRead);
	}

	return leftLengthToRead;
}

//# ****************************************************************
//# Write some content to 'lba' with 'length'; only called for writing initial data
//# ****************************************************************
int WriteBlock(flash_size_t lba, int length, Boolean skipChecking) {
	// Find the corresponding virtual region
	int lruPos;	// the position of the element in the LRU list
	virtualRegion *currentRegion = ConvertLbaToVirtualRegion(lba, &AccessStatistics.TotalWriteRequestTime, &lruPos, False);

	// Number of pages
	int numOfPage = length;

	// Before buffering, get a new block
	AllocateBlockToVirtualRegion(currentRegion, False);

	// Prepare MrM Tree node information
	int numberOfSub = numOfPage * MaxSubPage;
	flash_size_t chipAddr = (int) (currentRegion->currBlock->blockNum / BlocksPerChip);
	flash_size_t blockAddr = currentRegion->currBlock->blockNum;
	flash_size_t pageAddr = MaxPage - FreePageCnt[currentRegion->currBlock->blockNum];
	flash_size_t subPageOff = currentRegion->usedPageBuffer;

	if(DEBUG_HANK)
		printf("$ Block Write : lba %d PageCnt %d blockAddr %d pageAddr %d subPageOff %d. \n",
				lba, numOfPage, blockAddr, pageAddr, subPageOff);

	// Update Flash
	FreePageCnt[currentRegion->currBlock->blockNum] -= numOfPage;
	ValidPageCnt[currentRegion->currBlock->blockNum] += numOfPage;

	FreeSubPageCount(currentRegion->currBlock->blockNum, numberOfSub * -1);
	VaildSubPageCount(currentRegion->currBlock->blockNum, numberOfSub);

	// Set subpage valid in flash
	int i,j;
	for(i = 0;i < MaxPage;i++) {
		for(j = 0;j < MaxSubPage;j++) {
#ifdef BEYONDADDR
			SetFlashSubPageStatus(blockAddr, i, j, VALID_PAGE);
#endif
		}
	}
	// Create MrM node
	CreateMrMNode(currentRegion, lba, numberOfSub, chipAddr, blockAddr, pageAddr, subPageOff, skipChecking, lruPos, False);
	// statistic
	AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE * numOfPage;
	AccessStatistics.TotalWritePageCount += numOfPage;
	AccessStatistics.TotalWriteSubPageCount += MaxSubPage * numOfPage;
	//# Update the number of accessed logical pages
	AccessedLogicalPages += numOfPage;	//# advanced the number of set flags in the map
	for(j = 0;j < numOfPage; j++)
	{
		AccessedLogicalPageMap[blockAddr * MaxPage + j] = True; //# Set the flag on
	}

	// Update virtual region size
	VirtualRegionSizeUpdate(currentRegion);
	// Track the SRAM usage
	UpdateLruItemSize(&virtualRegionLRUSRAM, lruPos, currentRegion->size, &AccessStatistics.TotalWriteRequestTime, False);

	return 0;
}

//# ****************************************************************
//# Flush the cached content from SRAM to flash when the virtual region is evicted from LRU list
//# ****************************************************************
void FlushVirtualRegion(flash_size_t virtualRegionNum, int pos, Boolean getFreeBlockMethod, flash_huge_size_t *totalRequestTime) {
	if(DEBUG_HANK)
		printf("INFO : Subpage remaining (%d) in Virtual Region %d \n", virtualRegionList[virtualRegionNum].usedPageBuffer, virtualRegionNum);

	if(virtualRegionList[virtualRegionNum].usedPageBuffer > 0) {
		// Check the number of available page
		if(FreePageCnt[virtualRegionList[virtualRegionNum].currBlock->blockNum] == 0) {
			// Allocate a new block since there is no page left
			AllocateBlockToVirtualRegion(&virtualRegionList[virtualRegionNum], getFreeBlockMethod);
			// Update virtual region size
			VirtualRegionSizeUpdate(&virtualRegionList[virtualRegionNum]);
			// Track the SRAM usage
			UpdateLruItemSize(&virtualRegionLRUSRAM, pos, virtualRegionList[virtualRegionNum].size, &AccessStatistics.TotalWriteRequestTime, getFreeBlockMethod);
		}

		// Update page address
		flash_size_t pageAddr = MaxPage - FreePageCnt[virtualRegionList[virtualRegionNum].currBlock->blockNum];
		flash_size_t chipAddr = (int) (virtualRegionList[virtualRegionNum].currBlock->blockNum / BlocksPerChip);

		// Debug
		if (DEBUG_HANK) {
			printf("INFO : Flush buffer with size of %d to chip %d block %d (Available %d), Page %d. \n",
					virtualRegionList[virtualRegionNum].usedPageBuffer,
					chipAddr,
					virtualRegionList[virtualRegionNum].currBlock->blockNum,
					FreePageCnt[virtualRegionList[virtualRegionNum].currBlock->blockNum],
					pageAddr);
		}
		// Update block info
		FreePageCnt[virtualRegionList[virtualRegionNum].currBlock->blockNum] -= 1;
		ValidPageCnt[virtualRegionList[virtualRegionNum].currBlock->blockNum] += 1;
		// Update page info
		FreeSubPageCount(virtualRegionList[virtualRegionNum].currBlock->blockNum, (MaxSubPage - virtualRegionList[virtualRegionNum].usedPageBuffer) * -1);
		VaildSubPageCount(virtualRegionList[virtualRegionNum].currBlock->blockNum, virtualRegionList[virtualRegionNum].usedPageBuffer);
		// Statistic
		AccessStatistics.TotalWritePageCount += 1;
		AccessStatistics.TotalWriteSubPageCount += virtualRegionList[virtualRegionNum].usedPageBuffer;
		AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE;
		// buffer full, flush
		virtualRegionList[virtualRegionNum].usedPageBuffer = 0;
		// Update virtual region size
		VirtualRegionSizeUpdate(&virtualRegionList[virtualRegionNum]);
		// Change Node cached status
		int check = DeleteCachedNodeList(&virtualRegionList[virtualRegionNum]);
		if(check != virtualRegionList[virtualRegionNum].cachedTreeNodeCount)
			printf("Mismatch cached node count. \n");
		virtualRegionList[virtualRegionNum].cachedTreeNodeCount = 0;
		// Track the SRAM usage
		UpdateLruItemSize(&virtualRegionLRUSRAM, pos, virtualRegionList[virtualRegionNum].size, totalRequestTime, False);
		// Set the page valid in flash
		int q = 0;
		for(q = 0;q < MaxSubPage;q++) {
#ifdef BEYONDADDR
			SetFlashSubPageStatus(virtualRegionList[virtualRegionNum].currBlock->blockNum, pageAddr, q, VALID_PAGE);
#endif
		}
		// Logical Static
		AccessedLogicalPages += 1; // advanced the number of set flags in the map
		AccessedLogicalPageMap[virtualRegionList[virtualRegionNum].currBlock->blockNum * MaxPage + pageAddr] = True; // Update the number of accessed logical pages
	} else {
		if (DEBUG_HANK)
			printf("INFO : No Flush required. \n");
	}
}

int ConvertLbaToVirtualRegionNum(int lba) {
	if (lba > MaxLBA) {
		printf("ERROR: The request LBA (%d) is larger the the max LBA (%d) \n", lba, MaxLBA);
		return NULL;
	}
	// calculate virtual region number
	int virtualRegionNum = (int) (lba / virtualRegionAddrSize);
	if(virtualRegionNum >= VirtualRegionNum) {
		printf("ERROR: The region (%d) of request LBA (%d) exceed limit (%d) \n", lba, (int) (lba / virtualRegionAddrSize), VirtualRegionNum);
		return NULL;
	}
	if (DEBUG_HANK)
		printf("INFO : The request LBA (%d) is in region (%d), ", lba, virtualRegionNum);

	return virtualRegionNum;
}

//# ****************************************************************
//# The right WriteFunction to call
//# Divide and send requests to different regions
//# ****************************************************************
int WriteRegional(flash_size_t lba, int length, Boolean skipChecking, Boolean getFreeBlockMethod) {
	// Check if the incoming request overlaps multiple regions
	int headRegion = ConvertLbaToVirtualRegionNum(lba);
	int tailRegion = ConvertLbaToVirtualRegionNum(lba);
	int res = 0;

	// Within the same region, no worry
	if (headRegion == tailRegion) {
		return Write(lba, length, skipChecking, getFreeBlockMethod);
	} else {
		// Calculate how many region does it overlapped
		int overlap = tailRegion - headRegion;
		int i = 0;
		int currentRegion = headRegion;
		int startLba = lba;

		// Write to the first N regions
		while (i > overlap) {
			// Calculate segment length
			int len = (currentRegion + 1)  * virtualRegionAddrSize - startLba;
			// Write to region
			res += Write(startLba, len, skipChecking, getFreeBlockMethod);
			// Update starting lba for next write request
			startLba = (currentRegion + 1) * virtualRegionAddrSize;

			i += 1;
			currentRegion = headRegion + i;
		};
		// Write to tail region
		int lenTail = (lba + length) - startLba;
		res += Write(startLba, lenTail, skipChecking, getFreeBlockMethod);
	}

	return res;
}

//# ****************************************************************
//# Write some content to 'lba' with 'length'
//# ****************************************************************
int Write(flash_size_t lba, int length, Boolean skipChecking, Boolean getFreeBlockMethod) {
	// Find the corresponding virtual region
	int lruPos;	// the position of the element in the LRU list
	virtualRegion *currentRegion = ConvertLbaToVirtualRegion(lba, &AccessStatistics.TotalWriteRequestTime, &lruPos, getFreeBlockMethod);

	if(currentRegion == NULL) {
		return length;
	}

	// Calculate number of subpages
	if (DEBUG_HANK)
		printf("INFO : Write to lba %d with length %d. \n", lba, length);

	// Make sure there is always some block or free space before starting
	if (currentRegion->blockCount == 0 || FreePageCnt[currentRegion->currBlock->blockNum] == 0)
		AllocateBlockToVirtualRegion(currentRegion, getFreeBlockMethod);

	// Update virtual region size
	VirtualRegionSizeUpdate(currentRegion);
	// Track the SRAM usage
	UpdateLruItemSize(&virtualRegionLRUSRAM, lruPos, currentRegion->size, &AccessStatistics.TotalWriteRequestTime, False);

	if (currentRegion->currBlock->blockNum == -1) {
		printf("Error : Unable to get a new block. \n");
		return length;
	}

	// Debug
	if (DEBUG_HANK) {
		printf("# (LBA %d) Enter block %d, free page %d, free subpage %d \n",
				lba, currentRegion->currBlock->blockNum, FreePageCnt[currentRegion->currBlock->blockNum], FreeSubPageCnt[currentRegion->currBlock->blockNum]);
	}

	// Check if this write request is all cached in page cache
	Boolean allCached = False;
	if((MaxSubPage - currentRegion->usedPageBuffer) >= length) {
		allCached = True;
	}

	// Check if this write request stay in one single block
	Boolean withInOneBlock = False;
	if((FreeSubPageCnt[currentRegion->currBlock->blockNum] - currentRegion->usedPageBuffer) >= length) {
		withInOneBlock = True;
	}

	// Prepare MrM Tree node information
	flash_size_t chipAddr = (int) (currentRegion->currBlock->blockNum / BlocksPerChip);
	//flash_size_t blockAddr = currentRegion->currBlock->blockNum;
	flash_size_t pageAddr = MaxPage - FreePageCnt[currentRegion->currBlock->blockNum];
	flash_size_t subPageOff = currentRegion->usedPageBuffer;

	flash_size_t page = pageAddr;
	flash_size_t subPage = subPageOff;

	int len = length;
	int left = length;
	do {
		// Check block allocation status
		if (FreePageCnt[currentRegion->currBlock->blockNum] == 0 && left > 0) {
			// there is no page in current block, allocate a new block
			AllocateBlockToVirtualRegion(currentRegion, getFreeBlockMethod);
			// Update virtual region size
			VirtualRegionSizeUpdate(currentRegion);
			// Track the SRAM usage
			UpdateLruItemSize(&virtualRegionLRUSRAM, lruPos, currentRegion->size, &AccessStatistics.TotalWriteRequestTime, False);
			// Update MrM Tree node information
			chipAddr = (int) (currentRegion->currBlock->blockNum / BlocksPerChip);
			//blockAddr = currentRegion->currBlock->blockNum;
			page = MaxPage - FreePageCnt[currentRegion->currBlock->blockNum];
			subPage = currentRegion->usedPageBuffer;

			pageAddr = page;
			subPageOff = subPage;

			// Check all cached status
			if((MaxSubPage - currentRegion->usedPageBuffer) >= len) {
				allCached = True;
			}

		} else if (FreePageCnt[currentRegion->currBlock->blockNum] == 1 && left > 0) {
			// there is only one page left, try to flush current buffer
			int availableBuffer = MaxSubPage - currentRegion->usedPageBuffer;
			// Still some space left, partially caching current request
			if (availableBuffer > 0) {
				// Force creating a mrm node to avoid spanning a node over two blocks
				if (left > availableBuffer) {
					// Debug
					if (DEBUG_HANK) {
						printf("# (LBA %d) Buffering : %d, Left : %d, Total Buffered : %d \n",
								lba, availableBuffer, (left - availableBuffer), currentRegion->usedPageBuffer);
					}
					// Put data into Buffer
					ChangeUsedBuffer(currentRegion, availableBuffer);
					// Update virtual region size
					VirtualRegionSizeUpdate(currentRegion);
					// Track the SRAM usage
					UpdateLruItemSize(&virtualRegionLRUSRAM, lruPos, currentRegion->size, &AccessStatistics.TotalWriteRequestTime, False);
					// Update free subpage count
					FreeSubPageCount(currentRegion->currBlock->blockNum, availableBuffer * -1);
					// Create a MrM node for the data belongs to last block
					CreateMrMNode(currentRegion, lba, (len - left + availableBuffer), chipAddr, currentRegion->currBlock->blockNum, pageAddr, subPageOff,
								  skipChecking, lruPos, allCached);
					lba += (len - left + availableBuffer);
					len -= (len - left + availableBuffer);
					// Update left length
					left -= availableBuffer;
				} else {
					// Debug
					if (DEBUG_HANK) {
						printf("# (LBA %d) Buffering : %d, Left : %d, Total Buffered : %d \n",
								lba, left, 0,
								currentRegion->usedPageBuffer);
					}
					// Put data into Buffer
					ChangeUsedBuffer(currentRegion, left);
					// Update virtual region size
					VirtualRegionSizeUpdate(currentRegion);
					// Track the SRAM usage
					UpdateLruItemSize(&virtualRegionLRUSRAM, lruPos, currentRegion->size, &AccessStatistics.TotalWriteRequestTime, False);
					// Update free subpage count
					FreeSubPageCount(currentRegion->currBlock->blockNum, left * -1);
					// Update left length
					left = 0;
				}
			} else if (availableBuffer == 0) {
				printf(" ERROR in writing, availableBuffer = 0 \n");
			}
		} else if(FreePageCnt[currentRegion->currBlock->blockNum] > 1 && left > 0) {
			// Check buffer status
			if (currentRegion->usedPageBuffer + left <= MaxSubPage) {
				// current space in buffer is still sufficient, just buffer :)
				// Debug
				if (DEBUG_HANK) {
					printf("# (LBA %d) Buffering : %d, Left : %d, Total Buffered : %d \n",
							lba, left, 0, currentRegion->usedPageBuffer);
				}
				// Put data into Buffer
				ChangeUsedBuffer(currentRegion, left);
				// Update virtual region size
				VirtualRegionSizeUpdate(currentRegion);
				// Track the SRAM usage
				UpdateLruItemSize(&virtualRegionLRUSRAM, lruPos, currentRegion->size, &AccessStatistics.TotalWriteRequestTime, False);
				// Update free subpage count
				FreeSubPageCount(currentRegion->currBlock->blockNum, left * -1);
				// Update left length
				left = 0;
			} else {
				// Current space in buffer is not enough, need a new page
				int availableBuffer = MaxSubPage - currentRegion->usedPageBuffer;
				// Still some space left, partially caching current request
				if (availableBuffer > 0) {
					// Debug
					if (DEBUG_HANK) {
						printf("# (LBA %d) Buffering : %d, Left : %d, Total Buffered : %d \n",
								lba, availableBuffer, (left - availableBuffer),
								currentRegion->usedPageBuffer);
					}
					// Put data into Buffer
					ChangeUsedBuffer(currentRegion, availableBuffer);
					// Update virtual region size
					VirtualRegionSizeUpdate(currentRegion);
					// Track the SRAM usage
					UpdateLruItemSize(&virtualRegionLRUSRAM, lruPos, currentRegion->size, &AccessStatistics.TotalWriteRequestTime, False);
					// Update free subpage count
					FreeSubPageCount(currentRegion->currBlock->blockNum, availableBuffer * -1);
					// Update left length
					left -= availableBuffer;
				}
			}

		}
		// Let's flush the buffer when its full
		if (currentRegion->usedPageBuffer == MaxSubPage) {
			// Debug
			if (DEBUG_HANK) {
				printf("# (LBA %d) Flush buffer with size of %d to chip %d block %d (Available %d), Page %d. \n",
						lba, currentRegion->usedPageBuffer, chipAddr,
						currentRegion->currBlock->blockNum, FreePageCnt[currentRegion->currBlock->blockNum],
						pageAddr);
			}
			// buffer full, flush
			currentRegion->usedPageBuffer = 0;
			// Update virtual region size
			VirtualRegionSizeUpdate(currentRegion);
			// Track the SRAM usage
			UpdateLruItemSize(&virtualRegionLRUSRAM, lruPos, currentRegion->size, &AccessStatistics.TotalWriteRequestTime, False);
			// Statistics - write a page
			AccessStatistics.TotalWriteRequestTime += TIME_WRITE_PAGE;
			AccessStatistics.TotalWritePageCount += 1;
			AccessStatistics.TotalWriteSubPageCount += MaxSubPage;
			// Update block info
			FreePageCnt[currentRegion->currBlock->blockNum] -= 1;
			ValidPageCnt[currentRegion->currBlock->blockNum] += 1;
			VaildSubPageCount(currentRegion->currBlock->blockNum, MaxSubPage);
			// Change Node cached status
			int check = DeleteCachedNodeList(currentRegion);
			if(check != currentRegion->cachedTreeNodeCount)
				printf("Mismatch cached node count. \n");
			currentRegion->cachedTreeNodeCount = 0;
			// Set the page valid in flash
			int q = 0;
			for(q = 0;q < MaxSubPage;q++) {
#ifdef BEYONDADDR
				SetFlashSubPageStatus(currentRegion->currBlock->blockNum, page, q, VALID_PAGE);
#endif
			}
			// Logical Static
			AccessedLogicalPages += 1; // advanced the number of set flags in the map
			AccessedLogicalPageMap[currentRegion->currBlock->blockNum * MaxPage + page] = True; // Update the number of accessed logical pages
			// Update page address
			page = MaxPage - FreePageCnt[currentRegion->currBlock->blockNum];
		}
	} while (left > 0);

	// Create MrM Node
	if(withInOneBlock == True)
		CreateMrMNode(currentRegion, lba, length, chipAddr, currentRegion->currBlock->blockNum, pageAddr, subPageOff, skipChecking, lruPos, allCached);
	else
		CreateMrMNode(currentRegion, lba, len, chipAddr, currentRegion->currBlock->blockNum, pageAddr, subPageOff, skipChecking, lruPos, allCached);

	// Update virtual region size
	VirtualRegionSizeUpdate(currentRegion);
	// Track the SRAM usage
	UpdateLruItemSize(&virtualRegionLRUSRAM, lruPos, currentRegion->size, &AccessStatistics.TotalWriteRequestTime, False);

/*
	// Check the num Valid Subpages
	if (currentRegion->mrmTreeRoot != NULL && currentRegion->usedPageBuffer == 0) {
		int* vaildSubPageCount = (int*) malloc(sizeof(int) * currentRegion->blockCount);
		memset(vaildSubPageCount, 0, sizeof(int) * currentRegion->blockCount);

		MrMTreeTravse(currentRegion, currentRegion->mrmTreeRoot, vaildSubPageCount, False);

		int z = 0;
		for (z = 0; z < currentRegion->blockCount; z++) {
			int blkNum = ConvertIndexToBlkNum(currentRegion->allocatedBlockListHead, z);

			if (vaildSubPageCount[z] != ValidSubPageCnt[blkNum]) {
				printf("** Block %d, valid SubPgae %d (%d), \n", blkNum,
						vaildSubPageCount[z], ValidSubPageCnt[blkNum]);
			}
		}
		free(vaildSubPageCount);
	}
*/

	return left;
}

void CreateMrMNode(struct virtualRegion *currentRegion, flash_size_t lba,
		flash_size_t len, flash_size_t chipAddr, flash_size_t blockAddr,
		flash_size_t pageAddr, flash_size_t subPageOff, Boolean skipChecking,
		int lruPos, Boolean allCached) {
	if (len == 0) {
		printf("WTF, len = 0 at CreateMrMNode(). \n");
		return;
	}
	if (pageAddr + (len / MaxSubPage) > MaxPage) {
		printf("WTF, tree node should not span over two block at CreateMrMNode(). \n");
		return;
	}
	if(pageAddr > 256 || 0 > pageAddr) {
		printf("WTF, pageAddr in-correct at CreateMrMNode(). \n");
		return;
	}
	if(subPageOff > 32 || 0 > subPageOff) {
		printf("WTF, subPageOff in-correct at CreateMrMNode(). \n");
		return;
	}
	// Check the items in the MrM Tree
	if (currentRegion->mrmTreeRoot != NULL && currentRegion->nodeCount != 0) {
		// The tree is initialized
		if (!skipChecking) { // Check for overlapped region
			treeNode *res = Find(currentRegion->mrmTreeRoot, lba);
			if (res != NULL) { // ok, we find a tree node start with the same LBA
				// Check the length of MrM node and the new request
				if (len == res->length) { // That's great, the length is identical. We just need to update the mrm info
					// Invalidate those subpage belongs to the original block
					if(InvalidateSupPage(currentRegion, len, res, lruPos, False) == OVERLAP_NOT_IN_CACHE) {
						// Set the page invalid in flash
						SetSubPageStatusOnFlash(len, res, 0, INVALID_PAGE);
					}
					// Update MrM node information
					res->chipAddr = chipAddr;
					res->blockAddr = blockAddr;
					res->pageAddr = pageAddr;
					res->subPageOff = subPageOff;

					if(res->cacheStatus == False && allCached == True) {
						ChangeCacheTreeNodeCnt(currentRegion, 1);
						AddToCachedNodeList(currentRegion, res);
					} else if(res->cacheStatus == True && allCached == False) {
						ChangeCacheTreeNodeCnt(currentRegion, -1);
						DeleteCachedNodeFromList(currentRegion, res);
					}
					res->cacheStatus = allCached;
				} else if (len < res->length) {	// Acceptable, we need to split the found mrm node
					// Invalidate those subpage belongs to the original block
					if(InvalidateSupPage(currentRegion, len, res, lruPos, False) == OVERLAP_NOT_IN_CACHE) {
						// Set the page invalid in flash
						SetSubPageStatusOnFlash(len, res, 0, INVALID_PAGE);
					}
					// Prepare the split MrM node information for latter use
					flash_size_t lbaN = res->lba + len;
					flash_size_t lenN = res->length - len;
					flash_size_t chipAddrN = res->chipAddr;
					flash_size_t blockAddrN = res->blockAddr;
					flash_size_t subPageOffN = res->subPageOff + len;
					flash_size_t pageAddrN = res->pageAddr;
					Boolean allCachedN = res->cacheStatus;

					if (subPageOffN > MaxSubPage) {
						int add = (int) floor((double) subPageOffN / (double) MaxSubPage);
						pageAddrN += add;
						subPageOffN -= MaxSubPage * add;
					}
					// Delete original node
					if(res->cacheStatus == True) {
						ChangeCacheTreeNodeCnt(currentRegion, -1);
						DeleteCachedNodeFromList(currentRegion, res);
					}
					currentRegion->mrmTreeRoot = Delete(currentRegion, currentRegion->mrmTreeRoot, res->lba);
					currentRegion->nodeCount -= 1;
					// Update virtual region info
					ChangeVaildSubPageCnt(currentRegion, -len);

					// Insert the split node
					if (lenN == 0) {
						printf("WTF, len = 0 at CreateMrMNode(). \n");
						return;
					}
					RemoveOverlapped(currentRegion, currentRegion->mrmTreeRoot, lbaN,
							lenN, lruPos);
					currentRegion->mrmTreeRoot = Insert(currentRegion->mrmTreeRoot, lbaN,
							lenN, chipAddrN, blockAddrN, pageAddrN, subPageOffN, allCachedN);
					currentRegion->nodeCount += 1;
					if(allCachedN == True) {
						ChangeCacheTreeNodeCnt(currentRegion, 1);
						AddToCachedNodeList(currentRegion, insertedNode);
					}
					insertedNode = NULL;
					// Insert the update node
					RemoveOverlapped(currentRegion, currentRegion->mrmTreeRoot, lba,
							len, lruPos);
					currentRegion->mrmTreeRoot = Insert(currentRegion->mrmTreeRoot, lba,
							len, chipAddr, blockAddr, pageAddr, subPageOff, allCached);
					currentRegion->nodeCount += 1;
					if(allCached == True) {
						ChangeCacheTreeNodeCnt(currentRegion, 1);
						AddToCachedNodeList(currentRegion, insertedNode);
					}
					insertedNode = NULL;
					// Update virtual region info
					ChangeVaildSubPageCnt(currentRegion, len);
				} else if (len > res->length) {
					// Check overlapped with the node on right handside
					if (res->right != NULL)
						RemoveOverlapped(currentRegion, res->right, lba, len, lruPos);
					// Invalidate those subpage belongs to the original node
					if(InvalidateSupPage(currentRegion, res->length, res, lruPos, False) == OVERLAP_NOT_IN_CACHE){
						// Set the page invalid in flash
						SetSubPageStatusOnFlash(res->length, res, 0, INVALID_PAGE);
					}
					// Update virtual region info
					ChangeVaildSubPageCnt(currentRegion, -res->length);
					// Delete original node
					if(res->cacheStatus == True) {
						ChangeCacheTreeNodeCnt(currentRegion, -1);
						DeleteCachedNodeFromList(currentRegion, res);
					}
					currentRegion->mrmTreeRoot = Delete(currentRegion, currentRegion->mrmTreeRoot, res->lba);
					currentRegion->nodeCount -= 1;

					// Insert this node
					RemoveOverlapped(currentRegion, currentRegion->mrmTreeRoot, lba,
							len, lruPos);
					currentRegion->mrmTreeRoot = Insert(currentRegion->mrmTreeRoot, lba,
							len, chipAddr, blockAddr, pageAddr, subPageOff, allCached);
					currentRegion->nodeCount += 1;
					if(allCached == True) {
						ChangeCacheTreeNodeCnt(currentRegion, 1);
						AddToCachedNodeList(currentRegion, insertedNode);
					}
					insertedNode = NULL;
					// Update virtual region info
					ChangeVaildSubPageCnt(currentRegion, len);
				}
			} else { // We failed to find a block start with LBA, let's check for overlapped
				RemoveOverlapped(currentRegion, currentRegion->mrmTreeRoot, lba,
						len, lruPos);
				// Insert this node
				currentRegion->mrmTreeRoot = Insert(currentRegion->mrmTreeRoot, lba, len, chipAddr,
						blockAddr, pageAddr, subPageOff, allCached);
				currentRegion->nodeCount += 1;
				if(allCached == True) {
					ChangeCacheTreeNodeCnt(currentRegion, 1);
					AddToCachedNodeList(currentRegion, insertedNode);
				}
				insertedNode = NULL;
				// Update virtual region info
				ChangeVaildSubPageCnt(currentRegion, len);
			}
		}
	} else {
		// the tree is not initialized
		// Insert this node
		currentRegion->mrmTreeRoot = Insert(currentRegion->mrmTreeRoot, lba, len, chipAddr, blockAddr,
				pageAddr, subPageOff, allCached);
		currentRegion->nodeCount += 1;
		if(allCached == True) {
			ChangeCacheTreeNodeCnt(currentRegion, 1);
			AddToCachedNodeList(currentRegion, insertedNode);
		}
		insertedNode = NULL;
		// Update virtual region info
		ChangeVaildSubPageCnt(currentRegion, len);
	}

}

//* ****************************************************************
//* This function is not used to find a node starting with same LBA
//* It can only remove overlapped part on other nodes
//* ****************************************************************
void RemoveOverlapped(struct virtualRegion *currentRegion, treeNode *node, flash_size_t lba, flash_size_t len, int pos) {
	int saftLenCheck = node->length;
	if (lba < node->lba &&
		(lba + len - 1) >= node->lba &&
	    (lba + len - 1) <= (node->lba + node->length -1)) {
		// The new node is overlapped the original node on the left hand side

		// Continue check left subtree
		if (node->left != NULL)
			RemoveOverlapped(currentRegion, node->left, lba, len, pos);

		int overlappedLen = lba + len - node->lba;
		// Invalidate those subpage belongs to the original node
		if(InvalidateSupPage(currentRegion, overlappedLen, node, pos, False) == OVERLAP_NOT_IN_CACHE){
			// Set the page invalid in flash
			SetSubPageStatusOnFlash(overlappedLen, node, 0, INVALID_PAGE);
		}
		flash_size_t lbaN = node->lba + overlappedLen;
		flash_size_t lenN = node->length - overlappedLen;
		flash_size_t chipAddrN = node->chipAddr;
		flash_size_t blockAddrN = node->blockAddr;
		flash_size_t subPageOffN = node->subPageOff + overlappedLen;
		flash_size_t pageAddrN = node->pageAddr;
		Boolean allCachedN = node->cacheStatus;
		if (subPageOffN > MaxSubPage) {
			int add = (int) floor((double) subPageOffN / (double) MaxSubPage);
			pageAddrN += add;
			subPageOffN -= MaxSubPage * add;
		}
		// Update virtual region info
		ChangeVaildSubPageCnt(currentRegion, -overlappedLen);
		// Delete original node
		if(allCachedN == True) {
			ChangeCacheTreeNodeCnt(currentRegion, -1);
			DeleteCachedNodeFromList(currentRegion, node);
		}
		currentRegion->mrmTreeRoot = Delete(currentRegion, currentRegion->mrmTreeRoot, node->lba);
		currentRegion->nodeCount -= 1;
		// Insert the spilt node
		if(lenN > 0) {
			RemoveOverlapped(currentRegion, currentRegion->mrmTreeRoot, lbaN,
					lenN, pos);
			currentRegion->mrmTreeRoot = Insert(currentRegion->mrmTreeRoot,
					lbaN, lenN, chipAddrN, blockAddrN, pageAddrN, subPageOffN, allCachedN);
			currentRegion->nodeCount += 1;
			if(allCachedN == True) {
				ChangeCacheTreeNodeCnt(currentRegion, 1);
				AddToCachedNodeList(currentRegion, insertedNode);
			}
			insertedNode = NULL;
		}
	} else if (lba > node->lba &&
				lba <= (node->lba + node->length - 1) &&
				(lba + len - 1) >= (node->lba + node->length - 1)) {
		// The new node is overlapped on the right hand side

		// Continue check right subtree
		if (node->right != NULL)
			RemoveOverlapped(currentRegion, node->right, lba, len, pos);

		int overlappedLen = (node->lba + node->length) - lba;
		// Invalidate those subpage belongs to the original node
		if(InvalidateSupPage(currentRegion, overlappedLen, node, pos, False) == OVERLAP_NOT_IN_CACHE){
			// Set the page invalid in flash
			SetSubPageStatusOnFlash(overlappedLen, node, (lba - node->lba), INVALID_PAGE);
		}
		node->length -= overlappedLen;
		// Update virtual region info
		ChangeVaildSubPageCnt(currentRegion, -overlappedLen);

	} else if (lba > node->lba &&
			   lba < (node->lba + node->length - 1) &&
				(lba + len - 1) <= (node->lba + node->length - 1)) {
		// The new node is wrapped by the original node

		// Create a new right node
		flash_size_t lbaN = lba + len;
		flash_size_t lenN = (node->lba + node->length) - (lba + len);
		flash_size_t chipAddrN = node->chipAddr;
		flash_size_t blockAddrN = node->blockAddr;
		flash_size_t subPageOffN = node->subPageOff + (lbaN - node->lba);
		flash_size_t pageAddrN = node->pageAddr;
		Boolean allCachedN = node->cacheStatus;

		if (subPageOffN > MaxSubPage) {
			int add = (int) floor((double) subPageOffN / (double) MaxSubPage);
			pageAddrN += add;
			subPageOffN -= MaxSubPage * add;
		}
		saftLenCheck -= lenN;

		// Update original one to become a new left node
		node->length = lba - node->lba;
		// Invalidate overlapped subpages
		if(InvalidateSupPage(currentRegion, len, node, pos, False) == OVERLAP_NOT_IN_CACHE){
			// Set the page invalid in flash
			SetSubPageStatusOnFlash(len, node, (lba - node->lba), INVALID_PAGE);
		}
		saftLenCheck -= node->length;

		// Insert the new node
		RemoveOverlapped(currentRegion, currentRegion->mrmTreeRoot, lbaN,
				lenN, pos);
		currentRegion->mrmTreeRoot = Insert(currentRegion->mrmTreeRoot, lbaN,
				lenN, chipAddrN, blockAddrN, pageAddrN, subPageOffN, allCachedN);
		currentRegion->nodeCount += 1;
		if(allCachedN == True) {
			ChangeCacheTreeNodeCnt(currentRegion, 1);
			AddToCachedNodeList(currentRegion, insertedNode);
		}
		insertedNode = NULL;
		// Update virtual region info
		ChangeVaildSubPageCnt(currentRegion, -len);

		saftLenCheck -= len;
		if(saftLenCheck != 0) {
			printf("Error at RemoveOverlapped(), saftLenCheck %d \n", saftLenCheck);
		}
	} else if (lba <= node->lba
			&& lba < (lba + len - 1)
			&& (lba + len - 1) >= (node->lba + node->length - 1)) {
		// The original node is wrapped by the new node
		// Continue check right subtree
		if (node->right != NULL)
			RemoveOverlapped(currentRegion, node->right, lba, len, pos);

		// Continue check left subtree
		if (node->left != NULL)
			RemoveOverlapped(currentRegion, node->left, lba, len, pos);

		// Invalidate those subpage belongs to the original node
		if(InvalidateSupPage(currentRegion, node->length, node, pos, False) == OVERLAP_NOT_IN_CACHE){
			// Set the page invalid in flash
			SetSubPageStatusOnFlash(node->length, node, 0, INVALID_PAGE);
		}
		// Update virtual region info
		ChangeVaildSubPageCnt(currentRegion, -node->length);
		// Remove this node
		if(node->cacheStatus == True) {
			ChangeCacheTreeNodeCnt(currentRegion, -1);
			DeleteCachedNodeFromList(currentRegion, node);
		}
		currentRegion->mrmTreeRoot = Delete(currentRegion, currentRegion->mrmTreeRoot, node->lba);
		currentRegion->nodeCount -= 1;
	} else if ((lba + len - 1) < node->lba) {
		// The node is not overlapped, check left subtree
		if (node->left != NULL)
			RemoveOverlapped(currentRegion, node->left, lba, len, pos);
	} else if ((node->lba + node->length - 1) < lba) {
		// The node is not overlapped, check right subtree
		if (node->right != NULL)
			RemoveOverlapped(currentRegion, node->right, lba, len, pos);
	}
}
/*
 void RemoveSubtree(struct virtualRegion *currentRegion, treeNode *node)
 {
 if (node->right != NULL)
 {
 RemoveSubtree(currentRegion, node->right);
 }
 if (node->left != NULL)
 {
 RemoveSubtree(currentRegion, node->right);
 }
 if (node->right == NULL && node->right == NULL)
 {
 Delete(currentRegion->mrmTreeRoot, node->lba);
 currentRegion->nodeCount -= 1;
 }
 }
 */

void CheckSubPageAllocationStatus(int z) {
	char OutFilename[BUFFER_SIZE];	//* output file name
	sprintf(OutFilename, "%d_SubPageStatus_%s.log", z, FTLMethod);
	FILE *OutFp = fopen(OutFilename, "wb"); //# open output file

	int freeCount = 0, vaildCount = 0, invaildCount = 0, badBlockCnt = 0;
	int i = 0, j = 0, k = 0;

	for (i = 0; i < MaxBlock; i++) {
		fprintf(OutFp, "# Block %10d ", i);
#ifdef BEYONDADDR
		if(GetFlashSubPageStatus(i, 0, 0) == BAD_BLOCK) {
#else
		if(0) {
#endif
			fprintf(OutFp, " : BAD BLOCK \n");
			badBlockCnt += 1;
		} else {
			fprintf(OutFp, "\n");
			for(j = 0; j < MaxPage; j++) {
				fprintf(OutFp, "Page %4d : ", i * MaxPage + j);
				for(k = 0; k < MaxSubPage; k++) {
					fprintf(OutFp, "Subpage %4d ", k);
#ifdef BEYONDADDR
					if (GetFlashSubPageStatus(i, j, k) == FREE_PAGE) {
						freeCount += 1;
						fprintf(OutFp, "F, ");
					} else if (GetFlashSubPageStatus(i, j, k) == INVALID_PAGE){\
						invaildCount += 1;
						fprintf(OutFp, "I, ");
					} else if (GetFlashSubPageStatus(i, j, k) == VALID_PAGE){
						vaildCount += 1;
						fprintf(OutFp, "V, ");
					}
#endif
				}
				fprintf(OutFp, "\n");
			}
		}
	}
	fprintf(OutFp, "\n");
	fprintf(OutFp, "--> Free Count %d, Valid Count %d, Invalid Count %d <-- \n", freeCount, vaildCount, invaildCount);
	fprintf(OutFp, "--> F = Free, V = Valid, I = Invalid <-- \n");
	fprintf(OutFp, "--> Bad Block Count %d <-- \n", badBlockCnt);
	fclose(OutFp);	//* close file descriptor
}

void CheckBlockAllocationStatus(int j) {
	char OutFilename[BUFFER_SIZE];	//* output file name
	sprintf(OutFilename, "%d_Block_Allocation_%s.log", j, FTLMethod);
	FILE *OutFp = fopen(OutFilename, "wb"); //# open output file

	int vaildBlk = 0;
	int freeBlk = 0;

	int i = 0;
	for (i = 0; i < MaxBlock - 6; i += 6) {
		fprintf(OutFp, "Blk %5d Free %3d Valid %3d", i, FreePageCnt[i],
				ValidPageCnt[i]);
		fprintf(OutFp, " # Blk %5d Free %3d Valid %3d", i + 1,
				FreePageCnt[i + 1], ValidPageCnt[i + 1]);
		fprintf(OutFp, " # Blk %5d Free %3d Valid %3d", i + 2,
				FreePageCnt[i + 2], ValidPageCnt[i + 2]);
		fprintf(OutFp, " # Blk %5d Free %3d Valid %3d", i + 3,
				FreePageCnt[i + 3], ValidPageCnt[i + 3]);
		fprintf(OutFp, " # Blk %5d Free %3d Valid %3d", i + 4,
				FreePageCnt[i + 4], ValidPageCnt[i + 4]);
		fprintf(OutFp, " # Blk %5d Free %3d Valid %3d \n", i + 5,
				FreePageCnt[i + 5], ValidPageCnt[i + 5]);
	}

	for (i = 0; i < MaxBlock; i++) {
		if (ValidPageCnt[i] > 0)
			vaildBlk += 1;
		else
			freeBlk += 1;
	}

	fprintf(OutFp, "--> Total %d Free %d Valid %d <-- \n", freeBlk + vaildBlk,
			freeBlk, vaildBlk);
	fclose(OutFp);	//* close file descriptor
}

void CheckAccessedLogicalPages(int z) {
	char OutFilename[BUFFER_SIZE];	//* output file name
	sprintf(OutFilename, "%d_AccessCount_%s.log", z, FTLMethod);
	FILE *OutFp = fopen(OutFilename, "wb"); //# open output file

	int count = 0;
	int i = 0;
	for (i = 0; i < MaxPage * MaxBlock; i++) {
		if (i % MaxPage == 0) {
			fprintf(OutFp, "Block %10d ", i / MaxPage);
		}

		fprintf(OutFp, "Page %10d ", i);

		if (AccessedLogicalPageMap[i] == True) {
			count += 1;
			fprintf(OutFp, "T, ");
		} else {
			fprintf(OutFp, "F, ");
		}

		if ((i == MaxPage - 1) || ((i - (MaxPage - 1)) % MaxPage == 0)) {
			fprintf(OutFp, "\n");
		}

	}
	fprintf(OutFp, "\n");
	fprintf(OutFp, "--> Accessed Count %d ArraryCount %d <-- \n", AccessedLogicalPages, count);
	fclose(OutFp);	//* close file descriptor
}

void CheckRegionStatus(int j) {
	char OutFilename[BUFFER_SIZE];	//* output file name
	sprintf(OutFilename, "%d_Region_Status_%s.log", j, FTLMethod);
	FILE *OutFp = fopen(OutFilename, "wb"); //# open output file

	int validBlk = 0;
	int validSubPage = 0;
	int nodeCnt = 0;

	int i = 0;
	for (i = 0; i < VirtualRegionNum; i++) {
		fprintf(OutFp, "# Virtual Region %d \n", i);

		fprintf(OutFp, "# BlockCount %d \n", virtualRegionList[i].blockCount);
		validBlk += virtualRegionList[i].blockCount;

		fprintf(OutFp, "# Valid SubPage Count %d \n", virtualRegionList[i].validSubPage);
		validSubPage += virtualRegionList[i].validSubPage;

		fprintf(OutFp, "# MrM Tree Node Count %d \n", virtualRegionList[i].nodeCount);
		nodeCnt += virtualRegionList[i].nodeCount;

		fprintf(OutFp, "----------------------------\n");
	}
	fprintf(OutFp, "--> Valid Block %d Valid SubPage %d Node %d <-- \n",
			validBlk, validSubPage, nodeCnt);
	fclose(OutFp);	//* close file descriptor
}

void CheckVaildSubPageCount(int j) {
	char OutFilename[BUFFER_SIZE];	//* output file name
		sprintf(OutFilename, "%d_Block_SubPage_Status_%s.log", j, FTLMethod);
		FILE *OutFp = fopen(OutFilename, "wb"); //# open output file

		int i = 0;
		for (i = 0; i < MaxBlock - 6; i += 6) {
			fprintf(OutFp, "Blk %5d Free %5d Valid %5d, ", i, FreeSubPageCnt[i], ValidSubPageCnt[i]);
			fprintf(OutFp, "Blk %5d Free %5d Valid %5d, ", i+1, FreeSubPageCnt[i+1], ValidSubPageCnt[i+1]);
			fprintf(OutFp, "Blk %5d Free %5d Valid %5d, ", i+2, FreeSubPageCnt[i+2], ValidSubPageCnt[i+2]);
			fprintf(OutFp, "Blk %5d Free %5d Valid %5d, ", i+3, FreeSubPageCnt[i+3], ValidSubPageCnt[i+3]);
			fprintf(OutFp, "Blk %5d Free %5d Valid %5d, ", i+4, FreeSubPageCnt[i+4], ValidSubPageCnt[i+4]);
			fprintf(OutFp, "Blk %5d Free %5d Valid %5d, \n", i+5, FreeSubPageCnt[i+5], ValidSubPageCnt[i+5]);
		}
		fclose(OutFp);	//* close file descriptor
}

int CheckSubpage(virtualRegion* virtualRegionList, int regionNum, int blockNum) {
	int res = 0;

	if (virtualRegionList[regionNum].mrmTreeRoot != NULL && virtualRegionList[regionNum].blockCount != 0) {
		int* vaildSubPageCount = (int*) malloc(sizeof(int) * virtualRegionList[regionNum].blockCount);
		memset(vaildSubPageCount, 0, sizeof(int) * virtualRegionList[regionNum].blockCount);

		MrMTreeTravse(&virtualRegionList[regionNum],
				      virtualRegionList[regionNum].mrmTreeRoot,
					  vaildSubPageCount,
					  False);

		int index = -1;
		if(ConvertBlkNumToIndex(virtualRegionList[regionNum].allocatedBlockListHead,
							 blockNum,
							 &index) == True)
			res = vaildSubPageCount[index];

		free(vaildSubPageCount);
	}

	return res;
}

void CheckSubpageUsage(virtualRegion* virtualRegionList, int* fromRegionTotal, int* fromListTotal) {
	int x = 0;

	do {
		if (virtualRegionList[x].mrmTreeRoot != NULL) {
			int* vaildSubPageCount = (int*) malloc(sizeof(int) * virtualRegionList[x].blockCount);
			memset(vaildSubPageCount, 0, sizeof(int) * virtualRegionList[x].blockCount);
			MrMTreeTravse(&virtualRegionList[x],
					virtualRegionList[x].mrmTreeRoot, vaildSubPageCount, False);
			int z = 0;
			for (z = 0; z < virtualRegionList[x].blockCount; z++) {
				int blkNum = ConvertIndexToBlkNum(virtualRegionList[x].allocatedBlockListHead, z);
				if (vaildSubPageCount[z] != ValidSubPageCnt[blkNum])
					printf("** Region %d, Block %d, valid SubPgae %d (%d), \n",
							x, blkNum, vaildSubPageCount[z],
							ValidSubPageCnt[blkNum]);

				*fromRegionTotal += vaildSubPageCount[z];
				*fromListTotal += ValidSubPageCnt[blkNum];
			}
			free(vaildSubPageCount);
		}
		x++;
	} while (x < VirtualRegionNum - 1);
}

void BeyondAddrMappingSimulation(FILE *fp) {
	InitializeBeyondMapping();
	InitializeVirtualRegions();
	InitializeLRUSRAMList();

	int tmp = 0;

	if (debugMode == 1) {	// for developing purpose

		printf("Number of FreeBlock %d. \n", BlocksInFreeBlockList);
		//+ Randomly select where are the data stored in the flash memory initially
		//+ according the parameter "InitialPercentage" to determine what's the percentage of data in ths flash memory initially.
		int BlockMaxRandomValue;//* The maximal random value, which allows us to select a block storing initial data

		BlockMaxRandomValue = (int) ((float) InitialPercentage * (RAND_MAX)/ 100);	//* The max random number equals "InitialPercentage" percentage of data.
/*
		int i, j, k;
		for(i = 0; i < (MaxBlock - LeastFreeBlocksInFBL() - BadBlockCnt); i++)
		//for(i = 0; i < 10; i++)
		{
			if(rand() <= BlockMaxRandomValue)
			{
				if(DEBUG_HANK)
					printf("# Initial data (%5d) LBA (%5d) \n", i, i * MaxPage * MaxSubPage);
				WriteBlock(i * MaxPage * MaxSubPage, MaxPage, False);
			}
		}
	*/	//# Log information
		printf("# Initial Percentage of data: %.02f%%\n", (float)AccessedLogicalPages / (MaxPage * MaxBlock) * 100);
		printf("# Number of FreeBlock %d. \n", BlocksInFreeBlockList);

		CheckAccessedLogicalPages(0);
		CheckBlockAllocationStatus(0);
		CheckRegionStatus(0);
		CheckSubPageAllocationStatus(0);
/*
		Write(200, 8192, False, False, &lastCacheStatus);

		Write(10, 10, False, False, &lastCacheStatus);

		Write(10, 2, False, False, &lastCacheStatus);


		Write(50, 8000, False, &lastCacheStatus, False);

		Write(40, 20, False, &lastCacheStatus, False);

		Write(250, 10, False, &lastCacheStatus, False);

		Write(255, 10, False, &lastCacheStatus, False);

		Write(270, 50, False, &lastCacheStatus, False);

		Write(8000, 38000, False, &lastCacheStatus, False);

		Write(1000, 50000, False, &lastCacheStatus, False);
*/
		// Flush SRAM back to flash before finishing up
		RemoveElemenetsFromLRUListS(&virtualRegionLRUSRAM, &AccessStatistics.TotalReadRequestTime);
/*
		int x = 0;
		do {
			if(virtualRegionList[x].mrmTreeRoot != NULL) {
				int *vaildSubPageCount = (int *) malloc(sizeof(int) * virtualRegionList[x].blockCount);
				memset(vaildSubPageCount, 0, sizeof(int) * virtualRegionList[x].blockCount);

				MrMTreeTravse(&virtualRegionList[x], virtualRegionList[x].mrmTreeRoot, vaildSubPageCount);

				int z = 0;
				for(z = 0;z < virtualRegionList[x].blockCount;z++) {
					int blkNum = ConvertIndexToBlkNum(virtualRegionList[x].allocatedBlockListHead, z);
					printf("** Region %d, Block %d, valid SubPgae %d (%d), \n",
								x,
								blkNum,
								vaildSubPageCount[z],
								ValidSubPageCnt[blkNum]);
				}

				free(vaildSubPageCount);
			}
			x++;
		} while (x < VirtualRegionNum - 1);

		CheckAccessedLogicalPages(1);
		CheckBlockAllocationStatus(1);
		CheckRegionStatus(1);
		CheckSubPageAllocationStatus(1);
*/
/*		PrintTrees();
		int res = Read(200, 8);
		printf("Read (200, 8), res = %d \n", res);

		 res = Read(270, 10);
		 printf("Read (270, 10), res = %d \n", res);

		 res = Read(250, 150);
		 printf("Read (250, 150), res = %d \n", res);

		 res = Read(8, 2);
		 printf("Read (8, 2), res = %d \n", res);
*/

		PrintTrees();
	} else {	// actual simulation
		printf("Number of FreeBlock %d. \n", BlocksInFreeBlockList);
		//+ Randomly select where are the data stored in the flash memory initially
		//+ according the parameter "InitialPercentage" to determine what's the percentage of data in ths flash memory initially.
		int fromRegionTotal = 0;
		int fromListTotal = 0;
		//Boolean checkLast = False;

		//* The maximal random value, which allows us to select a block storing initial data
		int BlockMaxRandomValue;
		BlockMaxRandomValue = (int) ((float) InitialPercentage * (RAND_MAX)/ 100);	//* The max random number equals "InitialPercentage" percentage of data.

		int i;
		for(i = 0; i < (MaxBlock - LeastFreeBlocksInFBL() - BadBlockCnt); i++)
		//for(i = 0; i < 10; i++)
		{
			if(rand() <= BlockMaxRandomValue)
			{
				if(DEBUG_HANK)
					printf("# Initial data (%5d) LBA (%5d) \n", i, i * MaxPage * MaxSubPage);
				WriteBlock(i * MaxPage * MaxSubPage, MaxPage, False);
			}
		}
		//# Log information
		printf("# Initial Percentage of data: %.02f%%\n", (float)AccessedLogicalPages / (MaxPage * MaxBlock) * 100);
		printf("# Number of FreeBlock %d. \n", BlocksInFreeBlockList);

		CheckAccessedLogicalPages(0);
		CheckBlockAllocationStatus(0);
		CheckRegionStatus(0);
		CheckSubPageAllocationStatus(0);

		//# reset statistic variables
		ResetStatisticVariable(&AccessStatistics);

		// ******************************* Start simulation ***************************
		//* fetch one write operation
		while (GetOneOperation(fp, &CurrentRequest.AccessType, &CurrentRequest.StartCluster, &CurrentRequest.Length, True)
				//&& tmp < 9222974 //2306594 //3881949 //4731953
				) {
			int res = -1;

			if(tmp % 1000 == 0)
				printf("---- %d ---- \n", tmp);

			if (CurrentRequest.AccessType == WriteType) { //# a write command
				res = WriteRegional(CurrentRequest.StartCluster, CurrentRequest.Length, False, False);

				if (res == 0)
					AccessStatistics.TotalWriteRequestCount++;
				else {
					AccessStatistics.FailedWriteOperation += 1;
					printf("Error in when writing to LBA %d with length %d. \n",
							CurrentRequest.StartCluster, CurrentRequest.Length);
				}

			} else if (CurrentRequest.AccessType == ReadType) { //# a read command
				res = Read(CurrentRequest.StartCluster, CurrentRequest.Length, True);
				if (res == 0)
					AccessStatistics.TotalReadRequestCount++;
				else {
					AccessStatistics.FailedReadOperation += 1;
					//printf("Error in when reading to LBA %d with length %d. \n",
					//		CurrentRequest.StartCluster, CurrentRequest.Length);
				}
			}
			tmp += 1;
		}
		// end of while

		// Flush SRAM back to flash before finishing up
		RemoveElemenetsFromLRUListS(&virtualRegionLRUSRAM, &AccessStatistics.TotalWriteRequestTime);

		CheckSubpageUsage(virtualRegionList, &fromRegionTotal, &fromListTotal);
		printf("** Valid SubPageCount : Region %d, List %d \n",fromRegionTotal, fromListTotal);

		CheckAccessedLogicalPages(1);
		CheckBlockAllocationStatus(1);
		CheckRegionStatus(1);
		CheckVaildSubPageCount(1);

		// ******************************* End of simulation *****************************
	}
	printf("FreeBlock %d. \n", BlocksInFreeBlockList);
	printf("Total Write %"PRINTF_LONGLONG" (Failed %"PRINTF_LONGLONG"). \n", AccessStatistics.TotalWriteRequestCount, AccessStatistics.FailedWriteOperation);
	printf("Total Read %"PRINTF_LONGLONG" (Failed %"PRINTF_LONGLONG"). \n", AccessStatistics.TotalReadRequestCount, AccessStatistics.FailedReadOperation);
	printf("Total Operations %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalWriteRequestCount + AccessStatistics.TotalReadRequestCount);
	printf("----------------------------------------- \n");
	printf("TotalBlockEraseCount %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalBlockEraseCount);
	printf("TotalLivePageCopyings %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalLivePageCopyings);
	printf("----------------------------------------- \n");
	printf("TotalWritePageCount %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalWritePageCount);
	printf("TotalReadPageCount %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalReadPageCount);
	printf("----------------------------------------- \n");
	printf("TotalWriteSubPageCount %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalWriteSubPageCount);
	printf("TotalReadSubPageCount %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalReadSubPageCount);
	printf("TotalLiveSubPageCopyings %"PRINTF_LONGLONG". \n",  AccessStatistics.TotalLiveSubPageCopyings);

	FinalizeTranslationTables();
}
