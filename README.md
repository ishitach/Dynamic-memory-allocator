# Malloc-C
Make an efficient memory allocator from a basic allocator based on implicit free list.


Copy all the files in lab folder to a protected directory in which you plan to do the work.

Type you name and email address in the header comment at the top of mm.c.

Type the command make to compile and link a basic memory allocator, the support routines, and the test driver. This basic memory allocator is based on an
implicit free list, first fit placement, and boundary tag coalescing.

Run the test driver mdriver to test the memory utilization and throughput
performance of this basic memory allocator.

Boundary tag optimization:

The mm.c file implements a simple memory allocator. It requires both a header and a footer for each block in order to perform constant-time coalescing. Modify
the allocator so that free blocks require a header and footer, but allocated blocks require only a header. Use the driver program to test the modified allocator. Your
implementation must pass the correctness tests performed by the driver program. 
