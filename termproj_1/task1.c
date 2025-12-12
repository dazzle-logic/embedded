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
unsigned int sw_irq[4]; // switch interrupt request

// switch mode
#define MODE_RESET 0
#define MODE_1 1
#define MODE_2 2
#define MODE_3 3
#define HIGH 1
#define LOW 0

int current_mode = MODE_RESET;
int flag = 0;
int current_led = 0; // MODE_2 (순차 점등)에서 사용할 변수
struct timer_list mode_1_timer;
struct timer_list mode_2_timer; // MODE_2에 사용될 타이머 추가
struct task_struct *mode_2_thread = NULL; // 사용하지 않지만 기존 변수 유지 (초기화 시 NULL 보장)

static void start_mode_1(void);
static void start_mode_2(void);
static void start_mode_3(int sw_index);
static void start_mode_reset(void);
static void mode_1_timer_cb(struct timer_list *timer);
static void mode_2_timer_cb(struct timer_list *timer); // 새로운 콜백 함수 선언

// ====================================================================
// GPIO/IRQ Setup & Teardown
// ====================================================================

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
        // 기존 모드 클린업
        start_mode_reset();

        // 새로운 모드 시작
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
            // MODE_3은 인터럽트 핸들러 내에서 바로 LED 토글을 처리하지 않고
            // MODE_3 상태 진입만 알림. 이후 SW[0]~SW[2] 입력 시 start_mode_3 호출됨.
            break;

        default:
            break;
        }
    }
    return IRQ_HANDLED;
}

// ====================================================================
// Module Init/Exit
// ====================================================================

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
        // IRQF_TRIGGER_RISING 사용
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
    start_mode_reset(); // 모든 동작 (타이머, 스레드, LED) 정리

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

    // MODE_1 타이머 정리
    del_timer_sync(&mode_1_timer);
    printk(KERN_INFO "MODE 1 Timer stopped.\n");

    // MODE_2 타이머 정리 (추가된 부분)
    del_timer_sync(&mode_2_timer);
    printk(KERN_INFO "MODE 2 Timer stopped.\n");
    current_led = 0; // 순차 점등 인덱스 초기화

    // MODE_2 스레드 정리 (기존 코드의 흔적)
    if (mode_2_thread != NULL) {
        kthread_stop(mode_2_thread);
        mode_2_thread = NULL;
        printk(KERN_INFO "Kernel thread stopped.\n");
    }
    // 모든 LED 끄기
    for (i = 0; i < 4; i++) {
        gpio_direction_output(led[i], LOW);
    }

    current_mode = MODE_RESET;
    printk(KERN_INFO "Mode reset complete. current_mode: %d\n", current_mode);
}

// --------------------------------------------------------------------
// MODE 1: All LED Toggle (Timer)
// --------------------------------------------------------------------

static void mode_1_timer_cb(struct timer_list *timer)
{
    int ret, i;
    printk(KERN_INFO "MODE 1 timer callback function !\n");
    if (flag == 0) {
        // All ON
        for (i = 0; i < 4; i++) {
            ret = gpio_direction_output(led[i], HIGH);
        }
        flag = 1;
    } else {
        // All OFF
        for (i = 0; i < 4; i++) {
            ret = gpio_direction_output(led[i], LOW);
        }
        flag = 0;
    }
    // 다음 만료 시간 설정
    mod_timer(timer, jiffies + HZ * 2);
}

static void start_mode_1(void)
{
    printk(KERN_INFO "start_mode_1 start (Timer)\n");
    flag = 0; // 초기 상태 (0: OFF -> 1: ON)
    timer_setup(&mode_1_timer, mode_1_timer_cb, 0);
    mode_1_timer.expires = jiffies + HZ * 2;
    add_timer(&mode_1_timer);
    printk(KERN_INFO "start_mode_1 done\n");
}

// --------------------------------------------------------------------
// MODE 2: Sequential LED ON (Timer) - 기존 두 번째 코드 로직 사용
// --------------------------------------------------------------------

static void mode_2_timer_cb(struct timer_list *timer)
{
    int i;
    printk(KERN_INFO "MODE 2 timer callback: LED[%d] ON.\n", current_led);

    for (i = 0; i < 4; i++) { // 일단 다 끄고
        gpio_direction_output(led[i], LOW);
    }
    gpio_direction_output(led[current_led], HIGH); // 현재 LED만 켜기

    // 다음 LED 인덱스로 이동 (순환)
    current_led = (current_led + 1) % 4;

    // 다음 만료 시간 설정
    mod_timer(timer, jiffies + HZ * 2);
}

static void start_mode_2(void)
{
    printk(KERN_INFO "start_mode_2 start (Timer)\n");
    current_led = 0; // 순차 점등 시작 인덱스 초기화
    timer_setup(&mode_2_timer, mode_2_timer_cb, 0);
    mode_2_timer.expires = jiffies + HZ * 2;
    add_timer(&mode_2_timer);
    printk(KERN_INFO "start_mode_2 done\n");
}

static void start_mode_3(int sw_index)
{
    if (sw_index >= 0 && sw_index <= 2) {
        int led_index = sw_index;
        // 현재 LED 상태를 읽어서 토글
        int new_led_state = !(gpio_get_value(led[led_index]));
        printk(KERN_INFO "mode 3 : led[%d] => %s.\n", led_index, new_led_state ? "HIGH" : "LOW");
        gpio_direction_output(led[led_index], new_led_state);
    }
}


MODULE_LICENSE("GPL");
module_init(switch_module_exam1_init);
module_exit(switch_module_exam1_exit);





