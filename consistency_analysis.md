# NAND Flash SSD Simulator - 데이터 일관성(Consistency) 위험 분석

## 시스템 엔지니어 관점에서의 3가지 핵심 위험 요소

---

## 1. **Power-Loss 시나리오에서의 L2P 매핑 불일치 (Mapping Inconsistency)**

### 문제 정의
현재 구현은 `save_persistent_state()`를 프로그램 종료 시 `cleanup_ftl()`에서만 호출합니다. 만약 시스템이 비정상 종료되면 (예: 전원 차단, 프로세스 강제 종료), 메모리에 있는 L2P 테이블과 실제 NAND에 기록된 OOB 메타데이터가 불일치하게 됩니다.

### 발생 시나리오
```
시간 T0: LBA 100 -> PBA 50 (L2P 테이블 업데이트, 메모리)
시간 T1: 데이터 쓰기 완료 (NAND 물리적 쓰기)
시간 T2: [전원 차단 발생]
시간 T3: 시스템 재부팅
결과: L2P 테이블은 복구되지 않음, LBA 100은 "unmapped" 상태
```

### 실무적 해결 방안
- **Journaling 메커니즘**: 모든 매핑 변경 사항을 NAND의 특정 영역(예: Reserved Block)에 순차적으로 기록
- **Copy-on-Write (CoW)**: 매핑 테이블 자체를 NAND에 주기적으로 스냅샷 형태로 저장
- **Transaction Log**: Write 작업을 트랜잭션 단위로 묶어, 완료되지 않은 작업은 복구 시 롤백

### 면접 어필 포인트
> "실제 SSD 컨트롤러는 DRAM의 L2P 테이블을 주기적으로 NAND로 플러시하며, ZNS SSD 같은 경우 Zone 단위로 메타데이터를 관리해 복구 시간을 단축합니다."

---

## 2. **Race Condition in L2P Table Access (동시성 문제)**

### 문제 정의
현재 코드는 단일 스레드 기반이지만, 실제 SSD는 여러 I/O 요청을 동시에 처리합니다. 만약 두 스레드가 동일한 LBA에 대해 동시에 쓰기를 시도하면, L2P 테이블 업데이트와 페이지 무효화(invalidate) 작업이 원자적으로 처리되지 않아 다음 문제가 발생합니다:

### 발생 시나리오
```
Thread A: ftl_write(LBA=10) 시작
  -> invalidate_old_mapping(10) 호출, PBA 20을 INVALID로 설정
Thread B: ftl_write(LBA=10) 시작 (Thread A가 L2P 업데이트 전)
  -> invalidate_old_mapping(10) 호출, 동일한 PBA 20을 또 INVALID 처리
Thread A: L2P[10] = 새로운 PBA 30
Thread B: L2P[10] = 새로운 PBA 31 (Thread A의 쓰기를 덮어씀)

결과: PBA 30의 데이터는 orphan 상태 (L2P에서 참조되지 않음)
      Invalid page count가 실제보다 +1 증가 (이중 카운팅)
```

### 실무적 해결 방안
- **Per-LBA Fine-grained Locking**: 각 LBA마다 별도의 lock을 두어 특정 주소에 대한 동시 쓰기만 차단
- **Lock-free Data Structures**: Atomic CAS(Compare-And-Swap) 연산으로 L2P 테이블을 무잠금으로 업데이트
- **Request Serialization**: 동일 LBA에 대한 요청을 큐에 넣고 순차 처리

### 면접 어필 포인트
> "실제 NVMe SSD는 Submission Queue와 Completion Queue를 사용해 병렬 처리하며, Intel Optane 같은 경우 다층 캐시 일관성 프로토콜(MESI)을 차용합니다."

---

## 3. **Partial Write Failure와 Orphaned Pages (부분 실패로 인한 고아 페이지)**

### 문제 정의
현재 구현에서 `ftl_write()` 함수는 다음 순서로 동작합니다:
1. 기존 페이지 무효화 (`invalidate_old_mapping`)
2. 새 페이지 찾기 (`find_next_free_page`)
3. 데이터 쓰기 및 OOB 업데이트
4. L2P 테이블 업데이트

만약 3번 단계에서 하드웨어 오류(Bad Block 발생, ECC 실패)가 발생하면, 1번에서 이미 무효화된 기존 페이지는 복구할 수 없고, 4번의 L2P 업데이트는 실행되지 않아 데이터가 영구 손실됩니다.

### 발생 시나리오
```
1. 사용자가 LBA 500에 데이터 쓰기 요청
2. 기존 PBA 1000을 INVALID로 설정 (데이터 손실 시작)
3. 새로운 PBA 2000에 쓰기 시도 -> [하드웨어 오류 발생]
4. L2P[500]은 여전히 PBA 1000을 가리키지만, 해당 페이지는 INVALID 상태
5. 다음 read 시도 시 "Page is not valid" 에러 발생
```

### 실무적 해결 방안
- **Write-Before-Invalidate**: 새 페이지에 쓰기가 성공적으로 완료된 후에만 기존 페이지를 무효화
- **Bad Block Management (BBM)**: 불량 블록을 미리 감지하고 예비 블록(spare block)으로 대체
- **RAID 5/6 in SSD**: 패리티 정보를 분산 저장해 일부 페이지 손상 시에도 복구 가능

### 면접 어필 포인트
> "엔터프라이즈 SSD는 End-to-End Data Protection을 위해 DIF(Data Integrity Field)를 사용하며, LDPC(Low-Density Parity-Check) 같은 강력한 ECC로 Raw BER 10^-15 수준까지 보정합니다."

---

## 추가 고려사항: WAF(Write Amplification Factor) 증가로 인한 수명 단축

### 문제 정의
현재 구현은 Sequential allocation으로 free page를 찾지만, Random Write 패턴에서는 많은 블록에 invalid page가 분산되어 GC 효율이 급격히 떨어집니다. 이는 WAF를 3배 이상 증가시켜 SSD 수명을 크게 단축시킵니다.

### 실무적 해결 방안
- **Hot/Cold Data Separation**: 자주 업데이트되는 데이터는 별도 영역에 저장
- **Dynamic Wear Leveling**: P/E cycle이 적은 블록을 우선 사용해 수명 평준화
- **Over-Provisioning 튜닝**: 여유 공간을 20% 이상 확보해 GC 빈도 감소

---

## 결론: 실무 SSD 개발에서의 핵심 과제

이 3가지 위험 요소는 모두 "**원자성(Atomicity)**"과 "**내구성(Durability)**"의 부족에서 발생합니다. 실제 상용 SSD는 다음 기술로 해결합니다:

1. **Power-Loss Protection (PLP)**: 슈퍼커패시터로 전원 차단 시 DRAM 내용을 NAND로 플러시
2. **Firmware-level ACID Transactions**: 데이터베이스의 트랜잭션 개념을 FTL에 적용
3. **Multi-level Error Correction**: LDPC → Soft-decision decoding → RAID recovery 순차 적용

면접에서 이런 내용을 언급하면, "하드웨어 제약을 이해하고 시스템 레벨 설계 능력이 있는 신입"으로 강하게 어필할 수 있습니다.
