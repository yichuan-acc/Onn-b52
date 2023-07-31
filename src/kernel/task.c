#include <onix/task.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/memory.h>
#include <onix/assert.h>
#include <onix/interrupt.h>
#include <onix/string.h>
#include <onix/bitmap.h>
#include <onix/syscall.h>
#include <onix/list.h>

#define NR_TASKS 64

extern u32 volatile jiffies; // 时间片的数量
extern u32 jiffy;            // 时间片的毫秒值

extern bitmap_t kernel_map;
extern void task_switch(task_t *next);

static task_t *task_table[NR_TASKS]; // 任务表，所有任务的数组 task_table
static list_t block_list;            // 任务默认阻塞链表
static list_t sleep_list;            // 睡眠队列

static task_t *idle_task;

// 从 task_table 里获得一个空闲的任务
static task_t *get_free_task()
{
    for (size_t i = 0; i < NR_TASKS; i++)
    {
        if (task_table[i] == NULL)
        {
            task_table[i] = (task_t *)alloc_kpage(1); // todo free_kpage
            return task_table[i];                     // 返回一个任务
        }
    }
    panic("No more tasks");
}

// 从任务数组中查找  某种状态  的任务，自己除外
static task_t *task_search(task_state_t state)
{
    // 在不可中断状态下，原子操作
    assert(!get_interrupt_state());
    task_t *task = NULL;
    task_t *current = running_task();

    for (size_t i = 0; i < NR_TASKS; i++)
    {
        task_t *ptr = task_table[i];
        if (ptr == NULL)
            continue;

        if (ptr->state != state)
            continue;
        if (current == ptr)
            continue;

        // task 为空 || task时间片小于ptr的(优先级更高) || 选择最晚执行的那个
        if (task == NULL || task->ticks < ptr->ticks || ptr->jiffies < task->jiffies)
            task = ptr;
    }

    // 如果task为NULL 状态以及准备好
    if (task == NULL && state == TASK_READY)
    {
        task = idle_task;
    }

    return task;
}

// 任务阻塞
void task_block(task_t *task, list_t *blist, task_state_t state)
{
    assert(!get_interrupt_state());
    assert(task->node.next == NULL);
    assert(task->node.prev == NULL);

    if (blist == NULL)
    {
        blist = &block_list;
    }

    list_push(blist, &task->node);

    assert(state != TASK_READY && state != TASK_RUNNING);

    task->state = state;

    task_t *current = running_task();
    if (current == task)
    {
        schedule();
    }
}

// 解除任务阻塞
void task_unblock(task_t *task)
{
    assert(!get_interrupt_state());

    // 移除task
    list_remove(&task->node);

    assert(task->node.next == NULL);
    assert(task->node.prev == NULL);

    task->state = TASK_READY;
}

void task_sleep(u32 ms)
{

    assert(!get_interrupt_state()); // 不可中断

    u32 ticks = ticks > 0 ? ticks : 1; // 至少休眠一个时间片

    // 记录目标全局时间片，在那个时刻需要唤醒任务
    task_t *current = running_task();
    current->ticks = jiffies + ticks;

    // 从睡眠链表找到当前任务唤醒的时间点更晚的任务，进行插入排序
    list_t *list = &sleep_list;
    list_node_t *anchor = &list->tail;

    for (list_node_t *ptr = list->head.next; ptr != &list->tail; ptr = ptr->next)
    {
        task_t *task = element_entry(task_t, node, ptr);

        if (task->ticks > current->ticks)
        {
            anchor = ptr;
            break;
        }
    }

    assert(current->node.next == NULL);
    assert(current->node.prev == NULL);

    // 插入链表
    list_insert_before(anchor, &current->node);

    // 阻塞状态是睡眠
    current->state = TASK_SLEEPING;

    // 调度执行其他任务
    schedule();
}

void task_wakeup()
{

    assert(!get_interrupt_state());

    list_t *list = &sleep_list;
    for (list_node_t *ptr = list->head.next; ptr != &list->tail;)
    {
        task_t *task = element_entry(task_t, node, ptr);

        if (task->ticks > jiffies)
        {
            break;
        }

        // unblock
        ptr = ptr->next;

        task->ticks = 0;
        task_unblock(task);
    }
}

// 把PCB放到内核栈的开头
task_t *running_task()
{
    // 获取当前任务
    asm volatile(
        "movl %esp, %eax\n"
        "andl $0xfffff000, %eax\n");
}

void task_yield()
{
    schedule();
}
// #define PAGE_SIZE 0X1000

// task_t *a = (task_t *)0x1000;
// task_t *b = (task_t *)0x2000;

// extern void task_switch(task_t *next);

void schedule()
{
    // 不可中断，
    assert(!get_interrupt_state());

    task_t *current = running_task();

    // 数组中找到一个就绪状态的任务
    task_t *next = task_search(TASK_READY);

    assert(next != NULL);
    assert(next->magic == ONIX_MAGIC);

    // 当前状态为执行，就变成就绪
    if (current->state == TASK_RUNNING)
    {
        current->state = TASK_READY;
    }

    //
    if (!current->ticks)
    {
        current->ticks = current->priority;
    }

    // 下一个进程开始执行
    next->state = TASK_RUNNING;

    // 相等就不需要切换
    if (next == current)
        return;

    // 任务切换
    task_switch(next);
}

static task_t *task_create(target_t target, const char *name, u32 priority, u32 uid)
{
    // 得到一页内存
    task_t *task = get_free_task();
    memset(task, 0, PAGE_SIZE);

    u32 stack = (u32)task + PAGE_SIZE;

    stack -= sizeof(task_frame_t);
    task_frame_t *frame = (task_frame_t *)stack;
    frame->ebx = 0x11111111;
    frame->esi = 0x22222222;
    frame->edi = 0x33333333;
    frame->ebp = 0x44444444;
    frame->eip = (void *)target;

    strcpy((char *)task->name, name);

    task->stack = (u32 *)stack;
    task->priority = priority;
    task->ticks = task->priority;
    task->jiffies = 0;
    task->state = TASK_READY;
    task->uid = uid;
    task->vmap = &kernel_map;
    task->pde = KERNEL_PAGE_DIR; // page directory entry
    task->magic = ONIX_MAGIC;

    BMB;
    return task;
}

//
static void task_setup()
{
    task_t *task = running_task();
    task->magic = ONIX_MAGIC;
    task->ticks = 1; // 处理处 task->ticks-- 刚好调度

    memset(task_table, 0, sizeof(task_table));
}

// u32 _ofp thread_a()

// u32 thread_a()
// {
//     // BMB;
//     // 中断返回后，中断关闭
//     // asm volatile("sti\n");
//     set_interrupt_state(true);

//     while (true)
//     {
//         printk("A");
//         // schedule();
//         // yield();
//         test();
//     }
// }

// 添加外部声明？
extern void idle_thread();
extern void init_thread();
extern void test_thread();

void task_init()
{
    list_init(&block_list);
    list_init(&sleep_list);

    task_setup();

    // 内核用户的3个线程
    idle_task = task_create(idle_thread, "idle", 1, KERNEL_USER);
    // task_create(thread_a, "a", 5, KERNEL_USER);
    task_create(init_thread, "init", 5, NORMAL_USER);
    task_create(test_thread, "test", 5, KERNEL_USER);
}
