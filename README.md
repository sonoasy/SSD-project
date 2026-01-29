# SSD-project - 업그레이드 버전

## 프로젝트 개요

기존 단순 key-value 방식의 SSD 시뮬레이터를 **실무 수준**으로 업그레이드한 프로젝트입니다.

### 주요 개선 사항

1. **NAND Flash 물리적 제약 구현**
   - Page 단위 프로그래밍 (2KB)
   - Block 단위 삭제 (128 pages/block)
   - **No Overwrite**: 덮어쓰기 금지 (실제 NAND Flash 동작 재현)

2. **FTL (Flash Translation Layer) 구현**
   - L2P (Logical-to-Physical) 매핑 테이블
   - Garbage Collection (GC) with Greedy victim selection
   - WAF (Write Amplification Factor) 계산

3. **데이터 영속성**
   - 프로그램 종료 후에도 데이터 보존 (`nand_flash.bin`)
   - 재실행 시 기존 상태 자동 복구

---

## 파일 구조

```
SSD-project/
├── nand_flash.c/h       # 물리적 NAND Flash 시뮬레이션 (Page/Block 제약)
├── ftl.c/h              # Flash Translation Layer (L2P, GC, WAF)
├── ssd_new.c/h          # 기존 인터페이스 호환 레이어
├── testshell_new.c      # 향상된 테스트 쉘 (기존 + 새 명령어)
├── Makefile             # 빌드 시스템
└── README.md            # 이 문서
```

---

## 빌드 및 실행

### 빌드
```bash
make
```

### 실행
```bash
./ssd_simulator
```

### 자동 테스트
```bash
make test     # TestApp1, 2, 3 자동 실행
make stats    # 통계 출력
```

---

## 사용 가능한 명령어

### 기본 I/O 명령어 (기존 호환)
- `W <idx> <data>`: 특정 LBA에 쓰기 (예: `W 3 0xAAAABBBB`)
- `R <idx>`: 특정 LBA에서 읽기 (예: `R 3`)
- `fullwrite <data>`: 모든 LBA(0~99)에 동일 데이터 쓰기
- `fullread`: 모든 LBA(0~99) 읽기

### 테스트 애플리케이션
- `testapp1`: Full Write/Read 검증
- `testapp2`: Aging Write 및 Over Write 검증 (WAF 확인)
- `testapp3`: **[NEW]** GC 동작 검증 (반복 덮어쓰기로 invalid page 생성)

### 디버깅 명령어 (NEW)
- `stats`: FTL 및 NAND 통계 출력 (WAF, GC 횟수 등)
- `l2p`: L2P 매핑 테이블 출력
- `gc`: 강제 GC 발동
- `help`: 모든 명령어 목록
- `exit`: 프로그램 종료 (자동 영속성 저장)

---

## 실행 예시

```bash
$ ./ssd_simulator
========================================
  SSD Simulator with FTL & GC
  Type 'help' for available commands
========================================

ssd> W 0 0xDEADBEEF
[FTL] Initialization complete (Logical Pages: 100)
[SSD] FTL initialized
[SSD] Write success: LBA 0 <- 0xDEADBEEF

ssd> R 0
[SSD] Read success: LBA 0 -> 0xDEADBEEF

ssd> testapp2
[TestApp2] Aging Write 시작 (LBA 0~5에 30번 반복 쓰기)
...
=== Aging 후 통계 ===

========== FTL Statistics ==========
Total Host Writes:   180
Total NAND Writes:   180
Total GC Count:      0
Write Amplification: 1.00x
Free Pages:          12620 / 12800
====================================

ssd> stats
...

ssd> exit
Shutting down SSD simulator...
[FTL] Shutting down...
Goodbye!
```

---

## 핵심 기술 설명

### 1. NAND Flash 제약 구현

**문제**: 기존 코드는 `nand.txt`에 직접 덮어쓰기 → 실제 NAND와 동작 방식 다름

**해결**: 
- `nand_flash.c`에서 Page 상태 관리 (FREE/VALID/INVALID)
- 덮어쓰기 시도 시 **에러 반환** → FTL이 GC로 해결

```c
// CRITICAL: 덮어쓰기 금지 (NAND Flash 제약)
if (page->oob.state != PAGE_FREE) {
    fprintf(stderr, "[NAND] Cannot overwrite! Need erase first!\n");
    return -1;
}
```

### 2. FTL (Flash Translation Layer)

