## 术语

## RISCV 模式

### 介绍

- 机器模式 (M-mode)
  - 主要的特性是拦截和处理异常的能力。
    - 同步异常：在指令执行期间产生，如访问了无效的存储器地址或执行了具有无效的指令时。
    - 中断：与指令流异步的外部事件，比如鼠标的点击。
  - 异常处理
- 用户模式 (U-mode)
  - 不能访问特权控制状态寄存器和执行特权指令，否则产生非法指令异常。
  - 物理地址访问受限，允许M模式指定U模式可以访问的内存地址；PMP (Physical Memory Protection)功能。
- 监督模式 (S-mode)
  - 提供了比较传统的虚拟内存系统

### 模式切换

- 用户模式到机器模式 (User to Machine)

  在U模式下发生了异常，则控制权移交给M模式。

- 机器模式到用户模式 (Machine to User)

  通过将mstatus.MPP设置为U，然后执行特权指令 mret，则可完成M到U模式的切换。

- 用户切换到监督模式 (User to Supervisor)

  异常委托机制，通过该机制可选择性的将中断和同步异常交给S模式处理，而完全绕过M模式。

## lab1_1 系统调用

### 修改内容

` kernal/strap.c`

```c
//
// handling the syscalls. will call do_syscall() defined in kernel/syscall.c
//
static void handle_syscall(trapframe *tf) {
  // tf->epc points to the address that our computer will jump to after the trap handling.
  // for a syscall, we should return to the NEXT instruction after its handling.
  // in RV64G, each instruction occupies exactly 32 bits (i.e., 4 Bytes)
  tf->epc += 4;

  // TODO (lab1_1): remove the panic call below, and call do_syscall (defined in
  // kernel/syscall.c) to conduct real operations of the kernel side for a syscall.
  // IMPORTANT: return value should be returned to user app, or else, you will encounter
  // problems in later experiments!
    
  // 观察do_syscall函数，可知其有八个参数，分别为0-7号寄存器
  // 观察寄存器表，可知a0寄存器用于存储返回值
  long regs = do_syscall(tf->regs.a0, tf->regs.a1, tf->regs.a2, tf->regs.a3, tf->regs.a4, tf->regs.a5, tf->regs.a6, tf->regs.a7);
  tf->regs.a0 = regs;
}
```

### 运行结果

```bash
spike obj/riscv-pke obj/app_helloworld
In m_start, hartid:0
HTIF is available!
(Emulated) memory size: 2048 MB
Enter supervisor mode...
Application: obj/app_helloworld
Application program entry point (virtual address): 0x0000000081000000
Switch to user mode...
Hello world!
User exit with code:0.
System is shutting down with exit code 0.
```



### 思考题

#### 1这里请读者思考为什么要将tf->epc的值进行加4处理？这个问题请结合你对RISC-V指令集架构的理解，以及系统调用的原理回答。

**EPC** ：Exception Program Counter (CP0 Register 14, Select 0), 异常返回地址寄存器，用于存储异常返回地址。

系统调用后 EPC应返回的是下一条指令的地址，而在RV64G中，每条指令有32位，也即四个字节，所以应当加四

#### 2我们的PKE操作系统内核是如何得到应用程序中“hello world!”字符串的地址的呢？

printu函数将用户要打印的字符串地址存储在const char* buf并通过调用` do_user_call(SYS_user_print, (uint64)buf, n, 0, 0, 0, 0, 0); `传递给do_user_call，其进行系统调用ecall并将该地址传递给操作系统内核



## lab1_2 异常处理

### 修改内容

`kernal/machine/mtrap.c`

```C
//
// handle_mtrap calls cooresponding functions to handle an exception of a given type.
//
void handle_mtrap() {
  uint64 mcause = read_csr(mcause);
  switch (mcause) {
    case CAUSE_FETCH_ACCESS:
      handle_instruction_access_fault();
      break;
    case CAUSE_LOAD_ACCESS:
      handle_load_access_fault();
    case CAUSE_STORE_ACCESS:
      handle_store_access_fault();
      break;
    case CAUSE_ILLEGAL_INSTRUCTION:
      // TODO (lab1_2): call handle_illegal_instruction to implement illegal instruction
      // interception, and finish lab1_2.
      // 在此处调用 handle_illegal_instruction()函数即可
      handle_illegal_instruction();

      break;
    case CAUSE_MISALIGNED_LOAD:
      handle_misaligned_load();
      break;
    case CAUSE_MISALIGNED_STORE:
      handle_misaligned_store();
      break;

    default:
      sprint("machine trap(): unexpected mscause %p\n", mcause);
      sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
      panic( "unexpected exception happened in M-mode.\n" );
      break;
  }
```



