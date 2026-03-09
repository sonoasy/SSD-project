/*
 * testshell.c - Enhanced Test Shell
 * 
 * 기존 명령어 유지 + FTL 기능 추가
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include "ssd.h"
#include <stdlib.h>
#include <string.h>
#include "ftl.h"   // FTL 타입 알기 위해
extern FTL g_ftl;  // 다른 .c 파일에 있는 전역 변수 사용 선언


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
/*
void testapp4() {
    // 전체 LBA 0~999를 50라운드 반복 쓰기
    // 100 * 50 = 5000번 쓰기 → GC 발동 유도
    for (int round = 0; round < 130; round++) {
        for (int i = 0; i < 1000; i++) {
            int lba = (i + round * 13) % 1000;  // 13씩 offset
	    char data[20];
            sprintf(data, "0x%08X", round * 1000 + i);
            write(lba, data);
        }
    }
    ssd_print_statistics();
}
*/
/*void testapp4() {
    srand(42);
    for (int i = 0; i < 10000; i++) {
        int lba = rand() % 100;
        char data[20];
        sprintf(data, "0x%08X", i);
        write(lba, data);
    }
    ssd_print_statistics();
}

void testapp4() {
    //srand(42);
    for (int round = 0; round < 10000; round++) {
        int write_count = 50 + (round % 7) * 13;  // 라운드마다 다른 개수
        srand(42+round);
	for (int i = 0; i < write_count; i++) {
            //int lba = rand() % 100;
            //if (rand()%100 < 90) lba = rand()%100;          // hot
            //else lba = 100 + rand()%900;    // cold
	    int r = rand() % 100;
            int lba;// (r < 80) ? (rand()%16) : (rand()%100); // 80%는 0~15 
            if (r < 80) lba = rand() % 16;           // hot: 0~15
            else lba = 16 + rand() % 84;      // cold: 16~99 (hot 제외) 
	    char data[20];
            sprintf(data, "0x%08X", round * 100 + i);
            write(lba, data);
        }
    }
    ssd_print_statistics();
}


void testapp4() {
    for (int round = 0; round < 100000; round++) {
        int write_count = 50 + (round % 7) * 13;
        srand(42 + round);

        int base, len;
        if (round % 2 == 0) { base = 1;   len = 100; }   // 1~100
        else                { base = 300; len = 101; }   // 300~400

        for (int i = 0; i < write_count; i++) {
            int r = rand() % 100;
            int lba;

            // hot: 16개, cold: 나머지 (len-16개)
            if (r < 80) {
                lba = base + (rand() % 16);              // hot: base~base+15
            } else {
                lba = base + 16 + (rand() % (len - 16)); // cold: base+16 ~ base+len-1
            }

            char data[20];
            sprintf(data, "0x%08X", round * 100 + i);
            write(lba, data);
        }
    }
    ssd_print_statistics();
}

*/
/*
void testapp4() {
    const int MAX_ROUND = 100000;   // 10만 라운드
    const int STEP = 5000;          // 5000 라운드마다 체크

    
    
    long long prev_host = 0;
    long long prev_nand = 0;

    for (int round = 0; round < MAX_ROUND; round++) {
        int write_count = 50 + (round % 7) * 13;
        srand(42 + round);

        int base, len;
        if (round % 2 == 0) { base = 1;   len = 100; }   // 1~100
        else                { base = 300; len = 101; }   // 300~400 (포함이라 101)

        for (int i = 0; i < write_count; i++) {
            int r = rand() % 100;
            int lba;

            // hot: 16개( base~base+15 )가 80%
            // cold: 나머지( base+16~base+len-1 )가 20%
            if (r < 80) lba = base + (rand() % 16);
            else        lba = base + 16 + (rand() % (len - 16));

            char data[20];
            sprintf(data, "0x%08X", round * 100 + i);
            write(lba, data);
        }

        //  5000 라운드마다 interval WAF 계산 + ASCII 그래프 출력
        if ((round + 1) % STEP == 0) {
    //        long long cur_host = (long long)g_ftl.total_host_writes;
  //          long long cur_nand = (long long)g_ftl.nand.total_page_writes;
//
    //        long long dh = cur_host - prev_host;
  //          long long dn = cur_nand - prev_nand;
//
    //        double interval_waf = (dh > 0) ? ((double)dn / (double)dh) : 0.0;
  //          double total_waf    = (cur_host > 0) ? ((double)cur_nand / (double)cur_host) : 0.0;
//
        //    // 막대 길이: (waf-1.0)*50 정도로 시각화
      //      int bar = (int)((interval_waf - 1.0) * 50.0);
    //        if (bar < 0) bar = 0;
  //          if (bar > 80) bar = 80;
//
        //    printf("%6d  total=%.4f  interval=%.4f  dh=%lld dn=%lld  |",
      //             round + 1, total_waf, interval_waf, dh, dn);
    //       for (int k = 0; k < bar; k++) putchar('#');
  //          putchar('\n');
//
  //          prev_host = cur_host;
//            prev_nand = cur_nand;
              ssd_print_statistics();
       	}
    }

    // 마지막에 전체 통계 한 번
    //ssd_print_statistics();

   
}
*/

