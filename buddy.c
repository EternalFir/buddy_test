#include "buddy.h"
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
#include <assert.h>

#define NULL ((void *)0)
#define SINGLEPAGESIZE (1024*4)

int _pow(int n) {
    if (n <= 0)
        return 1;
    return 2 * _pow(n - 1);
}

int _log(int n) {
    if (n == 0)
        return -1;
    int ans = 0;
    while (n > 1) {
        ans++;
        n >>= 1;
    }
    return ans;
}

int _log_up(int n) {
    if (n == 0)
        return -1;
    int ans = 1;
    int cnt = 0;
    while (n > ans) {
        ans *= 2;
        cnt++;
    }
    return cnt;
}

int _hi(int n) {
    if (n < 0)
        return -1;
    int cnt = 0;
    while (n > 1) {
        cnt++;
        n >>= 1;
    }
    return cnt;
}

void *start_addr;
int max_block_size;
int max_rank;
int *available_table;

int *header_rank;

struct PageBlock {
    int index;
    int rank;
//    int status;
    struct PageBlock *prev;
    struct PageBlock *next;
};

struct PageBlock **available_block_array;

struct PageBlock *blockCreate(int index_in, int rank_in) {
    struct PageBlock *new_block = malloc(sizeof(struct PageBlock));
    new_block->index = index_in;
    new_block->rank = rank_in;
    new_block->prev = NULL;
    new_block->next = NULL;
    struct PageBlock *prev_in = available_block_array[rank_in];
//    new_block->status = status_in;
    if (prev_in != NULL) {
        new_block->prev = prev_in;
        if (prev_in->next != NULL) {
            new_block->next = prev_in->next;
            prev_in->next->prev = new_block;
        }
        prev_in->next = new_block;
    } else {
        available_block_array[rank_in] = new_block;
    }
    return new_block;
}

void blockDestroy(struct PageBlock *page_in) {
    if (page_in->prev != NULL) {
        page_in->prev->next = page_in->next;
    }
    if (page_in->next != NULL) {
        page_in->next->prev = page_in->prev;
    }
    if(page_in==available_block_array[page_in->rank])
        available_block_array[page_in->rank]=page_in->next;
    free(page_in);
}

int getSize(int index) {
    return max_block_size / (_pow(_hi(index)));
}

int getNum(int index) {
    return (index - _pow(_hi(index))) * getSize(index);
}

void *getAddr(int index) {
    return start_addr + SINGLEPAGESIZE * getNum(index);
}

int init_page(void *p, int pgcount) {
//    printf("%d", _log_up(8));

    max_rank = _log(pgcount);
    max_block_size = pgcount;
    start_addr = p;
    available_table = malloc(sizeof(int) * pgcount * 2);
    memset(available_table, 0, sizeof(int) * pgcount * 2);
    available_block_array = malloc(sizeof(struct PageBlock *) * (max_rank + 1));
    header_rank = malloc(sizeof(int) * pgcount);
    memset(header_rank, -1, sizeof(int) * pgcount);
    struct PageBlock *top_block = blockCreate(1, _log(pgcount));
//    available_block_array[_log(pgcount)] = top_block;
    if (top_block->prev != NULL)
        assert(0);
    return OK;
}

void *alloc_pages(int rank) {
    // get suitable block
    int find = -1;
    rank--;
    if (rank < 0 || rank > _log(max_block_size))
        return -EINVAL;
    for (int i = rank; i <= max_rank; ++i) {
        if (available_block_array[i] != NULL) {
            find = i;
            break;
        }
    }
    if (find == -1)
        return -ENOSPC;
    while (find != rank) {
        struct PageBlock *chosen = available_block_array[find];
        struct PageBlock *new1 = blockCreate(chosen->index * 2, chosen->rank - 1);
        struct PageBlock *new2 = blockCreate(chosen->index * 2 + 1, chosen->rank - 1);
        if (new1->next != new2)
            printf("opps");
        if(new1->prev!=NULL)
            assert(0);
        blockDestroy(chosen);
        find--;
    }
    struct PageBlock *chosen = available_block_array[rank];
    available_table[chosen->index] = 1;
    header_rank[getNum(chosen->index)] = rank;
    void *addr = getAddr(chosen->index);
    blockDestroy(chosen);
    return addr;
}

int return_pages(void *p) {
    int num = (int) (p - start_addr) / SINGLEPAGESIZE;
    if (num < 0 || num >= max_block_size) { // out of range
        return -EINVAL;
    }
    if (header_rank[num] == -1) // not allocated yet
        return -EINVAL;
    int rank = header_rank[num];
    int index = (num + max_block_size) / _pow(rank);
    header_rank[num] = -1;
    available_table[index] = 0;
    struct PageBlock *new_block = blockCreate(index, rank);
//    if (new_block->prev != NULL) // maybe(?
//        assert(0);
    int change = 1;
    while (change) { // merge
        change = 0;
        struct PageBlock *object =available_block_array[new_block->rank];
        while (object != NULL) {
            if (new_block!=object &&(new_block->index >> 1 == object->index >> 1)) { // find a buddy
                struct PageBlock *higher = blockCreate(new_block->index >> 1, new_block->rank + 1);
//                if (higher->prev != NULL) // maybe(?
//                    assert(0);
                blockDestroy(object);
                blockDestroy(new_block);
                new_block = higher;
                change = 1;
                break;
            }
            object = object->next;
        }
    }
    return OK;
}

int query_ranks(void *p) {
    int num = (int) (p - start_addr) / SINGLEPAGESIZE;
    if (num < 0 || num >= max_block_size) { // out of range
        return -EINVAL;
    }
    int rank = 0;
    int index = max_block_size + num;
    while (available_table[index] != 1 && index != 0) {
        rank++;
        index >>= 1;
    }
    if (index != 0) { // allocated
        return rank + 1;
    }
    rank=0;
    index = max_block_size + num;
    while (available_table[((index >> 1) << 1)] +available_table[((index >> 1 )<< 1)+1] == 0 && index>1) {
        rank++;
        index >>= 1;
    }
    return rank + 1;
}

int query_page_counts(int rank) {
    rank--;
    if (rank < -1 || rank > _log(max_block_size))
        return -EINVAL;
    int cnt = 0;
    struct PageBlock *obj = available_block_array[rank];
    while (obj != NULL) {
        cnt++;
        obj = obj->next;
    }
    return cnt;
}

// warning: rank is 0-based inside but 1-based outside
