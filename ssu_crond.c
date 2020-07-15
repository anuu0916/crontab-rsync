#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#define BUFFER_SIZE 1024

void run_command(char *command, char (*cycle)[32], char *buf);

int try=0;
pid_t d_pid;
FILE *fp, *logfp;
time_t clk;
struct tm *tm;

int main(void)
{
	pid_t pid;
	char *crontab_file = "ssu_crontab_file"; //명령어 파일 이름
	char *buf[128];
	char command[128][BUFFER_SIZE] = {0};
	char tmp[BUFFER_SIZE];
	char cycle[5][32];
	int line;
	int i, j;
	int fd, maxfd;

	/*데몬 프로세스 생성*/
	if((pid = fork()) < 0){ //자식 프로세스 생성
		fprintf(stderr, "fork error\n");
		exit(1);
	}
	else if(pid != 0) //부모 프로세스 종료
		exit(0);

	d_pid = getpid();
	setsid(); //세션 리더 설정
	signal(SIGTTIN, SIG_IGN); //입출력 시그널 무시
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	maxfd = getdtablesize();
	
	for(fd = 0; fd < maxfd; fd++) //fd 종료
		close(fd);

	umask(0);

	fd = open("/dev/null", O_RDWR);
	dup(0);
	dup(0);
	/*데몬 생성 완료*/

	while(1){ //무한루프
		fp = fopen(crontab_file, "r"); //ssu_crontab_file open
		fseek(fp, 0, SEEK_SET);
		line=0, i=0;
		buf[0] = (char *)malloc(sizeof(char)*BUFFER_SIZE);
		memset(buf[0], 0, BUFFER_SIZE);
		while(fscanf(fp, "%[^\n]", buf[line]) != EOF){ //라인 단위로 파일 읽음
			fseek(fp, 1, SEEK_CUR);
			line++;
			buf[line] = (char *)malloc(sizeof(char)*BUFFER_SIZE);
			memset(buf[line], 0, BUFFER_SIZE);
		}
	
		clk = time(NULL);
		tm = localtime(&clk); //현재 시각 구함

		if(tm->tm_sec == 0){ //0초일 때 명령어 수행
		int count, length;
		for(i=0; i<line; i++){ //라인 수 (명령어 수)만큼 반복
			memset(command[i], 0, BUFFER_SIZE);
			memset(tmp, 0, BUFFER_SIZE);
			strcpy(tmp, buf[i]); //명령어 복사
			j=0;
			count=0;
			length=0;
			char *p = strtok(buf[i], " "); //공백 기준으로 토큰 자름
		while(p!=NULL){
				if(count < 5){ //주기는 cycle 배열에 복사
					strcpy(cycle[j++], p);
				}
				else{ //명령어는 command 배열에 복사
					sprintf(command[i]+length, "%s ", p);
					length += strlen(p) + 1;
				}

				count++;
				p = strtok(NULL, " ");
			}
			run_command(command[i], cycle, tmp); //명령어 실행
		}

		for(i=0; i<line; i++) //free buf
			free(buf[i]);

		}
		sleep(1); //1초 sleep
		fclose(fp);
	}

}

