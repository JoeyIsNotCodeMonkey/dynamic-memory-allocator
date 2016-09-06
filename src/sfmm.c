/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "sfmm.h"

/**
 * You should store the head of your free list in this variable.
 * Doing so will make it accessible via the extern statement in sfmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */
sf_free_header* freelist_head = NULL;
sf_free_header* heap_start = NULL;
sf_free_header* heap_end = NULL;

#define PAGE_SIZE 4096

#define WSIZE       8       /* Word and header/footer size (bytes) */ 
#define DSIZE       16       /* Double word size (bytes) */

static void sf_init();
static void add_to_freelist(sf_free_header *ptr);
static void remove_from_freelist(sf_free_header *ptr);

static sf_free_header *find_fit(size_t size);
static sf_free_header *place(sf_free_header *ptr, size_t size);
static sf_free_header *coalesce(sf_free_header *ptr); 
static sf_free_header *extend_heap(size_t size);
static size_t align(size_t size);


void* sf_malloc(size_t size) {
    // size_t extendsize; /* Amount to extend heap if no fit */
	sf_free_header *ptr;
	sf_free_header *cursor;

	//if first call of sf_malloc
	if(freelist_head == NULL){
		sf_init();		
	} 

	/* Ignore spurious requests */
    if (size == 0){
    	return NULL;
    }

	if((ptr = find_fit(align(size))) != NULL){
		place(ptr, size);
		
		return (void *)((char *)ptr + 8);	
	} else {
		int count, i;
		if(align(size) % PAGE_SIZE != 0)
			count = align(size)/PAGE_SIZE + 1;
		else
			count= align(size)/PAGE_SIZE;
		
		ptr = extend_heap(count * PAGE_SIZE);
		
	}
		
	place(ptr, size);
	
	return (void *)((char *)ptr + 8);		
}

void sf_free(void *ptr) {
	sf_free_header *new_free_block;
	sf_header *header;
	sf_footer *footer;

	ptr = (char *)ptr - 8; //move ptr to header

	header = (sf_header *)ptr;
	header->alloc = 0;
	header->requested_size = 0;

	footer = (sf_footer *)((char *)ptr + (header->block_size<<4) - 8);
	footer->alloc = header->alloc;

	//coalesce check
	new_free_block = coalesce((sf_free_header *)ptr);

	//add the new block to free list
	add_to_freelist(new_free_block);
}

static sf_free_header *extend_heap(size_t size){
	sf_free_header *ptr = (sf_free_header *) ((char *)sf_sbrk(size));
	
	ptr->header.alloc = 0;
	ptr->header.block_size = size >> 4;
	ptr->header.requested_size = 0;

	//update heap_end
	heap_end = (sf_free_header *)((char *)ptr + size);
	
	ptr = coalesce(ptr);
	
	add_to_freelist(ptr);

	return ptr;
}

static sf_free_header *coalesce(sf_free_header *ptr){
	bool prev_alloc = false;
	bool next_alloc = false;
	sf_header *header = NULL;
	sf_footer *footer = NULL;
	size_t block_size = ptr->header.block_size << 4;

	if((sf_free_header *)((char *)ptr + block_size) != heap_end){ //check whether it is the last block in heap
		header = (sf_header *)((char *)ptr + block_size); //next header
	}

	if(ptr != heap_start){ //check whether it is the head of heap
		footer = (sf_footer *)((char *)ptr - 8); //prev footer
	}
	

	if(footer == NULL){
		prev_alloc = true;
	} else if (footer->alloc == 1){
		prev_alloc = true;
	}
	
	if(header == NULL){
		next_alloc = true;
	} else if(header->alloc ==1){
		next_alloc = true;
	}


	if (prev_alloc && next_alloc) {            /* Case 1 */
		return ptr;
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2 */  //this header is actually next block header
    	block_size = block_size + (header->block_size << 4);
    	ptr->header.block_size = block_size >> 4; //update header
    	footer = (sf_footer *)((char *)ptr + block_size - 8);
    	footer->block_size = ptr->header.block_size;
    	remove_from_freelist(ptr);
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
		block_size = block_size + (footer->block_size << 4); //total block size
		header = (sf_header *)((char *)ptr - (footer->block_size << 4));
		header->block_size = block_size >> 4;
		footer = (sf_footer *)((char *)ptr + (ptr->header.block_size << 4) - 8);
		footer->block_size = header->block_size;
		ptr = (sf_free_header *)header;
		remove_from_freelist(ptr);
    }

    else {                                     /* Case 4 */
		size_t prev_block_size, next_block_size;
		prev_block_size = footer->block_size << 4;
		next_block_size = header->block_size << 4;
		block_size = block_size + prev_block_size + next_block_size;

		header = (sf_header *)((char *)ptr - prev_block_size);
		header->block_size = block_size >> 4;
		footer = (sf_footer *)((char *)ptr + (block_size - prev_block_size) - 8);
		footer->block_size = header->block_size;
		ptr = (sf_free_header *)header;
		remove_from_freelist(ptr);
		remove_from_freelist((sf_free_header *)((char *)ptr + (block_size - next_block_size)));
    }

    return ptr;

}