**L2P 매핑 테이블**:
- 사용자가 보는 논리 주소(LBA)를 물리 주소(PBA)로 변환
- 동일 LBA에 재쓰기 시: 새 PBA에 쓰고 기존 PBA는 INVALID로 마킹

```c
// 기존 페이지 무효화 (덮어쓰기 대신)
ftl_invalidate_old_page(ftl, lba);

// 새 위치에 쓰기
uint32_t pba = ftl_find_free_page(ftl);
nand_write_page(&ftl->nand, pba, data, lba);

// L2P 테이블 업데이트
ftl->l2p_table[lba] = pba;
```

### 3. Garbage Collection (GC)

**발동 조건**: Free pages가 부족할 때 자동 발동

**동작 순서**:
1. **Victim 선택**: Invalid page가 가장 많은 블록 선택 (Greedy 전략)
2. **Valid 데이터 이동**: 해당 블록의 유효한 페이지들을 새 위치로 복사
3. **Block 삭제**: 전체 블록을 삭제하여 free pages 확보
4. **L2P 업데이트**: 이동된 페이지의 매핑 정보 갱신

```c
// GC 핵심 로직
uint32_t victim = ftl_select_victim_block(ftl);  // Invalid page 가장 많은 블록
ftl_gc_one_block(ftl, victim);                    // Valid 데이터 이동
nand_erase_block(&ftl->nand, victim);             // 블록 삭제
```

### 4. WAF (Write Amplification Factor)

**정의**: 실제 NAND에 쓰인 데이터량 / 사용자가 요청한 쓰기량

**의미**:
- WAF = 1.0x: 이상적 (GC 없음)
- WAF = 3.0x: 사용자가 1GB 쓸 때 실제 NAND는 3GB 쓰기 (GC 때문)
- WAF가 높을수록 SSD 수명 단축

**계산**:
```c
double waf = (double)total_nand_writes / total_host_writes;
```

---

## 면접 어필 포인트

### 1. 시스템 레벨 이해도
- "단순 파일 I/O가 아닌 **하드웨어 제약을 코드로 모델링**했습니다"
- "NAND Flash는 덮어쓰기가 불가능하기 때문에, FTL이 이를 추상화해야 합니다"

### 2. 알고리즘 설계 능력
- "GC의 Victim Selection을 **Greedy 전략**으로 구현했고, Cost-Benefit 같은 다른 전략도 이해하고 있습니다"
- "WAF를 최소화하기 위해서는 Hot/Cold data separation이나 Over-provisioning 조절이 필요합니다"

### 3. 실무 연결성
- "TestApp2에서 30번 반복 쓰기로 **Aging 시뮬레이션**을 했습니다"
- "엔터프라이즈 SSD는 LDPC ECC, Power-Loss Protection, Wear Leveling 등을 추가로 구현합니다"

### 4. 코드 품질
- "기존 인터페이스는 **하위 호환성**을 유지하면서, 내부만 FTL로 교체했습니다"
- "Makefile로 빌드 자동화하고, 자동 테스트 스크립트를 작성했습니다"

---

## 다음 단계 (2단계 프롬프트 대응)

2단계에서 추가할 내용:
- **멀티스레드**: pthread로 Foreground I/O와 Background GC 분리
- **Lock Contention**: mutex로 L2P 테이블 동기화, 대기 시간 측정
- **성능 리포트**: GC 전후 Latency 비교

---

## 기술 스택

- **언어**: C (C99)
- **컴파일러**: GCC with `-Wall -Wextra -O2`
- **의존성**: 없음 (표준 라이브러리만 사용)
- **플랫폼**: Linux (Ubuntu 24 테스트 완료)

---

## 라이선스

MIT License (기존 프로젝트 계승)

---

## 작성자

Lisa (sonoasy) - 31살 신입 시스템 엔지니어
포트폴리오 프로젝트: 실무 수준 SSD 시뮬레이터

**면접 대비 핵심 질문 예상 답변**:
- Q: "왜 NAND Flash는 덮어쓰기가 안 되나요?"
  - A: "전하를 가두는 Floating Gate 구조 때문에, 삭제는 블록 단위로만 가능합니다"
- Q: "WAF를 줄이려면 어떻게 해야 하나요?"
  - A: "Over-provisioning 증가, Hot/Cold data 분리, 더 효율적인 GC 알고리즘 사용"
- Q: "L2P 테이블이 너무 크면 어떻게 하나요?"
  - A: "Demand-based FTL이나 Hybrid FTL로 일부만 DRAM에 캐싱"
