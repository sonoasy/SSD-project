# Makefile

CC = gcc
CFLAGS = -Wall -g  # 디버그 정보 포함
TARGET = testapp

# 소스 파일
SRC = ssd.c testshell.c
OBJ = $(SRC:.c=.o)

# 기본 빌드 규칙
all: $(TARGET)

# 실행 파일 생성
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

# 개별 파일 컴파일
ssd.o: ssd.c ssd.h
	$(CC) $(CFLAGS) -c ssd.c

testshell.o: testshell.c ssd.h
	$(CC) $(CFLAGS) -c testshell.c

# 클린 규칙
clean:
	rm -f $(OBJ) $(TARGET)

# 테스트 실행
test: $(TARGET)
	./$(TARGET) < test_commands.txt
