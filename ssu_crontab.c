#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_SIZE 1024
#define SECOND_TO_MICRO 1000000

void remove_command(char **input_token, char **buf, int line, int command_num);
void add_command(char **input_token, int count);
void ssu_runtime(struct timeval *begin_t, struct timeval *end_t);

char *crontab_file = "ssu_crontab_file"; //crontab_file 파일명
char *logfile = "ssu_crontab_log"; //crontab_log 파일명
FILE *fp; //ssu_crontab_file을 가리키는 파일 포인터
FILE *logfp; //ssu_crontab_log를 가리키는 파일 포인터

int main(void)
{
	struct stat statbuf;
	char *buf[128];
	char input[BUFFER_SIZE];
	char *input_token[32] = {0};
	char *cycle[5] = {0};
	char *p;
	size_t length;
	int count, line;
	int i, j;
	struct timeval begin_t, end_t;

	gettimeofday(&begin_t, NULL); //시작 시간 기록

	if(access(crontab_file, F_OK) == 0) //crontab file이 존재할 경우
		fp = fopen(crontab_file, "r+");
	else //crontab file이 존재하지 않을 경우
		fp = fopen(crontab_file, "w+");



	while(1){ //무한루프
		//ssu_crontab_file에 저장된 모든 명령어 출력
		rewind(fp); //file offset을 처음으로 이동
		line=0, j=0;
		buf[0] = (char *)malloc(sizeof(char)*BUFFER_SIZE); //공간 할당
		memset(buf[0], 0, BUFFER_SIZE);
		while(!feof(fp)){ //파일 끝까지
			if(fscanf(fp, "%[^\n]", buf[line]) == EOF) //파일 끝이면 break
				break;
			strcat(buf[line], "\n");
			printf("%d. %s", line, buf[line]); //명령어 출력
			fseek(fp, 1, SEEK_CUR);
			line++;
			buf[line] = (char *)malloc(sizeof(char)*BUFFER_SIZE);
			memset(buf[line], 0, BUFFER_SIZE);
		}
		line--;

		memset(input, 0, BUFFER_SIZE);
		printf("\n20182629>"); //프롬프트 출력	
		fgets(input, BUFFER_SIZE, stdin); //라인 단위로 명령어 입력받음
		input[strlen(input)-1] = 0; //개행문자 지워줌

		if(!strncmp(input, "\0", 1)) //개행문자만 입력했을 경우 프롬프트 재출력
			continue;

		//띄어쓰기 기준으로 토큰 분리
		count=0;
		p = strtok(input, " ");
		while(p != NULL){
			input_token[count++] = p;
			p = strtok(NULL, " ");
		}
		count--; //명령어 개수

		if(!strcmp(input_token[0], "add")){ //add 명령어 입력
			if(count<6){ //실행 주기와 명령어를 입력하지 않았을 때 에러 처리
				printf("usage : add <실행주기> <명령어>\n");
				continue;
			}

			add_command(input_token, count); //add 명령어 실행
		}
		else if(!strcmp(input_token[0], "remove")){ //remove 명령어 입력
			if(count == 0){
				printf("COMMAND_NUMBER를 입력하지 않았습니다.\n");
				continue;
			}
			if(isdigit(input_token[1][0]) == 0){ //입력받은 번호가 숫자가 아닐 때
				printf("COMMAND_NUMBER가 잘못되었습니다.\n");
				continue;
			}
			int command_num = atoi(input_token[1]);
			if(command_num > line || command_num < 0){ //입력받은 번호가 잘못되었을 때
				printf("COMMAND_NUMBER가 잘못되었습니다.\n");
				continue;
			}
			remove_command(input_token, buf, line, command_num); //remove 명령어 실행
		}
		else if(!strcmp(input_token[0], "exit")){ //exit 명령어 입력
			gettimeofday(&end_t, NULL); //현재시간 구함
			ssu_runtime(&begin_t, &end_t); //프로그램 실행시간 출력
			exit(0);
		}
	}
}

