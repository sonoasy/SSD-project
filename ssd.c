/*
 * ssd.c - SSD Interface Layer (기존 인터페이스 유지)
 * 
 * 기존 프로젝트와의 호환성을 위해 동일한 함수 시그니처 유지
 * 내부는 FTL + NAND Flash로 구현
 */

#define _CRT_SECURE_NO_WARNINGS
#include "ssd_new.h"
#include "ftl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ==================== GLOBAL FTL INSTANCE ====================
static FTL g_ftl;
static int g_initialized = 0;

// ==================== INTERNAL HELPERS ====================

static void ensure_initialized() {
    if (!g_initialized) {
        ftl_init(&g_ftl);
        g_initialized = 1;
        printf("[SSD] FTL initialized\n");
    }
}

static void convert_hex_to_bytes(const char* hex_str, uint8_t* buffer) {
    // "0x12345678" -> bytes 배열로 변환
    // 기존 프로젝트는 4바이트 hex 값 사용
    unsigned int value;
    sscanf(hex_str, "0x%X", &value);
    
    // Little-endian으로 저장 (x86 호환)
    buffer[0] = (value >> 0) & 0xFF;
    buffer[1] = (value >> 8) & 0xFF;
    buffer[2] = (value >> 16) & 0xFF;
    buffer[3] = (value >> 24) & 0xFF;
    
    // 나머지는 0으로 채움 (PAGE_SIZE까지)
    memset(buffer + 4, 0, PAGE_SIZE - 4);
}

static unsigned int convert_bytes_to_hex(const uint8_t* buffer) {
    // bytes 배열 -> unsigned int
    unsigned int value = 0;
    value |= ((unsigned int)buffer[0] << 0);
    value |= ((unsigned int)buffer[1] << 8);
    value |= ((unsigned int)buffer[2] << 16);
    value |= ((unsigned int)buffer[3] << 24);
    return value;
}

// ==================== PUBLIC API (기존 인터페이스 유지) ====================

void write(int idx, char* data) {
    ensure_initialized();
    
    if (idx < 0 || idx >= TOTAL_LOGICAL_PAGES) {
        printf("[SSD] 할당된 범위 밖입니다 (0~%d)\n", TOTAL_LOGICAL_PAGES - 1);
        return;
    }
    
    // Hex string을 바이트 배열로 변환
    uint8_t buffer[PAGE_SIZE];
    convert_hex_to_bytes(data, buffer);
    
    // FTL을 통해 쓰기
    if (ftl_write(&g_ftl, (uint32_t)idx, buffer) == 0) {
        printf("[SSD] Write success: LBA %d <- %s\n", idx, data);
    } else {
        printf("[SSD] Write failed: LBA %d\n", idx);
    }
}

unsigned int read(int idx) {
    ensure_initialized();
    
    if (idx < 0 || idx >= TOTAL_LOGICAL_PAGES) {
        printf("[SSD] 할당된 범위 밖입니다 (0~%d)\n", TOTAL_LOGICAL_PAGES - 1);
        return 0;
    }
    
    // FTL을 통해 읽기
    uint8_t buffer[PAGE_SIZE];
    if (ftl_read(&g_ftl, (uint32_t)idx, buffer) == 0) {
        unsigned int value = convert_bytes_to_hex(buffer);
        
        // 기존 프로젝트와의 호환성: result.txt에 저장
        FILE* rfp = fopen("result.txt", "w+");
        if (rfp) {
            fprintf(rfp, "0x%08X\n", value);
            fclose(rfp);
        }
        
        printf("[SSD] Read success: LBA %d -> 0x%08X\n", idx, value);
        return value;
    } else {
        printf("[SSD] Read failed: LBA %d (no data)\n", idx);
        return 0;
    }
}

// ==================== EXTENDED API (새로운 기능) ====================

void ssd_print_statistics() {
    ensure_initialized();
    ftl_print_statistics(&g_ftl);
    nand_print_statistics(&g_ftl.nand);
}

void ssd_print_l2p_table() {
    ensure_initialized();
    ftl_print_l2p_table(&g_ftl);
}

void ssd_force_gc() {
    ensure_initialized();
    printf("[SSD] Forcing Garbage Collection...\n");
    ftl_trigger_gc(&g_ftl);
}

void ssd_shutdown() {
    if (g_initialized) {
        printf("[SSD] Shutting down...\n");
        ftl_cleanup(&g_ftl);
        g_initialized = 0;
    }
}
