#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>       
#include <linux/gpio.h>     
#include <linux/uaccess.h>  
#include <linux/interrupt.h> 
#include <linux/timer.h>    
#include <linux/jiffies.h>  

#define DEV_NUMBER 223
#define DEV_NAME "pir_driver"
#define HIGH 1
#define LOW 0
#define PIR_GPIO 7
#define ALARM_INTERVAL (HZ * 2) 


static void pir_timer_cb(struct timer_list *t);
void set_all_leds(int value);

int led[4]={23,24,25,1};
int sw[4]={4,17,27,22};

static struct timer_list pir_timer; 
static int alarm_state = 0;          
static int led_flag = 0; 
static int pir_irq = 0;              
static int sw_irq[4] = {0, 0, 0, 0};

void set_all_leds(int value) {
    int i;
    for (i = 0; i < 4; i++) {
        gpio_direction_output(led[i], value); 
    }
}


static void pir_timer_cb(struct timer_list *t) {
    if (alarm_state == 1) { 
        printk(KERN_INFO "Alarm Timer: LED Toggle (%d)!\n", led_flag);
        led_flag = !led_flag; 
        set_all_leds(led_flag); 
        mod_timer(&pir_timer, jiffies + ALARM_INTERVAL);
    }
}

static irqreturn_t pir_irq_handler(int irq, void *dev_id) {
    if (alarm_state == 0) { 
        printk(KERN_ALERT "PIR Detect! Alarm Start!\n");
        alarm_state = 1;

        timer_setup(&pir_timer, pir_timer_cb, 0); 
        pir_timer.expires = jiffies + ALARM_INTERVAL; 
        add_timer(&pir_timer); 
    }
    return IRQ_HANDLED;
}

static irqreturn_t switch_irq_handler(int irq, void *dev_id) {
    if (alarm_state == 1) {  
      int i;
        printk(KERN_ALERT "Switch Pressed! Alarm Stop!\n");
        
       
        del_timer_sync(&pir_timer);
        for(i=0; i<4; i++)
          gpio_direction_output(led[i],LOW);
        alarm_state = 0;
        led_flag = 0; 
    }
    return IRQ_HANDLED;
}

static int pir_alarm_open(struct inode *inode, struct file *file) {
    int ret, i;
    printk(KERN_INFO "pir_alarm_driver_open!\n");
    
    
    for (i = 0; i < 4; i++) {
        ret = gpio_request(led[i], "LED");
        ret = gpio_request(sw[i], "SW");
    }
    
    ret = gpio_request(PIR_GPIO, "pir");
    pir_irq = gpio_to_irq(PIR_GPIO);
    ret = request_irq(pir_irq, pir_irq_handler, IRQF_TRIGGER_FALLING, "PIR_IRQ", pir_irq_handler); 
    
    for (i = 0; i < 4; i++) {
        sw_irq[i] = gpio_to_irq(sw[i]);
        ret = request_irq(sw_irq[i], switch_irq_handler, IRQF_TRIGGER_RISING, "SW_IRQ", switch_irq_handler); 
        gpio_direction_output(led[i],LOW);
    }
    return 0;
}


static int pir_alarm_release(struct inode *inode, struct file *file) {
    int i;
    printk(KERN_INFO "pir_alarm_driver_release!\n");

   
    if (alarm_state == 1) {
        del_timer_sync(&pir_timer); 
    }
    
    
    free_irq(pir_irq, pir_irq_handler); 
    gpio_free(PIR_GPIO); 
    for (i = 0; i < 4; i++) {
        free_irq(sw_irq[i], switch_irq_handler); 
        gpio_free(led[i]);
        gpio_free(sw[i]); 
    }

    return 0;
}

static ssize_t pir_alarm_read(struct file *file, char *buf, size_t length, loff_t *ofs) {
  
    return 0;
}

static ssize_t pir_alarm_write(struct file *file, const char *buf, size_t length, loff_t *ofs) {
  
    return 0;
}

static const struct file_operations pir_alarm_fops = {
    .owner   = THIS_MODULE,
    .open    = pir_alarm_open,
    .release = pir_alarm_release,
    .read    = pir_alarm_read,
    .write   = pir_alarm_write,
};

static int __init pir_alarm_init(void) {
    int ret;
    ret = register_chrdev(DEV_NUMBER, DEV_NAME, &pir_alarm_fops);
    if (ret < 0) {
        printk(KERN_ERR "Failed to register char device.\n");
        return ret;
    }
    return 0;
}

static void __exit pir_alarm_exit(void) {
    unregister_chrdev(DEV_NUMBER, DEV_NAME);
}

module_init(pir_alarm_init);
module_exit(pir_alarm_exit);
MODULE_LICENSE("GPL");