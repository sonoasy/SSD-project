/*
 * nand_flash.h - NAND Flash Hardware Abstraction Layer
 * 
 * 실제 NAND Flash의 물리적 제약을 구현:
 * - Page 단위 프로그래밍 (2KB)
 * - Block 단위 삭제 (128 pages)
 * - No Overwrite (덮어쓰기 금지)
 */

#ifndef NAND_FLASH_H
#define NAND_FLASH_H

#include <stdint.h>
#include <stdbool.h>

// ==================== HARDWARE CONFIGURATION ====================
#define PAGE_SIZE           2048        // 2KB data per page
#define OOB_SIZE            64          // Out-Of-Band metadata
#define PAGES_PER_BLOCK     128
#define TOTAL_BLOCKS        100         // 기존 프로젝트와 호환 (100개 LBA 지원)
#define TOTAL_PAGES         (TOTAL_BLOCKS * PAGES_PER_BLOCK)

// ==================== DATA STRUCTURES ====================

// Page 상태 (OOB 영역에 저장)
typedef enum {
    PAGE_FREE = 0,      // 삭제됨, 쓰기 가능
    PAGE_VALID = 1,     // 유효한 데이터 포함
    PAGE_INVALID = 2    // 오래된 데이터 (업데이트 후)
} PageState;

// Out-Of-Band 메타데이터
typedef struct {
    PageState state;
    uint32_t lba;           // 이 페이지가 매핑된 논리 주소
    uint32_t write_count;   // P/E cycle 카운터
    uint32_t timestamp;     // 쓰기 시각
} OOB;

// Physical Page 구조
typedef struct {
    uint8_t data[PAGE_SIZE];
    OOB oob;
} Page;

// Physical Block 구조
typedef struct {
    Page pages[PAGES_PER_BLOCK];
    uint32_t erase_count;           // Block-level P/E cycle
    uint32_t invalid_page_count;    // GC victim selection용
} Block;

// NAND Flash 전체 구조
typedef struct {
    Block blocks[TOTAL_BLOCKS];
    uint64_t total_page_writes;     // 통계
    uint64_t total_block_erases;
} NANDFlash;

// ==================== FUNCTION PROTOTYPES ====================

// 초기화 및 종료
void nand_init(NANDFlash *nand);
void nand_cleanup(NANDFlash *nand);

// 영속성 (파일 저장/로드)
bool nand_load_from_file(NANDFlash *nand, const char *filename);
void nand_save_to_file(NANDFlash *nand, const char *filename);

// NAND 기본 연산 (하드웨어 제약 엄수)
int nand_write_page(NANDFlash *nand, uint32_t pba, const uint8_t *data, uint32_t lba);
int nand_read_page(NANDFlash *nand, uint32_t pba, uint8_t *data);
void nand_erase_block(NANDFlash *nand, uint32_t block_idx);

// Page 상태 관리
PageState nand_get_page_state(NANDFlash *nand, uint32_t pba);
void nand_set_page_state(NANDFlash *nand, uint32_t pba, PageState state);

// 유틸리티
uint32_t nand_get_free_page_count(NANDFlash *nand);
uint32_t nand_get_invalid_page_count(NANDFlash *nand, uint32_t block_idx);
void nand_print_statistics(NANDFlash *nand);

#endif // NAND_FLASH_H
