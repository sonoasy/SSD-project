/*
 * ssd.h - SSD Interface (기존 + 확장)
 */

#pragma once
#ifndef SSD_H
#define SSD_H

// ==================== 기존 인터페이스 (testshell.c 호환) ====================
unsigned int read(int idx);      // read 함수 원형
void write(int idx, char* data); // write 함수 원형

// ==================== 확장 기능 (디버깅 및 통계) ====================
void ssd_print_statistics();     // FTL + NAND 통계 출력
void ssd_print_l2p_table();      // L2P 매핑 테이블 출력
void ssd_force_gc();             // 강제 GC 발동
void ssd_shutdown();             // 종료 시 영속성 저장

#endif // SSD_H
