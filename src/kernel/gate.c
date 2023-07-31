#include <onix/interrupt.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/syscall.h>
#include <onix/task.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 系统调用函数表
#define SYSCALL_SIZE 64

handler_t syscall_table[SYSCALL_SIZE];

void syscall_check(u32 nr)
{
    if (nr >= SYSCALL_SIZE)
    {
        panic("syscall nr error!!!");
    }
}

task_t *task = NULL;

static u32 sys_test()
{
    // LOGK("syscall test...\n");

    if (!task)
    {

        task = running_task();
        // LOGK("block task 0x%p \n", task);
        task_block(task, NULL, TASK_BLOCKED);
    }
    else
    {
        task_unblock(task);
        // LOGK("unblock task 0x%p \n", task);
        task = NULL;
    }
    return 255;
}

extern void task_yield();

//
static void sys_default()
{
    // 系统调用没有实现
    panic("syscall not implemented!!!");
}

//
void syscall_init()
{
    for (size_t i = 0; i < SYSCALL_SIZE; i++)
    {
        // 首先把系统调用表设置为默认的
        syscall_table[i] = sys_default;
    }

    // 0 号测试的系统调用
    // syscall_table[0] = sys_test; // ^

    syscall_table[SYS_NR_TEST] = sys_test;
    syscall_table[SYS_NR_SLEEP] = task_sleep;
    syscall_table[SYS_NR_YIELD] = task_yield;
}
