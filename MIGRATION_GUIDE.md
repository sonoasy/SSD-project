# SSD-project 업그레이드 가이드

## 방법 1: 기존 프로젝트 완전 교체 (추천)

### Step 1: 기존 저장소 클론
```bash
cd ~/workspace  # 또는 원하는 작업 디렉토리
git clone https://github.com/sonoasy/SSD-project.git
cd SSD-project
```

### Step 2: 기존 파일 백업
```bash
# backup 디렉토리 생성
mkdir -p backup/original

# 기존 파일들 백업
mv ssd.c backup/original/
mv ssd.h backup/original/
mv testshell.c backup/original/
mv Makefile backup/original/
mv run_test.sh backup/original/

# README.md는 나중에 업데이트할 예정이므로 백업만
cp README.md backup/original/
```

### Step 3: 새 파일들 복사
다운로드한 파일들을 프로젝트 디렉토리에 복사:

```bash
# 다운로드 디렉토리에서 (Downloads 폴더 경로는 환경에 따라 다를 수 있음)
cd ~/Downloads  # 또는 파일을 다운로드한 위치

# SSD-project 디렉토리로 복사
cp nand_flash.h ~/workspace/SSD-project/
cp nand_flash.c ~/workspace/SSD-project/
cp ftl.h ~/workspace/SSD-project/
cp ftl.c ~/workspace/SSD-project/
cp ssd_new.h ~/workspace/SSD-project/ssd.h  # 이름 변경
cp ssd_new.c ~/workspace/SSD-project/ssd.c  # 이름 변경
cp testshell_new.c ~/workspace/SSD-project/testshell.c  # 이름 변경
cp Makefile ~/workspace/SSD-project/
cp README_NEW.md ~/workspace/SSD-project/README.md  # 기존 README 교체
```

### Step 4: 파일 이름 정리 (중요!)
ssd.c에서 헤더 파일 include 수정:

```bash
cd ~/workspace/SSD-project

# ssd.c 파일 열어서 다음 부분 수정
# Before: #include "ssd_new.h"
# After:  #include "ssd.h"
```

에디터로 `ssd.c` 파일을 열고:
```c
// 기존
#include "ssd_new.h"

// 변경
#include "ssd.h"
```

마찬가지로 `testshell.c` 파일도 수정:
```c
// 기존
#include "ssd_new.h"

// 변경
#include "ssd.h"
```

### Step 5: Makefile 업데이트
`Makefile`을 열어서 파일 이름 수정:

```makefile
# Before
SOURCES = testshell_new.c ssd_new.c ftl.c nand_flash.c
HEADERS = ssd_new.h ftl.h nand_flash.h

# After
SOURCES = testshell.c ssd.c ftl.c nand_flash.c
HEADERS = ssd.h ftl.h nand_flash.h
```

### Step 6: 빌드 테스트
```bash
make clean
make

# 실행 테스트
./ssd_simulator

# 프롬프트가 나오면
ssd> help
ssd> testapp1
ssd> exit
```

### Step 7: Git 커밋 및 푸시
```bash
# 변경사항 확인
git status

# 새 파일들 추가
git add nand_flash.h nand_flash.c ftl.h ftl.c
git add ssd.h ssd.c testshell.c
git add Makefile README.md

# 기존 파일 삭제 (백업은 이미 했으므로)
git rm backup/original/ssd.c backup/original/ssd.h backup/original/testshell.c
# 또는 backup 디렉토리는 .gitignore에 추가

# 커밋
git commit -m "feat: Upgrade to production-level SSD simulator with FTL & GC

Major changes:
- Implement NAND Flash hardware constraints (Page/Block, No Overwrite)
- Add Flash Translation Layer (FTL) with L2P mapping
- Implement Garbage Collection with Greedy victim selection
- Add WAF (Write Amplification Factor) calculation
- Maintain backward compatibility with original interface
- Add new debug commands (stats, l2p, gc)
- Add TestApp3 for GC verification"

# GitHub에 푸시
git push origin main
```

---

## 방법 2: 새 브랜치로 분리 (기존 코드 보존)

기존 코드를 `main` 브랜치에 남기고, 새 코드를 `advanced-ftl` 브랜치로 관리:

### Step 1-2: 동일 (저장소 클론 및 백업)

### Step 3: 새 브랜치 생성
```bash
cd ~/workspace/SSD-project
git checkout -b advanced-ftl
```

### Step 4-6: 동일 (파일 복사, 수정, 빌드 테스트)

### Step 7: 브랜치 푸시
```bash
git add .
git commit -m "feat: Add advanced FTL implementation"
git push -u origin advanced-ftl
```

이제 GitHub에서:
- `main` 브랜치: 기존 단순 버전
- `advanced-ftl` 브랜치: 업그레이드 버전

