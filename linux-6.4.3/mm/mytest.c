#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE 7 // 8时第8个会卡很久
#define MEMORY_SIZE 100 * 1024 * 1024 // 100MB

int main() {
    char *array[ARRAY_SIZE];

    printf("Before execution:\n");
    system("free");
    system("cat /proc/meminfo | grep Zswap");
    printf("--------------------------------------------------------\n");

    // 为每个元素分配100MB的内存
    for (int i = 0; i < ARRAY_SIZE; ++i) {
        array[i] = (char *)malloc(MEMORY_SIZE);
        if (array[i] == NULL) {
            printf("Memory allocation failed for index %d\n", i);
            return 1; 
        }

        memset(array[i], 'a', MEMORY_SIZE);
        printf("[%d]-------Allocate 100MB more memory successfully.\n", i + 1);
        system("free");
        system("cat /proc/meminfo | grep Zswap");
        printf("--------------------------------------------------------\n");
    }

    
    printf("Memory successfully allocated for each element in the array.\n");
    printf("----------------------------------------------------------------------------\n");

    printf("Now you can input 0-6 and 0-100000000 to access the element of array, [0, 0] to exit.\n");
    printf("--------------------------------------------------------\n");
    int row, col;
    while(1) {
        printf("input row and colomn:\n");
        scanf("%d %d", &row, &col);
        if (row == 0 && col == 0) {
            break;
        }
        printf("arr[%d, %d] is %c\n", row, col, array[row][col]);
        system("free");
        system("cat /proc/meminfo | grep Zswap");
        printf("--------------------------------------------------------\n");
    }

    for (int i = 0; i < ARRAY_SIZE; ++i) {
        free(array[i]);
        printf("[%d]-------Free 100MB memory successfully.\n", i + 1);
        system("free");
        system("cat /proc/meminfo | grep Zswap");
        printf("--------------------------------------------------------\n");
    }

    printf("Memory successfully freed.\n");
    return 0;
}