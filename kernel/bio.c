// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
extern uint ticks;

// 哈希函数
int hash(int blockno) {
  return blockno % NBUCKET;
}


struct {
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  
  // lab8-2
  struct spinlock big_lock;
  struct spinlock lock[NBUCKET];
  struct buf head[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.big_lock, "bcache_big_lock");
  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // 初始化桶链表 lab8-2
  for (int i = 0; i < NBUCKET; ++i) {
    initlock(&bcache.lock[i], "bcache_bucket");
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  // 初始化buf lab8-2
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  // lab8-2
  int index = hash(blockno);
  int min_tick = __UINT32_MAX__;
  struct buf* target_buf = 0;

  acquire(&bcache.lock[index]);

  // Is the block already cached?
  // 命中
  for(b = bcache.head[index].next; b != &bcache.head[index]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      // 记录使用时间lab8-2
      // b->tick = ticks;
      release(&bcache.lock[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.lock[index]);

  // 不命中
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  acquire(&bcache.big_lock);
  acquire(&bcache.lock[index]);
  // 当前桶中找空闲
  for (b = bcache.head[index].next; b != &bcache.head[index]; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock[index]);
      release(&bcache.big_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  for (b = bcache.head[index].next; b != &bcache.head[index]; b = b->next) {
    if (b->refcnt == 0 && (target_buf == 0 || b->tick < min_tick)) {
      min_tick = b->tick;
      target_buf = b;
    }
  }
  
  if (target_buf) {
    target_buf->dev = dev;
    target_buf->blockno = blockno;
    target_buf->refcnt++;
    target_buf->valid = 0;
    release(&bcache.lock[index]);
    release(&bcache.big_lock);
    acquiresleep(&target_buf->lock);
    return target_buf;
  }

  // 从其他桶找空闲块
  for (int i = hash(index + 1); i != index; i = hash(i + 1)) {
    acquire(&bcache.lock[i]);
    for (b = bcache.head[i].next; b != &bcache.head[i]; b = b->next) {
      if (b->refcnt == 0 && (target_buf == 0 || b->tick < min_tick)) {
        min_tick = b->tick;
        target_buf = b;
      }
    }
    if (target_buf) {
      target_buf->dev = dev;
      target_buf->refcnt++;
      target_buf->valid = 0;
      target_buf->blockno = blockno;
      // 从原桶中删除
      target_buf->next->prev = target_buf->prev;
      target_buf->prev->next = target_buf->next;
      release(&bcache.lock[i]);
      //加锁
      target_buf->next = bcache.head[index].next;
      target_buf->prev = &bcache.head[index];
      bcache.head[index].next->prev = target_buf;
      bcache.head[index].next = target_buf;
      release(&bcache.lock[index]);
      release(&bcache.big_lock);
      acquiresleep(&target_buf->lock);
      return target_buf;
    }
    release(&bcache.lock[i]);
  }

  release(&bcache.lock[index]);
  release(&bcache.big_lock);
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  // lab8-2
  int index = hash(b->blockno);

  acquire(&bcache.lock[index]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // lab8-2
    // no one is waiting for it.
    // b->next->prev = b->prev;
    // b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
    // LRU记录辅助
    b->tick = ticks;
  }
  
  release(&bcache.lock[index]);
}

void
bpin(struct buf *b) {
  // lab8-2
  int index = hash(b->blockno);
  acquire(&bcache.lock[index]);
  b->refcnt++;
  release(&bcache.lock[index]);
}

void
bunpin(struct buf *b) {
  // lab8-2
  int index = hash(b->blockno);
  acquire(&bcache.lock[index]);
  b->refcnt--;
  release(&bcache.lock[index]);
}
