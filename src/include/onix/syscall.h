#ifndef ONIX_SYSCALL_H
#define ONIX_SYSCALL_H

#include <onix/types.h>

// 系统调用号
typedef enum syscall_t
{
    SYS_NR_TEST,
    SYS_NR_SLEEP,
    SYS_NR_YIELD,
} syscall_t;

u32 test();
void yield();
void sleep(u32 ms);

#endif // ! ONIX_SYSCALL_H
