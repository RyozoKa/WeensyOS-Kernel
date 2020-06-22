#include "malloc.h"
#include "process.h"



typedef char ALIGN[32];

union header {
	struct {
		size_t size;
		unsigned is_free;
		union header *next;
	} s;
	ALIGN stub;
};
typedef union header header_t;
// TODO: Implement these functions
size_t num_allocs = 0;

// Store some simple meta data
header_t *head, *tail;
header_t *get_free_block(size_t size);
void *malloc(size_t size)
{
	size_t total_size;
	void *block;
	header_t *header;
	if (!size)
		return NULL;
	header = get_free_block(size);
	if (header) {
		header->s.is_free = 0;
        ++num_allocs;
		return (void*)(header + 1);
	}
	total_size = sizeof(header_t) + size;
	block = sys_sbrk(total_size);
	if (block == (void*) -1)
		return NULL;
	header = block;
	header->s.size = size;
	header->s.is_free = 0;
	header->s.next = NULL;
	if (!head)
		head = header;
	if (tail)
		tail->s.next = header;
	tail = header;
    ++num_allocs;
	return (void*)(header + 1);
}

header_t *get_free_block(size_t size)
{
	header_t *curr = head;
	while(curr) {
		if (curr->s.is_free && curr->s.size >= size)
			return curr;
		curr = curr->s.next;
	}
	return NULL;
}

void *calloc(size_t num, size_t nsize)
{
	size_t size;
	void *block;
	if (!num || !nsize)
		return NULL;
	size = num * nsize;

	if (nsize != size / num)
		return NULL;
	block = malloc(size);
	if (!block)
		return NULL;
	memset(block, 0, size);
	return block;
}

void free(void *block)
{
	header_t *header, *tmp;
	void *programbreak;

	if (!block)
		return;
	header = (header_t*)block - 1;

	programbreak = sys_sbrk(0);
	if ((char*)block + header->s.size == programbreak) 
    {
		if (head == tail) 
			head = tail = NULL;
        else 
        {
			tmp = head;
			while (tmp) 
            {
				if(tmp->s.next == tail) 
                {
					tmp->s.next = NULL;
					tail = tmp;
			    }
			    tmp = tmp->s.next;
			}
		}
		sys_sbrk(0 - sizeof(header_t) - header->s.size);
		return;
	}
	header->s.is_free = 1;
    --num_allocs;
}

void *realloc(void *block, size_t size)
{
	header_t *header;
	void *ret;
	if (!block || !size)
		return malloc(size);
	header = (header_t*)block - 1;
	if (header->s.size >= size)
		return block;
	ret = malloc(size);
	if (ret) 
    {
		memcpy(ret, block, header->s.size);
		free(block);
	}
	return ret;
}

void defrag()
{

}

int heap_info(heap_info_struct * info)
{

    header_t** Blocks = (header_t**) malloc( (num_allocs + 2) * sizeof(uintptr_t) );
    long* SzArray = (long*)malloc( (num_allocs + 1) * sizeof(long*) );
    unsigned int i = 0;
    size_t free = 0, max = 0;
    header_t* hd = tail;
    while(hd)
    {   
        if(hd->s.is_free)
        {
            free += hd->s.size;
            if(hd->s.size > max)
                max = hd->s.size;
        }
        hd = hd->s.next;
    }

    //Simple sort
    for(i = 0; i < num_allocs; ++i)
    {
        for(unsigned int j = i; j < num_allocs; ++j)
        {   
            if(Blocks[i]->s.size < Blocks[j]->s.size)
            {
                header_t* tmp = Blocks[i];
                Blocks[i] = Blocks[j];
                Blocks[j] = tmp;
            }
            SzArray[i] = Blocks[i]->s.size;
            (*(char**)&Blocks ) += sizeof(ALIGN);   //We want the data, not the header
        }
    }
    info->num_allocs = num_allocs;
    info->size_array = SzArray;
    info->ptr_array = (void**)Blocks;
    info->free_space = free;
    info->largest_free_chunk = max;

    return 0;
}
