/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h>


void* expend_heap(size_t size);
void initialize_freelist();
void add_freelist(sf_block *the_block);
void remove_freelist(sf_block *the_block);
void* initialize_heap(size_t size);
size_t get_block_size(size_t size);
int get_freelist_index(size_t size);
sf_block* search_freelist(size_t size);
sf_block* alloc_freelist(size_t size, sf_block* free_block);
sf_block* split(size_t size, sf_block* free_block);
sf_block* coalescing_two_blocks(sf_block* a, sf_block* b);

long size_masking_bit = 0xFFFFFFFFFFFFFFC0;
//long size_masking_bit = ~63;


/**
 * expend the heap size when there is not enough space
 * return NULL: unable to expend the page
 *        coalesced_block: when the last block was freeblock
 *        new_freeblock: when the last page was full
 */
void* expend_heap(size_t size) {
    sf_block* old_epilogue = sf_mem_end() - 16;
    size_t curr_page_size = 0;
    size_t prev_free_size = 0;
    sf_block* old_freeblock;
    //when there is a free block
    if(!(old_epilogue->header & PREV_BLOCK_ALLOCATED)) {
        old_freeblock = (void*) old_epilogue - (old_epilogue->prev_footer & size_masking_bit);
        prev_free_size = old_freeblock->header & size_masking_bit;
    }
    sf_block* can_grow;
    while(size > (curr_page_size + prev_free_size) && (can_grow = sf_mem_grow()) != NULL) {
        curr_page_size += PAGE_SZ;
    }
    sf_block* new_epilogue = sf_mem_end() - 16;
    new_epilogue->header = 0;
    new_epilogue->header |= THIS_BLOCK_ALLOCATED;
    //when there is a free block
    if(!(old_epilogue->header & PREV_BLOCK_ALLOCATED)) {
        sf_block* new_freeblock = old_epilogue;
        new_freeblock->header = curr_page_size; //page_size - epilogue_size
        add_freelist(new_freeblock);
        new_epilogue->prev_footer = new_freeblock->header;
        coalescing_two_blocks(old_freeblock, new_freeblock);
    }
    //when the heap is full
    else {
        sf_block* new_freeblock = (void*) old_epilogue;
        new_freeblock->header = curr_page_size;
        new_freeblock->header |= PREV_BLOCK_ALLOCATED;
        new_freeblock->header |= THIS_BLOCK_ALLOCATED;
        new_epilogue->prev_footer = new_freeblock->header;
        add_freelist(new_freeblock);
    }
    if(can_grow == NULL) {
        return NULL;
    }
    else {
        sf_block* found = search_freelist(size);
        sf_block *the_block = alloc_freelist(size, found);
        return the_block;
    }
    return NULL;
}

/**
 * initialize the freelist (first call)
 */
void initialize_freelist() {
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
}

/**
 * add free block into the free list
 */
void add_freelist(sf_block *the_block) {
    int index = get_freelist_index(the_block->header & size_masking_bit);
    // the_block->body.links.next = &sf_free_list_heads[index];
    // the_block->body.links.prev = sf_free_list_heads[index].body.links.prev;
    // (*sf_free_list_heads[index].body.links.prev).body.links.next = the_block;
    // sf_free_list_heads[index].body.links.prev = the_block;
    the_block->body.links.prev = &sf_free_list_heads[index];
    the_block->body.links.next = sf_free_list_heads[index].body.links.next;
    (*sf_free_list_heads[index].body.links.next).body.links.prev = the_block;
    sf_free_list_heads[index].body.links.next = the_block;
}

void remove_freelist(sf_block *the_block) {
    (the_block->body.links.prev)->body.links.next = the_block->body.links.next;
    (the_block->body.links.next)->body.links.prev = the_block->body.links.prev;
    the_block->body.links.prev = NULL;
    the_block->body.links.next = NULL;
}

/**
 * initialize the heap & add first block (first call)
 * return new_block: newly created block(always allocated)
 */
