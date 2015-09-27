/* 
 * Simple, 32-bit and 64-bit clean allocator based on an implicit free list,
 * first fit placement, and boundary tag coalescing, as described in the
 * CS:APP2e text.  Blocks are aligned to double-word boundaries.  This
 * yields 8-byte aligned blocks on a 32-bit processor, and 16-byte aligned
 * blocks on a 64-bit processor.  However, 16-byte alignment is stricter
 * than necessary; the assignment only requires 8-byte alignment.  The
 * minimum block size is four words.
 *
 * This allocator uses the size of a pointer, e.g., sizeof(void *), to
 * define the size of a word.  This allocator also uses the standard
 * type uintptr_t to define unsigned integers that are the same size
 * as a pointer, i.e., sizeof(uintptr_t) == sizeof(void *).
 */

/* Submitted by: Ishita Chourasia   	 */ 

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include<unistd.h>

#include "memlib.h"
#include "mm.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
	/* Team name */
	"ishitach",
  };


/* Basic constants and macros: */
#define WSIZE      sizeof(void *) /* Word and header/footer size (bytes) */
#define DSIZE      (2 * WSIZE)    /* Doubleword size (bytes) */
#define CHUNKSIZE  (1 << 12)      /* Extend heap by this amount (bytes) */

#define MAX(x, y)  ((x) > (y) ? (x) : (y))  

/* Pack a size and allocated bit into a word. */
#define PACK(size, alloc)  ((size) | (alloc))    

/* Read and write a word at address p. */
#define GET(p)       (*(uintptr_t *)(p))
#define PUT(p, val)  (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p. */
#define GET_SIZE(p)   (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)  (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer. */
#define HDRP(bp)  ((char *)(bp) - WSIZE)
#define FTRP(bp)  ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks. */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


#define NEXT_FREE(bp) *(int *)(bp)
#define PreviousFreeBlock(ptr) (*(void **) (ptr))	// This returns the previous free block in the explicitly maitained free list
#define NextFreeBlock(ptr) (*(void **) (ptr + WSIZE))	// This returns the next free block in the explicitly maitained free list
#define SetPreviousFree(bp, previous) (*((void **)(bp)) = previous)
#define SetNextFree(bp, next) (*((void **)(bp + WSIZE)) = next)

/* Global variables: */
static char *heap_listp; /* Pointer to first block */  	
					
static char *htp;			//This is the pointer pointing to the heap
static char *head;  			//This is the pointer used to maintain the explicit free list

/* Function prototypes for internal helper routines: */
static void *coalesce(void *bp);		//Coalesces a newly created free block with its adjacent blocks after checking the 							//necessary conditions
static void *extend_heap(size_t words);		// This routine extends the heap to a predefined size known as chunk size.
static void *find_fit(size_t asize);		// This is the key routine which finds the necessary free block of appropriate size for 						//allocation 
static void place(void *bp, size_t asize);

/* Function prototypes for heap consistency checker routines: */
static void checkblock(void *bp);
static void checkheap(bool verbose);
static void printblock(void *bp); 

//Routines added for adding and deleting blocks
void Add_Fb(void *bp,size_t size_of_block); 	//For adding newly created free block to the explictly maintained free list
void Delete_Fb(void *bp,size_t size_of_block); //For deleting newly allocated block from the explicitly maintained free list

/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Initialize the memory manager.  Returns 0 if the memory manager was
 *   successfully initialized and -1 otherwise.
 */
int
mm_init(void) 
{
	htp=mem_sbrk(55*DSIZE); 			// Lists being created
	/* Create the initial empty heap. */
	if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
		return (-1);
	PUT(heap_listp, 0);                            /* Alignment padding */
	PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
	PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
	PUT(heap_listp+WSIZE+DSIZE, PACK(0, 1));      /* Epilogue header */
	heap_listp += DSIZE;

	
	int lk=0;
	head=htp;
	while(lk<55){
		SetNextFree(htp+lk*DSIZE,0); 		// Initialize head to NULL ( yet no free block )
		++lk;
	}

	if (extend_heap(CHUNKSIZE/WSIZE) == (void *)-1)/* Extend the empty heap with a free block of CHUNKSIZE bytes */
	return (-1);

	return (0);
}

