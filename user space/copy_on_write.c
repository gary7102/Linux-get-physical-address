#include <stdio.h>
#include <sys/syscall.h> /* Definition of SYS_* constants */
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>



                void *my_get_physical_addresses(void *vaddr)
                {
                    unsigned long paddr;
                    long syscall_result = syscall(450, &vaddr, &paddr); // Call the syscall with vaddr

                    if (syscall_result == -1) { // Check for errors
                         perror("syscall failed");
                         return NULL; // Handle error appropriately
                    }
                    return (void *)paddr; // Return the physical address
                }

                 int global_a=123;  //global variable

                 void print_dash(void)
                 {
                    printf("======================================================================================================\n");
                 }


                 int main()
                 {
                   int      loc_a;
                               int          status;
                   void     *parent_use, *child_use;

                   printf("===========================Before Fork==================================\n");
                   parent_use=my_get_physical_addresses(&global_a);
                   printf("pid=%d: global variable global_a:\n", getpid());
                   printf("Offest of logical address:[%p]   Physical address:[%p]\n", &global_a,parent_use);
                   printf("========================================================================\n");


                   if(fork())
                   { /*parent code*/
                     printf("vvvvvvvvvvvvvvvvvvvvvvvvvv  After Fork by parent  vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n");
                     parent_use=my_get_physical_addresses(&global_a);
                     printf("pid=%d: global variable global_a:\n", getpid());
                     printf("******* Offset of logical address:[%p]   Physical address:[%p]\n", &global_a,parent_use);
                     printf("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n");
                     wait(&status);
                   }
                   else
                   { /*child code*/

                     printf("llllllllllllllllllllllllll  After Fork by child  llllllllllllllllllllllllllllllll\n");
                     child_use=my_get_physical_addresses(&global_a);
                     printf("******* pid=%d: global variable global_a:\n", getpid());
                     printf("******* Offset of logical address:[%p]   Physical address:[%p]\n", &global_a, child_use);
                     printf("llllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllllll\n");
                     printf("____________________________________________________________________________\n");

                     /*----------------------- trigger CoW (Copy on Write) -----------------------------------*/
                     global_a=789;

                     printf("iiiiiiiiiiiiiiiiiiiiiiiiii  Test copy on write in child  iiiiiiiiiiiiiiiiiiiiiiii\n");
                     child_use=my_get_physical_addresses(&global_a);
                     printf("******* pid=%d: global variable global_a:\n", getpid());
                     printf("******* Offset of logical address:[%p]   Physical address:[%p]\n", &global_a, child_use);
                     printf("iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii\n");
                     printf("____________________________________________________________________________\n");
                     sleep(1);
                   }
                 }
