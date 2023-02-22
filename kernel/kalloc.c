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

// 用于统计每个物理页的引用数
int refer[32698];
struct spinlock rf_lock;

// 增加物理页的引用数
int addrefer(uint64 pa){
  int ans;
  acquire(&rf_lock);
  ans = ++refer[(pa-(uint64)end)>>12];
  release(&rf_lock);
  return ans;
}

// 减少物理页的引用数
int subrefer(uint64 pa){
  int ans;
  acquire(&rf_lock);
  ans = --refer[(pa-(uint64)end)>>12];
  release(&rf_lock);
  return ans;
}

// 获取物理页的引用数
int getrefer(uint64 pa){
  return refer[(pa-(uint64)end)>>12];
}


// cow 的核心函数，把 cow 的 pte 变成正常的 pte.
int cowalloc(pte_t* pte){
  uint64 pa = PTE2PA(*pte);

  acquire(&rf_lock);
  if(getrefer(pa) > 1){
    // 引用次数超过 1 需要分配新物理页
    char *mem;
    if((mem = kalloc()) == 0){
      release(&rf_lock);
      return -1;
    }
    memmove(mem, (void *)pa, PGSIZE);
    // 不需要调用 mappages，直接修改 pte 即可
    *pte = PA2PTE((uint64)mem) | PTE_FLAGS(*pte);
    --refer[(pa-(uint64)end)>>12];
    release(&rf_lock);
  } else {
    // 引用次数小于 1 只需修改 pte
    release(&rf_lock);
  }
  *pte |= PTE_W;
  *pte &= ~PTE_COW;
  return 0;
}

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  // 初始化引用数组锁
  initlock(&rf_lock, "refer");
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

  // 如果引用数大于 1 不需要释放物理页
  if(subrefer((uint64)pa) > 0){
    return;
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    // 此处无需加锁，因为 kalloc 不会分配相同物理地址的页
    refer[((uint64)r - (uint64)end)>>12] = 1;
  }
  return (void*)r;
}