/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Allocate a block with at least "size" bytes of payload, unless "size" is
 *   zero.  Returns the address of this block if the allocation was successful
 *   and NULL otherwise.
 */
void *
mm_malloc(size_t size) 
{
	size_t asize;      /* Adjusted block size */
	size_t extendsize; /* Amount to extend heap if no fit */
	void *bp;		// amount to extend heap if no fit 

	/* Ignore spurious requests. */
	if (size == 0)		//No allocation done due to empty space
		return (NULL);

	/* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);

	/* Search the free list for a fit. */
	if ((bp = find_fit(asize)) != NULL) {
		place(bp, asize);	//places th block in the list
		return (bp);
	}

	/* No fit found.  Get more memory and place the block. */
	extendsize = MAX(asize, CHUNKSIZE);			// calculates the max of the total required size and the previously 									//provided chunk size
	if ((bp = extend_heap(extendsize / WSIZE)) == NULL)  //if the size requirments is not met, extends the size of the heap 
		return (NULL);
	place(bp, asize);		//placing of block into heap 
	return (bp);
} 

/* 
 * Requires:
 *   "bp" is either the address of an allocated block or NULL.
 *
 * Effects:
 *   Free a block.
 */
void
mm_free(void *bp)
{
	size_t size;

	/* Ignore spurious requests. */
	if (bp == NULL)
		return;

	/* Free and coalesce the block. */
	size = GET_SIZE(HDRP(bp));
	PUT(HDRP(bp), PACK(size, 0));	// Packs the size of the block and the allocation status of the block in the header
	PUT(FTRP(bp), PACK(size, 0));	// Packs the size of the block and the allocation status of the block in the footer
	coalesce(bp);			// coalesces the newly block in the explicictly maintained list
}

/* Add_Fb : This will add a free block to the explicitly maintained free list . If a free block is to be allocated, traverse the free list. It follows the lifo property similar to stack */ 
 
void Add_Fb(void *bp,size_t size_of_block) {  		//Adding the newly created free blocks to the list

	//Doing by stack property
    void *ptr = bp;

	int num=size_of_block/6000;//Hash index calculation
	if(num>=50)
	   num=49;
	char *p=htp;

	head=p+DSIZE*num; //Direct's the function to particular list

	if(NextFreeBlock(head)==0){
		SetNextFree(head,ptr);// Pointer rearrangement 
		SetPreviousFree(ptr,head);// Pointer rearrangements
		SetNextFree(ptr,0);

}
	else{
		SetPreviousFree(NextFreeBlock(head),ptr);
		// Freeing the previous pointer of the next block to the present block and assinging it to ptr
		SetNextFree(ptr,NextFreeBlock(head));//setting the next pointer of ptr to the previously next block
		SetNextFree(head,ptr);// Pointer rearrangements
		SetPreviousFree(ptr,head);// Pointer rearrangements

}
}
/*Delete_Fb: This will help in updating the list when a free block is alllocated, or any block which is already free
 is extended while coalescing. The link of the free list is removed from the explicitly maintained list*/
 
void Delete_Fb(void *bp, size_t size_of_block) {
	int num	= size_of_block/6000; // Hash Index
	if(num >= 50)
		num = 49;
	char *p = htp;
	head = p+DSIZE*num;
	void *next_blk = (void *) NextFreeBlock(bp); // Next free block pointer
	void *previous_blk = (void *) PreviousFreeBlock(bp);// Previous free block pointer
	if(*(void **)(head + WSIZE)==0)
		return;
	
	else{
		if (previous_blk == head && next_blk!=0) {
		SetNextFree(head,next_blk); 	// Sets the next block in the list to free
		SetPreviousFree(next_blk,head);	// Sets the previous block in the list to free
		
	      } 
	else if(previous_blk!=head && next_blk!=0){
		SetNextFree(previous_blk, next_blk);	// Sets the next block in the list to free
		SetPreviousFree(next_blk,previous_blk);	// Sets the previous block in the list to free
		
		}

	else if (previous_blk==head && next_blk == 0) {
		SetNextFree(previous_blk,0);// Sets the next block in the list to free
		
		}
	else if (previous_blk!=head && next_blk == 0) {
		SetNextFree(previous_blk,0);// Sets the next block in the list to free
		
		}
	}
}