면접 때는 "두 버전 모두 구현했고, 점진적으로 발전시킨 과정을 보여줄 수 있습니다"라고 어필!

---

## 방법 3: 디렉토리 분리

하나의 저장소에 두 버전 모두 유지:

```
SSD-project/
├── basic/           # 기존 버전
│   ├── ssd.c
│   ├── ssd.h
│   └── testshell.c
├── advanced/        # 업그레이드 버전
│   ├── nand_flash.c/h
│   ├── ftl.c/h
│   ├── ssd.c/h
│   └── testshell.c
└── README.md        # 두 버전 모두 설명
```

### 구현
```bash
cd ~/workspace/SSD-project

# 기존 파일을 basic 디렉토리로 이동
mkdir basic
git mv ssd.c ssd.h testshell.c Makefile basic/

# advanced 디렉토리 생성 및 새 파일 복사
mkdir advanced
cp ~/Downloads/nand_flash.* advanced/
cp ~/Downloads/ftl.* advanced/
cp ~/Downloads/ssd_new.* advanced/  # 이름 변경 필요
cp ~/Downloads/testshell_new.c advanced/testshell.c
cp ~/Downloads/Makefile advanced/

git add advanced/
git commit -m "feat: Add advanced FTL version alongside basic version"
git push
```

---

## 추천하는 최종 프로젝트 구조

```
SSD-project/
├── README.md                    # 프로젝트 전체 설명 (업그레이드 버전 위주)
├── Makefile                     # 빌드 시스템
├── .gitignore                   # nand_flash.bin, *.o, ssd_simulator 제외
│
├── nand_flash.h                 # NAND Flash 하드웨어 계층
├── nand_flash.c
├── ftl.h                        # Flash Translation Layer
├── ftl.c
├── ssd.h                        # SSD 인터페이스 (기존 호환)
├── ssd.c
├── testshell.c                  # 테스트 쉘
│
├── docs/                        # (선택) 문서화
│   ├── consistency_analysis.md  # 일관성 위험 분석
│   └── architecture.md          # 아키텍처 설계 문서
│
└── backup/                      # (로컬만) 기존 코드 백업
    └── original/
        ├── ssd.c
        ├── ssd.h
        └── testshell.c
```

---

## .gitignore 파일 생성

```bash
cd ~/workspace/SSD-project
cat > .gitignore << 'EOF'
# Build outputs
*.o
ssd_simulator
ssd

# NAND persistent storage
nand_flash.bin
result.txt
nand.txt

# Backup (local only)
backup/

# IDE files
.vscode/
.idea/
*.swp
*~

# macOS
.DS_Store
EOF

git add .gitignore
git commit -m "chore: Add .gitignore"
```

---

## README.md 업데이트 전략

기존 README를 확장해서 "Before/After" 비교 추가:

```markdown
# SSD-project

## Overview
가상 SSD 시뮬레이션 및 테스트 프로그램 (Production-level Upgrade)

## Version History
- **v1.0** (Original): 기본 read/write 기능
- **v2.0** (Current): NAND Flash 제약 + FTL + GC 구현

## Features (v2.0)
✅ NAND Flash 물리적 제약 (Page/Block, No Overwrite)
✅ Flash Translation Layer (L2P mapping)
✅ Garbage Collection (Greedy victim selection)
✅ WAF (Write Amplification Factor) 계산
✅ 데이터 영속성 (nand_flash.bin)
✅ 기존 인터페이스 100% 호환

## Quick Start
```bash
make
./ssd_simulator
```

(나머지 내용은 README_NEW.md 참고)
```

---

## 면접 때 보여줄 GitHub 페이지 구성

1. **README.md**: 
   - 한눈에 보이는 기능 목록
   - Before/After 비교 이미지 (선택)
   - 빌드 및 실행 방법

2. **코드 하이라이트**:
   - `ftl.c`의 GC 로직 부분
   - `nand_flash.c`의 덮어쓰기 금지 체크

3. **Issues/Projects** (선택):
   - "Future Improvements" 이슈 생성
   - 멀티스레드, WAF 최적화 등 TODO 작성

---

## 최종 체크리스트

- [ ] 기존 코드 백업 완료
- [ ] 새 파일들 복사 및 이름 변경
- [ ] ssd.c, testshell.c에서 헤더 include 수정
- [ ] Makefile 파일명 업데이트
- [ ] `make clean && make` 성공
- [ ] `./ssd_simulator` 실행 및 `testapp1/2/3` 동작 확인
- [ ] .gitignore 생성
- [ ] Git 커밋 메시지 작성
- [ ] GitHub 푸시
- [ ] GitHub 페이지에서 README 확인

---

완료하면 GitHub 링크 공유해주세요. 제가 확인해드릴게요!