void run_command(char *command, char (*cycle)[32], char *buf)
{
	char *run_time;
	char run_log[BUFFER_SIZE] = {0};
	char *logfile = "ssu_crontab_log";
	int i, j;
	int flag = 0; //수행 시간 검사에 사용될 변수
	int cnt;
	int val;
	int starcnt=0, barcnt=0, commacnt=0, slashcnt=0; //기호 개수 count 변수
	int cyclenum[5][32]; //숫자 저장 배열
	time_t curtime;
	
	/*수행 시간 검사*/
	for(i=0; i<5; i++){
		starcnt=0; barcnt=0; commacnt=0; slashcnt=0;
		flag=0;
		if(!strcmp(cycle[i], "*")) //*이면 넘어감
			continue;
		cnt=0;
		for(j=0; j<strlen(cycle[i]); j++){
			printf("%d, %d : %c\n", i, j, cycle[i][j]);
			if(isdigit(cycle[i][j]) != 0){ //숫자일 경우
				cyclenum[i][cnt] = atoi(cycle[i]+j); //atoi()로 정수 변환
				val = atoi(cycle[i]+j);

				if(strlen(cycle[i]) == 1){ //길이가 1일 때 숫자 비교
					if(i==0 && tm->tm_min == val)
						flag++;
					else if(i==1 && tm->tm_hour == val)
						flag++;
					else if(i==2 && tm->tm_mday == val)
						flag++;
					else if(i==3 && (tm->tm_mon+1) == val)
						flag++;
					else if(i==4 && tm->tm_wday == val)
						flag++;
					
				}
				if(cyclenum[i][cnt] > 9) //두자리 수이면 j 1 증가
					j++;

				cnt++;
			}
			else{
				//기호 개수 count
				if(cycle[i][j] == '*'){
					starcnt++;
					if(j == strlen(cycle[i])-1) //마지막 문자가 *일 때
						flag++;
					continue;
				}
				else if(cycle[i][j] == '-')
					barcnt++;
				else if(cycle[i][j] == ',')
					commacnt++;
				else if(cycle[i][j] == '/')
					slashcnt++;


				if(cycle[i][j] == '-'){ //-를 만났을 때 숫자 범위 검사
					if(i==0 && (tm->tm_min >= cyclenum[i][cnt-1] && tm->tm_min <= atoi(cycle[i]+(j+1))))
						flag++;
					else if(i==1 && (tm->tm_hour >= cyclenum[i][cnt-1] && tm->tm_hour <= atoi(cycle[i]+(j+1))))
						flag++;
					else if(i==2 && (tm->tm_mday >= cyclenum[i][cnt-1] && tm->tm_mday <= atoi(cycle[i]+(j+1))))
						flag++;
					else if(i==3 && ((tm->tm_mon+1) >= cyclenum[i][cnt-1] && (tm->tm_mon+1) <= atoi(cycle[i]+(j+1))))
						flag++;
					else if(i==4 && (tm->tm_wday >= cyclenum[i][cnt-1] && tm->tm_wday <= atoi(cycle[i]+(j+1))))
						flag++;
				}
				else if(cycle[i][j] == ','){ //,를 만났을 때
					if(barcnt==0 && slashcnt==0 && starcnt==1){ //'*,'의 형태일 때
						flag++;
						j+=strlen(cycle[i]);
						continue;
					}
					else if(barcnt==0 && slashcnt==0) //'숫자,'의 형태일 때
						continue;
					else if((barcnt>0 || slashcnt>0) && (flag >= (barcnt+slashcnt))){ //-나 /가 있을 경우 현재 실행시간이 맞을 때
						j+=strlen(cycle[i]); 
						continue;
					}

					else{
						flag = 0;
						//목록 중간에 주기가 있을 때 cyclenum 배열에서 제외
						if(barcnt > 0)
							cnt -= barcnt+1;
						if(slashcnt > 0)
							cnt -= slashcnt;
						//count변수 초기화
						barcnt=0; starcnt=0; slashcnt=0;
						continue;
					}
				}
				else if(cycle[i][j] == '/'){ // '/'를 만났을 때
					int first, last;
					int snum = atoi(cycle[i]+(j+1)); //주기 저장
					int k;
					/* 범위의 첫 숫자와 마지막 숫자를 구한 뒤 주기만큼 더해가며 수행시간이 맞는지 확인*/ 
					if(i==0){
						if(cycle[i][j-1] == '*'){ //'*/숫자'이면 범위 지정
							first = -1;
							last = 59;
						}
						else{ //'숫자-숫자/숫자'의 형태일 경우
							first = cyclenum[i][cnt-2]-1;
							last = cyclenum[i][cnt-1];
						}
						for(k=first+snum; k<=last; k+=snum){ //주기 더해가며 숫자 확인
							if(tm->tm_min == k){ //수행 주기가 맞을 때
								flag++;
								break;
							}
						}
					}
					else if(i==1){
						if(cycle[i][j-1] == '*'){
							first = -1;
							last = 23;
						}
						else{
							first = cyclenum[i][cnt-2]-1;
							last = cyclenum[i][cnt-1];
						}
						for(k=first+snum; k<=last; k+=snum){
							if(tm->tm_hour == k){
								flag++;
								break;
							}
						}
					}
					else if(i==2){
						if(cycle[i][j-1] == '*'){
							first = 0;
							last = 31;
						}
						else{
							first = cyclenum[i][cnt-2]-1;
							last = cyclenum[i][cnt-1];
						}
						for(k=first+snum; k<=last; k+=snum){
							if(tm->tm_mday == k){
								flag++;
								break;
							}
						}
					}
					else if(i==3){
						if(cycle[i][j-1] == '*'){
							first = 0;
							last = 12;
						}
						else{
							first = cyclenum[i][cnt-2]-1;
							last = cyclenum[i][cnt-1];
						}
						for(k=first+snum; k<=last; k+=snum){
							if((tm->tm_mon+1) == k){
								flag++;
								break;
							}
						}
					}
					else if(i==4){
						if(cycle[i][j-1] == '*'){
							first = -1;
							last = 6;
						}
						else{
							first = cyclenum[i][cnt-2]-1;
							last = cyclenum[i][cnt-1];
						}
						for(k=first+snum; k<=last; k+=snum){
							if(tm->tm_wday == k){
								flag++;
								break;
							}
						}
					}
				}
			}

		}
		
			
		// ,로 이어진 숫자 검사 (cyclenum 배열에 있는 숫자와 비교)
		if(commacnt>0 && barcnt==0 && slashcnt==0){
			if(i==0){
				for(j=0; j<cnt; j++){
					if(tm->tm_min == cyclenum[i][j]){
						flag++;
						break;
					}
				}
			}
			else if(i==1){
				for(j=0; j<cnt; j++){
					if(tm->tm_hour == cyclenum[i][j]){
						flag++;
						break;
					}
				}
			}
			else if(i==2){
				for(j=0; j<cnt; j++){
					if(tm->tm_mday == cyclenum[i][j]){
						flag++;
						break;
					}
				}
			}
			else if(i==3){
				for(j=0; j<cnt; j++){
					if((tm->tm_mon+1) == cyclenum[i][j]){
						flag++;
						break;
					}
				}
			}
			else if(i==4){
				for(j=0; j<cnt; j++){
					if(tm->tm_wday == cyclenum[i][j]){
						flag++;
						break;
					}
				}
			}
		}

		if(flag==0) //실행 시간이 아니면 return
			return;
		if((barcnt>0 || slashcnt>0) && (flag < (barcnt+slashcnt))) //-나 /가 있을 때 &조건으로 실행 시간이 맞지 않을 때
			return;
	}

	if(system(command) != 0){ //명령어가 정상 실행되지 않았을 때
		printf("command fail\n");
		return;
	}
	
	/*정상 실행됐다면 log 작성*/
	if((logfp = fopen(logfile, "a+")) == NULL){ //log file open
		fprintf(stderr, "logfile open error\n");
		exit(1);
	}
	
	
	run_time = ctime(&clk); //수행 시간 문자열 : 실행 전에 복사해놓기
	run_time[strlen(run_time)-1] = 0; //개행 제거
	sprintf(run_log, "[%s] %s %s\n", run_time, "run", buf); //logfile에 쓸 문자열
	if(fwrite(run_log, strlen(run_log), 1, logfp) != 1){ //logfile에 write
		fprintf(stderr, "fwrite error\n");
		exit(1);
	}

	fclose(logfp);

	return;
}
