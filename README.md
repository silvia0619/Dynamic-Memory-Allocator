# Dynamic-Memory-Allocator

In this assignment we will assume that each "memory row" is 8 bytes (64 bits) in size. All pointers returned by your sf_malloc are to be 64-byte
aligned; that is, they will be addresses that are multiples of 64. This requirement permits such pointers to be used to store any of the basic
machine data types in a "naturally aligned" fashion. A value stored in memory is said to be naturally aligned if the address at which it is stored is
a multiple of the size of the value. For example, an int value is naturally aligned when stored at an address that is a multiple of 4. A long
double value is naturally aligned when stored at an address that is a multiple of 16. Keeping values naturally aligned in memory is a hardware-imposed 
requirement for some architectures, and improves the efficiency of memory access in other architectures

## Description

Created an allocator for the x86-64 architecture with the following features:
* Free lists segregated by size class, using first-fit policy within each size class.
* Immediate coalescing of blocks on free with adjacent free blocks.
* Boundary tags to support efficient coalescing, with footer optimization that allows footers to be omitted from allocated blocks.
* Block splitting without creating splinters.
* Allocated blocks aligned to (64-byte) boundaries.
* Free lists maintained using last in first out (LIFO) discipline.
* Use of a prologue and epilogue to achieve required alignment and avoid edge cases at the end of the heap.
* You will implement your own versions of the malloc, realloc, and free functions.
* You will use existing Criterion unit tests and write your own to help debug your implementation.

## Getting Started

### Testing program
```c
bin/sfmm_tests --verbose=0
```

