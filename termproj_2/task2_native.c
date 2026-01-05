#include <stdio.h>
#include <stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<string.h>

#define DEVICE_FILE "/dev/led_driver"

int main(int argc,char *argv[]){
  int dev;
  char input_line[256];
  char mod;
  dev=open(DEVICE_FILE,O_RDWR);
  if(dev<0){
    printf("open failled!");
    return -1;
  }
  while(1){
    if (fgets(input_line, sizeof(input_line), stdin) == NULL)
    break;
    if(input_line[0]=='\n')
    continue;
    mod=input_line[0];
    if(mod=='q')
    break;

    write(dev,&mod,1);
  }
  close(dev);
  return 0;
}
