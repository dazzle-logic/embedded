#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define DRIVER_NODE "/dev/pir_driver"

int main(void) {
    int dev;
    char buffer[100];

    if (dev < 0) {
        printf("ERROR");
        return -1;
    }
    
    while(1) {
     int bytes_read = read(dev, buffer, sizeof(buffer)); 
        
        if (bytes_read > 0) {
           
            buffer[bytes_read] = '\0';
            printf("PIR Event Detected: %s\n", buffer);
        }
    }
    close(dev);
    printf("Application finished. Driver closed.\n");
    
    return 0;
}