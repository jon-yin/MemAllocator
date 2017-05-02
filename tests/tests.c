#include <criterion/criterion.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sfmm.h"

/**
 *  HERE ARE OUR TEST CASES NOT ALL SHOULD BE GIVEN STUDENTS
 *  REMINDER MAX ALLOCATIONS MAY NOT EXCEED 4 * 4096 or 16384 or 128KB
 */

Test(sf_memsuite, Malloc_an_Integer, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(sizeof(int));
  *x = 4;
  cr_assert(*x == 4, "Failed to properly sf_malloc space for an integer!");
}

Test(sf_memsuite, Free_block_check_header_footer_values, .init = sf_mem_init, .fini = sf_mem_fini) {
  void *pointer = sf_malloc(sizeof(short));
  sf_free(pointer);
  pointer = (char*)pointer - 8;
  sf_header *sfHeader = (sf_header *) pointer;
  cr_assert(sfHeader->alloc == 0, "Alloc bit in header is not 0!\n");
  sf_footer *sfFooter = (sf_footer *) ((char*)pointer + (sfHeader->block_size << 4) - 8);
  cr_assert(sfFooter->alloc == 0, "Alloc bit in the footer is not 0!\n");
}

Test(sf_memsuite, SplinterSize_Check_char, .init = sf_mem_init, .fini = sf_mem_fini){
  void* x = sf_malloc(32);
  void* y = sf_malloc(32);
  (void)y;

  sf_free(x);

  x = sf_malloc(16);

  sf_header *sfHeader = (sf_header *)((char*)x - 8);
  cr_assert(sfHeader->splinter == 1, "Splinter bit in header is not 1!");
  cr_assert(sfHeader->splinter_size == 16, "Splinter size is not 16");

  sf_footer *sfFooter = (sf_footer *)((char*)sfHeader + (sfHeader->block_size << 4) - 8);
  cr_assert(sfFooter->splinter == 1, "Splinter bit in header is not 1!");
}

Test(sf_memsuite, Check_next_prev_pointers_of_free_block_at_head_of_list, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  memset(x, 0, 0);
  cr_assert(freelist_head->next == NULL);
  cr_assert(freelist_head->prev == NULL);
}

Test(sf_memsuite, Coalesce_no_coalescing, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  int *y = sf_malloc(4);
  memset(y, 0, 0);
  sf_free(x);

  //just simply checking there are more than two things in list
  //and that they point to each other
  cr_assert(freelist_head->next != NULL);
  cr_assert(freelist_head->next->prev != NULL);
}

//#
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//#

Test(sf_memsuite, Test_malloc_and_free_repeatedly, .init = sf_mem_init, .fini = sf_mem_fini){
  void* ptr = sf_malloc(1000);
  void* anotherAlloc = sf_malloc(1300);
   void* anotherAlloc2 = sf_malloc(2000);
   //Assert that the requested sizes are consistent.
   char* start1 = ((char*)ptr) - 8;
   char* start2 = ((char*)anotherAlloc) - 8;
   char* start3 = ((char*)anotherAlloc2) - 8;
   cr_assert(((sf_header*)start1) -> requested_size == 1000, "Requested size is not consistent");
   cr_assert(((sf_header*)start2) -> requested_size == 1300, "Requested size is not consistent");
   cr_assert(((sf_header*)start3) -> requested_size == 2000, "Requested size is not consistent");
   //sf_varprint(ptr);
   //sf_varprint(anotherAlloc);
   //sf_varprint(anotherAlloc2);
   //sf_blockprint(freelist_head);
   sf_free(ptr);
   sf_free(anotherAlloc);
   sf_free(anotherAlloc2);
   //sf_snapshot(true);
   cr_assert(freelist_head -> next == NULL, "Blocks didn't coalesce");
}

Test(sf_memsuite, Test_realloc, .init = sf_mem_init, .fini = sf_mem_fini){
  void* ptr = sf_malloc(1000);
  void* newptr = sf_realloc(ptr,990);
  //sf_varprint(newptr);
  char* start = ((char*)newptr - 8);
  cr_assert(((sf_header*)start) -> requested_size == 990, "Requested size didn't change");
  void* newMem = sf_malloc(100);
  newMem = sf_realloc(newMem,3000);
  //sf_varprint(newMem);
  start = ((char*)newMem - 8);
  cr_assert(((sf_header*)start) -> requested_size == 3000, "Requested size didn't change");
  cr_assert(((sf_header*)start) -> padding_size == 8, "Padding from realloc should be 8");
  void* largeBlock =  sf_malloc(4000);
  largeBlock = sf_realloc(largeBlock,200);
  //sf_varprint(largeBlock);
  start = ((char*)largeBlock - 8);
  cr_assert(((sf_header*)start) -> requested_size == 200, "Requested size didn't change");
  cr_assert(((sf_header*)start) -> padding_size == 8, "Requested size didn't change");
   *((int*)(newptr)) = 600;
  sf_realloc(newptr, 10);
  cr_assert(*((int*)newptr) == 600, "Value not saved in realloc.");
}

Test(sf_memsuite, Test_info, .init = sf_mem_init, .fini = sf_mem_fini)
{
  info* info = sf_malloc(sizeof(info));
  cr_assert(sf_info(info) == 0,"Failed to assign info");
  void* ptr = sf_malloc(2048);
  void* ptr2 = sf_malloc(30);
  void * ptr3 = sf_malloc(548);
  void* ptr4 = sf_malloc(4200);
  sf_realloc(ptr4, 200);
  sf_info(info);
  cr_assert(info -> allocatedBlocks == 5, "There should be 5 blocks");
  //sf_varprint(ptr);
  //sf_varprint(ptr2);
  //sf_varprint(ptr3);
  //sf_varprint(ptr4);
  sf_free(ptr);
  sf_free(ptr2);
  sf_free(ptr3);
  sf_free(ptr4);
  sf_info(info);
  //printf("%d\n", (int)info -> allocatedBlocks);
  cr_assert(info -> allocatedBlocks == 1, "There should be 1 block");
  cr_assert(info -> peakMemoryUtilization < 0.001, "Should now be small amounts of memory used");
  //printf("%d\n" ,(int)info -> splinterBlocks);
  cr_assert(info -> splinterBlocks == 0, "No splinters");
  cr_assert(info -> splintering == 0, "No splinters exist");
  cr_assert(info -> coalesces > 2, "Coalesced more than twice");


}
