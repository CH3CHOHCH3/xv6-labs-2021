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

#include <limits.h>
#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct {
  struct spinlock lock[NBUCKET];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf *head[NBUCKET];
  struct spinlock eviction;
} bcache;

extern uint ticks;

void
binit(void)
{
  struct buf *b;
  initlock(&bcache.eviction, "bcache");
  // 初始化每个桶的锁
  for(int i = 0; i < NBUCKET; ++i)
    initlock(&bcache.lock[i], "bcache");

  // 把 bufs 平均分配给所有桶
  int idx = 0;
  for(b = bcache.buf; b < bcache.buf + NBUF; b++){
    b->next = bcache.head[idx];
    bcache.head[idx] = b;
    initsleeplock(&b->lock, "buffer");
    idx = (idx + 1)%NBUCKET;
  }
  
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int idx = blockno%NBUCKET;
  acquire(&bcache.lock[idx]);

  // Is the block already cached?
  for(b = bcache.head[idx]; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

  // 释放桶 idx 的锁，避免环形请求造成死锁
  release(&bcache.lock[idx]);
  // 获取驱逐锁，避免多个 bget 为同一 blockno 进行多次驱逐
  acquire(&bcache.eviction);
  // 如果其他 bget 已经实现驱逐，则直接返回。此处无需获取锁，
  // 其他 bget 得不到驱逐锁无法对桶 idx 进行写操作。
  for(b = bcache.head[idx]; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.eviction);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // 遍历所有桶，寻找 ticks 最小的 buf 并驱逐；注意驱逐后要把
  // 该 buf 放到桶 idx 中，这期间 buf 原来的桶和桶 idx 都必须
  // 保持。
  int hold = -1;
  struct buf *minbuf = 0;
  struct buf *minprev = 0;
  uint minticks = UINT_MAX;
  for(int i = 0; i < NBUCKET; ++i){
    if(i == idx) continue;
    acquire(&bcache.lock[i]);

    int finded = 0;
    struct buf *prev = 0;
    for(b = bcache.head[i]; b; prev = b, b = b->next){
      if(!(b->refcnt) && b->ticks < minticks){
        minticks = b->ticks; 
        minprev = prev;
        minbuf = b;
        finded = 1;
      }
    }
    if (finded) {
      if (~hold){
        release(&bcache.lock[hold]);
      }
      hold = i;
    } else {
      release(&bcache.lock[i]);
    }
  }

  if (minbuf) {
    acquire(&bcache.lock[idx]);
    // 调整链表结构
    if (minprev) {
      minprev->next = minbuf->next;
    } else {
      bcache.head[hold] = minbuf->next;
    }
    minbuf->next = bcache.head[idx];
    bcache.head[idx] = minbuf;
    // 覆写 buf 信息
    minbuf->dev = dev;
    minbuf->blockno = blockno;
    minbuf->valid = 0;
    minbuf->refcnt = 1;
    // 释放相关锁
    release(&bcache.lock[idx]);
    release(&bcache.lock[hold]);
    release(&bcache.eviction);
    acquiresleep(&minbuf->lock);
    return minbuf;
  }
  release(&bcache.eviction);
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

  // 此处无需获取锁，因为对 b->ticks 和 b->refcnt 的操作
  // 无需是原子的。
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->ticks = ticks;
  }
}

void
bpin(struct buf *b) {
  int i = b->blockno%NBUCKET;
  acquire(&bcache.lock[i]);
  b->refcnt++;
  release(&bcache.lock[i]);
}

void
bunpin(struct buf *b) {
  int i = b->blockno%NBUCKET;
  acquire(&bcache.lock[i]);
  b->refcnt--;
  release(&bcache.lock[i]);
}