### 运行结果

```bash
In m_start, hartid:0
HTIF is available!
(Emulated) memory size: 2048 MB
Enter supervisor mode...
Application: obj/app_illegal_instruction
Application program entry point (virtual address): 0x0000000081000000
Switch to user mode...
Going to hack the system by running privilege instructions.
Illegal instruction!
System is shutting down with exit code -1.
```



## lab1_3 (外部)中断

### 修改内容

` kernal/strap.c`

```C
//
// global variable that store the recorded "ticks"
static uint64 g_ticks = 0;
void handle_mtimer_trap() {
  sprint("Ticks %d\n", g_ticks);
  // TODO (lab1_3): increase g_ticks to record this "tick", and then clear the "SIP"
  // field in sip register.
  // hint: use write_csr to disable the SIP_SSIP bit in sip.
  // 全局变量g_ticks，对时钟中断的次数进行计数，为了确保系统持续正常运行，该计数应每次都会完成加一操作。所以，handle_mtimer_trap()首先需要对g_ticks进行加一
  // 其次，由于处理完中断后，SIP（Supervisor Interrupt Pending，即S模式的中断等待寄存器）寄存器中的SIP_SSIP位仍然为1（由M态的中断处理函数设置），如果该位持续为1的话会导致机器始终处于中断状态。所以，handle_mtimer_trap()还需要对SIP的SIP_SSIP位清零，以保证下次再发生时钟中断时，M态的函数将该位置一会导致S模式的下一次中断。
  g_ticks++;
  write_csr(sip, 0L);
}
```



### 运行结果

```bash
In m_start, hartid:0
HTIF is available!
(Emulated) memory size: 2048 MB
Enter supervisor mode...
Application: obj/app_long_loop
Application program entry point (virtual address): 0x0000000081000000
Switch to user mode...
Hello world!
wait 0
wait 5000000
wait 10000000
Ticks 0
wait 15000000
wait 20000000
Ticks 1
wait 25000000
wait 30000000
wait 35000000
Ticks 2
wait 40000000
wait 45000000
Ticks 3
wait 50000000
wait 55000000
wait 60000000
Ticks 4
wait 65000000
wait 70000000
Ticks 5
wait 75000000
wait 80000000
wait 85000000
Ticks 6
wait 90000000
wait 95000000
Ticks 7
User exit with code:0.
```



### 思考题

#### 1 请读者做完lab1_3的实验后思考为什么死循环并不会导致死机。

由于初始时时钟中断部分尚未完成，在收到第一个时钟信号后，程序便会发生崩溃，因此死循环并不会导致死机

## lab2_1 虚实地址转换

### 修改内容

` /kernel/vmm.c`

```C
void *user_va_to_pa(pagetable_t page_dir, void *va)
{
  // TODO (lab2_1): implement user_va_to_pa to convert a given user virtual address "va"
  // to its corresponding physical address, i.e., "pa". To do it, we need to walk
  // through the page table, starting from its directory "page_dir", to locate the PTE
  // that maps "va". If found, returns the "pa" by using:
  // pa = PYHS_ADDR(PTE) + (va - va & (1<<PGSHIFT -1))
  // Here, PYHS_ADDR() means retrieving the starting address (4KB aligned), and
  // (va - va & (1<<PGSHIFT -1)) means computing the offset of "va" in its page.
  // Also, it is possible that "va" is not mapped at all. in such case, we can find
  // invalid PTE, and should return NULL.
    
  //为了在page_dir所指向的页表中查找逻辑地址va，就必须通过调用页表操作相关函数找到包含va的页表项（PTE），通过该PTE的内容得知va所在的物理页面的首地址，最后再通过计算va在页内的位移得到va最终对应的物理地址。
  uint64 va_tmp = (uint64)va;
  pte_t *pte = page_walk(page_dir, va_tmp, 0);

 if (pte)
  {
    //首先通过页表项*pte取得页地址
    //将pte的低十位即各种设置位置0，左移十二位留出页偏移的空间
    uint64 pa = PTE2PA(*pte);
    //与页内偏移相加即为物理地址
    pa += (va_tmp & ((1 << PGSHIFT) - 1));
    return (void *)pa;
  }
  else
  {
    return NULL;
  }
  //panic("You have to implement user_va_to_pa (convert user va to pa) to print messages in lab2_1.\n");
}
```



### 运行结果

