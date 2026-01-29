/*
 * ftl.c - Flash Translation Layer Implementation
 * 
 * LBA -> PBA 매핑, Garbage Collection, WAF 계산
 */

#include "ftl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ==================== INITIALIZATION ====================

void ftl_init(FTL *ftl) {
    memset(ftl, 0, sizeof(FTL));
    
    // NAND Flash 초기화
    if (!nand_load_from_file(&ftl->nand, "nand_flash.bin")) {
        printf("[FTL] No persistent state found, initializing fresh NAND...\n");
        nand_init(&ftl->nand);
    } else {
        printf("[FTL] Persistent state loaded successfully\n");
    }
    
    // L2P 테이블 초기화 (0xFFFFFFFF = unmapped)
    for (int i = 0; i < TOTAL_LOGICAL_PAGES; i++) {
        ftl->l2p_table[i] = 0xFFFFFFFF;
    }
    
    // 기존 매핑 복구 (NAND의 OOB에서 LBA 정보 읽기)
    for (uint32_t pba = 0; pba < TOTAL_PAGES; pba++) {
        if (nand_get_page_state(&ftl->nand, pba) == PAGE_VALID) {
            uint32_t block_idx = pba / PAGES_PER_BLOCK;
            uint32_t page_idx = pba % PAGES_PER_BLOCK;
            uint32_t lba = ftl->nand.blocks[block_idx].pages[page_idx].oob.lba;
            
            if (lba < TOTAL_LOGICAL_PAGES) {
                ftl->l2p_table[lba] = pba;
            }
        }
    }
    
    ftl->next_free_page = 0;
    ftl->total_host_writes = 0;
    ftl->total_gc_count = 0;
    
    printf("[FTL] Initialization complete (Logical Pages: %d)\n", TOTAL_LOGICAL_PAGES);
}

void ftl_cleanup(FTL *ftl) {
    printf("[FTL] Shutting down...\n");
    nand_save_to_file(&ftl->nand, "nand_flash.bin");
}

// ==================== CORE I/O OPERATIONS ====================

int ftl_write(FTL *ftl, uint32_t lba, const uint8_t *data) {
    if (lba >= TOTAL_LOGICAL_PAGES) {
        fprintf(stderr, "[FTL] LBA %u out of range\n", lba);
        return -1;
    }
    
    ftl->total_host_writes++;
    
    // Step 1: 기존 페이지 무효화 (No Overwrite 제약 준수)
    ftl_invalidate_old_page(ftl, lba);
    
    // Step 2: Free page 찾기
    uint32_t pba = ftl_find_free_page(ftl);
    
    // Step 3: GC 필요 여부 확인
    if (pba == 0xFFFFFFFF) {
        printf("[FTL] No free pages, triggering GC...\n");
        ftl_trigger_gc(ftl);
        pba = ftl_find_free_page(ftl);
        
        if (pba == 0xFFFFFFFF) {
            fprintf(stderr, "[FTL] CRITICAL: GC failed, no space available\n");
            return -1;
        }
    }
    
    // Step 4: NAND에 물리적 쓰기
    if (nand_write_page(&ftl->nand, pba, data, lba) != 0) {
        fprintf(stderr, "[FTL] NAND write failed at PBA %u\n", pba);
        return -1;
    }
    
    // Step 5: L2P 테이블 업데이트
    ftl->l2p_table[lba] = pba;
    
    return 0;
}

int ftl_read(FTL *ftl, uint32_t lba, uint8_t *data) {
    if (lba >= TOTAL_LOGICAL_PAGES) {
        fprintf(stderr, "[FTL] LBA %u out of range\n", lba);
        return -1;
    }
    
    // L2P 테이블에서 PBA 조회
    uint32_t pba = ftl->l2p_table[lba];
    
    if (pba == 0xFFFFFFFF) {
        fprintf(stderr, "[FTL] LBA %u not mapped (no data written)\n", lba);
        return -1;
    }
    
    // NAND에서 데이터 읽기
    return nand_read_page(&ftl->nand, pba, data);
}

// ==================== GARBAGE COLLECTION ====================

void ftl_trigger_gc(FTL *ftl) {
    printf("[GC] Starting Garbage Collection...\n");
    ftl->total_gc_count++;
    
    // Victim 블록 선택 (Greedy 전략: invalid page가 가장 많은 블록)
    uint32_t victim_block_idx = ftl_select_victim_block(ftl);
    
    if (victim_block_idx == 0xFFFFFFFF) {
        fprintf(stderr, "[GC] No victim block found (all blocks are full of valid data)\n");
        return;
    }
    
    printf("[GC] Selected victim: Block %u (Invalid pages: %u)\n",
           victim_block_idx, nand_get_invalid_page_count(&ftl->nand, victim_block_idx));
    
    // 해당 블록의 valid 데이터를 새 위치로 이동
    ftl_gc_one_block(ftl, victim_block_idx);
    
    // 블록 삭제
    nand_erase_block(&ftl->nand, victim_block_idx);
    
    printf("[GC] Block %u erased successfully\n", victim_block_idx);
}