void* initialize_heap(size_t size) {
    void *new_heap = sf_mem_grow();
    if(new_heap == NULL) {
        debug("failed to create a new heap");
        sf_errno = ENOMEM;
        return NULL;
    }

    //add prologue after 7 unused rows ((unused)7rows - (prev_footer)1row)
    sf_block *prologue = sf_mem_start() + 48;
    prologue->header = 64;
    prologue->header |= THIS_BLOCK_ALLOCATED;

    //when the first block size > PAGE_SZ
    size_t curr_page_size = PAGE_SZ - 128;
    while(size > curr_page_size){
        if(sf_mem_grow() == NULL) {
            sf_block *new_block = (void*) prologue + 64;
            new_block->header = curr_page_size;
            new_block->header |= PREV_BLOCK_ALLOCATED;
            add_freelist(new_block);
            return NULL;
        }
        curr_page_size += PAGE_SZ;
    }

    //add a block ((unused)7rows + (prologue)1row + (payload)7rows - (prev_footer)1row)
    sf_block *new_block = (void*) prologue + 64;
    new_block->header = size;
    new_block->header |= PREV_BLOCK_ALLOCATED;
    new_block->header |= THIS_BLOCK_ALLOCATED;


    //add epilogue
    sf_block *epilogue = sf_mem_end() - 16;

    epilogue->header = 0;
    epilogue->header |= THIS_BLOCK_ALLOCATED;
    if(curr_page_size - size != 0) {
        sf_block *init_block = (void*) new_block + size;
        init_block->header = curr_page_size - size;
        init_block->header |= PREV_BLOCK_ALLOCATED;
        add_freelist(init_block);
        epilogue->prev_footer = init_block->header;
    }
    return new_block;
}

/**
 * add the header size & resize it multiple of 64
 * return size: the size of the block multiple of 64
 */
size_t get_block_size(size_t size) {
    //add header size
    size = size + 8;
    //when the size is less then the min size which is 64 bytes
    if(size < 64) {
        return 64;
    }
    //find the leftover
    size_t padding_size = size % 64;
    //when it fits perfect
    if(padding_size == 0) {
        return size;
    }
    //add the padding
    else {
        return size + (64 - padding_size);
    }
}

/**
 * find the index of the free list large enough to hold the block
 * return index: the index of the freelist
 *        -1: when there is an error
 */
int get_freelist_index(size_t size) {
    int M = 64;
    int a = 2;
    int b = 3;
    int c = 5;
    for(int i = 0; i < (NUM_FREE_LISTS); i++) {
        if(i == 0 || i == 1 || i == 2) {
            if(size == M * (i + 1)) {
                return i;
            }
        }
        else if(i >= NUM_FREE_LISTS - 1) {
            if(size > (M * b)) {
                return i;
            }
        }
        else {
            if(size > (M * b) && size <= (M * c)) {
                return i;
            }
            a = b;
            b = c;
            c = a + b;
        }
    }
    return -1;
}

/**
 * find the freelist large enough to hold the block
 */
sf_block* search_freelist(size_t size) {
    int index = get_freelist_index(size);
    for(int i = index; i < NUM_FREE_LISTS; i++) {
        if(sf_free_list_heads[i].body.links.next != &sf_free_list_heads[i]
            && sf_free_list_heads[i].body.links.prev != &sf_free_list_heads[i]) {

            sf_block* curr_block = &sf_free_list_heads[i];
            while(curr_block->body.links.next != &sf_free_list_heads[i]) {
                curr_block = curr_block->body.links.next;
                if ((curr_block->header & size_masking_bit) >= size) {
                    curr_block->body.links.prev->body.links.next = curr_block->body.links.next;
                    curr_block->body.links.next->body.links.prev = curr_block->body.links.prev;
                    return curr_block;
                }
            }
        }
    }
    return NULL;
}

/**
 * add up the two blocks
 */