/*
 * Requires:
 *   "ptr" is either the address of an allocated block or NULL.
 *
 * Effects:
 *   Reallocates the block "ptr" to a block with at least "size" bytes of
 *   payload, unless "size" is zero.  If "size" is zero, frees the block
 *   "ptr" and returns NULL.  If the block "ptr" is already a block with at
 *   least "size" bytes of payload, then "ptr" may optionally be returned.
 *   Otherwise, a new block is allocated and the contents of the old block
 *   "ptr" are copied to that new block.  Returns the address of this new
 *   block if the allocation was successful and NULL otherwise.
 */
void *
mm_realloc(void *ptr, size_t size)
{
	size_t oldsize;
	size_t total_size; 
	void *newptr;
	
		oldsize = GET_SIZE(HDRP(ptr));			// Gets the present size of the allocated block which has to be 								//reallocated	
	
	/* If size == 0 then this is just free, and we return NULL. */
	if (size == 0) {
		mm_free(ptr);				//this frees the block
		return (NULL);
	}

	/* If oldptr is NULL, then this is just malloc. */
	if (ptr == NULL)
		return (mm_malloc(size));		// allocates the block of the mentioned size

	if (size <= DSIZE)
		total_size = 2 * DSIZE;
			else
		total_size = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);//total_size required for the new block to be allocated
	if(oldsize == total_size) return ptr;
	if (oldsize >= total_size) 
	{
		
		if((oldsize - total_size) != (DSIZE))
		{
			if((oldsize - total_size) != 0)
			{
				PUT(HDRP(ptr), PACK(total_size, 1));//packs the size of the block and the allocation(1) status in the header
				PUT(FTRP(ptr), PACK(total_size, 1));//packs the size of the block and the allocation(1) status in the footer
				void *bp = NEXT_BLKP(ptr);//Gets the pointer of the next block in the heap
				PUT(HDRP(bp), PACK(oldsize - total_size, 0));
				//packs the size of the block and the allocation(0) status in the header
				
				PUT(FTRP(bp), PACK(oldsize - total_size, 0));
				//packs the size of the block and the allocation(1) status in the footer
				
				coalesce(bp);	// coaleseces the block 
			}
			return ptr;
		}
		
		
	}
	
		
	size_t next_blkp_size = (size_t)GET_SIZE(HDRP(NEXT_BLKP(ptr)));
	if((size_t)GET_ALLOC(HDRP(NEXT_BLKP(ptr)))==0   &&   next_blkp_size + oldsize - total_size > DSIZE)
	{
		Delete_Fb(NEXT_BLKP(ptr),next_blkp_size);//Deletes the block  from the explicictly maintained free list
		PUT(HDRP(ptr), PACK(total_size, 1));//packs the size of the block and the allocation(1) status in the header
		PUT(FTRP(ptr), PACK(total_size, 1));//packs the size of the block and the allocation(1) status in the footer
		void *bp = NEXT_BLKP(ptr);
		PUT(HDRP(bp), PACK(next_blkp_size + oldsize - total_size, 0));
		//packs the size of the block and the allocation(0) status in the header
		PUT(FTRP(bp), PACK(next_blkp_size + oldsize - total_size, 0));
		//packs the size of the block and the allocation(0) status in the footer
		Add_Fb(bp,next_blkp_size + oldsize - total_size);
		//Adds the block to the explicictly maintained free list checkheap(1);
		return ptr;
	}
	
	newptr = mm_malloc(size);

	/* If realloc() fails the original block is left untouched  */
	if (newptr == NULL)
		return (NULL);	
		
	/* Copy the old data. */

	if (size < oldsize)
		oldsize = size;
	memcpy(newptr, ptr, oldsize);

	/* Free the old block. */
	mm_free(ptr);

	return (newptr);
}

/*
 * The following routines are internal helper routines.
 */

/*
 * Requires:
 *   "bp" is the address of a newly freed block.
 *
 * Effects:
 *   Perform boundary tag coalescing.  Returns the address of the coalesced
 *   block.
 */