```bash
cytmo@Cytmo-Laptop:~/riscv-pke[lab2_1_pagetable]> make run                                         
linking  obj/util.a ...
Util lib has been build into "obj/util.a"
linking  obj/spike_interface.a ...
Spike lib has been build into "obj/spike_interface.a"
compiling kernel/vmm.c
linking obj/riscv-pke ...
PKE core has been built into "obj/riscv-pke"
linking obj/app_helloworld_no_lds ...
User app has been built into "obj/app_helloworld_no_lds"
********************HUST PKE********************
spike obj/riscv-pke obj/app_helloworld_no_lds
In m_start, hartid:0
HTIF is available!
(Emulated) memory size: 2048 MB
Enter supervisor mode...
PKE kernel start 0x0000000080000000, PKE kernel end: 0x0000000080007000, PKE kernel size: 0x0000000000007000 .
free physical memory address: [0x0000000080007000, 0x0000000087ffffff] 
kernel memory manager is initializing ...
KERN_BASE 0x0000000080000000
physical address of _etext is: 0x0000000080004000
kernel page table is on 
User application is loading.
user frame 0x0000000087fbc000, user stack 0x000000007ffff000, user kstack 0x0000000087fbb000 
Application: obj/app_helloworld_no_lds
Application program entry point (virtual address): 0x00000000000100f6
Switch to user mode...
Hello world!
User exit with code:0.
System is shutting down with exit code 0.
```





## lab2_2 简单内存分配和回收

### 修改内容

`  /kernel/vmm.c`

```C
void user_vm_unmap(pagetable_t page_dir, uint64 va, uint64 size, int free)
{
  // TODO (lab2_2): implement user_vm_unmap to disable the mapping of the virtual pages
  // in [va, va+size], and free the corresponding physical pages used by the virtual
  // addresses when if free is not zero.
  // basic idea here is to first locate the PTEs of the virtual pages, and then reclaim
  // (use free_page() defined in pmm.c) the physical pages. lastly, invalidate the PTEs.
  // as naive_free reclaims only one page at a time, you only need to consider one page
  // to make user/app_naive_malloc to produce the correct hehavior.

  //首先通过lookup_pa函数得到物理地址，然后通过free_page函数释放物理页面。
  uint64 pa = lookup_pa(page_dir, va);
  free_page((void *)pa);
  // panic("You have to implement user_vm_unmap to free pages using naive_free in lab2_2.\n");
}
```



### 运行结果

```bash
cytmo@Cytmo-Laptop:~/riscv-pke[lab2_2_allocatepage]> make run                                  
linking  obj/util.a ...
Util lib has been build into "obj/util.a"
linking  obj/spike_interface.a ...
Spike lib has been build into "obj/spike_interface.a"
compiling kernel/vmm.c
linking obj/riscv-pke ...
PKE core has been built into "obj/riscv-pke"
linking obj/app_naive_malloc ...
User app has been built into "obj/app_naive_malloc"
********************HUST PKE********************
spike obj/riscv-pke obj/app_naive_malloc
In m_start, hartid:0
HTIF is available!
(Emulated) memory size: 2048 MB
Enter supervisor mode...
PKE kernel start 0x0000000080000000, PKE kernel end: 0x0000000080007000, PKE kernel size: 0x0000000000007000 .
free physical memory address: [0x0000000080007000, 0x0000000087ffffff] 
kernel memory manager is initializing ...
KERN_BASE 0x0000000080000000
physical address of _etext is: 0x0000000080004000
kernel page table is on 
User application is loading.
user frame 0x0000000087fbc000, user stack 0x000000007ffff000, user kstack 0x0000000087fbb000 
Application: obj/app_naive_malloc
Application program entry point (virtual address): 0x0000000000010078
Switch to user mode...
s: 0000000000400000, {a 1}
User exit with code:0.
System is shutting down with exit code 0.
```



## lab2_3 缺页异常

### 修改内容

` /kernel/strap.c`

