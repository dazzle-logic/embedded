#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

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

struct task_struct *mode_2_thread = NULL; 


static DECLARE_WAIT_QUEUE_HEAD(wait_queue);
volatile int sw_interrupt_event = 0;
int sw_index_buffer = -1;


#define LED_SW_MAGIC 'L'
#define IOCTL_MODE_1        _IO(LED_SW_MAGIC, 0x01)
#define IOCTL_MODE_2        _IO(LED_SW_MAGIC, 0x02)
#define IOCTL_MODE_3        _IO(LED_SW_MAGIC, 0x03)
#define IOCTL_MODE_RESET    _IO(LED_SW_MAGIC, 0x04)
#define IOCTL_MODE_3_TOGGLE _IOW(LED_SW_MAGIC, 0x05, int)


static int led_switch_gpio_request(void);
static void led_switch_gpio_free(void);
irqreturn_t irq_handler(int irq, void *dev_id);
static void start_mode_reset(void);
static void mode_1_timer_cb(struct timer_list *timer);
static void start_mode_1(void);
static void mode_2_timer_cb(struct timer_list *timer);
static void start_mode_2(void);
static void mode_3_toggle_led(int sw_index);
static int sw_led_driver_open(struct inode * inode, struct file * file);
static int sw_led_driver_release(struct inode * inode, struct file * file);
static ssize_t sw_led_driver_read(struct file * file, char * buf, size_t length, loff_t * ofs);
static long sw_led_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg);






