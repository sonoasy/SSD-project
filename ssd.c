#define _CRT_SECURE_NO_WARNINGS
#include "ssd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void write(int idx, char* data) {
    FILE* fp = fopen("nand.txt", "r+");  // 기존 데이터를 유지하면서 특정 위치에 덮어쓰기
    if (fp == NULL) {
        // 파일이 없으면 새로 생성
        fp = fopen("nand.txt", "w+");
        if (fp == NULL) {
            printf("파일 생성 실패\n");
            return;
        }
    }
    fseek(fp, idx * 12, SEEK_SET);
    unsigned int value = (unsigned int)strtoul(data, NULL, 16); // 문자열을 unsigned int로 변환
    fprintf(fp, "0x%08X\n", value);
    fflush(fp);
    fclose(fp);
   
}

unsigned int read(int idx) {
    FILE* fp = fopen("nand.txt", "r");
    if (fp == NULL) {
        printf("저장된 값이 없습니다");
        return;
    }

    FILE* rfp = fopen("result.txt", "w+");
    if (rfp == NULL) {
        printf("결과 파일 생성 실패");
        fclose(fp);
        return;
    }

    char buffer[100];
    fseek(fp, idx * 12, SEEK_SET);
    fgets(buffer, sizeof(buffer), fp);

    // buffer에서 "0x"를 제거하고 숫자 부분만 변환
    unsigned int value;
    sscanf(buffer, "0x%X", &value); // 문자열을 정수로 변환
    fprintf(rfp, "0x%08X\n", value);

    fclose(rfp);
    fclose(fp);
  
    return value;
}
