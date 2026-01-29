/*
 * testshell.c - Enhanced Test Shell
 * 
 * 기존 명령어 유지 + FTL 기능 추가
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include "ssd_new.h"
#include <stdlib.h>
#include <string.h>

void fullwrite(char* data) {
    for (int idx = 0; idx < 100; idx++) {
        write(idx, data);  // 모든 LBA에 같은 값을 기록
    }
}

void fullread() {
    for (int idx = 0; idx < 100; idx++) {
        printf("0x%08X\n", read(idx));  // 모든 LBA에서 값을 읽음
    }
}

void testapp1() {
    char* test_value = "0xABCDFFFF";
    // Full Write 수행
    fullwrite(test_value);
    printf("Full Write 완료: %s\n", test_value);
    // Full Read 및 비교 수행
    printf("Full Read 및 값 검증 중...\n");
    fullread();
}

void testapp2() {
    char* aging_value = "0xAAAABBBB";
    char* overwrite_value = "0x12345678";
    
    printf("[TestApp2] Aging Write 시작 (LBA 0~5에 30번 반복 쓰기)\n");
    // 0~5 LBA에 30번 Write
    for (int i = 0; i < 30; i++) {
        for (int idx = 0; idx < 6; idx++) {
            write(idx, aging_value);
        }
        if ((i + 1) % 10 == 0) {
            printf("  ... %d회 완료\n", i + 1);
        }
    }
    printf("Aging Write 완료: %s\n", aging_value);
    
    // 통계 출력 (WAF 확인)
    printf("\n=== Aging 후 통계 ===\n");
    ssd_print_statistics();
    
    // 0~5 LBA에 1번 Over Write
    printf("\n[TestApp2] Over Write 수행\n");
    for (int idx = 0; idx < 6; idx++) {
        write(idx, overwrite_value);
    }
    printf("Over Write 완료: %s\n", overwrite_value);
    
    // Read 및 비교 수행
    printf("\n값 검증 중...\n");
    for (int idx = 0; idx < 6; idx++) {
        unsigned int value = read(idx);
        unsigned int expected;
        sscanf(overwrite_value, "0x%X", &expected);
        if (value == expected) {
            printf("  LBA %d: PASS (0x%08X)\n", idx, value);
        } else {
            printf("  LBA %d: FAIL (Expected 0x%08X, Got 0x%08X)\n", idx, expected, value);
        }
    }
    
    // 최종 통계
    printf("\n=== 최종 통계 ===\n");
    ssd_print_statistics();
}

// NEW: TestApp3 - GC 동작 검증
void testapp3() {
    printf("[TestApp3] GC(Garbage Collection) 동작 테스트\n\n");
    
    // Step 1: LBA 0~50에 초기 데이터 쓰기
    printf("Step 1: LBA 0~50에 초기 데이터 쓰기\n");
    for (int i = 0; i <= 50; i++) {
        char data[20];
        sprintf(data, "0x%08X", i * 100);
        write(i, data);
    }
    
    printf("\n=== 초기 쓰기 후 통계 ===\n");
    ssd_print_statistics();
    
    // Step 2: LBA 0~50에 반복 덮어쓰기 (Invalid 페이지 생성)
    printf("\nStep 2: LBA 0~50에 10번 반복 덮어쓰기 (GC 발동 유도)\n");
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i <= 50; i++) {
            char data[20];
            sprintf(data, "0x%08X", (round + 1) * 1000 + i);
            write(i, data);
        }
        printf("  ... Round %d 완료\n", round + 1);
    }
    
    printf("\n=== 반복 쓰기 후 통계 (GC 발동 확인) ===\n");
    ssd_print_statistics();
    
    // Step 3: 데이터 무결성 검증
    printf("\nStep 3: 데이터 무결성 검증 (LBA 0~10)\n");
    for (int i = 0; i <= 10; i++) {
        unsigned int value = read(i);
        unsigned int expected = 10 * 1000 + i; // 마지막 round 값
        if (value == expected) {
            printf("  LBA %d: PASS\n", i);
        } else {
            printf("  LBA %d: FAIL (Expected 0x%08X, Got 0x%08X)\n", i, expected, value);
        }
    }
}

void executecommand(char *cmd) {
    // 명령어 파싱
    char* token = strtok(cmd, " ");
    
    if (token == NULL) {
        printf("명령어가 잘못되었습니다.\n");
        return;
    }
    
    if (strcmp(token, "W") == 0) {
        token = strtok(NULL, " ");  // idx 가져옴
        int idx = atoi(token);
        token = strtok(NULL, " ");  // 데이터 가져옴
        char* data = token;
        
        if (idx < 0 || idx > 99) {
            printf("할당된 범위 밖입니다 (0~99)\n");
            return;
        }
        if (strlen(data) != 10) {
            printf("값이 잘못 입력되었습니다 (형식: 0xXXXXXXXX)\n");
            return;
        }
        
        write(idx, data);
    }
    else if (strcmp(token, "R") == 0) {
        token = strtok(NULL, " ");  // idx 가져옴
        int idx = atoi(token);
        
        if (idx < 0 || idx > 99) {
            printf("할당된 범위 밖입니다 (0~99)\n");
            return;
        }
        
        read(idx);
    }
    else if (strcmp(token, "help") == 0) {
        printf("==================== 사용 가능한 명령어 ====================\n");
        printf("기본 명령어:\n");
        printf("  W <idx> <data>   - 특정 LBA에 쓰기 (예: W 3 0xAAAABBBB)\n");
        printf("  R <idx>          - 특정 LBA에서 읽기 (예: R 3)\n");
        printf("  fullwrite <data> - 모든 LBA(0~99)에 동일 데이터 쓰기\n");
        printf("  fullread         - 모든 LBA(0~99) 읽기\n");
        printf("  exit             - 프로그램 종료\n");
        printf("\n테스트 애플리케이션:\n");
        printf("  testapp1         - Full Write/Read 검증\n");
        printf("  testapp2         - Aging Write 및 Over Write 검증\n");
        printf("  testapp3         - GC 동작 검증 (NEW)\n");
        printf("\n디버깅 명령어 (NEW):\n");
        printf("  stats            - FTL 및 NAND 통계 출력 (WAF 포함)\n");
        printf("  l2p              - L2P 매핑 테이블 출력\n");
        printf("  gc               - 강제 GC 발동\n");
        printf("===========================================================\n");
    }
    else if (strcmp(token, "fullread") == 0) {
        fullread();
    }
    else if (strcmp(token, "fullwrite") == 0) {
        token = strtok(NULL, " ");  // 데이터 가져옴
        if (token == NULL || strlen(token) != 10 || token[0] != '0' || token[1] != 'x') {
            printf("잘못된 입력 값입니다 (형식: 0xXXXXXXXX)\n");
            return;
        }
        for (int i = 2; i < 10; i++) {
            if (!((token[i] >= '0' && token[i] <= '9') || 
                  (token[i] >= 'A' && token[i] <= 'F') ||
                  (token[i] >= 'a' && token[i] <= 'f'))) {
                printf("값은 0~9, A~F만 허용됩니다.\n");
                return;
            }
        }
        fullwrite(token);
    }
    else if (strcmp(token, "testapp1") == 0) {
        testapp1();
    }
    else if (strcmp(token, "testapp2") == 0) {
        testapp2();
    }
    else if (strcmp(token, "testapp3") == 0) {  // NEW
        testapp3();
    }
    else if (strcmp(token, "stats") == 0) {  // NEW
        ssd_print_statistics();
    }
    else if (strcmp(token, "l2p") == 0) {  // NEW
        ssd_print_l2p_table();
    }
    else if (strcmp(token, "gc") == 0) {  // NEW
        ssd_force_gc();
    }
    else {
        printf("알 수 없는 명령어입니다. 'help'를 입력하세요.\n");
    }
}

int main(int argc, char* argv[]) {
    printf("========================================\n");
    printf("  SSD Simulator with FTL & GC\n");
    printf("  Type 'help' for available commands\n");
    printf("========================================\n\n");
    
    while (1) {
        printf("ssd> ");
        char cmd[1000];
        fgets(cmd, sizeof(cmd), stdin);
        cmd[strcspn(cmd, "\n")] = '\0';  // 줄바꿈 문자 제거
        
        if (strcmp(cmd, "exit") == 0) {
            printf("Shutting down SSD simulator...\n");
            ssd_shutdown();
            printf("Goodbye!\n");
            return 0;
        }
        
        executecommand(cmd);
        printf("\n");
    }
}