static void *
coalesce(void *ptr) 
{
	bool prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));	// Get allocated status of the previous block
	bool next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));	// Get the allocated status of the next block
	size_t size = GET_SIZE(HDRP(ptr));			// Size of the present block


		
	// Since previous and next are free, it coalesces the three of them and return the pointer to the previous block
	if (prev_alloc && next_alloc) {                 /* Case 1 */
		Add_Fb(ptr,size);// Adds free block to the explicitly maintained free list
		return ptr;
	}
	
	 else if (prev_alloc && !next_alloc) {         /* Case 2 */
		Delete_Fb(NEXT_BLKP(ptr),GET_SIZE(HDRP(NEXT_BLKP(ptr))));
		// Deletes the previously embedded block from the explicictly maintained free list
		size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));//calulation of the total new size to be allocated at the new pointer location
		PUT(HDRP(ptr), PACK(size, 0));// packs the size of the block and the allocation(0) status in the header
		PUT(FTRP(ptr), PACK(size, 0));// packs the size of the block and the allocation(0) status in the footer
		Add_Fb(ptr,size);// Adds free block to the explicitly maintained free list	
	}
	
	 else if (!prev_alloc && next_alloc) {         /* Case 3 */
		Delete_Fb(PREV_BLKP(ptr),GET_SIZE(HDRP(PREV_BLKP(ptr))));
		size += GET_SIZE(HDRP(PREV_BLKP(ptr)));//calulation of the total new size to be allocated at the new pointer location
		PUT(FTRP(ptr), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
		ptr = PREV_BLKP(ptr);
		Add_Fb(ptr,size);// Adds free block to the explicitly maintained free list
	
	}
	
	 else {                                        /* Case 4 */
		Delete_Fb(PREV_BLKP(ptr),GET_SIZE(HDRP(PREV_BLKP(ptr))));
		// Deletes the Prev free block from the explicitly maintained list
		Delete_Fb(NEXT_BLKP(ptr),GET_SIZE(HDRP(NEXT_BLKP(ptr))));
		//Deletes the Next free block from the explicitly maintained list
		size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + 
		    GET_SIZE(HDRP(NEXT_BLKP(ptr)));	 //calulation of the total new size to be allocated at the new pointer location
		PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));// packs the size of the block and the allocation(0) status in the header
		PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));// packs the size of the block and the allocation(0) status in the footer
		ptr = PREV_BLKP(ptr);
		Add_Fb(ptr,size);		// Adds free block to the explicitly maintained free list
	}
	return (ptr);
}

/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Extend the heap with a free block and return that block's address.
 */
static void *	
extend_heap(size_t words) 
{
	void *bp;
	size_t size;				// Allocate an even number of words to maintain alignment. 

	/* Allocate an even number of words to maintain alignment. */
	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	if ((bp = mem_sbrk(size)) == (void *)-1)  
		return (NULL);

	/* Initialize free block header/footer and the epilogue header. */
	PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
	PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

	/* Coalesce if the previous block was free. */
	return (coalesce(bp));
}

/*
 * Requires:
 *   None.
 *
 * Effects:
 *   Find a fit for a block with "asize" bytes.  Returns that block's address
 *   or NULL if no suitable block was found. 
 */
static void *
find_fit(size_t asize)
{
	void *bp;
	char *p=htp;
	int num=asize/6000; // Hash Index
	if(num>=50)
		num=49;
	char *k;
	int new_num;
	for(new_num=num;new_num<50;new_num++){
		k=p+DSIZE*new_num; // directs to the particular list where the ree block has to be inserted

		/* Search for the first fit. */
	for (bp = NextFreeBlock(k); (bp != 0)&&(k<p+DSIZE*50); bp = NextFreeBlock(bp)) 
	//This is traversing the whole explicictly maintained free list to get the exact or nearly exact fit for allocation
			{
				if (asize <= GET_SIZE(HDRP(bp)))// checks the size with the required size.
					   return bp;
    			}
  		}
	/* No fit was found. */
	return (NULL);
}

