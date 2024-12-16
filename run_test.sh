#!/bin/bash

# 자동화된 빌드 및 테스트 수행

echo "자동화된 빌드를 시작합니다..."

# Makefile을 이용한 빌드
make clean
make all

# 빌드가 성공한 경우, 테스트 실행
if [ $? -eq 0 ]; then
    echo "빌드 성공, 테스트 실행..."
    make test  # 테스트 실행
else
    echo "빌드 실패"
    exit 1
fi

# 테스트 완료 후 클린
make clean