uint32_t ftl_select_victim_block(FTL *ftl) {
    uint32_t max_invalid_count = 0;
    uint32_t victim_block_idx = 0xFFFFFFFF;
    
    for (uint32_t b = 0; b < TOTAL_BLOCKS; b++) {
        uint32_t invalid_count = nand_get_invalid_page_count(&ftl->nand, b);
        
        // Invalid page가 0인 블록은 제외 (완전히 비어있거나 모두 유효)
        if (invalid_count > 0 && invalid_count > max_invalid_count) {
            max_invalid_count = invalid_count;
            victim_block_idx = b;
        }
    }
    
    return victim_block_idx;
}

void ftl_gc_one_block(FTL *ftl, uint32_t victim_block_idx) {
    uint8_t temp_buffer[PAGE_SIZE];
    
    // 블록 내의 모든 valid page를 새 위치로 복사
    for (uint32_t p = 0; p < PAGES_PER_BLOCK; p++) {
        uint32_t old_pba = victim_block_idx * PAGES_PER_BLOCK + p;
        
        if (nand_get_page_state(&ftl->nand, old_pba) == PAGE_VALID) {
            // Valid 데이터 읽기
            uint32_t lba = ftl->nand.blocks[victim_block_idx].pages[p].oob.lba;
            
            if (lba >= TOTAL_LOGICAL_PAGES) {
                continue; // Invalid LBA, skip
            }
            
            if (nand_read_page(&ftl->nand, old_pba, temp_buffer) != 0) {
                fprintf(stderr, "[GC] Failed to read PBA %u\n", old_pba);
                continue;
            }
            
            // 새 위치 찾기
            uint32_t new_pba = ftl_find_free_page(ftl);
            if (new_pba == 0xFFFFFFFF) {
                fprintf(stderr, "[GC] No free page during migration\n");
                return;
            }
            
            // 새 위치에 쓰기
            if (nand_write_page(&ftl->nand, new_pba, temp_buffer, lba) != 0) {
                fprintf(stderr, "[GC] Failed to write to PBA %u\n", new_pba);
                continue;
            }
            
            // L2P 테이블 업데이트
            ftl->l2p_table[lba] = new_pba;
            
            // 기존 페이지를 invalid로 마킹 (이미 invalid일 수도 있음)
            nand_set_page_state(&ftl->nand, old_pba, PAGE_INVALID);
            
            printf("[GC] Migrated LBA %u: PBA %u -> %u\n", lba, old_pba, new_pba);
        }
    }
}

// ==================== INTERNAL UTILITIES ====================

uint32_t ftl_find_free_page(FTL *ftl) {
    // Sequential allocation 전략
    for (uint32_t i = 0; i < TOTAL_PAGES; i++) {
        uint32_t pba = (ftl->next_free_page + i) % TOTAL_PAGES;
        
        if (nand_get_page_state(&ftl->nand, pba) == PAGE_FREE) {
            ftl->next_free_page = (pba + 1) % TOTAL_PAGES;
            return pba;
        }
    }
    
    return 0xFFFFFFFF; // No free page
}

void ftl_invalidate_old_page(FTL *ftl, uint32_t lba) {
    uint32_t old_pba = ftl->l2p_table[lba];
    
    if (old_pba != 0xFFFFFFFF) {
        // 기존 페이지를 invalid로 마킹
        nand_set_page_state(&ftl->nand, old_pba, PAGE_INVALID);
    }
}

double ftl_calculate_waf(FTL *ftl) {
    if (ftl->total_host_writes == 0) {
        return 1.0;
    }
    
    uint64_t total_nand_writes = ftl->nand.total_page_writes;
    return (double)total_nand_writes / ftl->total_host_writes;
}

// ==================== STATISTICS & DEBUGGING ====================

void ftl_print_statistics(FTL *ftl) {
    double waf = ftl_calculate_waf(ftl);
    
    printf("\n========== FTL Statistics ==========\n");
    printf("Total Host Writes:   %lu\n", ftl->total_host_writes);
    printf("Total NAND Writes:   %lu\n", ftl->nand.total_page_writes);
    printf("Total GC Count:      %lu\n", ftl->total_gc_count);
    printf("Write Amplification: %.2fx\n", waf);
    printf("Free Pages:          %u / %d\n", 
           nand_get_free_page_count(&ftl->nand), TOTAL_PAGES);
    printf("====================================\n");
}

void ftl_print_l2p_table(FTL *ftl) {
    printf("\n========== L2P Mapping Table ==========\n");
    for (int i = 0; i < TOTAL_LOGICAL_PAGES; i++) {
        if (ftl->l2p_table[i] != 0xFFFFFFFF) {
            printf("LBA %3d -> PBA %5u\n", i, ftl->l2p_table[i]);
        }
    }
    printf("=======================================\n");
}
