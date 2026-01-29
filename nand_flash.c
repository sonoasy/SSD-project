/*
 * nand_flash.c - NAND Flash Hardware Simulation
 * 
 * 실제 NAND Flash 동작을 충실히 재현
 */

#include "nand_flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ==================== INITIALIZATION ====================

void nand_init(NANDFlash *nand) {
    memset(nand, 0, sizeof(NANDFlash));
    
    // 모든 페이지를 FREE 상태로 초기화
    for (uint32_t b = 0; b < TOTAL_BLOCKS; b++) {
        nand->blocks[b].erase_count = 0;
        nand->blocks[b].invalid_page_count = 0;
        
        for (uint32_t p = 0; p < PAGES_PER_BLOCK; p++) {
            nand->blocks[b].pages[p].oob.state = PAGE_FREE;
            nand->blocks[b].pages[p].oob.lba = 0xFFFFFFFF;
            nand->blocks[b].pages[p].oob.write_count = 0;
            nand->blocks[b].pages[p].oob.timestamp = 0;
        }
    }
    
    nand->total_page_writes = 0;
    nand->total_block_erases = 0;
}

void nand_cleanup(NANDFlash *nand) {
    // 영속성을 위해 파일에 저장
    nand_save_to_file(nand, "nand_flash.bin");
}

// ==================== PERSISTENCE ====================

bool nand_load_from_file(NANDFlash *nand, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return false;
    }
    
    size_t read_bytes = fread(nand, sizeof(NANDFlash), 1, fp);
    fclose(fp);
    
    return (read_bytes == 1);
}

void nand_save_to_file(NANDFlash *nand, const char *filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "[NAND] Failed to save to %s\n", filename);
        return;
    }
    
    fwrite(nand, sizeof(NANDFlash), 1, fp);
    fclose(fp);
}

// ==================== CORE NAND OPERATIONS ====================

int nand_write_page(NANDFlash *nand, uint32_t pba, const uint8_t *data, uint32_t lba) {
    if (pba >= TOTAL_PAGES) {
        fprintf(stderr, "[NAND] PBA %u out of range\n", pba);
        return -1;
    }
    
    uint32_t block_idx = pba / PAGES_PER_BLOCK;
    uint32_t page_idx = pba % PAGES_PER_BLOCK;
    Page *page = &nand->blocks[block_idx].pages[page_idx];
    
    // CRITICAL: 덮어쓰기 금지 (NAND Flash 제약)
    if (page->oob.state != PAGE_FREE) {
        fprintf(stderr, "[NAND] ERROR: Cannot overwrite PBA %u (state=%d). Need erase first!\n",
                pba, page->oob.state);
        return -1;
    }
    
    // 데이터 쓰기
    memcpy(page->data, data, PAGE_SIZE);
    
    // OOB 메타데이터 업데이트
    page->oob.state = PAGE_VALID;
    page->oob.lba = lba;
    page->oob.write_count++;
    page->oob.timestamp = (uint32_t)time(NULL);
    
    nand->total_page_writes++;
    
    return 0;
}

int nand_read_page(NANDFlash *nand, uint32_t pba, uint8_t *data) {
    if (pba >= TOTAL_PAGES) {
        fprintf(stderr, "[NAND] PBA %u out of range\n", pba);
        return -1;
    }
    
    uint32_t block_idx = pba / PAGES_PER_BLOCK;
    uint32_t page_idx = pba % PAGES_PER_BLOCK;
    Page *page = &nand->blocks[block_idx].pages[page_idx];
    
    if (page->oob.state != PAGE_VALID) {
        fprintf(stderr, "[NAND] Cannot read invalid page at PBA %u\n", pba);
        return -1;
    }
    
    memcpy(data, page->data, PAGE_SIZE);
    return 0;
}

