/* bigprog.c
 * Operate on a big array. The array is huge but we only operate on a part of
 * it. This is designed to test demand paging, when swapping is not implemented
 * yet */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

/* 20 MB array */
#define SIZE ((20 * 1024 * 1024)/sizeof(u_int32_t))
/* Create a struct to group the array so that we can fill the data in the right location for magic_num*/
struct big_array{
        u_int32_t bigarray[SIZE];
};
struct big_struct {
struct big_array bigarray1;
u_int32_t magic_num;
struct big_array bigarray2;
};

static struct big_struct big = {{0}, 344, {0}};

int
main()
{
        if (big.magic_num == 344) {
                printf("Passed bigprog test.\n");
                exit(0);
        } else {
                printf("bigprog test failed\n");
                exit(1);
        }
}
