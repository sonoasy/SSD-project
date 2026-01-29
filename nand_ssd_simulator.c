/*
 * NAND Flash SSD Simulator - Step 1: Basic Architecture
 * 
 * Hardware Constraints Implementation:
 * - No overwrite: Pages can only be written once before erase
 * - Page-level programming (2KB + OOB)
 * - Block-level erasing (128 pages per block)
 * - L2P mapping table for address translation
 * - Persistent storage via binary file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

// ==================== HARDWARE CONFIGURATION ====================
#define PAGE_SIZE           2048        // 2KB data area
#define OOB_SIZE            64          // Out-Of-Band metadata
#define PAGES_PER_BLOCK     128
#define TOTAL_BLOCKS        1024        // Total 256MB storage
#define TOTAL_PAGES         (TOTAL_BLOCKS * PAGES_PER_BLOCK)

#define STORAGE_FILE        "nand_storage.bin"
#define L2P_FILE            "l2p_table.bin"

// ==================== DATA STRUCTURES ====================

// Page state in OOB area
typedef enum {
    PAGE_FREE = 0,      // Erased, ready to write
    PAGE_VALID = 1,     // Contains valid data
    PAGE_INVALID = 2    // Outdated data (after update)
} PageState;

// Out-Of-Band metadata (64 bytes)
typedef struct {
    PageState state;
    uint32_t lba;           // Logical Block Address mapped to this page
    uint32_t write_count;   // Program/Erase cycle counter
    uint8_t ecc[52];        // Error Correction Code placeholder
    uint32_t timestamp;     // Write timestamp
} OOB;

// Physical page structure
typedef struct {
    uint8_t data[PAGE_SIZE];
    OOB oob;
} Page;

// Physical block structure
typedef struct {
    Page pages[PAGES_PER_BLOCK];
    uint32_t erase_count;           // Block-level P/E cycle
    uint32_t invalid_page_count;    // For GC victim selection
} Block;

// Flash Translation Layer (FTL)
typedef struct {
    Block *blocks;                  // Physical storage array
    uint32_t *l2p_table;           // LBA to PBA mapping
    uint32_t total_logical_pages;
    uint32_t next_free_page;       // Current write pointer
    uint64_t total_host_writes;    // Statistics
    uint64_t total_nand_writes;
} FTL;

// ==================== GLOBAL VARIABLES ====================
FTL ftl;

// ==================== FORWARD DECLARATIONS ====================
void init_ftl(void);
void cleanup_ftl(void);
bool load_persistent_state(void);
void save_persistent_state(void);
int ftl_write(uint32_t lba, const uint8_t *data);
int ftl_read(uint32_t lba, uint8_t *data);
void invalidate_old_mapping(uint32_t lba);
uint32_t find_next_free_page(void);
void erase_block(uint32_t block_idx);
void print_statistics(void);

// ==================== FTL INITIALIZATION ====================

void init_ftl(void) {
    printf("[FTL] Initializing NAND Flash SSD Simulator...\n");
    
    // Allocate physical blocks
    ftl.blocks = (Block *)calloc(TOTAL_BLOCKS, sizeof(Block));
    if (!ftl.blocks) {
        fprintf(stderr, "[ERROR] Failed to allocate NAND blocks\n");
        exit(1);
    }
    
    // Initialize L2P table (over-provisioning: 90% logical space)
    ftl.total_logical_pages = (TOTAL_PAGES * 9) / 10;
    ftl.l2p_table = (uint32_t *)malloc(ftl.total_logical_pages * sizeof(uint32_t));
    if (!ftl.l2p_table) {
        fprintf(stderr, "[ERROR] Failed to allocate L2P table\n");
        exit(1);
    }
    
    // Initialize all mappings to invalid
    memset(ftl.l2p_table, 0xFF, ftl.total_logical_pages * sizeof(uint32_t));
    
    ftl.next_free_page = 0;
    ftl.total_host_writes = 0;
    ftl.total_nand_writes = 0;
    
    // Try to load persistent state
    if (!load_persistent_state()) {
        printf("[FTL] No persistent state found, initializing fresh NAND...\n");
        // Mark all pages as FREE
        for (uint32_t b = 0; b < TOTAL_BLOCKS; b++) {
            ftl.blocks[b].erase_count = 0;
            ftl.blocks[b].invalid_page_count = 0;
            for (uint32_t p = 0; p < PAGES_PER_BLOCK; p++) {
                ftl.blocks[b].pages[p].oob.state = PAGE_FREE;
                ftl.blocks[b].pages[p].oob.lba = 0xFFFFFFFF;
                ftl.blocks[b].pages[p].oob.write_count = 0;
            }
        }
    } else {
        printf("[FTL] Persistent state loaded successfully\n");
    }
    
    printf("[FTL] Initialization complete\n");
    printf("      Total Blocks: %d\n", TOTAL_BLOCKS);
    printf("      Logical Pages: %u (90%% provisioning)\n", ftl.total_logical_pages);
    printf("      Physical Pages: %d\n", TOTAL_PAGES);
}

void cleanup_ftl(void) {
    printf("\n[FTL] Shutting down, persisting state...\n");
    save_persistent_state();
    free(ftl.blocks);
    free(ftl.l2p_table);
}

// ==================== PERSISTENCE LAYER ====================

bool load_persistent_state(void) {
    FILE *fp_storage = fopen(STORAGE_FILE, "rb");
    FILE *fp_l2p = fopen(L2P_FILE, "rb");
    
    if (!fp_storage || !fp_l2p) {
        if (fp_storage) fclose(fp_storage);
        if (fp_l2p) fclose(fp_l2p);
        return false;
    }
    
    // Load NAND blocks
    size_t read_blocks = fread(ftl.blocks, sizeof(Block), TOTAL_BLOCKS, fp_storage);
    if (read_blocks != TOTAL_BLOCKS) {
        fclose(fp_storage);
        fclose(fp_l2p);
        return false;
    }
    
    // Load L2P table
    size_t read_l2p = fread(ftl.l2p_table, sizeof(uint32_t), 
                            ftl.total_logical_pages, fp_l2p);
    if (read_l2p != ftl.total_logical_pages) {
        fclose(fp_storage);
        fclose(fp_l2p);
        return false;
    }
    
    // Load metadata
    fread(&ftl.next_free_page, sizeof(uint32_t), 1, fp_l2p);
    fread(&ftl.total_host_writes, sizeof(uint64_t), 1, fp_l2p);
    fread(&ftl.total_nand_writes, sizeof(uint64_t), 1, fp_l2p);
    
    fclose(fp_storage);
    fclose(fp_l2p);
    return true;
}

void save_persistent_state(void) {
    FILE *fp_storage = fopen(STORAGE_FILE, "wb");
    FILE *fp_l2p = fopen(L2P_FILE, "wb");
    
    if (!fp_storage || !fp_l2p) {
        fprintf(stderr, "[ERROR] Failed to save persistent state\n");
        if (fp_storage) fclose(fp_storage);
        if (fp_l2p) fclose(fp_l2p);
        return;
    }
    
    // Save NAND blocks
    fwrite(ftl.blocks, sizeof(Block), TOTAL_BLOCKS, fp_storage);
    
    // Save L2P table and metadata
    fwrite(ftl.l2p_table, sizeof(uint32_t), ftl.total_logical_pages, fp_l2p);
    fwrite(&ftl.next_free_page, sizeof(uint32_t), 1, fp_l2p);
    fwrite(&ftl.total_host_writes, sizeof(uint64_t), 1, fp_l2p);
    fwrite(&ftl.total_nand_writes, sizeof(uint64_t), 1, fp_l2p);
    
    fclose(fp_storage);
    fclose(fp_l2p);
    printf("[FTL] Persistent state saved successfully\n");
}

// ==================== CORE FTL OPERATIONS ====================

int ftl_write(uint32_t lba, const uint8_t *data) {
    if (lba >= ftl.total_logical_pages) {
        fprintf(stderr, "[ERROR] LBA %u out of range\n", lba);
        return -1;
    }
    
    ftl.total_host_writes++;
    
    // Step 1: Invalidate old mapping (no overwrite allowed)
    invalidate_old_mapping(lba);
    
    // Step 2: Find next free page
    uint32_t pba = find_next_free_page();
    if (pba == 0xFFFFFFFF) {
        fprintf(stderr, "[ERROR] No free pages available (GC needed)\n");
        return -1;
    }
    
    // Step 3: Write data to physical page
    uint32_t block_idx = pba / PAGES_PER_BLOCK;
    uint32_t page_idx = pba % PAGES_PER_BLOCK;
    
    Page *target_page = &ftl.blocks[block_idx].pages[page_idx];
    memcpy(target_page->data, data, PAGE_SIZE);
    
    // Update OOB metadata
    target_page->oob.state = PAGE_VALID;
    target_page->oob.lba = lba;
    target_page->oob.write_count++;
    target_page->oob.timestamp = (uint32_t)time(NULL);
    
    // Step 4: Update L2P mapping
    ftl.l2p_table[lba] = pba;
    ftl.total_nand_writes++;
    
    printf("[WRITE] LBA %u -> PBA %u (Block %u, Page %u)\n", 
           lba, pba, block_idx, page_idx);
    
    return 0;
}

int ftl_read(uint32_t lba, uint8_t *data) {
    if (lba >= ftl.total_logical_pages) {
        fprintf(stderr, "[ERROR] LBA %u out of range\n", lba);
        return -1;
    }
    
    // Lookup PBA from L2P table
    uint32_t pba = ftl.l2p_table[lba];
    if (pba == 0xFFFFFFFF) {
        fprintf(stderr, "[ERROR] LBA %u not mapped (no data written)\n", lba);
        return -1;
    }
    
    uint32_t block_idx = pba / PAGES_PER_BLOCK;
    uint32_t page_idx = pba % PAGES_PER_BLOCK;
    
    Page *source_page = &ftl.blocks[block_idx].pages[page_idx];
    
    // Verify page state
    if (source_page->oob.state != PAGE_VALID) {
        fprintf(stderr, "[ERROR] Page at PBA %u is not valid\n", pba);
        return -1;
    }
    
    memcpy(data, source_page->data, PAGE_SIZE);
    printf("[READ] LBA %u <- PBA %u (Block %u, Page %u)\n", 
           lba, pba, block_idx, page_idx);
    
    return 0;
}

void invalidate_old_mapping(uint32_t lba) {
    uint32_t old_pba = ftl.l2p_table[lba];
    
    if (old_pba != 0xFFFFFFFF) {
        uint32_t block_idx = old_pba / PAGES_PER_BLOCK;
        uint32_t page_idx = old_pba % PAGES_PER_BLOCK;
        
        Page *old_page = &ftl.blocks[block_idx].pages[page_idx];
        old_page->oob.state = PAGE_INVALID;
        
        ftl.blocks[block_idx].invalid_page_count++;
        
        printf("[INVALIDATE] Old PBA %u for LBA %u (Block %u now has %u invalid pages)\n",
               old_pba, lba, block_idx, ftl.blocks[block_idx].invalid_page_count);
    }
}

uint32_t find_next_free_page(void) {
    // Sequential allocation strategy
    for (uint32_t i = 0; i < TOTAL_PAGES; i++) {
        uint32_t pba = (ftl.next_free_page + i) % TOTAL_PAGES;
        uint32_t block_idx = pba / PAGES_PER_BLOCK;
        uint32_t page_idx = pba % PAGES_PER_BLOCK;
        
        if (ftl.blocks[block_idx].pages[page_idx].oob.state == PAGE_FREE) {
            ftl.next_free_page = (pba + 1) % TOTAL_PAGES;
            return pba;
        }
    }
    
    return 0xFFFFFFFF; // No free page found
}

void erase_block(uint32_t block_idx) {
    if (block_idx >= TOTAL_BLOCKS) {
        fprintf(stderr, "[ERROR] Block index %u out of range\n", block_idx);
        return;
    }
    
    Block *block = &ftl.blocks[block_idx];
    block->erase_count++;
    block->invalid_page_count = 0;
    
    // Erase all pages in the block
    for (uint32_t p = 0; p < PAGES_PER_BLOCK; p++) {
        memset(&block->pages[p], 0xFF, sizeof(Page)); // Physical erase
        block->pages[p].oob.state = PAGE_FREE;
        block->pages[p].oob.lba = 0xFFFFFFFF;
        block->pages[p].oob.write_count = 0;
    }
    
    printf("[ERASE] Block %u erased (P/E cycles: %u)\n", 
           block_idx, block->erase_count);
}

// ==================== STATISTICS & DIAGNOSTICS ====================

void print_statistics(void) {
    uint32_t free_blocks = 0;
    uint32_t free_pages = 0;
    uint32_t valid_pages = 0;
    uint32_t invalid_pages = 0;
    
    for (uint32_t b = 0; b < TOTAL_BLOCKS; b++) {
        bool has_valid = false;
        for (uint32_t p = 0; p < PAGES_PER_BLOCK; p++) {
            switch (ftl.blocks[b].pages[p].oob.state) {
                case PAGE_FREE: free_pages++; break;
                case PAGE_VALID: valid_pages++; has_valid = true; break;
                case PAGE_INVALID: invalid_pages++; break;
            }
        }
        if (!has_valid) free_blocks++;
    }
    
    double waf = (ftl.total_host_writes > 0) ? 
                 (double)ftl.total_nand_writes / ftl.total_host_writes : 1.0;
    
    printf("\n========== SSD Statistics ==========\n");
    printf("Total Host Writes:   %lu\n", ftl.total_host_writes);
    printf("Total NAND Writes:   %lu\n", ftl.total_nand_writes);
    printf("Write Amplification: %.2fx\n", waf);
    printf("Free Blocks:         %u / %d (%.1f%%)\n", 
           free_blocks, TOTAL_BLOCKS, 100.0 * free_blocks / TOTAL_BLOCKS);
    printf("Free Pages:          %u / %d\n", free_pages, TOTAL_PAGES);
    printf("Valid Pages:         %u\n", valid_pages);
    printf("Invalid Pages:       %u\n", invalid_pages);
    printf("====================================\n");
}

// ==================== TEST SCENARIOS ====================

void test_basic_operations(void) {
    printf("\n[TEST] Basic Write/Read Operations\n");
    
    uint8_t write_data[PAGE_SIZE];
    uint8_t read_data[PAGE_SIZE];
    
    // Test 1: Write to LBA 0
    memset(write_data, 0xAA, PAGE_SIZE);
    ftl_write(0, write_data);
    
    // Test 2: Read from LBA 0
    ftl_read(0, read_data);
    if (memcmp(write_data, read_data, PAGE_SIZE) == 0) {
        printf("[PASS] Data integrity verified\n");
    } else {
        printf("[FAIL] Data mismatch\n");
    }
    
    // Test 3: Overwrite LBA 0 (should invalidate old page)
    memset(write_data, 0xBB, PAGE_SIZE);
    ftl_write(0, write_data);
    ftl_read(0, read_data);
    if (memcmp(write_data, read_data, PAGE_SIZE) == 0) {
        printf("[PASS] Overwrite handled correctly\n");
    } else {
        printf("[FAIL] Overwrite failed\n");
    }
}

void test_consistency_scenarios(void) {
    printf("\n[TEST] Consistency Edge Cases\n");
    
    // Edge case 1: Write beyond logical capacity
    printf("Test: Writing to out-of-range LBA...\n");
    uint8_t dummy[PAGE_SIZE] = {0};
    int result = ftl_write(ftl.total_logical_pages + 100, dummy);
    printf(result == -1 ? "[PASS] Rejected invalid LBA\n" : "[FAIL] Accepted invalid LBA\n");
    
    // Edge case 2: Read unmapped LBA
    printf("Test: Reading uninitialized LBA...\n");
    result = ftl_read(1000, dummy);
    printf(result == -1 ? "[PASS] Rejected unmapped read\n" : "[FAIL] Returned garbage data\n");
}

// ==================== MAIN ENTRY POINT ====================

int main(void) {
    init_ftl();
    
    test_basic_operations();
    test_consistency_scenarios();
    
    print_statistics();
    cleanup_ftl();
    
    return 0;
}
