#include <onix/onix.h>
#include <onix/types.h>
#include <onix/io.h>
#include <onix/string.h>
#include <onix/console.h>
#include <onix/stdarg.h>
#include <onix/printk.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/global.h>
#include <onix/task.h>
#include <onix/interrupt.h>
#include <onix/stdlib.h>
#include <onix/rtc.h>
#include <onix/memory.h>
#include <onix/bitmap.h>
#include <onix/list.h>

extern void console_init();
extern void gdt_init();
extern void interrupt_init();
extern void clock_init();
extern void hang();
extern void time_init();
extern void rtc_init();
extern void memory_map_init();
extern void mapping_init();
extern void memory_test();
extern void bitmap_test_s();
extern void memory_test_44();
extern void task_init();
extern void syscall_init();

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 参数传递，从右向左压入栈中
// 所以先读取左边参数
void test_arg(int cnt, ...)
{
    va_list args;

    // args获取了参数的列表
    va_start(args, cnt);

    int arg;
    while (cnt--)
    {
        arg = va_arg(args, int);
    }
    va_end(args);
}

void test_printk()
{
    int cnt = 30;
    while (cnt--)
    {
        /* code */
        printk("hello world sss  %#010x\n", cnt);
    }
}

void test_assert()
{
    assert(3 < 5);
    assert(3 > 5);
}

void test_panic()
{
    panic("..Out Of Memory");
}

void test_debug()
{
    BMB;
    DEBUGK("debug onix!!!\n");
}

void test_interrupt()
{
    asm volatile(
        "sti\n");

    u32 counter = 0;
    while (true)
    {
        /* code */
        DEBUGK("looping in kernel init %d...\n", counter++);
        delay(100000000);
    }
}

void test_42()
{

    BMB;
    memory_test();
}

void intr_test()
{
    bool intr = interrupt_disable();

    // do something

    set_interrupt_state(intr);
}

void test_intr_45()
{
    bool intr = interrupt_disable();
    set_interrupt_state(true);

    LOGK("%d\n", intr);
    LOGK("%d\n", get_interrupt_state());

    BMB;

    intr = interrupt_disable();

    BMB;
    set_interrupt_state(true);

    LOGK("%d\n", intr);
    LOGK("%d\n", get_interrupt_state());
}

void test_list()
{
    u32 count = 3;
    list_t holder;
    list_t *list = &holder;
    list_init(list);
    list_node_t *node;

    while (count--)
    {
        // 分配页表 ，返回的是一个32位的地址就可以
        //  抽象为 链表节点
        node = (list_node_t *)alloc_kpage(1);

        // 只要把地址push到链表中即可
        list_push(list, node);
    }

    while (!list_empty(list))
    {
        node = list_pop(list);
        free_kpage((u32)node, 1);
    }

    count = 3;
    while (count--)
    {
        /* code */
        node = (list_node_t *)alloc_kpage(1);
        list_pushback(list, node);
    }

    LOGK("list size %d\n", list_size(list));

    while (!list_empty(list))
    {
        /* code */
        node = list_popback(list);
        free_kpage((u32)node, 1);
    }

    node = (list_node_t *)alloc_kpage(1);
    list_pushback(list, node);

    LOGK("search node 0x%p --> %d\n", node, list_search(list, node));
    LOGK("search node 0x%p --> %d\n", 0, list_search(list, 0));

    list_remove(node);
    free_kpage((u32)node, 1);
}

void kernel_init()
{
    memory_map_init();
    mapping_init();
    // console_init();
    // gdt_init();
    interrupt_init();
    // task_init();
    clock_init();
    // time_init();
    // rtc_init();
    // set_alarm(2);

    // bitmap_test_s();
    // memory_test_44();

    task_init();
    syscall_init();

    // test_list();

    set_interrupt_state(true);
    // asm volatile("sti");
    // hang();
}