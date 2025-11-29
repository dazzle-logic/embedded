#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define DEVICE_FILE "/dev/pir_driver"

int main(void)
{
    int dev;
    char buf[100];
    int i;

    dev = open(DEVICE_FILE, O_RDWR);
    if (dev < 0) {
        printf("driver open failed!\n");
        printf("※ /dev/pir_driver 파일의 주번호(223) 및 권한(666)을 확인하세요.\n");
        return -1;
    }

    printf("--- PIR 알람 시스템 활성화 ---\n");
    printf("PIR 센서의 움직임을 대기합니다. (SW 입력 시 알람 해제)\n");

    for (i = 0; i < 1000; i++) {
        if (read(dev, &buf, sizeof(buf)) > 0) {
            printf("[PIR EVENT] %s\n", buf);
        } else {
            printf("[SYSTEM] Read Error or Interrupted.\n");
            break;
        }
    }

    close(dev);
    printf("PIR 프로그램 종료.\n");
    return 0;
}