```C
void handle_user_page_fault(uint64 mcause, uint64 sepc, uint64 stval)
{
  sprint("handle_page_fault: %lx\n", stval);
  switch (mcause)
  {
  case CAUSE_STORE_PAGE_FAULT:
    // TODO (lab2_3): implement the operations that solve the page fault to
    // dynamically increase application stack.
    // hint: first allocate a new physical page, and then, maps the new page to the
    // virtual address that causes the page fault.

   //首先判断缺页的逻辑地址stval在用户进程逻辑地址空间中的位置，
    //看是不是比USER_STACK_TOP小，且比我们预设的可能的用户栈的最小栈底指针要大
    // virtual address of stack top of user process
    // vmm.h 中定义 #define USER_STACK_TOP 0x7ffff000
    // 比我们预设的可能的用户栈的最小栈底指针要大，用户栈空间一个上限，20个4KB的页面，即81920，转换为16进制即为14000
    if ((stval < 0x7ffff000) && (stval >= (0x7ffff000 - 0x14000)))
    {
      //若为合法的逻辑地址，则分配一个新的物理页，并将其映射到缺页的逻辑地址上
      void *pa = alloc_page();
      //去除低十二位的页内偏移
      uint64 va = stval & 0xfffffffffffff000;
      //U (User) 位表示该页是不是一个用户模式页。如果U=1，表示用户模式下的代码可以访问该页，否则就表示不能访问。
      //R (Read) 、W (Write) 和X (eXecutable) 位分别表示此页对应的实页是否可读、可写和可执行。
      user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa,
                  prot_to_type(PROT_WRITE | PROT_READ, 1));
    }
    else
    {
      panic("illegal logical address.\n");
    };

    break;
  default:
    sprint("unknown page fault.\n");
    break;
  }
}
```



### 运行结果

```bash
cytmo@Cytmo-Laptop:~/riscv-pke[lab2_3_pagefault]> make run        
linking  obj/util.a ...
Util lib has been build into "obj/util.a"
linking  obj/spike_interface.a ...
Spike lib has been build into "obj/spike_interface.a"
compiling kernel/strap.c
linking obj/riscv-pke ...
PKE core has been built into "obj/riscv-pke"
linking obj/app_sum_sequence ...
User app has been built into "obj/app_sum_sequence"
********************HUST PKE********************
spike obj/riscv-pke obj/app_sum_sequence
In m_start, hartid:0
HTIF is available!
(Emulated) memory size: 2048 MB
Enter supervisor mode...
PKE kernel start 0x0000000080000000, PKE kernel end: 0x0000000080007000, PKE kernel size: 0x0000000000007000 .
free physical memory address: [0x0000000080007000, 0x0000000087ffffff] 
kernel memory manager is initializing ...
KERN_BASE 0x0000000080000000
physical address of _etext is: 0x0000000080004000
kernel page table is on 
User application is loading.
user frame 0x0000000087fbc000, user stack 0x000000007ffff000, user kstack 0x0000000087fbb000 
Application: obj/app_sum_sequence
Application program entry point (virtual address): 0x0000000000010096
Switch to user mode...
handle_page_fault: 000000007fffdff8
handle_page_fault: 000000007fffcff8
handle_page_fault: 000000007fffbff8
Summation of an arithmetic sequence from 0 to 1000 is: 500500 
User exit with code:0.
System is shutting down with exit code 0.
```





## lab3_1 进程创建（fork）

### 修改内容

` /kernel/process.c`

```C
int do_fork(process *parent)
{
  sprint("will fork a child from parent %d.\n", parent->pid);
  process *child = alloc_process();

  for (int i = 0; i < parent->total_mapped_region; i++)
  {
    // browse parent's vm space, and copy its trapframe and data segments,
    // map its code segment.
    switch (parent->mapped_info[i].seg_type)
    {
    case CONTEXT_SEGMENT:
      *child->trapframe = *parent->trapframe;
      break;
    case STACK_SEGMENT:
      memcpy((void *)lookup_pa(child->pagetable, child->mapped_info[0].va),
             (void *)lookup_pa(parent->pagetable, parent->mapped_info[i].va), PGSIZE);
      break;
    case CODE_SEGMENT:{
      // TODO (lab3_1): implment the mapping of child code segment to parent's
      // code segment.
      // hint: the virtual address mapping of code segment is tracked in mapped_info
      // page of parent's process structure. use the information in mapped_info to
      // retrieve the virtual to physical mapping of code segment.
      // after having the mapping information, just map the corresponding virtual
      // address region of child to the physical pages that actually store the code
      // segment of parent process.
      // DO NOT COPY THE PHYSICAL PAGES, JUST MAP THEM.

      //取得parent的代码段的物理地址，其虚拟地址存储在mapped_info[i].va
      uint64 pa = lookup_pa(parent->pagetable, parent->mapped_info[i].va);
      if (pa)
      {
        //若存在，则将child的代码段映射到parent的代码段上
        user_vm_map(child->pagetable, parent->mapped_info[i].va, PGSIZE, pa, prot_to_type(PROT_WRITE | PROT_READ | PROT_EXEC, 1));
      }
      // panic("You need to implement the code segment mapping of child in lab3_1.\n");

      // after mapping, register the vm region (do not delete codes below!)
      child->mapped_info[child->total_mapped_region].va = parent->mapped_info[i].va;
      child->mapped_info[child->total_mapped_region].npages =
          parent->mapped_info[i].npages;
      child->mapped_info[child->total_mapped_region].seg_type = CODE_SEGMENT;
      child->total_mapped_region++;
      break;}
    }
  }

  child->status = READY;
  child->trapframe->regs.a0 = 0;
  child->parent = parent;
  insert_to_ready_queue(child);

  return child->pid;
}
```



