#include "types.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

// 计数结构
struct {
    uint8 cnt;
    struct spinlock lock;
    //12 就是页面大小
} cow[(PHYSTOP - KERNBASE) >> 12];

// 增加
void add_cnt(uint64 pa) {
    if (pa < KERNBASE)
        return;
    
    pa = (pa - KERNBASE) >> 12;
    acquire(&cow[pa].lock);
    ++cow[pa].cnt;
    release(&cow[pa].lock);
}

// 减少 需要返回判断是否为零
uint8 sub_cnt(uint64 pa) {
    uint8 ans;
    if (pa < KERNBASE)
        return 0;
    pa = (pa - KERNBASE) >> 12;
    acquire(&cow[pa].lock);
    ans = --cow[pa].cnt;
    release(&cow[pa].lock);
    return ans;
}