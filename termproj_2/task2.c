#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/timer.h>

#define HIGH 1
#define LOW 0

static int current_led=0;
static struct timer_list led_timer;
static int current_mode=0;
#define DEV_MAJOR_NUMBER 220
#define DEV_NAME "led driver"


int led[4]={23,24,25,1};
static int led_state[4] ={0,0,0,0};

static void timer_cb(struct timer_list *t){
  static int flag=0;
  int i;

  if(current_mode==1){
  if(flag==0){
    for(i=0; i<4; i++)
      gpio_direction_output(led[i],HIGH);
    flag=1;
  }
  else{
    for(i=0; i<4; i++)
    gpio_direction_output(led[i],LOW);
    flag=0;
  }
}
 else if(current_mode==2){
  for(i=0; i<4; i++)
  gpio_direction_output(led[i],LOW);
  gpio_direction_output(led[current_led],HIGH);
  current_led=(current_led+1)%4;
 }
  if(current_mode==1||current_mode==2){
  led_timer.expires=jiffies+(2*HZ);
  add_timer(&led_timer);
  }
}
static int led_driver_write(struct file *file,const char *buf,size_t length, loff_t *ofs){
  char mode;
  int ret;
  int i;
  ret=copy_from_user(&mode,buf,length);
  if(ret<0) return -1;
  int mode_num=mode-'0';
  if(current_mode!=3){
  if(mode=='1'){
    current_mode=1;
    current_led=0;
    del_timer(&led_timer);
    led_timer.expires=jiffies+(2*HZ);
    add_timer(&led_timer);
    }
  if(mode=='2'){
    current_led=0;
    current_mode=2;
    del_timer(&led_timer);
    led_timer.expires=jiffies+(2*HZ);
    add_timer(&led_timer);
    }
  if(mode=='3'){
    current_led=0;
    current_mode=3;
    del_timer(&led_timer);
    for(i=0; i<4; i++){
    gpio_direction_output(led[i],LOW);
    led_state[i]=0;
    mode_num=-1;
   }
 }
}

  if(mode== '4'){
    current_led=0;
    current_mode=0;
    del_timer(&led_timer);
    for(i=0; i<4; i++)
    gpio_direction_output(led[i],LOW);
 }

  if(current_mode==3 && mode_num<4){
    if(led_state[mode_num]==0){
     gpio_direction_output(led[mode_num],HIGH);
     led_state[mode_num]=1;
    }
    else{
      gpio_direction_output(led[mode_num],LOW);
      led_state[mode_num]=0;
    }
    return length;
  }

  return length;
}

static ssize_t led_driver_read(struct file *file,char *buf,size_t length, loff_t *ofs){
  return 0;
}
static int led_driver_open(struct inode *inode,struct file *file){
  int i,ret;
  for(i=0; i<4; i++){
    ret=gpio_request(led[i],"LED");
    gpio_direction_output(led[i],LOW);
    if(ret<0)
      printk(KERN_INFO "led driver gpio request fail\n");
  }
  return 0;
}
static int led_driver_release(struct inode *inode,struct file *file){
  int i;
  del_timer(&led_timer);
  for(i=0; i<4; i++)
    gpio_free(led[i]);
  return 0;
}

static struct file_operations led_driver_fops={
  .owner=THIS_MODULE,
  .open=led_driver_open,
  .release=led_driver_release,
  .write=led_driver_write,
  .read=led_driver_read,
};

static int led_driver_init(void){
  timer_setup(&led_timer, timer_cb, 0);
  register_chrdev(DEV_MAJOR_NUMBER,DEV_NAME,&led_driver_fops);
  return 0;
}

static void led_driver_exit(void){
  del_timer(&led_timer);
  unregister_chrdev(DEV_MAJOR_NUMBER,DEV_NAME);

}

module_init(led_driver_init);
module_exit(led_driver_exit);
MODULE_LICENSE("GPL");