### 运行结果

```bash
cytmo@Cytmo-Laptop:~/riscv-pke[lab3_1_fork]> make run
linking  obj/spike_interface.a ...
Spike lib has been build into "obj/spike_interface.a"
compiling kernel/process.c
linking obj/riscv-pke ...
PKE core has been built into "obj/riscv-pke"
linking obj/app_naive_fork ...
User app has been built into "obj/app_naive_fork"
********************HUST PKE********************
spike obj/riscv-pke obj/app_naive_fork
In m_start, hartid:0
HTIF is available!
(Emulated) memory size: 2048 MB
Enter supervisor mode...
PKE kernel start 0x0000000080000000, PKE kernel end: 0x0000000080009000, PKE kernel size: 0x0000000000009000 .
free physical memory address: [0x0000000080009000, 0x0000000087ffffff] 
kernel memory manager is initializing ...
KERN_BASE 0x0000000080000000
physical address of _etext is: 0x0000000080005000
kernel page table is on 
Switch to user mode...
in alloc_proc. user frame 0x0000000087fbc000, user stack 0x000000007ffff000, user kstack 0x0000000087fbb000 
User application is loading.
Application: obj/app_naive_fork
CODE_SEGMENT added at mapped info offset:3
Application program entry point (virtual address): 0x0000000000010078
going to insert process 0 to ready queue.
going to schedule process 0 to run.
User call fork.
will fork a child from parent 0.
in alloc_proc. user frame 0x0000000087faf000, user stack 0x000000007ffff000, user kstack 0x0000000087fae000 
going to insert process 1 to ready queue.
Parent: Hello world! child id 1
User exit with code:0.
going to schedule process 1 to run.
Child: Hello world!
User exit with code:0.
no more ready processes, system shutdown now.
System is shutting down with exit code 0.
```



## lab3_2 进程yield

### 修改内容

` /kernel/syscall.c`

```C
ssize_t sys_user_yield()
{
  // TODO (lab3_2): implment the syscall of yield.
  // hint: the functionality of yield is to give up the processor. therefore,
  // we should set the status of currently running process to READY, insert it in
  // the rear of ready queue, and finally, schedule a READY process to run.

  //可知yield()为系统调用，则需要修改的函数存在于syscall.c
  // 已知进程释放CPU的动作应该是：
  // 将当前进程置为就绪状态（READY）；
  // 将当前进程加入到就绪队列的队尾；
  // 转进程调度。
  // 依次执行即可
  current->status = READY;
  insert_to_ready_queue(current);
  schedule();

  // panic( "You need to implement the yield syscall in lab3_2.\n" );
  return 0;
}
```



### 运行结果

