#!/bin/bash
# setup_project.sh - SSD-project 자동 업그레이드 스크립트

set -e  # 에러 발생 시 중단

echo "=========================================="
echo "  SSD-project Auto-upgrade Script"
echo "=========================================="
echo ""

# 현재 디렉토리 확인
if [ ! -f "ssd.c" ] && [ ! -f "testshell.c" ]; then
    echo "ERROR: 이 스크립트는 SSD-project 디렉토리에서 실행해야 합니다."
    echo "현재 위치: $(pwd)"
    exit 1
fi

# Step 1: 백업 디렉토리 생성
echo "[Step 1] 기존 파일 백업 중..."
mkdir -p backup/original
cp -f ssd.c backup/original/ 2>/dev/null || true
cp -f ssd.h backup/original/ 2>/dev/null || true
cp -f testshell.c backup/original/ 2>/dev/null || true
cp -f Makefile backup/original/ 2>/dev/null || true
cp -f README.md backup/original/ 2>/dev/null || true
echo "✓ 백업 완료: backup/original/"

# Step 2: 새 파일이 현재 디렉토리에 있는지 확인
if [ ! -f "nand_flash.c" ]; then
    echo ""
    echo "[Step 2] 새 파일 위치 입력"
    echo "다운로드한 파일들이 있는 디렉토리 경로를 입력하세요:"
    read -r DOWNLOAD_DIR
    
    if [ ! -d "$DOWNLOAD_DIR" ]; then
        echo "ERROR: 디렉토리를 찾을 수 없습니다: $DOWNLOAD_DIR"
        exit 1
    fi
    
    # 파일 복사
    echo "파일 복사 중..."
    cp "$DOWNLOAD_DIR/nand_flash.h" .
    cp "$DOWNLOAD_DIR/nand_flash.c" .
    cp "$DOWNLOAD_DIR/ftl.h" .
    cp "$DOWNLOAD_DIR/ftl.c" .
    cp "$DOWNLOAD_DIR/ssd_new.h" ./ssd.h
    cp "$DOWNLOAD_DIR/ssd_new.c" ./ssd.c
    cp "$DOWNLOAD_DIR/testshell_new.c" ./testshell.c
    cp "$DOWNLOAD_DIR/Makefile" .
    cp "$DOWNLOAD_DIR/README_NEW.md" ./README.md
    cp "$DOWNLOAD_DIR/.gitignore" .
    echo "✓ 파일 복사 완료"
else
    echo "[Step 2] 새 파일이 이미 있습니다. 건너뛰기..."
fi

# Step 3: 헤더 include 수정
echo ""
echo "[Step 3] 헤더 파일 include 자동 수정..."

# ssd.c 수정
if grep -q 'ssd_new.h' ssd.c 2>/dev/null; then
    sed -i 's/#include "ssd_new.h"/#include "ssd.h"/g' ssd.c
    echo "✓ ssd.c 수정 완료"
fi

# testshell.c 수정
if grep -q 'ssd_new.h' testshell.c 2>/dev/null; then
    sed -i 's/#include "ssd_new.h"/#include "ssd.h"/g' testshell.c
    echo "✓ testshell.c 수정 완료"
fi

# Step 4: Makefile 수정
echo ""
echo "[Step 4] Makefile 업데이트..."
if grep -q 'testshell_new.c' Makefile 2>/dev/null; then
    sed -i 's/testshell_new.c/testshell.c/g' Makefile
    sed -i 's/ssd_new.c/ssd.c/g' Makefile
    sed -i 's/ssd_new.h/ssd.h/g' Makefile
    echo "✓ Makefile 수정 완료"
fi

# Step 5: 빌드 테스트
echo ""
echo "[Step 5] 빌드 테스트 중..."
make clean > /dev/null 2>&1 || true
if make; then
    echo "✓ 빌드 성공!"
else
    echo "✗ 빌드 실패. 수동으로 확인이 필요합니다."
    exit 1
fi

# Step 6: 실행 테스트
echo ""
echo "[Step 6] 프로그램 실행 테스트..."
if [ -f "./ssd_simulator" ]; then
    echo "✓ ssd_simulator 생성 완료"
    echo ""
    echo "간단한 테스트를 실행하시겠습니까? (y/n)"
    read -r RESPONSE
    if [ "$RESPONSE" = "y" ] || [ "$RESPONSE" = "Y" ]; then
        echo "help" | ./ssd_simulator
    fi
else
    echo "✗ 실행 파일이 생성되지 않았습니다."
    exit 1
fi

# Step 7: Git 안내
echo ""
echo "=========================================="
echo "  설치 완료!"
echo "=========================================="
echo ""
echo "다음 단계:"
echo "1. Git 상태 확인:"
echo "   git status"
echo ""
echo "2. 변경사항 커밋:"
echo "   git add ."
echo "   git commit -m 'feat: Upgrade to production-level SSD simulator with FTL & GC'"
echo ""
echo "3. GitHub에 푸시:"
echo "   git push origin main"
echo ""
echo "4. 프로그램 실행:"
echo "   ./ssd_simulator"
echo ""
echo "백업 위치: backup/original/"
echo "=========================================="
