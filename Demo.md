# 2024 Linux project 1 demo 問題
```
1. Nx bit、privilege權限... 在哪一層page table使用到?
2. 使用 virtual address的優缺點 
3. virtual address 和physical address的差異?
4. page frame是甚麼?
5. system call 是甚麼?
6. copy_from_user()和copy_to_user()是甚麼? 怎麼運作的?
7. page fault 甚麼時候會發生
8. process & thread 差異

1. fork() 再kernel裡是用甚麼system call
2. process & thread 差異
3. page frame 是什麼
4. mm_struct在哪裡?marco是甚麼?current->mm
5. make 三條組合包 make & make modules_install & make install
6. page table 在哪?
7. MMU在做什麼?
8. 
```

# 甚麼時候會用到system call
1. 檔案操作
任何涉及檔案的操作，例如讀取、寫入、打開、關閉文件等，都需要通過 system call 來進行。這是因為文件系統是由內核管理的，只有內核有權直接與硬碟等存儲設備交互。  
例子：`open()`, `read()`, `write()`, `close()`
2. process control
創建新process、終止process、等待chile process完成等操作，都是privilege操作，必須通過 system call 請求內核完成。  
例子：`fork()`, `execve()`, `exit()`, `wait()`

# fork 的marco是甚麼
fork 本身並不是直接用Macro來定義的，而是通過system call 的方式來實現。fork() 是一個system call，用於創建一個new process，該process是當前進程的copy

<font size = 4>**kernel 中的fork**</font>
```
SYSCALL_DEFINE0(fork) {
    // fork 的內部實現
    return do_fork(SIGCHLD, current->mm, current->files, current->fs);
}
```
`do_fork()` function的作用
1. 分配 `task_struct`：為新進程分配一個 `task_struct` 結構體，這是kernel用來表示進程的數據結構。這個結構體包含了進程的 PID、狀態、記憶體資訊等。
2. 複製進程資源：根據 clone_flags 的設定，複製父進程的資源（例如記憶體空間、打開的文件描述符、訊號處理器等）。
3. 設置新進程的屬性：根據 do_fork 的參數，設置新進程的 TID、堆疊起始地址等。
4. 加入進程表：將新創建的進程加入內核的進程表，這樣內核就能管理這個新進程。
5. 返回 PID：將新進程的 PID 返回給父進程，並在子進程中返回 0。

# make localmodconfig 做了甚麼

`make localmodconfig` 的主要功能是掃描當前運行系統中的已加載模組(loadable module)，並根據這些模組生成一個內核配置文件 `config`，僅啟用與當前硬體和設備相關的模組。這樣生成的配置文件可以用來編譯出一個精簡的內核。

# make 做了甚麼
`make` 指令會根據 `.config` 文件的配置編譯 Linux Kernel。make 主要包含:
* 編譯 Kernel 本體：將核心代碼編譯為二進位文件，包括 vmlinux 文件（未壓縮的核心文件）和 bzImage 文件（壓縮的核心映像文件）。
* 編譯核心模組：根據`.config` 的設定，編譯出kernel modules

# make modules_install 做了甚麼

所有編譯好的kernel module 安裝到系統的模組目錄中，像是`/lib/modules/<kernel-version>/`

# make install 做了甚麼

安裝內核映像、System.map 和配置文件到 /boot，並更新引導加載器的配置，使系統可以引導新內核。

# page frame 是甚麼
* Page：指的是virtual memory中的一個固定大小的單位。OS將process的虛擬記憶體劃分為多個頁，並通過頁表管理這些頁的映射關係。
* Page Frame：指的是**physical memory**中的一個固定大小的單位，與頁的大小一致。頁框是實際分配在物理內存中的存儲空間，用來存儲頁的內容。