```bash
cytmo@Cytmo-Laptop:~/riscv-pke[lab3_2_yield]> make run        
linking  obj/util.a ...
Util lib has been build into "obj/util.a"
linking  obj/spike_interface.a ...
Spike lib has been build into "obj/spike_interface.a"
compiling kernel/syscall.c
linking obj/riscv-pke ...
PKE core has been built into "obj/riscv-pke"
linking obj/app_yield ...
User app has been built into "obj/app_yield"
********************HUST PKE********************
spike obj/riscv-pke obj/app_yield
In m_start, hartid:0
HTIF is available!
(Emulated) memory size: 2048 MB
Enter supervisor mode...
PKE kernel start 0x0000000080000000, PKE kernel end: 0x0000000080009000, PKE kernel size: 0x0000000000009000 .
free physical memory address: [0x0000000080009000, 0x0000000087ffffff] 
kernel memory manager is initializing ...
KERN_BASE 0x0000000080000000
physical address of _etext is: 0x0000000080005000
kernel page table is on 
Switch to user mode...
in alloc_proc. user frame 0x0000000087fbc000, user stack 0x000000007ffff000, user kstack 0x0000000087fbb000 
User application is loading.
Application: obj/app_yield
CODE_SEGMENT added at mapped info offset:3
Application program entry point (virtual address): 0x000000000001017c
going to insert process 0 to ready queue.
going to schedule process 0 to run.
User call fork.
will fork a child from parent 0.
in alloc_proc. user frame 0x0000000087faf000, user stack 0x000000007ffff000, user kstack 0x0000000087fae000 
going to insert process 1 to ready queue.
Parent: Hello world! 
Parent running 0 
going to insert process 0 to ready queue.
going to schedule process 1 to run.
Child: Hello world! 
Child running 0 
going to insert process 1 to ready queue.
going to schedule process 0 to run.
Parent running 10000 
going to insert process 0 to ready queue.
going to schedule process 1 to run.
Child running 10000 
going to insert process 1 to ready queue.
going to schedule process 0 to run.
Parent running 20000 
going to insert process 0 to ready queue.
going to schedule process 1 to run.
Child running 20000 
going to insert process 1 to ready queue.
going to schedule process 0 to run.
Parent running 30000 
going to insert process 0 to ready queue.
going to schedule process 1 to run.
Child running 30000 
going to insert process 1 to ready queue.
going to schedule process 0 to run.
Parent running 40000 
going to insert process 0 to ready queue.
going to schedule process 1 to run.
Child running 40000 
going to insert process 1 to ready queue.
going to schedule process 0 to run.
Parent running 50000 
going to insert process 0 to ready queue.
going to schedule process 1 to run.
Child running 50000 
going to insert process 1 to ready queue.
going to schedule process 0 to run.
Parent running 60000 
going to insert process 0 to ready queue.
going to schedule process 1 to run.
Child running 60000 
going to insert process 1 to ready queue.
going to schedule process 0 to run.
User exit with code:0.
going to schedule process 1 to run.
User exit with code:0.
no more ready processes, system shutdown now.
System is shutting down with exit code 0.
```



## lab3_3 循环轮转调度

### 修改内容

` /kernel/strap.c`

```C
void rrsched()
{
  // TODO (lab3_3): implements round-robin scheduling.
  // hint: increase the tick_count member of current process by one, if it is bigger than
  // TIME_SLICE_LEN (means it has consumed its time slice), change its status into READY,
  // place it in the rear of ready queue, and finally schedule next process to run.

  /*实现循环轮转调度时，应采取的逻辑为：

  判断当前进程的tick_count加1后是否大于等于TIME_SLICE_LEN？
  若答案为yes，则应将当前进程的tick_count清零，并将当前进程加入就绪队列，转进程调度；
  若答案为no，则应将当前进程的tick_count加1，并返回。
  */
  if(current->tick_count++ >= TIME_SLICE_LEN)
  {
    current->tick_count = 0;
    insert_to_ready_queue(current);
    schedule();
  }
  else
  {
    current->tick_count++;
  }
  // panic("You need to further implement the timer handling in lab3_3.\n");
}
```



### 运行结果

```bash
cytmo@Cytmo-Laptop:~/riscv-pke[lab3_3_rrsched]> make run        
linking  obj/util.a ...
Util lib has been build into "obj/util.a"
linking  obj/spike_interface.a ...
Spike lib has been build into "obj/spike_interface.a"
compiling kernel/strap.c
linking obj/riscv-pke ...
PKE core has been built into "obj/riscv-pke"
linking obj/app_two_long_loops ...
User app has been built into "obj/app_two_long_loops"
********************HUST PKE********************
spike obj/riscv-pke obj/app_two_long_loops
In m_start, hartid:0
HTIF is available!
(Emulated) memory size: 2048 MB
Enter supervisor mode...
PKE kernel start 0x0000000080000000, PKE kernel end: 0x0000000080009000, PKE kernel size: 0x0000000000009000 .
free physical memory address: [0x0000000080009000, 0x0000000087ffffff] 
kernel memory manager is initializing ...
KERN_BASE 0x0000000080000000
physical address of _etext is: 0x0000000080005000
kernel page table is on 
Switch to user mode...
in alloc_proc. user frame 0x0000000087fbc000, user stack 0x000000007ffff000, user kstack 0x0000000087fbb000 
User application is loading.
Application: obj/app_two_long_loops
CODE_SEGMENT added at mapped info offset:3
Application program entry point (virtual address): 0x000000000001017c
going to insert process 0 to ready queue.
going to schedule process 0 to run.
User call fork.
will fork a child from parent 0.
in alloc_proc. user frame 0x0000000087faf000, user stack 0x000000007ffff000, user kstack 0x0000000087fae000 
going to insert process 1 to ready queue.
Parent: Hello world! 
Parent running 0 
Parent running 10000000 
Ticks 0
Parent running 20000000 
Ticks 1
going to insert process 0 to ready queue.
going to schedule process 1 to run.
Child: Hello world! 
Child running 0 
Child running 10000000 
Ticks 2
Child running 20000000 
Ticks 3
going to insert process 1 to ready queue.
going to schedule process 0 to run.
Parent running 30000000 
Ticks 4
Parent running 40000000 
Ticks 5
going to insert process 0 to ready queue.
going to schedule process 1 to run.
Child running 30000000 
Ticks 6
Child running 40000000 
Ticks 7
going to insert process 1 to ready queue.
going to schedule process 0 to run.
Parent running 50000000 
Parent running 60000000 
Ticks 8
Parent running 70000000 
Ticks 9
going to insert process 0 to ready queue.
going to schedule process 1 to run.
Child running 50000000 
Child running 60000000 
Ticks 10
Child running 70000000 
Ticks 11
going to insert process 1 to ready queue.
going to schedule process 0 to run.
Parent running 80000000 
Ticks 12
Parent running 90000000 
Ticks 13
going to insert process 0 to ready queue.
going to schedule process 1 to run.
Child running 80000000 
Ticks 14
Child running 90000000 
Ticks 15
going to insert process 1 to ready queue.
going to schedule process 0 to run.
User exit with code:0.
going to schedule process 1 to run.
User exit with code:0.
no more ready processes, system shutdown now.
System is shutting down with exit code 0.
```