static int led_switch_gpio_request(void)
{
    int i, ret = 0;

    for (i = 0; i < 4; i++) {
        ret = gpio_request(led[i], "LED");
        if (ret < 0) {
            printk(KERN_ERR "LED GPIO %d request failed!\n", led[i]);
            goto fail_led;
        }
        gpio_direction_output(led[i], LOW);
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

    printk(KERN_INFO "irq_handler triggered for IRQ %d\n", irq); 


    for (i = 0; i < 4; i++) {
        if (irq == sw_irq[i]) {
            sw_index = i;
            break;
        }
    }
    

    if (sw_index != -1) {
        sw_index_buffer = sw_index;
        sw_interrupt_event = 1; 
        wake_up_interruptible(&wait_queue);
    }

    return IRQ_HANDLED;
}






static void start_mode_reset(void)
{
    int i;
    printk(KERN_INFO "start_mode_reset: cleanup\n");


    del_timer_sync(&mode_1_timer);
    del_timer_sync(&mode_2_timer);
    current_led = 0;


    for (i = 0; i < 4; i++) {
        gpio_direction_output(led[i], LOW);
    }

    current_mode = MODE_RESET;
    printk(KERN_INFO "Mode reset complete\n");
}


static void mode_1_timer_cb(struct timer_list *timer)
{
    int i;
    printk(KERN_INFO "MODE 1 timer callback\n");

    if (flag == 0) {
        for (i = 0; i < 4; i++) {
            gpio_direction_output(led[i], HIGH);
        }
        flag = 1;
    } else {
        for (i = 0; i < 4; i++) {
            gpio_direction_output(led[i], LOW);
        }
        flag = 0;
    }


    mod_timer(timer, jiffies + HZ * 2); 
}

static void start_mode_1(void)
{
    printk(KERN_INFO "start_mode_1\n");
    flag = 0;
    timer_setup(&mode_1_timer, mode_1_timer_cb, 0);
    mode_1_timer.expires = jiffies + HZ * 2;
    add_timer(&mode_1_timer);
}


static void mode_2_timer_cb(struct timer_list *timer)
{
    int i;
    printk(KERN_INFO "MODE 2 timer callback: LED[%d] ON\n", current_led);


    for (i = 0; i < 4; i++)
        gpio_direction_output(led[i], LOW);


    gpio_direction_output(led[current_led], HIGH);


    current_led = (current_led + 1) % 4;


    mod_timer(timer, jiffies + HZ * 2);
}

static void start_mode_2(void)
{
    printk(KERN_INFO "start_mode_2\n");
    current_led = 0;
    timer_setup(&mode_2_timer, mode_2_timer_cb, 0);
    mode_2_timer.expires = jiffies + HZ * 2;
    add_timer(&mode_2_timer);
}


static void mode_3_toggle_led(int sw_index)
{
    int led_index = sw_index;
    if (led_index >= 0 && led_index <= 2) {

        int new_led_state = !(gpio_get_value(led[led_index]));
        printk(KERN_INFO "mode 3 toggle: led[%d] => %s\n",
             led_index, new_led_state ? "HIGH" : "LOW");
        gpio_direction_output(led[led_index], new_led_state);
    } else {
        printk(KERN_WARNING "mode 3 toggle: Invalid SW index %d\n", sw_index);
    }
}






static int sw_led_driver_open(struct inode * inode, struct file * file){
    int res, i;
    printk(KERN_INFO "sw_led_driver_open!\n");
    

    res = led_switch_gpio_request();
    if(res < 0) return res;


    for(i=0;i<4;i++){
        res = request_irq(sw_irq[i], (irq_handler_t)irq_handler, 
                          IRQF_TRIGGER_RISING, "SW_IRQ", (void *)irq_handler);
        if(res < 0) {
            printk(KERN_ERR "request_irq for SW%d failed! (%d)\n", i, res);

            return res;
        }
    }
    return 0;
}


static int sw_led_driver_release(struct inode * inode, struct file * file){
    int i;
    printk(KERN_INFO "sw_led_driver_release!\n");
    

    start_mode_reset();


    for(i=0;i<4;i++){
        free_irq(sw_irq[i], (void *)irq_handler);
    }
    

    led_switch_gpio_free();
    
    return 0;
}


static ssize_t sw_led_driver_read(struct file * file, char * buf, size_t length, loff_t * ofs){
    int ret;
    char sw_idx_to_user;
    

    ret = wait_event_interruptible(wait_queue, sw_interrupt_event != 0); 
    if (ret) {
        return ret; 
    }
    

    sw_idx_to_user = (char)sw_index_buffer;
    ret = copy_to_user(buf, &sw_idx_to_user, sizeof(sw_idx_to_user));
    if(ret < 0) return -EFAULT;
    

    sw_interrupt_event = 0;
    sw_index_buffer = -1;
    
    printk(KERN_INFO "sw_led_driver_read: Switch Index %d sent to user.\n", sw_idx_to_user);
    
    return 1;
}


static long sw_led_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
    int ret;
    int sw_index_from_user;
    printk(KERN_INFO "sw_led_driver_ioctl: cmd=0x%x\n", cmd);
    

    if (cmd == IOCTL_MODE_1 || cmd == IOCTL_MODE_2 || cmd == IOCTL_MODE_RESET || cmd == IOCTL_MODE_3) {
        start_mode_reset(); 
    }

    switch(cmd){
        case IOCTL_MODE_1:
            current_mode = MODE_1;
            start_mode_1();
            break;
            
        case IOCTL_MODE_2:
            current_mode = MODE_2;
            start_mode_2();
            break;
            
        case IOCTL_MODE_3:
            current_mode = MODE_3; 
            break;
            
        case IOCTL_MODE_RESET:
            break;

        case IOCTL_MODE_3_TOGGLE:

            ret = copy_from_user(&sw_index_from_user, (int __user *)arg, sizeof(int)); 
            if (ret) return -EFAULT;


            mode_3_toggle_led(sw_index_from_user);
            break;

        default:
            return -ENOTTY;
    }
    return 0;
}


static struct file_operations sw_led_driver_fops = {
    .owner = THIS_MODULE,
    .open = sw_led_driver_open,
    .release = sw_led_driver_release,
    .read = sw_led_driver_read,
    .unlocked_ioctl = sw_led_driver_ioctl,
};


static struct miscdevice sw_led_misc_driver = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "sw_led_driver",
    .fops = &sw_led_driver_fops,
};


static int __init sw_led_init(void){
    printk(KERN_INFO "sw_led_driver init!\n");

    return misc_register(&sw_led_misc_driver);
}

static void __exit sw_led_exit(void){
    printk(KERN_INFO "sw_led_driver exit!\n");

    misc_deregister(&sw_led_misc_driver);
}

MODULE_LICENSE("GPL");
module_init(sw_led_init);
module_exit(sw_led_exit);
