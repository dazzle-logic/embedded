#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>


[cite_start]#define LED_SW_MAGIC 'L'
#define IOCTL_MODE_1        _IO(LED_SW_MAGIC, 0x01)
#define IOCTL_MODE_2        _IO(LED_SW_MAGIC, 0x02)
#define IOCTL_MODE_3        _IO(LED_SW_MAGIC, 0x03)
#define IOCTL_MODE_RESET    _IO(LED_SW_MAGIC, 0x04)
#define IOCTL_MODE_3_TOGGLE _IOW(LED_SW_MAGIC, 0x05, int)

#define DEVICE_FILE "/dev/sw_led_driver"
#define SW_RESET_INDEX 3


int current_native_mode = 0; 


void print_menu();
void mode_3_control(int dev);





void print_menu() {
    printf("\n--- ES-101 SW-LED 제어 프로그램 ---\n");
    printf("현재 모드: Mode %d\n", current_native_mode);
    printf("----------------------------------\n");
    printf("1. Mode 1 시작 (2초 깜빡임) [SW[0] 기능]\n");
    printf("2. Mode 2 시작 (2초 순차 점등) [SW[1] 기능]\n");
    printf("3. Mode 3 진입 (SW 개별 LED 제어) [SW[2] 기능]\n");
    printf("0. 모드 리셋 및 프로그램 종료 [SW[3] 기능]\n");
    printf("----------------------------------\n");
    printf("선택: ");
}





void mode_3_control(int dev) {
    char sw_index_char;
    int sw_idx_int;

    printf("\n--- Mode 3: SW 개별 제어 모드 진입 ---\n");
    printf("SW[0], SW[1], SW[2]를 누르면 해당 LED가 토글됩니다.\n");
    printf("SW[3] 감지 시 모드 리셋 및 메뉴로 복귀합니다.\n");

    while (1) {
        printf("스위치 입력을 대기합니다... (Blocking Read)\n");
        

        if (read(dev, &sw_index_char, 1) > 0) {
            sw_idx_int = (int)sw_index_char;


            if (sw_idx_int == SW_RESET_INDEX) {
                printf("SW[%d] (리셋) 감지! Mode 3 종료.\n", SW_RESET_INDEX);

                ioctl(dev, IOCTL_MODE_RESET, NULL);
                current_native_mode = 0;
                return; 
            }
            

            if (sw_idx_int >= 0 && sw_idx_int <= 2) {
                printf("SW[%d] 감지! -> LED[%d] 토글 명령 전송\n", sw_idx_int, sw_idx_int);
                


                ioctl(dev, IOCTL_MODE_3_TOGGLE, sw_idx_int); 
            }
        } else {

            perror("read failed");
            break;
        }
    }
}





int main(void) {
    int dev;
    int choice;


    dev = open(DEVICE_FILE, O_RDWR); [cite_start]
    if (dev < 0) {
        printf("드라이버 파일 열기 실패: %s\n", DEVICE_FILE);
        printf("※ /dev/sw_led_driver 파일이 생성되었는지, 권한 설정이 666인지 확인하세요.\n");
        return -1;
    }

    while (1) {
        print_menu();
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("잘못된 입력입니다. 숫자를 입력해주세요.\n");
            continue;
        }

        switch (choice) {
            case 1:
                printf("Mode 1 (2초 깜빡임) 시작 명령 전송\n");
                ioctl(dev, IOCTL_MODE_1, NULL);
                current_native_mode = 1;
                break;
            case 2:
                printf("Mode 2 (2초 순차 점등) 시작 명령 전송\n");
                ioctl(dev, IOCTL_MODE_2, NULL);
                current_native_mode = 2;
                break;
            case 3:
                ioctl(dev, IOCTL_MODE_3, NULL);
                current_native_mode = 3;
                mode_3_control(dev);
                break;
            case 0:
                printf("모드 리셋 및 프로그램 종료 명령 전송\n");
                ioctl(dev, IOCTL_MODE_RESET, NULL);
                close(dev);
                return 0;
            default:
                printf("유효하지 않은 선택입니다.\n");
                break;
        }
    }

    return 0;
}