##  lab3_challenge1 进程等待和数据段复制

### 修改内容

通过修改PKE内核和系统调用，为用户程序提供wait函数的功能，wait函数接受一个参数pid：

- 当pid为-1时，父进程等待任意一个子进程退出即返回子进程的pid；
- 当pid大于0时，父进程等待进程号为pid的子进程退出即返回子进程的pid；
- 如果pid不合法或pid大于0且pid对应的进程不是当前进程的子进程，返回-1。

补充do_fork函数，实验3_1实现了代码段的复制，你需要继续实现数据段的复制并保证fork后父子进程的数据段相互独立。

` /user/user_lib.h`

` /user/user_lib.c`

```C
//user_lib.h 添加系统调用定义
int wait(int pid);

//user_lic.c添加函数体
//wait
int wait(int pid)
{
  //若wait系统调用返回WAIT_NOT_END，子进程尚未终止，则继续等待，释放cpu
  //若返回WAIT_PID_ILLEGAL,则说明pid不合法，返回-1
  //若返回值不是以上两种，则说明子进程已经正常终止，返回子进程的pid
  while(1)
  {
    int status = do_user_call(SYS_user_wait, pid, 0, 0, 0, 0, 0, 0);
    switch (status)
    {
    case WAIT_NOT_END:
      yield();
      break;
    case WAIT_PID_ILLEGAL:
      return -1;
    default:
      return status;
      break;
    }
  }
}
```

` /kernel/syscall.h`

` /kernel/syscall.c`

```c
//添加wait操作的宏定义
#define SYS_user_wait (SYS_user_base + 6)

//跳转到wait的处理函数
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7)
{
  switch (a0)
  {
  case SYS_user_print:
    return sys_user_print((const char *)a1, a2);
  case SYS_user_exit:
    return sys_user_exit(a1);
  case SYS_user_allocate_page:
    return sys_user_allocate_page();
  case SYS_user_free_page:
    return sys_user_free_page(a1);
  case SYS_user_fork:
    return sys_user_fork();
  case SYS_user_yield:
    return sys_user_yield();
  //add here
  case SYS_user_wait:
    return sys_user_wait(a1);  
  default:
    panic("Unknown syscall %ld \n", a0);
  }
}
```



` /kernel/process.c`

` /kernel/process.h`

