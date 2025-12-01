#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

// 드라이버와 약속된 명령어들
#define LED_SW_MAGIC        'L'
#define IOCTL_MODE_1        _IO(LED_SW_MAGIC, 0x01)
#define IOCTL_MODE_2        _IO(LED_SW_MAGIC, 0x02)
#define IOCTL_MODE_3        _IO(LED_SW_MAGIC, 0x03)
#define IOCTL_MODE_RESET    _IO(LED_SW_MAGIC, 0x04)
#define IOCTL_MODE_3_TOGGLE _IOW(LED_SW_MAGIC, 0x05, int)

#define DEVICE_FILE "/dev/sw_led_driver"

#define SW_IDX_MODE_1 0
#define SW_IDX_MODE_2 1
#define SW_IDX_MODE_3 2
#define SW_IDX_RESET  3

// --- 함수 선언 ---
void mode_3_control(int dev);

// Mode 3: 개별 제어 모드 (서브 루프)
void mode_3_control(int dev) {
    char sw_index_char;
    int sw_idx_int;

    printf("\n[Mode 3 진입] SW[0]~[2]: LED 토글, SW[3]: 메인으로 복귀(리셋)\n");

    while (1) {
        // 여기서 또 다시 입력을 기다림 (Blocking)
        if (read(dev, &sw_index_char, 1) > 0) {
            sw_idx_int = (int)sw_index_char;
            
            // SW[3] (Reset) 버튼이 눌리면 이 함수(Mode 3)를 탈출!
            if (sw_idx_int == SW_IDX_RESET) {
                printf("  -> SW[3] 감지: Mode 3 종료 및 메인 복귀\n");
                // 드라이버 상태도 리셋해주는 게 안전함
                ioctl(dev, IOCTL_MODE_RESET, NULL);
                return; // 함수 종료 -> main()의 while 루프로 돌아감
            }

            // SW[0], SW[1], SW[2]는 LED 토글 명령으로 사용
            if (sw_idx_int >= 0 && sw_idx_int <= 2) {
                printf("  -> SW[%d] 감지: LED 토글\n", sw_idx_int);
                ioctl(dev, IOCTL_MODE_3_TOGGLE, &sw_idx_int);
            }
        }
    }
}

int main(void) {
    int dev;
    char sw_index_char; // 드라이버에서 읽어온 1바이트 데이터 저장
    int sw_idx_int;

    // 디바이스 드라이버 열기
    dev = open(DEVICE_FILE, O_RDWR);
    if (dev < 0) {
        printf("드라이버 열기 실패! 모듈이 올라가 있나요?\n");
        return -1;
    }

    printf("=== SW 기반 LED 제어 프로그램 시작 ===\n");
    printf("SW[0] (GPIO 4) : Mode 1 (깜빡임)\n");
    printf("SW[1] (GPIO 17): Mode 2 (순차 점등)\n");
    printf("SW[2] (GPIO 27): Mode 3 (개별 제어 진입)\n");
    printf("SW[3] (GPIO 22): Reset (모드 끄기)\n");
    printf("======================================\n");

    // 메인 무한 루프 (키보드 대신 read로 대기)
    while (1) {
        printf("\n[메인 대기] 명령을 내릴 스위치를 눌러주세요...\n");
        
        // 스위치 입력 대기 (Blocking)
        // 사용자가 스위치를 누를 때까지 여기서 프로그램은 잠듦
        if (read(dev, &sw_index_char, 1) > 0) {
            sw_idx_int = (int)sw_index_char; // char를 int로 변환
            
            // 눌린 스위치에 따라 모드 결정 (리모컨 로직)
            switch (sw_idx_int) {
                case SW_IDX_MODE_1: // SW[0] -> Mode 1
                    printf("  -> SW[0] 감지: Mode 1 시작!\n");
                    ioctl(dev, IOCTL_MODE_1, NULL);
                    break;

                case SW_IDX_MODE_2: // SW[1] -> Mode 2
                    printf("  -> SW[1] 감지: Mode 2 시작!\n");
                    ioctl(dev, IOCTL_MODE_2, NULL);
                    break;

                case SW_IDX_MODE_3: // SW[2] -> Mode 3 진입
                    printf("  -> SW[2] 감지: Mode 3 진입!\n");
                    ioctl(dev, IOCTL_MODE_3, NULL);
                    // Mode 3는 동작 방식이 다르므로 전용 함수로 제어권을 넘김
                    mode_3_control(dev); 
                    break;

                case SW_IDX_RESET: // SW[3] -> Reset
                    printf("  -> SW[3] 감지: 모든 모드 리셋(OFF)\n");
                    ioctl(dev, IOCTL_MODE_RESET, NULL);
                    break;

                default:
                    printf("  -> 알 수 없는 입력\n");
                    break;
            }
        }
    }

    close(dev);
    return 0;
}
