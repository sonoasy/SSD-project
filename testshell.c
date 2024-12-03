#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include "ssd.h" 
#include <stdlib.h>
#include<string.h>

void fullwrite(char* data) {

	
	for (int idx = 0; idx < 100; idx++) {
		write(idx, data);  // 모든 LBA에 같은 값을 기록
	}
}

void fullread() {  
	for (int idx = 0; idx < 100; idx++) {
		printf("0x%08X\n",read(idx));  // 모든 LBA에서 값을 읽음
	}
	
}

void testapp1() {
	char* test_value = "0xABCDFFFF";

	// Full Write 수행
	fullwrite(test_value);
	printf("Full Write 완료: %s\n", test_value);

	// Full Read 및 비교 수행
	printf("Full Read 및 값 검증 중...\n");
	fullread();
	//전체에 oxABCDFFFF
}

void testapp2() {
	char* aging_value = "0xAAAABBBB";
	char* overwrite_value = "0x12345678";

	// 0~5 LBA에 30번 Write
	for (int i = 0; i < 30; i++) {
		for (int idx = 0; idx < 6; idx++) {
			write(idx, aging_value);
		}
	}
	printf("Aging Write 완료: %s\n", aging_value);

	// 0~5 LBA에 1번 Over Write
	for (int idx = 0; idx < 6; idx++) {
		write(idx, overwrite_value);
	}
	printf("Over Write 완료: %s\n", overwrite_value);

	// Read 및 비교 수행
	printf("값 검증 중...\n");
	for (int idx = 0; idx < 6; idx++) {
		read(idx);
	}
}

void executecommand(char *cmd) {

	//write  ssd 3 0xAAAABBBB  
	//read   ssd R 2

	//read write면 cmd를 쪼개야함 
	
	printf("%s", cmd);

	char* token = strtok(cmd, " ");
	
	if (token == NULL) {
		printf("명령어가 잘못되었습니다.\n");
		return;
	}
	
	if (strcmp(token,"W") == 0) {
		
		token = strtok(NULL, " ");  // idx 가져옴

		int idx = atoi(token);  // 세 번째 토큰 (예: 3)

		token = strtok(NULL, " ");  // 데이터 가져옴 (예: 0xAAAABBBB)
		char* data = token;

		if (idx < 0 || idx > 99) {
			printf("할당된 범위 밖입니다\n");
			return;
		}
		if (strlen(data) != 10) {
			printf("값이 잘못 입력되었습니다\n");
			return;
		}
		write(idx, data);
		printf("쓰기 성공");
	}
	else if (strcmp(token, "R") == 0) {
		// read 명령 처리

		token = strtok(NULL, " ");  // idx 가져옴

		int idx = atoi(token);  // 세 번째 토큰 (예: 2)

		if (idx < 0 || idx > 99) {
			printf("할당된 범위 밖입니다.\n");
			return;
		}
		// ssd.c에 정의된 read 함수를 호출
		read(idx);
		printf("읽기 성공");

	}
	else if (strcmp(token, "help") == 0) {
		printf("사용 가능한 명령어: read, write, help, fullread, fullwrite\n");

	}
	else if (strcmp(token, "fullread") == 0) {
		fullread();

	}
	else if (strcmp(token, "fullwrite") == 0) {
		
		//token 잘라서 data에 넣기 예외처리랑 

		token = strtok(NULL, " ");  // 데이터 가져옴 (예: 0xAAAABBBB)

		if (strlen(token) != 10 || token[0] != '0' || token[1] != 'x') {
			printf("잘못된 입력 값입니다.\n");
			return;
		}

		for (int i = 2; i < 10; i++) {
			if (!((token[i] >= '0' && token[i] <= '9') || (token[i] >= 'A' && token[i] <= 'F'))) {
				printf("값은 0~9, A~F만 허용됩니다.\n");
				return;
			}
		}

		fullwrite(token);


	}
	else if (strcmp(cmd, "testapp1") == 0) {
		testapp1();
	}
	else if (strcmp(cmd, "testapp2") == 0) {
		testapp2();
	}
	else {
		printf("알 수 없는 명령어입니다.\n");
	}


}

int main(int argc, char* argv[]) {




	while (1) {

		printf("명령어를 입력: ");

		char cmd[1000];
		fgets(cmd, sizeof(cmd), stdin);  // 공백 포함 명령어 입력받기
		cmd[strcspn(cmd, "\n")] = '\0';  // 줄바꿈 문자 제거
		if (strcmp(cmd, "exit") == 0) {
			printf("exit shell!!");
			return 0;
		}
		executecommand(cmd);


		printf("\n");
	}





}