sf_block* coalescing_two_blocks(sf_block* a, sf_block* b) {
    size_t size_a = a->header & size_masking_bit;
    size_t prev_al = a->header & PREV_BLOCK_ALLOCATED;
    size_t al = a->header & THIS_BLOCK_ALLOCATED;
    size_t size_b = b->header & size_masking_bit;
    remove_freelist(a);
    remove_freelist(b);
    sf_block* block_ab = a;
    block_ab->header = size_a + size_b;
    block_ab->header |= prev_al;
    block_ab->header |= al;
    add_freelist(block_ab);

    sf_block* update_footer = (void*) block_ab + size_a + size_b;
    update_footer->prev_footer = block_ab->header;
    return block_ab;
}


/**
 * split the rest of the free block and return
 */
sf_block* split(size_t size, sf_block* free_block) {
    size_t free_block_size = free_block->header & size_masking_bit;
    if((free_block_size - size) >= 64) {
        sf_block* lower_part = free_block;
        lower_part->header = size;
        lower_part->header |= THIS_BLOCK_ALLOCATED;
        if(!lower_part->prev_footer) {
            lower_part->header |= PREV_BLOCK_ALLOCATED;
        }
        sf_block* upper_part = (void*)free_block + size;
        upper_part->header = free_block_size - size;
        upper_part->header |= PREV_BLOCK_ALLOCATED;
        add_freelist(upper_part);

        sf_block* next_block = (void*) free_block + free_block_size;
        next_block->prev_footer = upper_part->header;
        return lower_part;
    }
    //when the upper_part size is less than 64
    return NULL;
}

/**
 * allocate the free block and split the rest of the block
 */
sf_block* alloc_freelist(size_t size, sf_block* free_block) {
    size_t free_block_size = free_block->header & size_masking_bit;
    if(size == free_block_size) {
        //found the prefect fit
    }
    else {
        return split(size, free_block);
    }
    return NULL;
}

void *sf_malloc(size_t size) {
    if(size == 0) {
        return NULL;
    }

    //get the block size
    size_t block_size = get_block_size(size);
    sf_block *found;

    //when there is no heap
    if(sf_mem_start() == sf_mem_end()){
        initialize_freelist();
        // return initialize_heap(block_size);
        void* return_val = initialize_heap(block_size);
        if(return_val == NULL) {
            //failed to initialize the heap
            sf_errno = ENOMEM;
            return NULL;
        }
        sf_show_heap();
        return (void*) return_val + 16;
    }
    //when there is no matching freelist
    if((found = search_freelist(block_size))  == NULL) {
        sf_block* expended = expend_heap(block_size);
        if(expended == NULL) {
            //failed to expend the heap
            sf_errno = ENOMEM;
            return NULL;
        }
        else{
            // sf_show_heap();
            return (void*) expended + 16;
        }
    }
    else {
        sf_block *the_block = alloc_freelist(block_size, found);
        // sf_show_heap();
        return (void*) the_block + 16;
    }
    //unexpected error
    sf_errno = ENOMEM;
    return NULL;
}

void sf_free(void *pp) {
    sf_block* the_block = pp - 16;
    //the pp is NULL, not 64-byte aligned or al is 0
    if(pp == NULL) {
        debug("abort! the pp is NULL");
        abort();
    }
    if(!((pp - sf_mem_start() + 16) % 64)) {
        debug("abort! not 64byte aligned");
        abort();
    }
    if(!(the_block->header & THIS_BLOCK_ALLOCATED)) {
        debug("abort! al is 0");
        abort();
    }
    //the header of block is before the start of the mem_start or the footer is after mem_end
    sf_block* pp_next_block = (void*) the_block + (the_block->header & size_masking_bit);
    if(pp < sf_mem_start() || (void*) pp_next_block > sf_mem_end()) {
        debug("the block is in unvalid position");
        abort();
    }
    //the prev al is 0 when the prev al is not free
    sf_block* pp_prev_block;
    if(!(the_block->header & PREV_BLOCK_ALLOCATED)) {
        pp_prev_block = (void*) the_block - (the_block->prev_footer & size_masking_bit);
        if(pp_prev_block->header & THIS_BLOCK_ALLOCATED) {
            debug("PREV_BLOCK_ALLOCATED is 0 but prev_block header THIS_BLOCK_ALLOCATED is 1");
            abort();
        }
    }
    the_block->header ^= THIS_BLOCK_ALLOCATED;
    pp_next_block->prev_footer = the_block->header;
    pp_next_block->header ^= PREV_BLOCK_ALLOCATED;
    add_freelist(the_block);

    if(!(the_block->header & PREV_BLOCK_ALLOCATED)) {
        // sf_block* pp_prev_block = the_block - (the_block->prev_footer & size_masking_bit);
        sf_block* first_two_blocks = coalescing_two_blocks(pp_prev_block, the_block);
        if(!(pp_next_block->header & THIS_BLOCK_ALLOCATED)) {
            coalescing_two_blocks(first_two_blocks, pp_next_block);
        }
    }
    else {
        if(!(pp_next_block->header & THIS_BLOCK_ALLOCATED)) {
            coalescing_two_blocks(the_block, pp_next_block);
        }
    }
    // debug("\nsf_free\n");
    // sf_show_heap();
    return;
}

