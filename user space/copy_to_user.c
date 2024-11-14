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
    } else {
        printf("Input: %d, Output (Square): %d\n", input, output);
    }

    return 0;
}
