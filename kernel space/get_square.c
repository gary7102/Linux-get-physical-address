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