/* 
 * Requires:
 *   "bp" is the address of a free block that is at least "asize" bytes.
 *
 * Effects:
 *   Place a block of "asize" bytes at the start of the free block "bp" and
 *   split that block if the remainder would be at least the minimum block
 *   size. 
 */
static void
place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(HDRP(bp));   		//computes the size of the block

	if ((csize - asize) >= (2 * DSIZE)) { 
		Delete_Fb(bp,csize);
		PUT(HDRP(bp), PACK(asize, 1));//packs the size of the block and the allocation(1) status in the footer
		PUT(FTRP(bp), PACK(asize, 1));//packs the size of the block and the allocation(1) status in the header
		bp = NEXT_BLKP(bp);	      //gets the next block pointer
		PUT(HDRP(bp), PACK(csize - asize, 0));	//packs the size of the block and the allocation(0) status in the footer
		PUT(FTRP(bp), PACK(csize - asize, 0));  //packs the size of the block and the allocation(0) status in the header
		Add_Fb(bp,csize-asize);//Adds the newly created block to the explicitly maintained free list
	} else {
		PUT(HDRP(bp), PACK(csize, 1));//packs the size of the block and the allocation(1) status in the header
		PUT(FTRP(bp), PACK(csize, 1));//packs the size of the block and the allocation(1) status in the footer
		Delete_Fb(bp,csize);//Deletes the block  from the explicictly maintained free list
	}
}

/* 
 * The remaining routines are heap consistency checker routines. 
 */

/*
 * Requires:
 *   "bp" is the address of a block.
 *
 * Effects:
 *   Perform a minimal check on the block "bp".
 */
static void
checkblock(void *bp) 
{

	if ((uintptr_t)bp % DSIZE)
		printf("Error: %p is not doubleword aligned\n", bp);
	if (GET(HDRP(bp)) != GET(FTRP(bp))){
		size_t h=GET(HDRP(bp));
		size_t f=GET(FTRP(bp));
		size_t preh=GET(HDRP(PREV_BLKP(bp)));
		printf("Error: header %zu does not match footer %zu %p and previous block %zu\n,",h,f,bp,preh);
		}
}

/* 
 * Requires:
 *   None.
 *
 * Effects:
 *   Perform a minimal check of the heap for consistency. 
 */
void
checkheap(bool verbose) 
{
	void *bp;

	if (verbose)
		printf("Heap (%p):\n", heap_listp);

	if (GET_SIZE(HDRP(heap_listp)) != DSIZE ||
	    !GET_ALLOC(HDRP(heap_listp)))
		printf("Bad prologue header\n");
	checkblock(heap_listp);

	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		if (verbose)
			printblock(bp);
		checkblock(bp);
	}

	if (verbose)
		printblock(bp);
	if (GET_SIZE(HDRP(bp)) != 0 || !GET_ALLOC(HDRP(bp)))
		printf("Bad epilogue header\n");
}

/*
 * Requires:
 *   "bp" is the address of a block.
 *
 * Effects:
 *   Print the block "bp".
 */
static void
printblock(void *bp) 
{
	bool halloc, falloc;
	size_t hsize, fsize;

	checkheap(false);
	hsize = GET_SIZE(HDRP(bp));
	halloc = GET_ALLOC(HDRP(bp));  
	fsize = GET_SIZE(FTRP(bp));
	falloc = GET_ALLOC(FTRP(bp));  

	if (hsize == 0) {
		printf("%p: end of heap\n", bp);
		return;
	}

	printf("%p: header: [%zu:%c] footer: [%zu:%c]\n", bp, hsize, (halloc ? 'a' :'f') ,fsize, (falloc ? 'a' : 'f'));
}

/*
 * The last lines of this file configures the behavior of the "Tab" key in
 * emacs.  Emacs has a rudimentary understanding of C syntax and style.  In
 * particular, depressing the "Tab" key once at the start of a new line will
 * insert as many tabs and/or spaces as are needed for proper indentation.
 */

/* Local Variables: */
/* mode: c */
/* c-default-style: "bsd" */
/* c-basic-offset: 8 */
/* c-continued-statement-offset: 4 */
/* indent-tabs-mode: t */
/* End: */