```c
//process.h
#define WAIT_NOT_END -2
#define WAIT_PID_ILLEGAL -1
#define WAIT_END_NORMALLY 0
int sys_user_wait(int pid);

//process.c
//数据段复制
 case DATA_SEGMENT:
    {
      //进行数据段复制，将所有虚拟页映射到新的物理页
      //循环取得parent的数据段的所有页 mapping_info is unused if npages == 0
      if(parent->mapped_info[i].npages!=0)
      {
        //取得parent的数据段的物理地址，其虚拟地址存储在mapped_info[i].va
        uint64 address = lookup_pa(parent->pagetable, parent->mapped_info[i].va);
        //分配新页，并将父进程的数据段内容映射到新页上
        char *newaddress = alloc_page();
        memcpy(newaddress, (void *)address, PGSIZE);
        map_pages(child->pagetable, parent->mapped_info[i].va, PGSIZE,
                  (uint64)newaddress, prot_to_type(PROT_WRITE | PROT_READ, 1));
      }

      // after mapping, register the vm region (do not delete codes below!)
      child->mapped_info[child->total_mapped_region].va = parent->mapped_info[i].va;
      child->mapped_info[child->total_mapped_region].npages =
          parent->mapped_info[i].npages;
      child->mapped_info[child->total_mapped_region].seg_type = DATA_SEGMENT;
      child->total_mapped_region++;
      break;
    }
// 进程等待实现
// wait函数接受一个参数pid：
// 当pid为-1时，父进程等待任意一个子进程退出即返回子进程的pid；
// 当pid大于0时，父进程等待进程号为pid的子进程退出即返回子进程的pid；
// 如果pid不合法或pid大于0且pid对应的进程不是当前进程的子进程，返回-1。
int sys_user_wait(int pid)
{
  // 当pid为-1时，父进程等待任意一个子进程退出即返回子进程的pid；
  if (pid == -1)
  {
    bool found = FALSE;
    //检查进程池是否有属于当前进程的子进程
    for (int i = 0; i < NPROC; i++)
      if (procs[i].parent == current)
      {
        found = TRUE;
        //检查该子进程是否已经结束，结束，则返回子进程pid
        if (procs[i].status == ZOMBIE)
        {
          procs[i].status = FREE;
          return i;
        }
      }
    if (found)
      //如果有子进程，但是没有结束，则返回WAIT_NOT_END
      return WAIT_NOT_END;
    else
      //否则pid不合法，返回WAIT_PID_ILLEGAL
      return WAIT_PID_ILLEGAL;
  }
  // 当pid大于0时，父进程等待进程号为pid的子进程退出即返回子进程的pid；
  else if (pid > 0)
  {
    //检查pid是否合法
    if (pid >= NPROC)
      return WAIT_PID_ILLEGAL;
    //检查pid是否属于当前进程的子进程
    if (procs[pid].parent != current)
      return WAIT_PID_ILLEGAL;
    else
    {
      //检查该子进程是否已经结束，结束，则返回子进程pid
      if (procs[pid].status == ZOMBIE)
      {
        procs[pid].status = FREE;
        return pid;
      }
      else
        return WAIT_NOT_END;
    }
  }
  else
    return WAIT_PID_ILLEGAL;
}



```



### 运行结果

```bash
cytmo@Cytmo-Laptop:~/riscv-pke[lab3_challenge1_wait]> make run
linking  obj/util.a ...
Util lib has been build into "obj/util.a"
linking  obj/spike_interface.a ...
Spike lib has been build into "obj/spike_interface.a"
compiling kernel/process.c
linking obj/riscv-pke ...
PKE core has been built into "obj/riscv-pke"
linking obj/app_wait ...
User app has been built into "obj/app_wait"
********************HUST PKE********************
spike obj/riscv-pke obj/app_wait
In m_start, hartid:0
HTIF is available!
(Emulated) memory size: 2048 MB
Enter supervisor mode...
PKE kernel start 0x0000000080000000, PKE kernel end: 0x0000000080009000, PKE kernel size: 0x0000000000009000 .
free physical memory address: [0x0000000080009000, 0x0000000087ffffff] 
kernel memory manager is initializing ...
KERN_BASE 0x0000000080000000
physical address of _etext is: 0x0000000080005000
kernel page table is on 
Switch to user mode...
in alloc_proc. user frame 0x0000000087fbc000, user stack 0x000000007ffff000, user kstack 0x0000000087fbb000 
User application is loading.
Application: obj/app_wait
CODE_SEGMENT added at mapped info offset:3
DATA_SEGMENT added at mapped info offset:4
Application program entry point (virtual address): 0x00000000000100b0
going to insert process 0 to ready queue.
going to schedule process 0 to run.
User call fork.
will fork a child from parent 0.
in alloc_proc. user frame 0x0000000087fae000, user stack 0x000000007ffff000, user kstack 0x0000000087fad000 
going to insert process 1 to ready queue.
going to insert process 0 to ready queue.
going to schedule process 1 to run.
User call fork.
will fork a child from parent 1.
in alloc_proc. user frame 0x0000000087fa1000, user stack 0x000000007ffff000, user kstack 0x0000000087fa0000 
going to insert process 2 to ready queue.
going to insert process 1 to ready queue.
going to schedule process 0 to run.
going to insert process 0 to ready queue.
going to schedule process 2 to run.
Grandchild process end, flag = 2.
User exit with code:0.
going to schedule process 1 to run.
Child process end, flag = 1.
User exit with code:0.
going to schedule process 0 to run.
Parent process end, flag = 0.
User exit with code:0.
no more ready processes, system shutdown now.
System is shutting down with exit code 0.
```

