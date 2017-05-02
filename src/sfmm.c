/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include "sfmm.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/**
 * You should store the head of your free list in this variable.
 * Doing so will make it accessible via the extern statement in sfmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */
sf_free_header* freelist_head = NULL;
int sbrk_move = 0;
size_t allocatedBlocks= 0,splinterBlocks=0,padding=0,splintering=0,coalesces=0;
double totalMem = 0.0;

int isValidBlock(void* ptr)
{
	if (ptr > sf_sbrk(0) || ptr < (void*)((char*)(sf_sbrk(0))-sbrk_move))
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

int freeList_length()
{
	sf_free_header * second = freelist_head;
	int length = 0;
	while (second!= NULL)
	{
		second = second -> next;
		length++;
	}
	return length;
}

void removeFromList(sf_free_header* header)
{
	if (header == freelist_head)
	{
		freelist_head = header -> next;
		if (header -> next != NULL)
		{
			(header -> next) -> prev = NULL;
		}
	}
	else{
		sf_free_header* copy = freelist_head;
		while (copy != NULL)
			{
				if (copy == header)
					{
						if (copy -> prev != NULL)
							(copy -> prev) -> next = copy -> next;
						if (copy -> next != NULL)
							(copy -> next) -> prev = copy -> prev;
						return;
					}
			copy = copy -> next;

		}
	}
}

void insertFreeBlock(void* ptr)
{
	if (freelist_head == NULL)
	{
		freelist_head = (sf_free_header*)ptr;
		freelist_head -> next = NULL;
		freelist_head -> prev = NULL;
		return;
	}
	else
	{
		sf_free_header* castedPtr =(sf_free_header*)ptr;
		sf_free_header* copy = freelist_head;
		while (copy != NULL )
		{
			if (castedPtr < copy)
			{
				castedPtr -> prev = copy -> prev;
				castedPtr -> next = copy;
				copy -> prev = castedPtr;
				if (copy == freelist_head)
				{
					freelist_head = castedPtr;
				}
				return;
			}
			else
			{
				copy = copy ->next;
			}
		}
		copy = freelist_head;
		while (copy -> next != NULL )
		{
			copy = copy ->next;
		}
		//printf("IT IS THE LAST BLOCK\n");
		// Free block is the last block
		castedPtr -> next = NULL;
		castedPtr -> prev = copy;
		copy -> next = castedPtr;
	}
}

sf_free_header* coalesce(sf_header* header)
{
	char* startAddress = (char*) header;
	//printf("FREEING BLOCK OF SIZE: %d\n", ((sf_header*) startAddress) -> block_size );
	char* endAddress = startAddress + header -> block_size * 16 - 8;
	int currentBlockSize = header -> block_size;
	int coalesce = 0;
	//Coalesce only if both blocks are free.
	char* lastBlock = startAddress - 8;
	//printf("LAST BLOCK HAS Address: %p\n", (void*)lastBlock);
	//printf("sbrk pointer at %p\n", sf_sbrk(0));
	char* nextBlock = startAddress + header -> block_size * 16;
	//Check if lastBlock can be coalesced
	if (((sf_footer*)lastBlock) -> alloc == 0 && isValidBlock((void*)lastBlock))
	{
		coalesce = 1;
	//	printf("We are coalescing the block b4 us\n");
		startAddress = lastBlock - (((sf_footer*)lastBlock) -> block_size * 16) + 8;
		((sf_header*)startAddress) -> block_size += currentBlockSize;
		//Remove the old free block first since we are combining into alarger block
		((sf_footer*) endAddress) -> block_size = ((sf_header*)startAddress) -> block_size;
		removeFromList((sf_free_header*)startAddress);
	}
	if (((sf_header*)nextBlock) -> alloc == 0 && isValidBlock((void*)nextBlock))
	{
		coalesce = 1;
	//	printf("We are coalescing the block after us");
		endAddress += ((sf_header*)nextBlock) -> block_size * 16;
		((sf_header*)startAddress) -> block_size += ((sf_header*)nextBlock) -> block_size;
		//Remove the that's adjacent from the list.
		removeFromList((sf_free_header*)(nextBlock));
		((sf_footer*) endAddress) -> block_size = ((sf_header*) startAddress) -> block_size;
	}
	//Add a block after coalescing
	//sf_header * test_header = (sf_header*) startAddress;
	//sf_footer * test_footer = (sf_footer*) endAddress;
	//printf("FREELIST LENGTH: %d\n", freeList_length());
	//printf("HEADER BLOCK SIZE:%d\n", test_header -> block_size);
	//printf("FOOTER BLOCK SIZE:%d\n", test_footer -> block_size);
	sf_free_header* freeHeader = (sf_free_header*) startAddress;
	freeHeader -> next = NULL;
	freeHeader -> prev = NULL;
	coalesces += coalesce;
	return freeHeader;
	//insertFreeBlock((void*)(startAddress));
}

void* allocate(sf_free_header* ptr, size_t size)
{
	allocatedBlocks++;
	totalMem+= size;
	int blockSize = (ptr -> header).block_size * 16;
	//printf("BLOCK SIZE: %d\n", blockSize/16);
	char* startBlock = (char*)ptr;
	((sf_header*)startBlock) -> alloc = 1;
	((sf_header*)startBlock) -> requested_size = size;
	int paddingAmt = 16 - (size % 16);
	padding+= paddingAmt;
	((sf_header*)startBlock) -> padding_size = paddingAmt;
	//Check for splinter <32 bytes
	if (blockSize - (size+paddingAmt) - 16 < 32)
	{
		char* endBlock  = startBlock + blockSize - 8;
		if (blockSize - (size+paddingAmt) - 16 != 0)
		{
			splinterBlocks++;
			splintering += blockSize - (size+paddingAmt) - 16;
			((sf_header*)startBlock) -> splinter = 1;
			((sf_footer*) endBlock) -> splinter = 1;
		}
		else
		{
			((sf_header*)startBlock) -> splinter = 0;
			((sf_footer*) endBlock) -> splinter = 0;
		}
		((sf_header*)startBlock) -> splinter_size = blockSize - (size+paddingAmt) - 16;
		((sf_footer*) endBlock) -> alloc = 1;
		((sf_footer*) endBlock) -> block_size = blockSize/16;
		return (void*)(startBlock + 8);
	}
	else
	{
		// Must split the block into 2 blocks.
		((sf_header*)startBlock) -> block_size = (size + paddingAmt+16)/16;
		((sf_header*)startBlock) -> splinter = 0;
		((sf_header*)startBlock) -> splinter_size = 0;
		char* footpointer = startBlock + size + paddingAmt + 8;
		((sf_footer*)footpointer) -> alloc = 1;
		((sf_footer*)footpointer) -> splinter = 0;
		((sf_footer*)footpointer) -> block_size = ((sf_header*)startBlock) -> block_size;
		char* newStartBlock = footpointer + 8;
		((sf_header*)newStartBlock) -> alloc = 0;
		((sf_header*)newStartBlock) -> splinter = 0;
		((sf_header*)newStartBlock) -> splinter_size = 0;
		((sf_header*)newStartBlock) -> padding_size = 0;
		((sf_header*)newStartBlock) -> requested_size = 0;
		((sf_header*)newStartBlock) -> block_size = (blockSize - (size+paddingAmt + 16))/16;
		char* newEnd = newStartBlock + (((sf_header*)newStartBlock) -> block_size) * 16 - 8;
		((sf_footer*)newEnd) -> alloc = 0;
		((sf_footer*)newEnd) -> splinter = 0;
		((sf_footer*)newEnd) -> block_size = ((sf_header*)newStartBlock) -> block_size;
		void* freeBlock = (void*)(newStartBlock);
		insertFreeBlock(freeBlock);
		return (void*)(startBlock + 8);
	}

}



//Adds a header and footer tag to a a block that is added to the heap.
void head_foot(void* freeBlock, int size)
{
	//printf("Total Size: %d\n",size/16);
	//Clean block
	sf_free_header* freeHeader = (sf_free_header*) freeBlock;
	(freeHeader -> header).alloc = 0;
	(freeHeader -> header).padding_size = 0;
	(freeHeader -> header).splinter_size = 0;
	(freeHeader -> header).splinter = 0;
	//Right shift size of freeBlock by 4
	(freeHeader -> header).block_size = (size_t)(size/16);
	//printf("We can cast a sf_free_pointer to a regular sf_header pointer like so: %d", ((sf_header*)freeBlock) -> block_size);
	char* footpointer =  (char*)freeBlock + size - 8;
	((sf_footer*)footpointer) -> alloc = 0;
	((sf_footer*)footpointer) -> splinter = 0;
	((sf_footer*)footpointer) -> block_size = (size_t)(size/16);
	insertFreeBlock(freeHeader);
}

//Lock for a block first
void *sf_malloc(size_t size) {
	if (size == 0)
	{
		errno = EINVAL;
		return NULL;
	}
	//Search for an appropropriate sized block
	else
	{
		sf_free_header* copy = freelist_head;
		sf_free_header* bestFit = NULL;
		while (copy != NULL)
		{
			// Check for appropriate block size. Remember that true block size in bytes means multiplying by 16 the block size field.
			if ((copy->header).block_size * 16 >= (size+16))
			{
				if (bestFit == NULL)
				{
					bestFit = copy;
				}
				else
				{
					if ((bestFit->header).block_size * 16 - (size + 16) > (copy ->header).block_size * 16 - (size+16))
					{
						bestFit = copy;
					}
				}
			}
			copy = copy ->next;
		}
		if (bestFit != NULL)
		{
			removeFromList(bestFit);
			return allocate(bestFit, size);
		}
		// No block found, must create a new block using sbrk.
		int sizeAmt = (int)size +16;
		sizeAmt = sizeAmt / 4096;
		sizeAmt +=1;
		if (size % 4080 == 0)
		{
			sizeAmt -= 1;
		}
		//SizeAmt means the number of pgs  of memory in order to cover size requirement.
		sizeAmt *= 4096;
		//printf("ORIGINAL POSITION OF SBRK: %p\n", sf_sbrk(0));
		void* newBlock = sf_sbrk(sizeAmt);

		if (*(int*)newBlock == -1)
		{
			errno = ENOMEM;
			return NULL;

		}
		sbrk_move += sizeAmt;
		head_foot(newBlock, sizeAmt);
		return sf_malloc(size);
	}
}

void *sf_realloc(void *ptr, size_t size) {
	//Check for valid ptr
	char* startAddress = ((char*)ptr) - 8;
	char* endAddress = startAddress + ((sf_header*)(startAddress)) -> block_size * 16 - 8;
	sf_header* header = ((sf_header*)startAddress);
	sf_footer* footer = ((sf_footer*)endAddress);
	int oldPadding = header -> padding_size;
	int oldSplintersize = header -> splinter_size;
	int oldSplinter = header -> splinter;
	totalMem -= header -> requested_size;
	totalMem += size;

	if (header -> block_size != footer -> block_size ||
		header -> alloc != footer -> alloc ||
		header ->splinter != footer -> splinter
		)
	{
		errno = EINVAL;
		return NULL;
	}
	else if (header -> alloc != 1)
	{
		errno = EINVAL;
		return NULL;
	}

	if (size == 0)
	{
		sf_free(ptr);
		return NULL;
	}
	// Size is not zero
	int new_size = size / 16;
	if (new_size % 16 == 0)
		new_size -= 1;
	int old_size = header -> requested_size / 16;
	if (old_size % 16 == 0)
		old_size -= 1;
	int newPaddingAmt =  16 - size % 16;
	if (new_size == old_size)
	{
		// The current block is already appropriately sized.
		int splinterIncrement = header -> padding_size - newPaddingAmt;
		header -> padding_size = newPaddingAmt;
		header -> splinter_size += splinterIncrement;
		if (header -> splinter > 0)
		{
			header -> splinter = 1;
			footer -> splinter = 1;
		}
		else
		{
			header -> splinter = 0;
			footer -> splinter = 0;
		}
		padding += newPaddingAmt- oldPadding;
		splintering += header -> splinter_size - oldSplintersize;
		splinterBlocks += header -> splinter - oldSplinter;
		return ptr;
	}
	else if (new_size < old_size)
	{
		// Allocate to a block of smaller length.
		int block_size = header -> block_size * 16;
		if (block_size - (size + newPaddingAmt + 16) < 32)
		{
			// Check for adjacent free blocks
			char* nextBlock = endAddress + 8;
			sf_header * nextHeader = (sf_header*) nextBlock;
			int remaining_size = block_size - (size + newPaddingAmt + 16);
			//Check if can coalesce with the block after this one
			if (isValidBlock((void*)nextBlock) && nextHeader -> alloc == 0)
			{
				//Coalesce with the next block, first remove the block from list
				removeFromList((sf_free_header*)nextBlock);
				int sizeToAdd = nextHeader -> block_size * 16;
				header -> block_size = (size +newPaddingAmt + 16)/16;
				header -> alloc = 1;
				header -> splinter = 0;
				header -> splinter_size = 0;
				header -> requested_size = size;
				header -> padding_size = newPaddingAmt;
				char* alloc_footer = startAddress + size +newPaddingAmt + 8;
				((sf_footer*)alloc_footer) -> block_size = header -> block_size;
				((sf_footer*)alloc_footer) -> alloc = 1;
				((sf_footer*)alloc_footer) -> splinter = 0;
				char* startofblock = alloc_footer + 8;
				((sf_free_header*)startofblock) -> next = NULL;
				((sf_free_header*)startofblock) -> prev = NULL;
				((sf_free_header*)startofblock) -> header.block_size = (remaining_size + sizeToAdd)/16;
				((sf_free_header*)startofblock) -> header.alloc = 0;
				((sf_free_header*)startofblock) -> header.splinter_size = 0;
				((sf_free_header*)startofblock) -> header.splinter = 0;
				((sf_free_header*)startofblock) -> header.padding_size = 0;
				((sf_free_header*)startofblock) -> header.requested_size = 0;
				char* endofblock = startofblock + ((sf_free_header*)startofblock) -> header.block_size * 16;
				((sf_footer*)endofblock) -> block_size = ((sf_free_header*)startofblock) -> header.block_size;
				((sf_footer*)endofblock) -> alloc = 0;
				((sf_footer*)endofblock) -> splinter = 0;
				insertFreeBlock((sf_free_header*)startofblock);
				padding += newPaddingAmt- oldPadding;
				splintering += header -> splinter_size - oldSplintersize;
				splinterBlocks += header -> splinter - oldSplinter;
				return ptr;
			}
			else
			{
				//Otherwise create a splinter in this free block.
				header -> requested_size = size;
				header -> padding_size = newPaddingAmt;
				header -> splinter_size = header -> block_size * 16 - header -> padding_size - header ->requested_size - 16;
				if (header -> splinter_size > 0)
				{
					header -> splinter = 1;
					footer -> splinter = 1;
				}
				else
				{
					header -> splinter = 0;
					footer -> splinter = 0;
				}
				padding += newPaddingAmt- oldPadding;
				splintering += header -> splinter_size - oldSplintersize;
				splinterBlocks += header -> splinter - oldSplinter;
				return ptr;
			}
		}
		else
		{
			//Perform the split, add the other block to the free list.
			header -> block_size = (size + newPaddingAmt + 16)/16;
			header -> splinter = 0;
			header -> requested_size = size;
			header -> splinter_size = 0;
			header -> padding_size = newPaddingAmt;
			block_size -= size + newPaddingAmt + 16;
			char* endAddress = startAddress + header -> block_size * 16 - 8;
			((sf_footer*)endAddress) -> alloc = 1;
			((sf_footer*)endAddress) -> splinter = 0;
			((sf_footer*)endAddress) -> block_size = header -> block_size;
			char* newStartBlock = endAddress + 8;
			((sf_free_header*)newStartBlock) -> header.block_size = block_size / 16;
			((sf_free_header*)newStartBlock) -> header.alloc = 0;
			((sf_free_header*)newStartBlock) -> header.padding_size = 0;
			((sf_free_header*)newStartBlock) -> header.splinter = 0;
			((sf_free_header*)newStartBlock) -> header.splinter_size = 0;
			((sf_free_header*)newStartBlock) -> next = NULL;
			((sf_free_header*)newStartBlock) -> prev = NULL;
			char* newEnd = newStartBlock + block_size - 8;
			((sf_footer*)newEnd) -> alloc = 0;
			((sf_footer*)newEnd) -> splinter = 0;
			((sf_footer*)newEnd) -> block_size =((sf_free_header*)newStartBlock) -> header.block_size;
			insertFreeBlock(newStartBlock);
			padding += newPaddingAmt- oldPadding;
			splintering += header -> splinter_size - oldSplintersize;
			splinterBlocks += header -> splinter - oldSplinter;
			return ptr;
		}

	}
	else
	{
		//Requests a larger block of memory. Check first if block_size can fit.
		if (header -> block_size * 16 >= size + 16 + newPaddingAmt)
		{
			header -> requested_size = size;
			header -> padding_size = newPaddingAmt;
			header -> splinter_size = header -> block_size * 16 - newPaddingAmt - size - 16;
			if (header ->splinter_size >0)
			{
				header -> splinter = 1;
				footer -> splinter = 1;
			}
			else
			{
				header -> splinter = 0;
				footer -> splinter = 0;
			}
			padding += newPaddingAmt- oldPadding;
			splintering += header -> splinter_size - oldSplintersize;
			splinterBlocks += header -> splinter - oldSplinter;
			return ptr;
		}
		//If not check for coalescing with the next block.
		char* nextBlock = startAddress +header -> block_size;
		sf_header* nHeader = (sf_header*) nextBlock;
		if (nHeader -> alloc == 0)
		{
			int total_block_size = 16 * (header -> block_size + nHeader ->block_size);
			if (total_block_size >= size + newPaddingAmt + 16)
			{
				//Coalesce then return ptr. First remove the block.
				removeFromList((sf_free_header*)nHeader);
				if (total_block_size - (size + newPaddingAmt + 16) >= 32)
				{
					//coalesce then split
					header -> block_size = (size + newPaddingAmt + 16)/16;
					header -> requested_size = size;
					header -> padding_size = newPaddingAmt;
					header -> splinter = 0;
					header -> splinter_size = 0;
					char* newEnd = startAddress + header -> block_size * 16 -8;
					((sf_footer*)newEnd) -> alloc = 1;
					((sf_footer*)newEnd) -> block_size = header -> block_size;
					((sf_header*)newEnd) -> splinter = 0;
					char*newBlock = newEnd + 8;
					((sf_free_header*)newBlock) -> next = NULL;
					((sf_free_header*)newBlock) -> prev = NULL;
					((sf_free_header*)newBlock) -> header.block_size = (total_block_size - (size + newPaddingAmt + 16))/16;
					((sf_free_header*)newBlock) -> header.alloc= 0;
					((sf_free_header*)newBlock) -> header.splinter= 0;
					((sf_free_header*)newBlock) -> header.padding_size= 0;
					((sf_free_header*)newBlock) -> header.splinter_size= 0;
					char* endNewBlock = newBlock + total_block_size - (size + newPaddingAmt + 16) - 8;
					((sf_footer*) endNewBlock) -> block_size = ((sf_header*)newBlock) -> block_size;
					((sf_footer*) endNewBlock) -> alloc = ((sf_header*)newBlock) -> alloc;
					((sf_footer*) endNewBlock) -> splinter = ((sf_header*)newBlock) -> splinter;
					insertFreeBlock(((sf_free_header*)newBlock));
					padding += newPaddingAmt- oldPadding;
					splintering += header -> splinter_size - oldSplintersize;
					splinterBlocks += header -> splinter - oldSplinter;
					return ptr;
				}
				else
				{
					//make a splinter
					header -> block_size = total_block_size / 16;
					header -> requested_size = size;
					header -> padding_size = newPaddingAmt;
					header -> splinter_size = total_block_size - (size + newPaddingAmt + 16) ;
					char* endBlock =  startAddress + total_block_size - 8;
					((sf_footer*)endBlock) -> alloc = 1;
					((sf_footer*)endBlock) -> block_size = total_block_size/16;
					if (header -> splinter_size != 0)
					{
						header -> splinter = 1;
						((sf_footer*)endBlock) -> splinter = 1;
					}
					else
					{
						header -> splinter = 0;
						((sf_footer*)endBlock) -> splinter = 0;
					}
					padding += newPaddingAmt- oldPadding;
					splintering += header -> splinter_size - oldSplintersize;
					splinterBlocks += header -> splinter - oldSplinter;

					return ptr;
				}


			}
		}
		// Must malloc a new block in order to move the contents
		void * newMemory = sf_malloc(size);
		if (newMemory == NULL)
		{
			sf_free(ptr);
			return NULL;
		}
		else
		{
			memcpy(newMemory,ptr,header -> requested_size);
			sf_free(ptr);
			return newMemory;
		}

	}

}

void sf_free(void* ptr) {
	char* startAddress = ((char*)ptr) - 8;
	char* endAddress = (((sf_header*)startAddress) ->block_size * 16) + startAddress - 8;
	if ( ((sf_header*)startAddress) ->block_size != ((sf_footer*)endAddress) ->block_size ||
		((sf_header*)startAddress) ->alloc != ((sf_footer*)endAddress) ->alloc ||
		((sf_header*)startAddress) ->splinter != ((sf_footer*)endAddress) ->splinter
		|| ((sf_header*)startAddress) ->alloc == 0
		)
	{
		//printf("Invalid ptr passed");
		errno = EINVAL;
		return;
	}
	else
	{
		allocatedBlocks--;
		splinterBlocks-= ((sf_header*)startAddress) -> splinter;
		splintering -= ((sf_header*)startAddress) -> splinter_size;
		totalMem -= ((sf_header*)startAddress) -> requested_size;
		padding -= ((sf_header*)startAddress) -> padding_size;
		//printf("Valid ptr passed");
		sf_header* header = (sf_header*)startAddress;
		sf_footer* footer = (sf_footer*)endAddress;
		header -> alloc = 0;
		footer -> alloc = 0;
		sf_free_header* freeBlock = coalesce(header);
		//sf_blockprint(freeBlock);
		insertFreeBlock(freeBlock);
	}
}

int sf_info(info* ptr) {
	if (ptr == NULL || sbrk_move == 0)
	{
		return -1;
	}
	ptr -> allocatedBlocks = allocatedBlocks;
	ptr -> splinterBlocks = splinterBlocks;
	ptr -> padding = padding;
	ptr -> splintering = splintering;
	ptr -> coalesces = coalesces;
	ptr -> peakMemoryUtilization = totalMem/sbrk_move;
	return 0;
}



void test_method()
{
	printf("Available heap memory is %d\n", (4*4096 - sbrk_move)/4096);

}