void add_command(char **input_token, int count){
	int val;
	int flag = 0; //에러 처리 구분 변수. 1이면 에러
	int length;
	int i, j;
	int starcnt=0, barcnt=0, commacnt=0, slashcnt=0;
	char buf[BUFFER_SIZE] = {0};

	for(i=1; i<6; i++){ //실행주기 검사
		starcnt=0; barcnt=0; commacnt=0; slashcnt=0;
		for(j=0; j<strlen(input_token[i]); j++){
//			printf("%d,%d : %c\n", i, j, input_token[i][j]);
			flag = 0;
			if(isdigit(input_token[i][j]) == 0){ //기호일 때
				//기호 개수 count
				if(input_token[i][j] == '*')
					starcnt++;
				else if(input_token[i][j] == '-'){
					if(slashcnt > 0 || starcnt > 0) //주기 앞에 *이나 /가 있을 경우
						flag = 1;

					barcnt++;
				}
				else if(input_token[i][j] == ','){ //,를 만나면 개별 시간이므로 기호 개수 초기화
					commacnt++;
					starcnt=0;
					barcnt=0;
					slashcnt=0;
				}
				else if(input_token[i][j] == '/')
					slashcnt++;
				else //지정 기호가 아닐 때 에러
					flag=1;

				
				if(starcnt > 1 || barcnt > 1 || slashcnt > 1) //기호가 중복 사용됐을 때
					flag = 1;
				else if(input_token[i][j] != '*' && (j==0 || j==strlen(input_token[i])-1)) //*이 아닌 기호가 처음이나 마지막에 사용됐을 때
					flag = 1;
				else if(input_token[i][j] == '-' && !isdigit(input_token[i][j-1]) && !isdigit(input_token[i][j+1])) //-의 앞뒤에 숫자가 오지 않는 경우
					flag = 1;
				else if(input_token[i][j] == '-' && val > atoi(input_token[i]+(j+1))) // - 앞의 숫자가 뒤 숫자보다 클 경우
					flag = 1;
				else if(input_token[i][j] == '/' && !isdigit(input_token[i][j+1])) // /뒤에 숫자가 오지 않는 경우
					flag = 1;
				else if(input_token[i][j] == '/' && input_token[i][j-1] != '*' && barcnt==0) // / 앞에 주기가 아닌 경우
					flag = 1;
				else if(input_token[i][j] == ',' && input_token[i][j-1] == ',') // ,,로 입력한 경우
					flag =1;
				
			}
			else{ //숫자일 때
				val = atoi(input_token[i]+j); //int형 변환
				if(i==1 && (val<0 || val>59)) //분(0-59) 범위 검사
					flag = 1;
				else if(i==2 && (val<0 || val>23)) //시(0-23) 범위 검사
					flag = 1;
				else if(i==3 && (val<1 || val>31)) //일(1-31) 범위 검사
					flag = 1;
				else if(i==4 && (val<1 || val>12)) //월(1-12) 범위 검사
					flag = 1;
				else if(i==5 && (val<0 || val>6)) //요일(0-6) 범위 검사
					flag = 1;

				if(val > 9) //숫자가 2자리수일 경우
					j++;
				
			}

			if(flag==1){ //실행주기 에러 처리
				fprintf(stderr, "실행주기가 잘못되었습니다.\n");
				return;
			}

		}
	}

	//ssu_crontab_file에 실행주기와 명령어 기록
	fseek(fp, 0, SEEK_END); //파일의 마지막으로 오프셋 이동
	length = 0;
	for(i=1; i<count+1; i++){ //토큰 수만큼 반복
		if(i==count) //마지막 토큰일 때
			sprintf(buf+length, "%s\n", input_token[i]);
		else
			sprintf(buf+length, "%s ", input_token[i]);

		length += strlen(input_token[i]) + 1; //토큰 길이 + 공백 1칸만큼 길이 추가
	}
	fwrite(buf, strlen(buf), 1, fp); //파일에 기록

	//ssu_crontab_log에 로그 기록
	time_t clock = time(NULL); //현재 시간
	char *add_time;
	char add_log[BUFFER_SIZE] = {0};
	logfp = fopen(logfile, "a+"); //logfile open
	add_time = ctime(&clock); //수행시간 문자열
	add_time[strlen(add_time)-1] = 0; //개행 제거
	sprintf(add_log, "[%s] %s %s", add_time, "add", buf); //기록할 문자열 저장
	fwrite(add_log, strlen(add_log), 1, logfp); //파일에 기록
	fclose(logfp);

}

void remove_command(char **input_token, char **buf, int line, int command_num){
	int i;
	FILE *tmpfp;

	tmpfp = fopen("tmpfile", "w+"); //tmpfile open

	for(i=0; i<=line; i++){ //ssu_crontab_file에 기록된 명령어 수만큼 반복
		if(i==command_num) //삭제할 명령어 제외
			continue;
		else //나머지 명령어를 tmpfile에 작성
			fwrite(buf[i], strlen(buf[i]), 1, tmpfp);
	}
	
	fclose(tmpfp);
	fclose(fp);
	remove(crontab_file); //ssu_crontab_file 삭제
	rename("tmpfile", crontab_file); //tmpfile을 ssu_crontab_file로 rename
	//remove("tempfile");

	fp = fopen(crontab_file, "r+"); //파일 포인터 다시 연결
	
	//ssu_crontab_log에 로그 기록
	time_t clock = time(NULL); //현재 시간
	char *remove_time;
	char remove_log[BUFFER_SIZE] = {0};
	logfp = fopen(logfile, "a+"); //logfile open
	remove_time = ctime(&clock); //수행시간 문자열
	remove_time[strlen(remove_time)-1] = 0; //개행 제거
	sprintf(remove_log, "[%s] %s %s", remove_time, "remove", buf[command_num]); //기록할 문자열 저장
	fwrite(remove_log, strlen(remove_log), 1, logfp); //파일에 기록
	fclose(logfp);
}

void ssu_runtime(struct timeval *begin_t, struct timeval *end_t) //시간 측정 함수
{
	end_t->tv_sec -= begin_t->tv_sec;

	if(end_t->tv_usec < begin_t->tv_usec){
		end_t->tv_sec--;
		end_t->tv_usec += SECOND_TO_MICRO;
	}

	end_t->tv_usec -= begin_t->tv_usec;
	printf("Runtime: %ld:%06ld(sec:usec)\n", end_t->tv_sec, end_t->tv_usec);
}
