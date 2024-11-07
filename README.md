# <font color="#F7A004">Intro</font>

**<font size = 4>2024 Fall NCU Linux OS Project 1</font>**
* Add a system call that get physical addresses from virtual addresses
* 介紹 `copy_from_user` 及 `copy_to_user` 使用方法  
* 使用Copy on Write 機制來驗證system call 正確呼叫  
* 介紹 Demand Paging 在 memory 中的使用時機  

**<font size = 4>Environment</font>**
```
OS: Ubuntu 22.04
ARCH: X86_64
Kernel Version: 5.15.137
```

# <font color="#F7A004">Build Kernel</font>  




# <font color="#F7A004">`copy_from_user` 及 `copy_to_user`</font>

## copy_from_user
根據[bootlin](https://elixir.free-electrons.com/linux/v5.15.137/source/include/linux/uaccess.h#L189)

```c
unsigned long copy_from_user(void *to, const void __user *from, unsigned long n);
```

這個函數的功能是將user space的資料複製到kernel space。其中:  
`to`: 目標位址，是kernel space中的一個指標，用來存放從user space 複製過來的資料。  
`from`:來源位址，是user space中的一個指標，指向需要被複製的資料(ex: point to virtual address)。  
`n`: 要傳送資料的長度  
傳回值: 0 on success, or the number of bytes that could not be copied.  

## copy_to_user

```c
unsigned long copy_to_user(void __user *to, const void *from, unsigned long n);
```

這個函數的功能是將kernel space的資料複製到user space variable。其中:  
`to`: 目標地址(user space)  
`from`: 複製地址(kernel space)  
`n`: 要傳送資料的長度  
傳回值: 0 on success, or the number of bytes that could not be copied.  

## Purpose
* Prevents crashes due to invalid memory access.
* Maintains security by ensuring memory access respects user-space permissions.
* Enables error handling by providing feedback on failed memory operations.

:::success
這兩個 function 都是在 kernel space 中使用
:::

## Example

**<font size = 4>新增一個system call 作為範例</font>**
```c=1
#include <linux/kernel.h>       
#include <linux/syscalls.h>     
#include <linux/uaccess.h>      // For copy_from_user and copy_to_user

SYSCALL_DEFINE2(get_square, int __user *, input, int __user *, output) {
    int kernel_input;
    int result;

    // Copy the input value from user space to kernel space
    if (copy_from_user(&kernel_input, input, sizeof(int))) {
        return -EFAULT; // Return error if copy fails
    }

    // Calculate the square
    result = kernel_input * kernel_input;

    // Copy the result back to user space
    if (copy_to_user(output, &result, sizeof(int))) {
        return -EFAULT; // Return error if copy fails
    }

    return 0; // Return success
}
```

**<font size = 4>User code</font>**
```c=1
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>

int main() {
    int input;
    int output;

    printf("Enter an integer: ");
    if (scanf("%d", &input) != 1) {
        fprintf(stderr, "Invalid input\n");
        return 1;
    }

    // Call the system call with pointers to input and output
    long result = syscall(451, &input, &output);

    if (result == -1) {
        perror("syscall failed");
    }else{
        printf("Input: %d, Output (Square): %d\n", input, output);
    }

    return 0;
}
```
line 17傳入`&input`及`&output`，  
分別對應system call 的`int __user *, input`及`int __user *, output`，若正確複製則回傳值為0  
而user space的`output`已經在`copy_to_user()`時寫入新資料。  



**<font size = 4>執行結果 :</font>**  
![image](https://hackmd.io/_uploads/HyF41Ysl1g.png)  

system call 正確呼叫且輸出計算結果



# <font color="#F7A004">實作system call</font>

## Page Table in Linux

Page table 一般來說可以分為兩種結構，32 bit cpu使用4-level(10-10-12)或是 64 bit cpu使用5-level(9-9-9-9-12，加起來只有 48 因為最高的 16 位是sign extension)的架構，但也有3-level的結構，這可以透過 config 內的 `CONFIG_PGTABLE_LEVELS` 設定，基本上是基於處理器架構在設定的

- **Structure of page tables**
    - PGD (Page Global Directory)
    - P4D (Page 4 Directory，<font color="red">5-level 才有</font>)
    - PUD (Page Upper Directory)
    - PMD (Page Middle Directory)
    - PTE （page table entry）
    
使用4-level page table 為例:  

![linux_paging](https://hackmd.io/_uploads/rkIiRAVxJx.jpg)


可以看到Page table的base address 是存放在 CR3（又稱 PDBR，page directory base register）這個register，存放的是**physical address**。但我們需要的是他的virtual address，因此，使用 `task_struct->mm->pgd` 內儲存的則是 Process Global Directory(PGD) 的virtual address，

**補充：**
甚麼是`task_struct`及`mm_struct`可以參考下方 [what is mm_struct?](#mm_struct)

每個process有各自的page table，每當context switch發生時，CR3會載入新的page table base addr.，且CR3寫入時，TLB會被自動刷新，避免用到上一個process之TLB。

因此要從logical address轉換為physical address，需要一層一層下去查表，
順序為: `pgd_t` -> `p4d_t` -> `pud_t` -> `pmd_t` -> `pte_t`

其中舉例，若要查`p4d`的base address則需要`pgd_t + p4d_index`  
``` c=1 
pgd_t *pgd;
p4d_t *p4d;

pgd = pgd_offset(current->mm, vaddr);
p4d = p4d_offset(pgd, vaddr);
```
同理，若要查`pte`的base address則需要`pmd_t + ptd_index`  
```c=1
ptd_t *pte;

pte = pte_offset(pmd, vaddr);
```

我們可以直接到 [bootlin](https://elixir.bootlin.com/linux/v5.15.137/source/include/linux/pgtable.h#L88) 中看到這些offset function 的實作細節  

```c
// include/linux/pgtable.h line 88

#ifndef pte_offset_kernel
static inline pte_t *pte_offset_kernel(pmd_t *pmd, unsigned long address)
{
        return (pte_t *)pmd_page_vaddr(*pmd) + pte_index(address);
}
#define pte_offset_kernel pte_offset_kernel
#endif

//...

// line 106
/* Find an entry in the second-level page table.. */
#ifndef pmd_offset
static inline pmd_t *pmd_offset(pud_t *pud, unsigned long address)
{
        return pud_pgtable(*pud) + pmd_index(address);
}
#define pmd_offset pmd_offset
#endif

#ifndef pud_offset
static inline pud_t *pud_offset(p4d_t *p4d, unsigned long address)
{
        return p4d_pgtable(*p4d) + pud_index(address);
}
#define pud_offset pud_offset
#endif

static inline pgd_t *pgd_offset_pgd(pgd_t *pgd, unsigned long address)
{
        return (pgd + pgd_index(address));
};

/*
 * a shortcut to get a pgd_t in a given mm
 */
#ifndef pgd_offset
#define pgd_offset(mm, address)		pgd_offset_pgd((mm)->pgd, (address))
#endif
```
對應到前一張圖，找到前一層的Directory offset再加上當前Directory的 index，一層一層去找  

不過發現`p4d_offset`的實作細節沒有出現在這，但是`pud_offset`傳入的參數卻是`p4d_t *p4d`，後來在`arch/x86/include/asm/pgtable.h line 926`中找到
```c=926
// arch/x86/include/asm/pgtable.h line 926

/* to find an entry in a page-table-directory. */
static inline p4d_t *p4d_offset(pgd_t *pgd, unsigned long address)
{
        if (!pgtable_l5_enabled())
                return (p4d_t *)pgd;
        return (p4d_t *)pgd_page_vaddr(*pgd) + p4d_index(address);
}
```
其中`pgtable_l5_enabled()`check whether 5-level page table is enabled。因此如果系統使用的是4-level，則無需存取 `p4d_t`，且直接回傳以`(p4d_t*) pgd`，  
也就是說在4-level下 `pgd` = `p4d`  
相同道理，3-level下 `pgd` = `p4d` = `pud`  

另外，在system call code中使用`pgd = pgd_offset(current->mm, vaddr);` 時，實際上會呼叫以下這段Macro：  
```c
#define pgd_offset(mm, address)	pgd_offset_pgd((mm)->pgd, (address))
```
這個Macro的內容是：
`pgd_offset(current->mm, vaddr)`   
會被展開為 `pgd_offset_pgd((current->mm)->pgd, vaddr)`  


根據上述對linux中page table介紹，便可以寫出page table walk 的程式碼

## Page Table walk

新增一個檔案叫 `project1.c`，路徑為 `kernel/project1.c`
:::spoiler <font color = "yellow">範例</font>

```c=1
#include <linux/syscalls.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/mm_types.h>
#include <asm/pgtable.h>

SYSCALL_DEFINE2(my_get_physical_addresses,
                void *, user_vaddr, 
                unsigned long *, user_paddr) {
    
    unsigned long vaddr;
    unsigned long paddr = 0;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    unsigned long page_addr = 0;
    unsigned long page_offset = 0;

    // Copy the virtual address from user space to kernel space
    if (copy_from_user(&vaddr, user_vaddr, sizeof(unsigned long))) {
        printk("Error: Failed to copy virtual address from user space\n");
        return -EFAULT;
    }

    // Get the PGD (Page Global Directory) for the current process
    pgd = pgd_offset(current->mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        printk("PGD entry not valid or not present\n");
        return -EFAULT;    // #define	EFAULT		14	 /*Bad address*/
    }

    // Get the P4D (Page 4 Directory)
    p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        printk("P4D entry not valid or not present\n");
        return -EFAULT;
    }
    // Get the PUD (Page Upper Directory)
    pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud) || pud_bad(*pud)) {
        printk("PUD entry not valid or not present\n");
        return -EFAULT;
    }

    // Get the PMD (Page Middle Directory)
    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd) || pmd_bad(*pmd)) {
        printk("PMD entry not valid or not present\n");
        return -EFAULT;
    }

    // Get the PTE (Page Table Entry)
    pte = pte_offset_kernel(pmd, vaddr);
    if (!pte_present(*pte)) {
        printk("Page not present in memory\n");
        return -EFAULT;
    }

    // Compute physical address from PTE
    page_addr = pte_val(*pte) & PAGE_MASK;
    page_offset = vaddr & ~PAGE_MASK;
    paddr = page_addr | page_offset;

    // Copy the result back to user space
    if (copy_to_user(user_paddr, &paddr, sizeof(unsigned long))) {
        printk("Error: Failed to copy physical address to user space\n");
        return -EFAULT;
    }

    return 0;
}
```
:::

**<font size = 4>在程式碼中，line 64至66主要是用來計算physical address</font>**

:::success
**<font color = "yellow">以實際例子介紹line 64~66</font>**

**<font size = 4>新增test.c</font>**

```c=1
#include <stdio.h>
#include <sys/syscall.h>      /* Definition of SYS_* constants */
#include <unistd.h>

void * my_get_physical_addresses(void *vaddr){
        unsigned long paddr;

        long result = syscall(450, &vaddr, &paddr);

        return (void *)paddr;
};

int main()
{
    int a = 10;
    printf("Virtual addr. of arg a = %p\n", &a);
    printf("Physical addr. of arg a = %p\n", my_get_physical_addresses(&a));
}
```

**<font size = 4>結果:</font>**  
![截圖 2024-11-04 下午4.15.30](https://hackmd.io/_uploads/rkb3G-U-ke.png)

**<font size = 4>使用dmesg來查看kernel內的訊息</font>**  

![截圖 2024-11-04 下午4.16.24](https://hackmd.io/_uploads/rk4J7Z8ZJx.png)
![image](https://hackmd.io/_uploads/r1iZv-IWJe.png)


可以看到virtual address = `0x7ffe22a14794`
`pte_val(*pte)` = PTE base address = `0x8000000199b8f867`
另外，`PAGE_MASK` = `0xFFFFFFFFFFFFF000`因為page size 為 4KB
```c=64
page_addr = pte_val(*pte) & PAGE_MASK;
```
得`page_addr` = `0x8000000199b8f867` & `0xFFFFFFFFFFFFF000` = `0x8000000199b8f000`  
`page_addr` 為 **base address of the physical page frame**

```c=65
page_offset = vaddr & ~PAGE_MASK;
```
`page_offset` = `0x7ffe22a14794` & `0x0000000000000FFF` = `0x794`  
得到 **physical page frame的offset**

```c=66
paddr = page_addr | page_offset;
```
最後physical address = `0x8000000199b8f000` | `0x794` = `0x8000000199b8f794`
    
**<font size = 4>簡單來說，其實就只是需要先算出page frame address再和offset 相加而已，只不過是使用 bitwise`&` 及 `|` 來計算出結果</font>**
:::


:::warning
因為使用 `copy_from_user()`因此必須傳入pointer of pointer of `vaddr` ，  
所以即使 `my_get_physical_addresses(void *vaddr)`中的`void *vaddr`已經是pointer，  
但是在呼叫system calls時，`long result = syscall(450, &vaddr, &paddr);`  
需要傳送的參數是`&vaddr`(i.e. pointer of pointer of `vaddr`)
:::


## Add system call

**<font size = 5>1. Modified Makefile</font>**

修改 `kernel/Makefile`，增加 `project1.o`
```
obj-y     = fork.o exec_domain.o panic.o \
            cpu.o exit.o softirq.o resource.o \
            sysctl.o capability.o ptrace.o user.o \
            signal.o sys.o umh.o workqueue.o pid.o task_work.o \
            extable.o params.o \
            kthread.o sys_ni.o nsproxy.o \
            notifier.o ksysfs.o cred.o reboot.o \
            async.o range.o smpboot.o ucount.o regset.o \
            project1.o \
```
使得在編譯時也會編譯到`project1`這個檔案

**<font size = 5>2. Modified syscall Table</font>**

要新增自己的 system call，打開`arch/x86/entry/syscalls/syscall_64.tbl`
在第 374 行後面新增自己的 system call：
```
450     common  my_get_physical_addresses       sys_my_get_physical_addresses
```
這行有四個部分，每項之間由空白或 tab 隔開，它們代表的意義是：

* `450`
system call number，在使用系統呼叫時要使用這個數字
* `common`
支援的 ABI， 只能是 64、x32 或 common，分別表示「只支援 amd64」、「只支援 x32」或「都支援」
* `my_get_physical_addresses`
system call 的名字
* `sys_my_get_physical_addresses`
system call 對應的實作，kernel 中通常會用 sys 開頭來代表 system call 的實作

`syscall_64.tbl` 這個檔案會在編譯階段被讀取後轉為 header file 檔案位於: `arch/x86/include/generated/asm/syscalls_64.h`：  
![image](https://hackmd.io/_uploads/rJE4StogJl.png)


**<font size = 5>3. Modified `syscalls.h`</font>**

將 syscall 的原型添加進檔案 (`#endif` 之前)
路徑為: `include/linux/syscalls.h`  

![image](https://hackmd.io/_uploads/HyH4IFoeJg.png)

這定義了我們system call的prototype，`asmlinkage`代表我們的參數都可以在stack裡取用，
當 assembly code 呼叫 C function，並且是以 stack 方式傳參數時，在 C function 的 prototype 前面就要加上 `asmlinkage`



# <font color="#F7A004">Copy on Write</font>

* **<font size = 4>Copy on write:</font>** allows multiple processes to share the same physical memory until one intends to modify it.

![截圖 2024-10-25 下午3.35.54](https://hackmd.io/_uploads/SJli5p_eke.png)

可以看到程式執行時，parent process、child process中 `global_a` 的physical memory都是共用的，直到`global_a`被改動之後，os會分配新的physical memory 給改動的process，也因此驗證了system call 確實有正確呼叫



# <font color="#F7A004">Loader</font>

進入這章節前，先快速介紹Linux 中的Demand paging機制，可以對應到老師之前介紹的lazy allocation，不過lazy allocation相對廣義一些，demand paging 單純在memory 中使用

* __Demand Paging__:  pages of a process's memory are loaded into physical memory __only when they are actually needed__(ex: when the process tries to access them)

簡單來說，並不是一開始所有的virtual address都有對應到physical address，而是等到需要使用(access)時才載入到physical memory

因此，以<font color = "red">**process是否access the item**</font>作為區分，可以分為下列幾種情況:

## <font color = "green">case 1:</font> Array store in bss segment 
```c
// global variable
int a[2000000];    //uninitialized variable, store in bss segment
```
**執行結果:**  
![image](https://hackmd.io/_uploads/S136Q1Bekl.png)

可以看到，存放在 bss segment 的 array，
Load到memory中的只有到`a[1007]`，之後就沒有load 進memory



## <font color = "green">case 2:</font> array store in data segment
```c
// global variable
int a[2000000] = {1};    // initialized variable, store in Data segment
```
**執行結果:**  
![image](https://hackmd.io/_uploads/SkFP8kSekx.png)

可以看到，因為第一個element有被預設初始值，因此array `a`會預先載入幾個page至memory中，but only few page store in memory, 剩下尚未存取的需要透過page fault來載入至memory

**<font size = 5>補充:</font>** 
剛剛因為load至`a[15351]`，所以我想試看看預先存取`a[15352]` 產生page fault並將其load入physical memory，看看有甚麼結果
```c
a[15352] = 1;     // occur page fault, load to phy_mem
```
**執行結果:**  
![image](https://hackmd.io/_uploads/rJlIAySe1l.png)

可以看到 load 到`a[16375]`結束，而`a[16376]`尚未存取，
因此可得：
```
16375 - 15351 = 1024    
```

因為page size = 4KB，且一個int 4 bytes，而我們使用64位元架構，
因此page table entries size = 8 bytes(存兩個array element = 8 bytes)，因此：$$\dfrac{4KB}{8B} = \dfrac{2^{12}}{2^3} = 2^9 = 512$$
證明也是64位元架構page table entries 為512個

由此證明老師上課講解的內容


## <font color = "green">case 3:</font> loop through array

```c
// in local 
for(int i=0; i<2000000; i++)
{
    a[i] = 0;    //pre-accessing the array
}
```
**執行結果:**  
![image](https://hackmd.io/_uploads/B1CRwyBg1g.png)

In this particular case，不管是定義在Data segment or BSS segment，透過迴圈存取每個element，會造成page fault 並強迫load into memory，因此陣列中每個element 都有分配到各自的physical address


# <font color="#F7A004">Note</font>

## <font color = "#008000">BSS segment vs Data segment</font>
BSS segment 存放的資料為 **uninitialized global variable (initialized with 0)** 或是 **uninitialized static variable**，而存放在bss segment和data segment的差別可以從[case 1](##case1)及[case 2](##case2)看到，data segement中的資料會在程式載入時通常會**立即分配頁面**，因此分配到的記憶體更多


## <font color=" #008000">mm_struct</font>
**<font color = "#0000ff"><font size = 4>What is `mm_struct`?</font></font>**

task_struct 被稱為 process descriptor，因為其記錄了這個 process所有的context(ex: PID, scheduling info)，其中有一個被稱為 memory descriptor的結構 `mm_struct`，記錄了Linux視角下管理process address的資訊(ex: page tables)。  
![30528e172c325228bf23dec7772f0c73](https://hackmd.io/_uploads/SkgMiSY1Jg.png)  
圖源: [Linux源码解析-内存描述符（mm_struct）](https://blog.csdn.net/tiankong_/article/details/75676131)

因此 `struct mm_struct *mm = current->mm;` 指的是存取目前process的memory management 資訊 

By assigning `current->mm` to this pointer, now can access to the memory-related information (ex: page tables) for the process that is running the system call.


**<font color = "#0000ff"><font size = 4>What is `task_struct`?</font></font>**  

根據 [bootlin](https://elixir.bootlin.com/linux/v5.15.137/source/include/linux/sched.h#L721) `task_struct` is a key data structure in the Linux kernel that represents a process or thread. It holds all the information related to a process.   

其中比較重要的有:  

`pid_t pid`: The process ID.  
`pid_t tgid`: The thread group ID, which is the same as pid for the main thread of a process.  
`struct mm_struct *mm`: Pointer to the memory descriptor, which contains information about the process's memory mappings.  
`struct task_struct *parent`: Pointer to the parent process.  
`struct list_head children`: List head for tracking child processes.  
`struct list_head sibling`: List head for linking to sibling processes.  
`struct files_struct *files`: Pointer to the file descriptor table.  
`unsigned int flags`: Flags that represent various attributes and settings of the process.  


## <font color=" #008000">SYSCALL_DEFINE</font>

**<font size = 4>What is `SYSCALL_DEFINE2`?</font>**
根據 [bootlin](https://elixir.bootlin.com/linux/v5.15.137/source/include/linux/syscalls.h#L217)定義:

```c
#define SYSCALL_DEFINE1(name, ...) SYSCALL_DEFINEx(1, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE2(name, ...) SYSCALL_DEFINEx(2, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE3(name, ...) SYSCALL_DEFINEx(3, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE4(name, ...) SYSCALL_DEFINEx(4, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE5(name, ...) SYSCALL_DEFINEx(5, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE6(name, ...) SYSCALL_DEFINEx(6, _##name, __VA_ARGS__)

#define SYSCALL_DEFINE_MAXARGS	6
```

其中`SYSCALL_DEFINE1(name, ...)` 中的
* `1`表示system call 參數的個數，依此類推2、3、4、5、6 表示參數個數
* `name` 表示系統呼叫system call的名字

而後面的 `SYSCALL_DEFINEx(1, _##name, __VA_ARGS__)` 中的
* `_##name` 是一個預處理器拼接操作，會將 `_` 和 `name` 組合成一個標識符，  
例如，如果kernel中使用了 
```
SYSCALL_DEFINE1(my_get_physical_addresses, void *ptr)
```
則這個 Macro 會展開為：
```
asmlinkage long sys_my_get_physical_addresses(void *ptr);
```
* `__VA_ARGS__` 代表傳入的參數


## <font color = "#008000">What is `pud_pgtable(*pud)`?</font>

根據 [bootlin](https://elixir.bootlin.com/linux/v5.15.137/source/arch/arc/include/asm/pgtable-levels.h#L136)
```c
#define pud_pgtable(pud)	((pmd_t *)(pud_val(pud) & PAGE_MASK))
```
這個Macro的作用是回傳一個 `pmd_t *`的structure pointer，  
指向`pmd`（下一層）的page table base address。  

其中：

* `pud_val()`： 根據 [bootlin](https://elixir.bootlin.com/linux/v5.15.137/source/arch/arc/include/asm/page.h#L50) 
```c
#define pud_val(x)      	((x).pud)
```
`pud_t` 是一個 `struct`，`.pud` 是其內部的成員，用來存取這個page的實際值
* `PAGE_MASK` 一樣是`0xFFFFFFFFFFFFF000`因為page size 為 4KB

因此`pud_val(pud)`去`pud` page table中存值並且和`PAGE_MASK`做`&` 取得`pmd` page table base address ，並且回傳以`pmd_t *`的struct  

以此類推到`p4d_pgtable()`, `pmd_page_vaddr()`


## <font color= "#008000">How to get `pgd_index`?</font>
根據 [bootlin](https://elixir.bootlin.com/linux/v5.15.137/source/include/linux/pgtable.h#L85) 
```c
#ifndef pgd_index
/* Must be a compile-time constant, so implement it as a macro */
#define pgd_index(a)        (((a) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1))
#endif
```
其中
* `#define pgd_index(a)`：定義 `pgd_index` Macro，接受一個參數 `a`，代表一個virtual address
* `(((a) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1))`：這是用來計算 `a` 在 PGD 中的index的表達式。

**<font size = 4>舉例：</font>**  
在x86_64架構的 `PGDIR_SHIFT` 為 39 (48 - 9)，
且`PTRS_PER_PGD` 為 512，那麼 `pgd_index(a)` 的操作流程如下：

* 將虛擬地址 `a` 右移 39 位，提取出對應 PGD 的高位部分
* 將結果與 `511`（`PTRS_PER_PGD - 1`）做 bitwise `&`，確保index在有效範圍內

得到的結果即為 virtual address `a` 的 `pgd_index`，
並且可以依此類推到 `p4d_offset`、`pud_index`、`pmd_index`的計算方法

# <font color="#F7A004">Problems</font>
Project 中遇到的問題與解決方法

## <font color="#008000">`copy_from_user` pointer 問題</font>

在copy_from_user()中


## <font color="#008000">system call 未成功呼叫</font>

![image](https://hackmd.io/_uploads/BJwlBdkgyg.png)

可以看到error message: `syscall failed: Function not implemented` 

:::info
1. system call function code:
`SYSCALL_DEFINE1(my_get_physical_addresses, void *)`
![image](https://hackmd.io/_uploads/ByJPHKJgyx.png)

2. system call table
`linux-5.15.137/arch/x86/entry/syscalls/syscall_64.tbl` 中 
![image](https://hackmd.io/_uploads/HkACLKkxkg.png)


<font size = 4>竟然是my_get_physical_addresseses 和my_get_physical_addresses 少加了es</font>
:::

照理來說，若是兩者沒有match會使得make 失敗，也是我後來發現問題的原因。

**<font size=5>Solution:</font>**
將兩者名稱改至相同，互相match情況下先`make mrproper`再`make`，重新編譯kernel，就可以正常呼叫了

# <font color="#F7A004">Referenced</font>

* [linux系统中copy_to_user()函数和copy_from_user()函数的用法](https://blog.csdn.net/bhniunan/article/details/104088763)
* [where is base register of page table?](https://www.csie.ntu.edu.tw/~wcchen/asm98/asm/proj/b85506061/chap2/paging.html)
* [定址方式](https://www.csie.ntu.edu.tw/~wcchen/asm98/asm/proj/b85506061/chap2/overview.html)
* [實作一個回傳物理位址的系統呼叫](https://hackmd.io/@Mes/make_phy_addr_syscall#%E4%BF%AE%E6%94%B9-syscall_64tbl)
* [add a system call to kernel (v5.15.137)](https://hackmd.io/aist49C9R46-vaBIlP3LDA?view#add-a-system-call-to-kernel-v515137)
* [Kernel 的替換 & syscall 的添加](https://satin-eyebrow-f76.notion.site/Kernel-syscall-3ec38210bb1f4d289850c549def29f9f)
* [關於Linux尋址及page table的一些細節](https://www.cnblogs.com/QiQi-Robotics/p/15630380.html)
* [SYSCALL_DEFINEx宏源码解析](https://blog.csdn.net/qq_41345173/article/details/104071618)