void testapp4() {
    const int MAX_ROUND = 5000;
    const int STEP = 250;
    const int SAMPLE_CNT = MAX_ROUND / STEP;

    double interval_waf[SAMPLE_CNT];
    double total_waf[SAMPLE_CNT];

    uint64_t prev_host = 0;
    uint64_t prev_nand = 0;
    int idx = 0;
    srand(42); 
    for (int round = 0; round < MAX_ROUND; round++) {

        int write_count = 50 + (round % 7) * 13;
        //srand(42 + round);

        int base, len;
        if (round % 2 == 0) { base = 1;   len = 100; }
        else                { base = 300; len = 101; }

        for (int i = 0; i < write_count; i++) {
            int r = rand() % 100;
            int lba;//=rand()%1000;

            if (r < 80) lba =(rand() % 153);
            else lba = 153 + (rand() % 747);

            char data[20];
            sprintf(data, "0x%08X", round * 100 + i);
            write(lba, data);
        }

        // 라운드마다 저장
        if ((round + 1) % STEP == 0) {

            uint64_t cur_host = g_ftl.total_host_writes;
            uint64_t cur_nand = g_ftl.nand.total_page_writes;

            uint64_t dh = cur_host - prev_host;
            uint64_t dn = cur_nand - prev_nand;

            interval_waf[idx] = (dh ? (double)dn / (double)dh : 0.0);
            total_waf[idx]    = (cur_host ? (double)cur_nand / (double)cur_host : 0.0);

            prev_host = cur_host;
            prev_nand = cur_nand;
            idx++;
        }
    }

    printf("\n====== WAF Summary ======\n");
    for (int i = 0; i < SAMPLE_CNT; i++) {
        printf("%6d round  total=%.4f  interval=%.4f\n",
               (i+1)*STEP, total_waf[i], interval_waf[i]);
    }
}

/*
void testapp4() {
    const int MAX_ROUND = 5000;
    const int STEP = 250;
    const int SAMPLE_CNT = MAX_ROUND / STEP;

    double interval_waf[SAMPLE_CNT];
    double total_waf[SAMPLE_CNT];

    uint64_t prev_host = 0, prev_nand = 0;
    int idx = 0;

    // ✅ seed는 딱 1번만
    srand(42);

    // ✅ hot/cold 설정
    const int LBA_TOTAL = 1100;
    const int HOT_SIZE  = 176;        // hot 영역: 0~175
    const int COLD_BASE = 176;        // cold 영역 시작
    const int COLD_SIZE = LBA_TOTAL - HOT_SIZE; // 924

    // cold에 "약간의 순차성" 주기 위한 커서
    int cold_cursor = 0;

    for (int round = 0; round < MAX_ROUND; round++) {

        int write_count = 50 + (round % 7) * 13;

        for (int i = 0; i < write_count; i++) {

            // ✅ hot 확률 80% (원하면 90으로 올려도 됨)
            int is_hot = (rand() % 100) < 80;

            int lba;
            if (is_hot) {
                // hot은 작은 범위에 강하게 몰기
                lba = rand() % HOT_SIZE;
            } else {
                // cold는 완전 랜덤 대신 "지역성" 추가:
                //  - 70%: cold_cursor 기준으로 근처(작은 윈도우)에서 선택
                //  - 30%: cold 전체에서 랜덤
                if ((rand() % 100) < 70) {
                    int window = 16; // 근처 16개 정도
                    int offset = (rand() % window);
                    lba = COLD_BASE + (cold_cursor + offset) % COLD_SIZE;
                    cold_cursor = (cold_cursor + 1) % COLD_SIZE;
                } else {
                    lba = COLD_BASE + (rand() % COLD_SIZE);
                }
            }

            char data[20];
            sprintf(data, "0x%08X", round * 100 + i);
            write(lba, data);
        }

        if ((round + 1) % STEP == 0) {
            uint64_t cur_host = g_ftl.total_host_writes;
            uint64_t cur_nand = g_ftl.nand.total_page_writes;

            uint64_t dh = cur_host - prev_host;
            uint64_t dn = cur_nand - prev_nand;

            interval_waf[idx] = (dh ? (double)dn / (double)dh : 0.0);
            total_waf[idx]    = (cur_host ? (double)cur_nand / (double)cur_host : 0.0);

            prev_host = cur_host;
            prev_nand = cur_nand;
            idx++;
        }
    }

    printf("\n====== WAF Summary ======\n");
    for (int i = 0; i < SAMPLE_CNT; i++) {
        printf("%6d round  total=%.4f  interval=%.4f\n",
               (i+1)*STEP, total_waf[i], interval_waf[i]);
    }
}
*/
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
        
        if (idx < 0 || idx > 999) {
            printf("할당된 범위 밖입니다 (0~999)\n");
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
        
        if (idx < 0 || idx > 999) {
            printf("할당된 범위 밖입니다 (0~999)\n");
            return;
        }
        
        read(idx);
    }
    else if (strcmp(token, "help") == 0) {
        printf("==================== 사용 가능한 명령어 ====================\n");
        printf("기본 명령어:\n");
        printf("  W <idx> <data>   - 특정 LBA에 쓰기 (예: W 3 0xAAAABBBB)\n");
        printf("  R <idx>          - 특정 LBA에서 읽기 (예: R 3)\n");
        printf("  fullwrite <data> - 모든 LBA(0~999)에 동일 데이터 쓰기\n");
        printf("  fullread         - 모든 LBA(0~999) 읽기\n");
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
    else if (strcmp(token, "testapp4") == 0) {
    testapp4();
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
