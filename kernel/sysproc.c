#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_mmap(void){
  int len, prot, flags, fd;

  argint(1, &len);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fd);

  len = PGROUNDUP(len);
  struct proc *p = myproc();
  // 是否打开了文件 fd
  if(p->ofile[fd] == 0){
    return -1;
  }
  if(flags & MAP_SHARED){
    if(!(p->ofile[fd]->readable) && (prot & PROT_READ)) {
      return -1;
    }
    if(!(p->ofile[fd]->writable) && (prot & PROT_WRITE)) {
      return -1;
    }
  }
  for(int i = 0; i < VMACNT; ++i){
    // 找空闲 vma
    if(p->vmas[i].start == 0){
      p->vmas[i].start = p->sz;
      p->sz += len;

      p->vmas[i].len = len;
      p->vmas[i].prot = prot;
      p->vmas[i].flags = flags;
      p->vmas[i].f = filedup(p->ofile[fd]);

      return p->vmas[i].start;
    }
  }
  return -1;
}

uint64 sys_munmap(void){
  int len;
  uint64 start;

  argaddr(0, &start);
  argint(1, &len);

  return munmap(start, len);
}