#include <stdio.h>
#include <sys/syscall.h> /* Definition of SYS_* constants */
#include <unistd.h>




                void *my_get_physical_addresses(void *vaddr)
                {
                    unsigned long paddr;
                    long result = syscall(450, &vaddr, &paddr); // Call the syscall with vaddr

                    if (result == -1) { // Check for errors
                         perror("syscall failed");
                         return NULL; // Handle error appropriately
                    }
                    return (void *)paddr; // Return the physical address
                }


                 int a[2000000]={1};   //store int Data segment

                 void  main()
                 {
                   int      loc_a;
                   void     *phy_add;


                  a[15352] = 1;
                  // a[16376]=1;
                   for(int i=0; i<2000000; i++){
                        phy_add = my_get_physical_addresses(&a[i]);
                        if(phy_add == NULL){
                                printf("stop in a[%d], logical address = [%p]\n", i, &a[i]);
                                break;
                        }
                        printf("a[%d] logical address: [%p], physical address: [%p]\n", i, &a[i], phy_add);
                   }
                 }
