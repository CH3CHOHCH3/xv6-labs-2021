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
} kmem[NCPU];

// 获取 cpuid
int getcpu(){
    int id;
    push_off();
    id = cpuid();
    pop_off();
    return id;
}

void
kinit()
{
  // 初始化每个链表的锁
  for(int i = 0; i < NCPU; ++i){
    initlock(&kmem[i].lock, "kmem");
  }
  // kinit 时直接把所有页先分给一个 cpu
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

  int idx = getcpu();
  acquire(&kmem[idx].lock);
  r->next = kmem[idx].freelist;
  kmem[idx].freelist = r;
  release(&kmem[idx].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int idx = getcpu();
  acquire(&kmem[idx].lock);
  r = kmem[idx].freelist;
  if(r)
    kmem[idx].freelist = r->next;
  release(&kmem[idx].lock);

  // 如果当前 cpu 的空闲页链表为空，
  // 那么从其他 cpu 空闲页链表窃取。
  if(!r){
    for(int i = 0; i < NCPU; ++i){
      if(i == idx) continue;

      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r)
        kmem[i].freelist = r->next;
      release(&kmem[i].lock);
      
      if(r) break;
    }
  }
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
