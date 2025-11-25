#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define HIGH 1
#define LOW 0

static struct timer_list timer;
int flag = 0;
int led[4] = {23, 24, 25, 1};
int sw[4] = {4, 17, 27, 22};
int current_led = 0; //SW[1],SW[2]에서 사용 예정
int current_mod = 0; //SW[2]에서 사용
int led_state[4] = {0, 0, 0, 0};

irqreturn_t irq_handler(int irq, void *dev_id)
{
    printk(KERN_INFO "Debug %d\n", irq);
    switch (irq) {
    case 60:
        if (current_mod == 3) {
            mod3(0);
        } else {
            printk(KERN_INFO "sw1 interrupt ocurred!\n");
            current_mod = 1;
            del_timer(&timer); //기존 실행중인 타이머를 종료
            timer_setup(&timer, timer_cb, 0);
            timer.expires = jiffies + HZ * 2;
            add_timer(&timer);
        }
        break;
    case 61:
        if (current_mod == 3)
            mod3(1);
        else {
            printk(KERN_INFO "sw2 interrupt ocurred!\n");
            current_mod = 2;
            del_timer(&timer);
            current_led = 0;
            timer_setup(&timer, timer_cb2, 0);
            timer.expires = jiffies + HZ * 2;
            add_timer(&timer);
        }
        break;
    case 62:
        if (current_mod == 3)
            mod3(2);
        else {
            printk(KERN_INFO "sw3 interrupt ocurred!\n");
            del_timer(&timer);
            mod3(3);
        }
        break;
    case 63: { // 블록 처리
        int i;
        printk(KERN_INFO "sw4 interrupt ocurred!\n");
        del_timer(&timer);
        current_mod = 0; // 오타 수정: current_mode -> current_mod
        for (i = 0; i < 4; i++) {
            gpio_direction_output(led[i], LOW); // 오타 수정: gpio_direction_ouput
            led_state[i] = 0;
        }
        break;
    }
    }
    return IRQ_HANDLED; /* 0 대신 IRQ_HANDLED 사용 권장 */
}

static int __init module_init(void) // __init 추가 권장
{
    int ret, res, i;
    // switch gpio 요청 부분
    printk(KERN_INFO "switch_interrupt_init! (start)\n");

    for (i = 0; i < 4; i++) {
        res = gpio_request(sw[i], "sw");
        // request_irq는 sw 요청 성공 여부와 관계없이 시도해야 함
        res = request_irq(gpio_to_irq(sw[i]), (irq_handler_t)irq_handler, IRQF_TRIGGER_RISING, "IRQ", (void *)(irq_handler));
        if (res < 0)
            printk(KERN_INFO "Switch request_irq failed for sw[%d]! (IRQ %d)\n", i, gpio_to_irq(sw[i]));
    }

    // led gpio요청 부분
    for (i = 0; i < 4; i++) {
        ret = gpio_request(led[i], "LED");
        if (ret < 0)
            printk(KERN_INFO "LED gpio_request failed for led[%d]!\n", i);
        gpio_direction_output(led[i], LOW); // LOW = 0
    }
    return 0;
}

static void __exit module_exit(void) // __exit 추가 권장
{
    int i;
    del_timer(&timer); // 타이머를 먼저 해제

    for (i = 0; i < 4; i++) {
        free_irq(gpio_to_irq(sw[i]), (void *)(irq_handler));
        gpio_free(sw[i]);
        gpio_free(led[i]);
    }
    printk(KERN_INFO "Module exit and resources freed.\n");
}

static void timer_cb(struct timer_list *timer)
{
    int ret, i;
    if (flag == 0) {
        printk(KERN_INFO "timer callback: Turning all LEDs ON.\n");
        for (i = 0; i < 4; i++) {
            ret = gpio_direction_output(led[i], HIGH);
        }
        flag = 1;
    } else {
        printk(KERN_INFO "timer callback: Turning all LEDs OFF.\n");
        for (i = 0; i < 4; i++) {
            ret = gpio_direction_output(led[i], LOW); // 모든 LED 끄기
        }
        flag = 0; // 상태를 꺼짐(0)으로 변경
    }
    add_timer(timer, jiffies + HZ * 2); // add_timer 대신 mod_timer 사용 권장
}

static void timer_cb2(struct timer_list *timer)
{
    int i;
    for (i = 0; i < 4; i++) { //일단 다 끄고
        gpio_direction_output(led[i], LOW);
    }
    gpio_direction_output(led[current_led], HIGH);
    current_led = (current_led + 1) % 4;
    add_timer(timer, jiffies + HZ * 2); // add_timer 대신 mod_timer 사용 권장
}

static void mod3(int n)
{
    int i;
    if (n == 3) {
        // 모든 LED 끄기
        for (i = 0; i < 4; i++) {
            gpio_direction_output(led[i], LOW);
            led_state[i] = 0; // 상태 배열도 갱신
        }
    } else {
        if (led_state[n] == 0) {
            gpio_direction_output(led[n], HIGH);
            led_state[n] = 1;
        } else {
            gpio_direction_output(led[n], LOW);
            led_state[n] = 0;
        }
    }
}

module_init(module_init);
module_exit(module_exit);
MODULE_LICENSE("GPL");
