// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
// 修改为数组 每个CPU对应一个kmem lab8-1
} kmem[NCPU];

void
kinit()
{
  // 给每个kmem初始化 lab8-1
  for (int i = 0; i < NCPU; ++i)
  	initlock(&kmem[i].lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // 关闭中断 获取那个CPU lab8-1
  push_off();
  int index = cpuid();
  pop_off();

  acquire(&kmem[index].lock);
  r->next = kmem[index].freelist;
  kmem[index].freelist = r;
  release(&kmem[index].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  // 关闭中断 获取那个CPU
  push_off();
  int index = cpuid();
  pop_off();

  acquire(&kmem[index].lock);
  r = kmem[index].freelist;
  // 有空闲款
  if(r)
    kmem[index].freelist = r->next;
  else {
  	for (int i = 0; i < NCPU; ++i) {
		if (i == index)
			continue;
		acquire(&kmem[i].lock);
		r = kmem[i].freelist;
		if (r)
			kmem[i].freelist = r->next;
		release(&kmem[i].lock);
		if (r)
			break;
	}
  }
  release(&kmem[index].lock);
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
