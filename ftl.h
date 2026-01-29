/*
 * ftl.h - Flash Translation Layer
 * 
 * LBA(Logical Block Address) -> PBA(Physical Block Address) 매핑
 * Garbage Collection, WAF 계산 등 FTL 핵심 기능 구현
 */

#ifndef FTL_H
#define FTL_H

#include "nand_flash.h"
#include <stdint.h>
#include <stdbool.h>

// ==================== FTL CONFIGURATION ====================
#define TOTAL_LOGICAL_PAGES     100     // 기존 프로젝트와 호환 (LBA 0~99)
#define GC_THRESHOLD            10      // Free pages가 10% 이하일 때 GC 발동

// ==================== DATA STRUCTURES ====================

typedef struct {
    NANDFlash nand;                     // 물리적 NAND Flash
    uint32_t l2p_table[TOTAL_LOGICAL_PAGES];  // LBA -> PBA 매핑 테이블
    uint32_t next_free_page;            // 다음 쓰기 위치 (순차 할당)
    
    // 통계
    uint64_t total_host_writes;         // 호스트가 요청한 쓰기 수
    uint64_t total_gc_count;            // GC 발동 횟수
} FTL;

// ==================== FUNCTION PROTOTYPES ====================

// 초기화 및 종료
void ftl_init(FTL *ftl);
void ftl_cleanup(FTL *ftl);

// 기본 I/O (기존 ssd.c 인터페이스와 호환)
int ftl_write(FTL *ftl, uint32_t lba, const uint8_t *data);
int ftl_read(FTL *ftl, uint32_t lba, uint8_t *data);

// Garbage Collection
void ftl_trigger_gc(FTL *ftl);
uint32_t ftl_select_victim_block(FTL *ftl);
void ftl_gc_one_block(FTL *ftl, uint32_t victim_block_idx);

// 내부 유틸리티
uint32_t ftl_find_free_page(FTL *ftl);
void ftl_invalidate_old_page(FTL *ftl, uint32_t lba);
double ftl_calculate_waf(FTL *ftl);

// 통계 및 디버깅
void ftl_print_statistics(FTL *ftl);
void ftl_print_l2p_table(FTL *ftl);

#endif // FTL_H