static void add_to_freelist(sf_free_header *ptr){
	if(freelist_head == NULL){
		freelist_head = ptr;
		freelist_head->next = NULL;
		freelist_head->prev = NULL;
	} else {
		ptr->next = freelist_head;
		freelist_head->prev = ptr;
		ptr->prev = NULL;
		freelist_head = ptr;
	}
}

static void remove_from_freelist(sf_free_header *ptr){
	if(ptr->prev == NULL && ptr->next == NULL){
		freelist_head = NULL;
	} else if(ptr->prev == NULL){
		ptr->next->prev = NULL;
		freelist_head = ptr->next;
	} else if(ptr->next == NULL){
		ptr->prev->next = NULL;
	} else if (ptr->prev != NULL && ptr->next != NULL){
		ptr->prev->next = ptr->next;
		ptr->next->prev = ptr->prev;
	}
	
}


void* sf_realloc(void *ptr, size_t size) {
	size_t oldsize;
    void *newptr;
    ptr = (char *)ptr - 8;//move the ptr to header

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
		sf_free(ptr);
		return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
		return sf_malloc(size);
    }

    sf_header* header = (sf_header *)ptr;
    oldsize = (header->block_size) << 4;
    if(align(size) < oldsize){
    	place(ptr, size);
    	return (void *)((char *)ptr + 8);
    } 
    
    // else newsize > oldsize
    newptr = sf_malloc(size);

    //newptr = (char *)newptr - 8;//move the newptr to header

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
		return 0;
    }

    /* Copy the old payload. *//////////////  
    memcpy(newptr, (char *)ptr + 8, oldsize-16);


    /* Free the old block. */
    ptr = (char *)ptr + 8;//move the ptr to payload
    sf_free(ptr);
    return (void *)newptr;//move newptr back to payload
}

void* sf_calloc(size_t nmemb, size_t size) {
	size_t total = nmemb * size;
	void* ptr = malloc(total);
	//set payload to 0
	memset(ptr, 0, total);
    return ptr;
}

void sf_init(){
	freelist_head = (sf_free_header *)sf_sbrk(0);
	sf_free_header* record  = freelist_head;
	if((long)freelist_head % 16 != 8){
		while((long)freelist_head % 16 != 8){
			(char *)freelist_head++;	
		}
	}
	size_t padding = (char *)freelist_head - (char *)record;
	sf_sbrk(padding);
	freelist_head = (sf_free_header *)sf_sbrk(PAGE_SIZE);

	heap_start = freelist_head; //store the heap starting address
	heap_end = (sf_free_header *)((char *)freelist_head + PAGE_SIZE); //heap end address

	freelist_head->header.alloc = 0;
	freelist_head->header.block_size = PAGE_SIZE >> 4;
	freelist_head->header.requested_size = 0;
	freelist_head->next = NULL; //i am going to make a doubly linked list
	freelist_head->prev = NULL;

	//set footer
	sf_footer *footer = (sf_footer *)((char *)freelist_head + PAGE_SIZE - 8);
	footer->alloc = 0;
	footer->block_size = PAGE_SIZE >> 4;

}



static sf_free_header *find_fit(size_t size){
	sf_free_header *ptr = freelist_head;
	size_t block_size = ptr->header.block_size << 4;
	while(ptr != NULL){
		if(block_size > size){
			return ptr;
		}
		ptr = ptr->next;
	}
	return NULL;
}

static sf_free_header *place(sf_free_header *ptr, size_t size){
	size_t old_size = ptr->header.block_size << 4;
	sf_footer *footer;
	sf_header *header;

	if(old_size-align(size) > 24){ //sizeof(header)(8) + least payload(16)
		//set allocated block header
		ptr->header.requested_size = size;
		ptr->header.alloc = 1;
		ptr->header.block_size = align(size) >> 4;

		//set allocated block footer
		footer = (sf_footer *)((char *)ptr + align(size) - 8);
		footer->alloc = ptr->header.alloc;
		footer->block_size = ptr->header.block_size;

		//set header for the free block
		header = (sf_header *)((char *)footer + 8); 
		header->requested_size = 0;
		header->block_size = (old_size - align(size)) >> 4;
		header->alloc = 0;

		//update the old footer
		footer = (sf_footer *)((char *)ptr + old_size - 8);
		footer->alloc = header->alloc;
		footer->block_size = header->block_size;

		//update the free list
		sf_free_header *new_free_block = (sf_free_header *)(header);
		if(ptr->prev != NULL){
			ptr->prev->next = new_free_block;
			new_free_block->prev = ptr->prev;
		}

		if(ptr->next != NULL){
			new_free_block->next = ptr->next;
			ptr->next->prev = new_free_block;
		}

		if(ptr->prev == NULL){
			freelist_head = new_free_block;
		}


	} else {
		//set allocated block header
		ptr->header.requested_size = size;
		ptr->header.alloc = 1;
		//block_size remain same

		//update the old footer
		footer = (sf_footer *)((char *)ptr + old_size - 8);
		footer->alloc = header->alloc;
		footer->block_size = header->block_size;
	}
}

static size_t align(size_t size){
	while(size % 16 != 0){
		size++;
	}

	return size+16;
}

