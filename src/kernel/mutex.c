#include <onix/mutex.h>
#include <onix/task.h>
#include <onix/interrupt.h>
#include <onix/assert.h>

void mutex_init(mutex_t *mutex)
{
    mutex->value = false; // 初始化时没有人持有
    list_init(&mutex->waiters);
}

// 尝试持有互斥量
void mutex_lock(mutex_t *mutex)
{
    // 关闭中断
    bool intr = interrupt_disable();

    task_t *current = running_task();

    while (mutex->value == true)
    {
        // 当value 为 true，标识已经被别人持有
        // 将当前任务加入互斥量等待队列中。
        task_block(current, &mutex->waiters, TASK_BLOCKED);
    }

    // 无人持有
    assert(mutex->value == false);

    // 持有
    mutex->value++;
    assert(mutex->value == true);

    // 持有之前的中断状态
    set_interrupt_state(intr);
}

// 释放量
void mutex_unlock(mutex_t *mutex)
{
    // 关闭中断，保证原子操作
    bool intr = interrupt_disable();

    // 取消持有
    mutex->value--;

    assert(mutex->value == false);

    // 如果等待队列，则恢复执行
    if (!list_empty(&mutex->waiters))
    {
        task_t *task = element_entry(task_t, node, mutex->waiters.tail.prev);

        assert(task->magic == ONIX_MAGIC);

        task_unblock(task);

        // 保证新进程获得互斥量，不然可能饿死
        task_yield();
    }

    // 恢复之前的状态
    set_interrupt_state(intr);
}