void nand_erase_block(NANDFlash *nand, uint32_t block_idx) {
    if (block_idx >= TOTAL_BLOCKS) {
        fprintf(stderr, "[NAND] Block %u out of range\n", block_idx);
        return;
    }
    
    Block *block = &nand->blocks[block_idx];
    
    // 모든 페이지를 FREE 상태로 초기화
    for (uint32_t p = 0; p < PAGES_PER_BLOCK; p++) {
        memset(&block->pages[p], 0xFF, sizeof(Page)); // 물리적 삭제 시뮬레이션
        block->pages[p].oob.state = PAGE_FREE;
        block->pages[p].oob.lba = 0xFFFFFFFF;
        block->pages[p].oob.write_count = 0;
    }
    
    block->erase_count++;
    block->invalid_page_count = 0;
    nand->total_block_erases++;
}

// ==================== PAGE STATE MANAGEMENT ====================

PageState nand_get_page_state(NANDFlash *nand, uint32_t pba) {
    if (pba >= TOTAL_PAGES) {
        return PAGE_FREE; // 안전한 기본값
    }
    
    uint32_t block_idx = pba / PAGES_PER_BLOCK;
    uint32_t page_idx = pba % PAGES_PER_BLOCK;
    
    return nand->blocks[block_idx].pages[page_idx].oob.state;
}

void nand_set_page_state(NANDFlash *nand, uint32_t pba, PageState state) {
    if (pba >= TOTAL_PAGES) {
        return;
    }
    
    uint32_t block_idx = pba / PAGES_PER_BLOCK;
    uint32_t page_idx = pba % PAGES_PER_BLOCK;
    
    PageState old_state = nand->blocks[block_idx].pages[page_idx].oob.state;
    nand->blocks[block_idx].pages[page_idx].oob.state = state;
    
    // invalid page count 업데이트
    if (old_state != PAGE_INVALID && state == PAGE_INVALID) {
        nand->blocks[block_idx].invalid_page_count++;
    } else if (old_state == PAGE_INVALID && state != PAGE_INVALID) {
        nand->blocks[block_idx].invalid_page_count--;
    }
}

// ==================== UTILITY FUNCTIONS ====================

uint32_t nand_get_free_page_count(NANDFlash *nand) {
    uint32_t count = 0;
    
    for (uint32_t b = 0; b < TOTAL_BLOCKS; b++) {
        for (uint32_t p = 0; p < PAGES_PER_BLOCK; p++) {
            if (nand->blocks[b].pages[p].oob.state == PAGE_FREE) {
                count++;
            }
        }
    }
    
    return count;
}

uint32_t nand_get_invalid_page_count(NANDFlash *nand, uint32_t block_idx) {
    if (block_idx >= TOTAL_BLOCKS) {
        return 0;
    }
    
    return nand->blocks[block_idx].invalid_page_count;
}

void nand_print_statistics(NANDFlash *nand) {
    uint32_t free_pages = 0;
    uint32_t valid_pages = 0;
    uint32_t invalid_pages = 0;
    
    for (uint32_t b = 0; b < TOTAL_BLOCKS; b++) {
        for (uint32_t p = 0; p < PAGES_PER_BLOCK; p++) {
            switch (nand->blocks[b].pages[p].oob.state) {
                case PAGE_FREE: free_pages++; break;
                case PAGE_VALID: valid_pages++; break;
                case PAGE_INVALID: invalid_pages++; break;
            }
        }
    }
    
    printf("\n========== NAND Flash Statistics ==========\n");
    printf("Total Page Writes:   %lu\n", nand->total_page_writes);
    printf("Total Block Erases:  %lu\n", nand->total_block_erases);
    printf("Free Pages:          %u / %d (%.1f%%)\n", 
           free_pages, TOTAL_PAGES, 100.0 * free_pages / TOTAL_PAGES);
    printf("Valid Pages:         %u\n", valid_pages);
    printf("Invalid Pages:       %u\n", invalid_pages);
    printf("===========================================\n");
}
