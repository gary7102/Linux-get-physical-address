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
    printf("Virtual  addr. of arg a = %p\n", &a);
    printf("Physical addr. of arg a = %p\n", my_get_physical_addresses(&a));
}
