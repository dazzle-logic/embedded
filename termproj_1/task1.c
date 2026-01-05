#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/delay.h>

int led[4] = {23, 24, 25, 1};
int sw[4] = {4, 17, 27, 22};
unsigned int sw_irq[4];

#define MODE_RESET 0
#define MODE_1 1
#define MODE_2 2
#define MODE_3 3
#define HIGH 1
#define LOW 0

int current_mode = MODE_RESET;
int flag = 0;
int current_led = 0;
struct timer_list mode_1_timer;
struct timer_list mode_2_timer;

static void start_mode_1(void);
static void start_mode_2(void);
static void start_mode_3(int sw_index);
static void start_mode_reset(void);
static void mode_1_timer_cb(struct timer_list *timer);
static void mode_2_timer_cb(struct timer_list *timer);


static int led_switch_gpio_request(void)
{
    int i, ret = 0;
    for (i = 0; i < 4; i++) {
        ret = gpio_request(led[i], "LED");
        if (ret < 0) {
            printk(KERN_ERR "LED GPIO %d request failed!\n", led[i]);
            goto fail_led;
        }
        gpio_direction_output(led[i], 0);
    }

    for (i = 0; i < 4; i++) {
        ret = gpio_request(sw[i], "SW");
        if (ret < 0) {
            printk(KERN_ERR "SW GPIO %d request failed!\n", sw[i]);
            goto fail_sw;
        }
        gpio_direction_input(sw[i]);
        sw_irq[i] = gpio_to_irq(sw[i]);
    }
    printk(KERN_INFO "GPIO setup done.\n");
    return 0;

fail_sw:
    for (i = i - 1; i >= 0; i--) {
        gpio_free(sw[i]);
    }
fail_led:
    for (i = 3; i >= 0; i--) {
        gpio_free(led[i]);
    }
    return ret;
}

static void led_switch_gpio_free(void)
{
    int i;
    for (i = 0; i < 4; i++) {
        gpio_free(led[i]);
        gpio_free(sw[i]);
    }
    printk(KERN_INFO "GPIO free done.\n");
}

irqreturn_t irq_handler(int irq, void *dev_id)
{
    int i;
    int sw_index = -1;
    printk(KERN_INFO "Debug %d\n", irq);

    for (i = 0; i < 4; i++) {
        if (irq == sw_irq[i]) {
            sw_index = i;
            break;
        }
    }

    if (sw_index == 3) {
        printk(KERN_INFO "sw4 interrupt ocurred! (RESET)\n");
        start_mode_reset();
    } else if (current_mode == MODE_3) {
        if (sw_index >= 0 && sw_index <= 2) {
            start_mode_3(sw_index);
        }
    } else {
        start_mode_reset();

        switch (sw_index) {
        case 0:
            printk(KERN_INFO "sw1 interrupt ocurred! (MODE 1)\n");
            current_mode = MODE_1;
            start_mode_1();
            break;

        case 1:
            printk(KERN_INFO "sw2 interrupt ocurred! (MODE 2)\n");
            current_mode = MODE_2;
            start_mode_2();
            break;

        case 2:
            printk(KERN_INFO "sw3 interrupt ocurred! (MODE 3)\n");
            current_mode = MODE_3;
            break;

        default:
            break;
        }
    }
    return IRQ_HANDLED;
}


static int __init switch_module_exam1_init(void)
{
    int res, i;
    printk(KERN_INFO "switch_module_exam1_init start!\n");
    res = led_switch_gpio_request();
    if (res < 0) {
        printk(KERN_ERR "GPIO request failed, module exit.\n");
        return res;
    }

    for (i = 0; i < 4; i++) {
        res = request_irq(sw_irq[i], (irq_handler_t)irq_handler, IRQF_TRIGGER_RISING, "IRQ", (void *)(irq_handler));
        if (res < 0)
            printk(KERN_ERR "request_irq failed for SW[%d] (IRQ %d)!\n", i, sw_irq[i]);
    }
    printk(KERN_INFO "switch_module_exam1_init done!\n");
    return 0;
}

static void __exit switch_module_exam1_exit(void)
{
    int i;
    printk(KERN_INFO "switch_interrupt_exit start!\n");
    start_mode_reset();

    for (i = 0; i < 4; i++) {
        free_irq(sw_irq[i], (void *)(irq_handler));
    }
    led_switch_gpio_free();
    printk(KERN_INFO "switch_interrupt_exit done!\n");
}
static void start_mode_reset(void)
{
    int i;
    printk(KERN_INFO "--- start_mode_reset: Current mode cleanup ---\n");

    del_timer(&mode_1_timer);
    printk(KERN_INFO "MODE 1 Timer stopped.\n");

    del_timer(&mode_2_timer);
    printk(KERN_INFO "MODE 2 Timer stopped.\n");
    current_led = 0;

    for (i = 0; i < 4; i++) {
        gpio_direction_output(led[i], LOW);
    }

    current_mode = MODE_RESET;
    printk(KERN_INFO "Mode reset complete. current_mode: %d\n", current_mode);
}

static void mode_1_timer_cb(struct timer_list *timer)
{
    int ret, i;
    printk(KERN_INFO "MODE 1 timer callback function !\n");
    if (flag == 0) {
        for (i = 0; i < 4; i++) {
            ret = gpio_direction_output(led[i], HIGH);
        }
        flag = 1;
    } else {
        for (i = 0; i < 4; i++) {
            ret = gpio_direction_output(led[i], LOW);
        }
        flag = 0;
    }
    mod_timer(timer, jiffies + HZ * 2);
}

static void start_mode_1(void)
{
    printk(KERN_INFO "start_mode_1 start (Timer)\n");
    flag = 0;
    timer_setup(&mode_1_timer, mode_1_timer_cb, 0);
    mode_1_timer.expires = jiffies + HZ * 2;
    add_timer(&mode_1_timer);
    printk(KERN_INFO "start_mode_1 done\n");
}


static void mode_2_timer_cb(struct timer_list *timer)
{
    int i;
    printk(KERN_INFO "MODE 2 timer callback: LED[%d] ON.\n", current_led);

    for (i = 0; i < 4; i++) {
        gpio_direction_output(led[i], LOW);
    }
    gpio_direction_output(led[current_led], HIGH);

    current_led = (current_led + 1) % 4;

    mod_timer(timer, jiffies + HZ * 2);
}

static void start_mode_2(void)
{
    printk(KERN_INFO "start_mode_2 start (Timer)\n");
    current_led = 0;
    timer_setup(&mode_2_timer, mode_2_timer_cb, 0);
    mode_2_timer.expires = jiffies + HZ * 2;
    add_timer(&mode_2_timer);
    printk(KERN_INFO "start_mode_2 done\n");
}

static void start_mode_3(int sw_index)
{
    if (sw_index >= 0 && sw_index <= 2) {
        int led_index = sw_index;
        int new_led_state = !(gpio_get_value(led[led_index]));
        printk(KERN_INFO "mode 3 : led[%d] => %s.\n", led_index, new_led_state ? "HIGH" : "LOW");
        gpio_direction_output(led[led_index], new_led_state);
    }
}


MODULE_LICENSE("GPL");
module_init(switch_module_exam1_init);
module_exit(switch_module_exam1_exit);