void *sf_realloc(void *pp, size_t rsize) {
    sf_block* the_block = pp - 16;
    //the pp is NULL
    if(pp == NULL) {
        debug("abort! the pp is NULL");
        sf_errno = EINVAL;
        abort();
    }
    //pp is not 64-byte aligned
    if(!((pp - sf_mem_start() + 16)%64)) {
        debug("abort! not 64byte aligned");
        sf_errno = EINVAL;
        abort();
    }
    //pp's allocated value is 0
    if(!(the_block->header & THIS_BLOCK_ALLOCATED)) {
        debug("abort! al is 0");
        sf_errno = EINVAL;
        abort();
    }
    //the header of block is before the start of the mem_start or the footer is after mem_end
    sf_block* pp_next_block = (void*) the_block + (the_block->header & size_masking_bit);
    if(pp < sf_mem_start() || (void*) pp_next_block > sf_mem_end()) {
        debug("the block is in unvalid position");
        sf_errno = EINVAL;
        abort();
    }
    //the prev al is 0 when the prev al is not free
    sf_block* pp_prev_block;
    if(!(the_block->header & PREV_BLOCK_ALLOCATED)) {
        pp_prev_block = the_block - (the_block->prev_footer & size_masking_bit);
        if(pp_prev_block->header & THIS_BLOCK_ALLOCATED) {
            debug("PREV_BLOCK_ALLOCATED is 0 but prev_block header THIS_BLOCK_ALLOCATED is 1");
            sf_errno = EINVAL;
            abort();
        }
    }
    size_t rblock_size = get_block_size(rsize);
    size_t pp_size = the_block->header & size_masking_bit;
    if(rsize == 0) {
        sf_free(pp);
        return NULL;
    }
    else if(pp_size == rsize) {
        return pp;
    }
    else if(pp_size < rsize) {
        sf_block* malloced = sf_malloc(rsize);
        if(malloced == NULL) {
            // sf_show_heap();
            return NULL;
        }
        memcpy(pp, malloced + 16, pp_size - 8);
        sf_free(pp);
        return malloced;
    }
    else if(pp_size > rsize) {
        if(rblock_size < pp_size) {
            sf_block* pp_next_block = (void*) the_block + (the_block->header & size_masking_bit);
            sf_block* return_val = split(rblock_size, the_block);
            if(!(pp_next_block->header & THIS_BLOCK_ALLOCATED)) {
                sf_block* split_next_block = (void*) return_val + (return_val->header & size_masking_bit);
                coalescing_two_blocks(split_next_block, pp_next_block);
            }
            if(return_val == NULL) {
                return NULL;
            }
            return (void*) return_val + 16;
        }
        else {
            if(memcpy(pp, the_block + 16, rsize - 8) == NULL){
                sf_errno = ENOMEM;
                return NULL;
            }
            return (void*) the_block + 16;
        }
    }
    sf_errno = ENOMEM;
    return NULL;
}
