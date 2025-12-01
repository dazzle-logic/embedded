#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/timer.h>


#define DEV_MAJOR_NUMBER 223
#define DEV_NAME "pir_driver"
#define PIR_GPIO 7
#define HIGH 1
#define LOW 0

int led[4] = {23, 24, 25, 1};
int sw[4] = {4, 17, 27, 22};
unsigned int sw_irq[4];

static DECLARE_WAIT_QUEUE_HEAD(wait_queue);
int irq_num = 0;
int alarm_active = 0;
int led_toggle_flag = 0;

struct timer_list alarm_timer;

irqreturn_t pir_irq_handler(int irq, void *dev_id);
irqreturn_t sw_irq_handler(int irq, void *dev_id);
static void alarm_timer_cb(struct timer_list *timer);
static void start_alarm(void);
static void stop_alarm(void);


irqreturn_t sw_irq_handler(int irq, void *dev_id)
{

    printk(KERN_INFO "SW Interrupt received. Stopping alarm.\n");
    stop_alarm();
    return IRQ_HANDLED;
}


irqreturn_t pir_irq_handler(int irq, void *dev_id)
{
    irq_num = irq;
    printk(KERN_INFO "PIR Detect! IRQ %d. Starting alarm.\n", irq);
    

    if (alarm_active == 0) {
        start_alarm();
    }
    
    wake_up_interruptible(&wait_queue);
    return IRQ_HANDLED;
}


static void alarm_timer_cb(struct timer_list *timer)
{
    int i;
    printk(KERN_INFO "Alarm Timer CB: LED Toggle\n");


    if (led_toggle_flag == 0) {
        for (i = 0; i < 4; i++) gpio_direction_output(led[i], HIGH);
        led_toggle_flag = 1;
    } else {
        for (i = 0; i < 4; i++) gpio_direction_output(led[i], LOW);
        led_toggle_flag = 0;
    }


    mod_timer(timer, jiffies + HZ * 2);
}


static void start_alarm(void)
{
    int i;
    printk(KERN_INFO "Alarm Started (LED ON/OFF Toggle)\n");
    alarm_active = 1;
    led_toggle_flag = 0;


    timer_setup(&alarm_timer, alarm_timer_cb, 0);
    alarm_timer.expires = jiffies + HZ * 2;
    add_timer(&alarm_timer);
}


static void stop_alarm(void)
{
    int i;
    if (alarm_active == 0) return;

    printk(KERN_INFO "Alarm Stopped by SW input\n");
    del_timer_sync(&alarm_timer);
    alarm_active = 0;


    for (i = 0; i < 4; i++) gpio_direction_output(led[i], LOW);
}


static int pir_driver_open(struct inode * inode, struct file * file)
{
    int ret, i;
    printk(KERN_INFO "pir_driver_open!\n");


    for (i = 0; i < 4; i++) {
        if (gpio_request(led[i], "LED") < 0) return -ENOMEM;
        gpio_direction_output(led[i], LOW);
    }

    for (i = 0; i < 4; i++) {
        if (gpio_request(sw[i], "SW") < 0) return -ENOMEM;
        gpio_direction_input(sw[i]);
        sw_irq[i] = gpio_to_irq(sw[i]);
    }


    ret = request_irq(gpio_to_irq(PIR_GPIO), (irq_handler_t)pir_irq_handler,
                      IRQF_TRIGGER_FALLING, "PIR_IRQ", (void *)pir_irq_handler);
    if (ret < 0) printk(KERN_INFO "PIR request_irq failed!\n");


    for (i = 0; i < 4; i++) {
        ret = request_irq(sw_irq[i], (irq_handler_t)sw_irq_handler,
                          IRQF_TRIGGER_RISING, "SW_ALARM_OFF", (void *)sw_irq_handler); 
        if (ret < 0) printk(KERN_INFO "SW request_irq failed!\n");
    }

    return 0;
}


static ssize_t pir_driver_read(struct file * file, char * buf, size_t length, loff_t * ofs)
{
    int ret;
    char msg[100];
    printk(KERN_INFO "pir_driver_read!\n");


    ret = wait_event_interruptible(wait_queue, irq_num != 0);
    
    if (gpio_to_irq(PIR_GPIO) == irq_num) {
        sprintf(msg, "PIR Detect");
    } else {

        sprintf(msg, "PIR Not Detect (Manual Wake)"); 
    }
    

    ret = copy_to_user(buf, msg, sizeof(msg));
    irq_num = 0;
    
    if (ret < 0) printk(KERN_INFO "pir driver copy to user failed!\n");
    return sizeof(msg);
}


static int pir_driver_release(struct inode * inode, struct file * file)
{
    int i;
    printk(KERN_INFO "pir_driver_release!\n");
    
    stop_alarm();
    

    free_irq(gpio_to_irq(PIR_GPIO), (void *)pir_irq_handler);
    gpio_free(PIR_GPIO); 


    for (i = 0; i < 4; i++) {
        free_irq(sw_irq[i], (void *)sw_irq_handler);
        gpio_free(sw[i]);
        gpio_free(led[i]);
    }
    
    return 0;
}


static struct file_operations pir_driver_fops = {
    .owner = THIS_MODULE,
    .open = pir_driver_open,
    .release = pir_driver_release,
    .read = pir_driver_read,
};

static int pir_driver_init(void)
{
    printk(KERN_INFO "pir_driver_init!\n");

    register_chrdev(DEV_MAJOR_NUMBER, DEV_NAME, &pir_driver_fops);
    return 0;
}

static void pir_driver_exit(void)
{
    printk(KERN_INFO "pir_driver_exit!\n");
    unregister_chrdev(DEV_MAJOR_NUMBER, DEV_NAME);
}

MODULE_LICENSE("GPL");
module_init(pir_driver_init);
module_exit(pir_driver_exit);
