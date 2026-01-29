# 🚀 빠른 시작 가이드 (5분 완성)

## 📋 준비물
- [ ] GitHub 계정
- [ ] Git 설치됨
- [ ] 다운로드한 파일들 (9개)

---

## ⚡ 가장 빠른 방법 (자동 스크립트)

```bash
# 1. GitHub에서 기존 프로젝트 클론
cd ~/workspace
git clone https://github.com/sonoasy/SSD-project.git
cd SSD-project

# 2. 다운로드한 파일들을 프로젝트 디렉토리에 복사
# (Downloads 폴더에서 드래그&드롭 또는 cp 명령어)

# 3. 자동 설치 스크립트 실행
chmod +x setup_project.sh
./setup_project.sh

# 4. Git 커밋 및 푸시
git add .
git commit -m "feat: Upgrade to production-level SSD simulator"
git push
```

**완료!** 이제 https://github.com/sonoasy/SSD-project 에서 확인 가능

---

## 🔧 수동 방법 (세밀한 제어)

### 1️⃣ 프로젝트 준비
```bash
cd ~/workspace
git clone https://github.com/sonoasy/SSD-project.git
cd SSD-project
```

### 2️⃣ 백업
```bash
mkdir -p backup/original
mv ssd.c ssd.h testshell.c Makefile backup/original/
```

### 3️⃣ 파일 복사
다운로드 폴더에서 복사:
```
nand_flash.h → SSD-project/
nand_flash.c → SSD-project/
ftl.h → SSD-project/
ftl.c → SSD-project/
ssd_new.h → SSD-project/ssd.h      (이름 변경!)
ssd_new.c → SSD-project/ssd.c      (이름 변경!)
testshell_new.c → SSD-project/testshell.c (이름 변경!)
Makefile → SSD-project/
README_NEW.md → SSD-project/README.md (이름 변경!)
.gitignore → SSD-project/
```

### 4️⃣ 코드 수정
**ssd.c** 파일 열기:
```c
// 수정 전
#include "ssd_new.h"

// 수정 후
#include "ssd.h"
```

**testshell.c** 파일 열기:
```c
// 수정 전
#include "ssd_new.h"

// 수정 후
#include "ssd.h"
```

**Makefile** 파일 열기:
```makefile
# 수정 전
SOURCES = testshell_new.c ssd_new.c ftl.c nand_flash.c
HEADERS = ssd_new.h ftl.h nand_flash.h

# 수정 후
SOURCES = testshell.c ssd.c ftl.c nand_flash.c
HEADERS = ssd.h ftl.h nand_flash.h
```

### 5️⃣ 빌드 테스트
```bash
make clean
make

# 성공하면
./ssd_simulator
```

프롬프트가 나오면:
```
ssd> help
ssd> testapp1
ssd> exit
```

### 6️⃣ Git 커밋
```bash
git status  # 변경사항 확인

git add .
git commit -m "feat: Upgrade to production-level SSD simulator with FTL & GC

Major changes:
- Implement NAND Flash hardware constraints
- Add Flash Translation Layer with L2P mapping
- Implement Garbage Collection
- Add WAF calculation
- Maintain backward compatibility"

git push origin main
```

---

## ✅ 최종 파일 구조 확인

```
SSD-project/
├── README.md          ✓
├── Makefile           ✓
├── .gitignore         ✓
├── nand_flash.h       ✓
├── nand_flash.c       ✓
├── ftl.h              ✓
├── ftl.c              ✓
├── ssd.h              ✓
├── ssd.c              ✓
├── testshell.c        ✓
└── backup/
    └── original/
        ├── ssd.c
        ├── ssd.h
        └── testshell.c
```

---

## 🐛 문제 해결

### "No such file or directory: ssd_new.h"
→ ssd.c와 testshell.c에서 `#include "ssd_new.h"`를 `#include "ssd.h"`로 수정

### "undefined reference to ftl_init"
→ Makefile에서 SOURCES에 `ftl.c nand_flash.c`가 포함되었는지 확인

### 빌드는 되는데 실행이 안 됨
→ `./ssd_simulator` (앞에 `./` 필수)

### Git push가 안 됨
→ `git remote -v`로 원격 저장소 확인
→ SSH 키 설정 필요하면: https://docs.github.com/en/authentication

---

## 📞 도움이 필요하면

1. 빌드 에러 로그 확인: `make 2>&1 | tee build.log`
2. GitHub Issues에 질문 올리기
3. 나한테 물어봐! (Claude)

---

## 🎯 다음 단계

업그레이드 완료 후:
1. GitHub README 확인
2. 2단계 (멀티스레드) 진행 여부 결정
3. 면접 준비: 코드 리뷰 연습

**Good luck!** 🚀
