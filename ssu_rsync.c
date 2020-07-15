#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <utime.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024

static void ssu_signal_handler(int signo);
void *rsync(void *arg);
void ssu_runtime(struct timeval *begin_t, struct timeval *end_t);
void backup_tmpfile(char *filename);

char srcpath[BUFFER_SIZE], dstpath[BUFFER_SIZE];
char *logfile = "ssu_rsync_log"; //로그 파일 이름
char saved_path[BUFFER_SIZE];
struct file_info{ //파일 정보 구조체
	char fname[64]; //파일 이름
	off_t fsize; //파일 크기
};
struct file_info filelist[BUFFER_SIZE];
struct timeval begin_t, end_t;
int tmpcnt = 0;
int cnt = 0;
char *tmpfilename[BUFFER_SIZE];

int main(int argc, char *argv[])
{
	char *src, *dst;
	char *rsync_time;
	char rsync_log[BUFFER_SIZE];
	char input_command[BUFFER_SIZE];
	char tmp[BUFFER_SIZE];
	char *p;
	int filenum1, filenum2;
	int i,j;
	int status;
	DIR *dp1, *dp2;
	FILE *fp, *logfp;
	struct dirent **srclist, **dstlist;
	struct stat statbuf1, statbuf2;
	pthread_t tid1;
	time_t clock;

	gettimeofday(&begin_t, NULL); //시작 시간 기록

	if(argc<3){ //인자를 적게 입력했을 때
		fprintf(stderr, "usage : %s [option] <src> <dst>\n", argv[0]);
		exit(1);
	}

	char *ptr;
	if(argc == 3){ //옵션이 없을 때
		src = argv[1];
		dst = argv[2];
		ptr = strrchr(argv[0], '/'); // ./ssu_rsync의 형태면 ssu_rsync의 형태로 저장
		sprintf(input_command, "%s %s %s", ptr+1, argv[1], argv[2]); //log파일에 저장할 명령어
	}
	else{ //옵션이 있을 때
		src = argv[2];
		dst = argv[3];
		ptr = strrchr(argv[0], '/');
		sprintf(input_command, "%s %s %s %s", ptr+1, argv[1], argv[2], argv[3]); //log파일에 저장할 명령어
	}

	getcwd(saved_path, BUFFER_SIZE); //현재 작업 디렉토리 경로 구함

	//입력받은 src와 dst 절대경로 변환
	if(realpath(src, srcpath) == NULL || realpath(dst, dstpath) == NULL){ //존재하지 않는 파일 또는 디렉토리일 때 에러 처리
		fprintf(stderr, "usage : %s [option] <src> <dst>\n", argv[0]);
		exit(1);
	}

	stat(srcpath, &statbuf1);
	stat(dstpath, &statbuf2);

	if(access(srcpath, R_OK) < 0 || access(srcpath, W_OK) < 0){ //접근권한 확인
		fprintf(stderr, "usage : %s [option] <src> <dst>\n", argv[0]);
		exit(1);
	}

	if(!S_ISDIR(statbuf2.st_mode)){ //dst 인자가 디렉토리가 아닐 때
		fprintf(stderr, "usage : %s [option] <src> <dst>\n", argv[0]);
		exit(1);
	}
	
	if((dp2 = opendir(dstpath)) == NULL || access(dstpath, R_OK) < 0|| access(dstpath, W_OK) < 0){ //opendir 에러와 접근권한 확인
		fprintf(stderr, "usage : %s [option] <src> <dst>\n", argv[0]);
		exit(1);
	}

	if(signal(SIGINT, ssu_signal_handler) == SIG_ERR){ //SIGINT 발생 시 ssu_signal_handler 동작
		fprintf(stderr, "cannot handle SIGINT\n");
		exit(EXIT_FAILURE);
	}

	if((filenum2 = scandir(dstpath, &dstlist, NULL, alphasort)) == -1){
		fprintf(stderr, "scandir error for %s\n", dst);
		exit(1);
	}

	/*SIGINT 발생 시 dst 디렉토리를 원래 상태로 만들기 위한 .tmp파일 생성*/
	for(i=0; i<filenum2; i++){
		if(dstlist[i]->d_name[0] == '.')
			continue;
		backup_tmpfile(dstlist[i]->d_name); //.tmp 생성 함수
	}

	if(S_ISDIR(statbuf1.st_mode)){ //src 인자가 디렉토리일 때
		if((dp1 = opendir(srcpath)) == NULL){
			fprintf(stderr, "usage : %s [option] <src> <dst>\n", argv[0]);
			exit(1);
		}
		if((filenum1 = scandir(srcpath, &srclist, NULL, alphasort)) == -1){
			fprintf(stderr, "scandir error for %s\n", src);
			exit(1);
		}

		/*src와 dst디렉토리 파일 비교*/
		for(i=0; i<filenum1; i++){ //src 디렉토리
			chdir(srcpath);
			if(srclist[i]->d_name[0] == '.')
				continue;

			stat(srclist[i]->d_name, &statbuf1); //src디렉토리 내부 파일 stat()
			realpath(srclist[i]->d_name, tmp); //절대경로 구함
			strcpy(filelist[cnt].fname, tmp); //절대경로 구조체 저장
			filelist[cnt++].fsize = statbuf1.st_size; //파일 사이즈 구조체 저장
			for(j=0; j<filenum2; j++){ //dst 디렉토리
				chdir(dstpath);
				if(dstlist[j]->d_name[0] == '.')
					continue;

				if(!strcmp(srclist[i]->d_name, dstlist[j]->d_name)){ //이름이 같을 때
					stat(dstlist[j]->d_name, &statbuf2);
					if(statbuf1.st_mtime == statbuf2.st_mtime && statbuf1.st_size == statbuf2.st_size){ //수정 시간과 파일 크기가 같을 때
						printf("%s is already synced\n", dstlist[j]->d_name); //같은 파일로 인식
						j+=filenum2;
						cnt--;
						continue;
					}
				}
			}
		}

		pthread_t tid[cnt];
		for(i=0; i<cnt; i++){ //쓰레드 생성
			if(pthread_create(&tid[i], NULL, rsync, (void *)&filelist[i]) != 0){ //동기화 진행
				fprintf(stderr, "pthread_create error\n");
				exit(1);
			}
			pthread_join(tid[i], NULL); //쓰레드가 끝날 때까지 대기
			//sleep(1);
		}

	}
	else{ //src 인자가 파일일 때
		if((fp = fopen(srcpath, "r+")) == NULL){
			fprintf(stderr, "usage : %s [option] <src> <dst>\n", argv[0]);
			exit(1);
		}

		int flag = 0;
		strcpy(filelist[cnt].fname, srcpath); //파일 절대경로 구조체 저장
		filelist[cnt++].fsize = statbuf1.st_size; //파일 사이즈 구조체 저장
		char fname[BUFFER_SIZE];
		p = strrchr(srcpath, '/'); //파일 이름 상대경로 저장
		p++;
		strcpy(fname, p); //상대경로 이름 저장

		chdir(dstpath);
		for(i=0; i<filenum2; i++){ //dst 디렉토리 파일과 비교
			if(dstlist[i]->d_name[0] == '.')
				continue;

			if(!strcmp(fname, dstlist[i]->d_name)){ //이름이 같을 때
				stat(dstlist[i]->d_name, &statbuf2);
				if(statbuf1.st_mtime == statbuf2.st_mtime && statbuf1.st_size == statbuf2.st_size){ //수정 시간과 파일 크기가 같으면
					printf("%s is already synced\n", dstlist[i]->d_name); //같은 파일로 인식
					//sleep(2);
					flag = 1;
					cnt = 0;
					break;
				}
			}
		}
		
		if(flag == 0){ //같은 파일이 없을 때
			if(pthread_create(&tid1, NULL, rsync, (void *)&srcpath) != 0){ //동기화 진행
				fprintf(stderr, "pthread_create error\n");
				exit(1);
			}
			//sleep(1);

			pthread_join(tid1, (void *)&status); //쓰레드 종료까지 대기
		}

	}

	sleep(1);

	/*SIGINT 발생 시를 대비해 만든 .tmp파일 제거*/
	chdir(dstpath);
	for(i=0; i<tmpcnt; i++){
		remove(tmpfilename[i]);
		free(tmpfilename[i]);
	}

	/*logfile 작성*/
	chdir(saved_path);
	clock = time(NULL); //현재시간 구함
	if((logfp = fopen(logfile, "a+")) == NULL){ //logfile open
		fprintf(stderr, "fopen error for %s\n", logfile);
		exit(1);
	}
	rsync_time = ctime(&clock); //동기화 시간 문자열
	rsync_time[strlen(rsync_time)-1] = 0; //개행 제거
	sprintf(rsync_log, "[%s] %s\n", rsync_time, input_command); //logfile에 쓸 명령어
	fwrite(rsync_log, strlen(rsync_log), 1, logfp); //logfile 작성

	/*동기화한 파일 정보 작성*/
	for(i=0; i<cnt; i++){
		memset(rsync_log, 0, BUFFER_SIZE);
		p = strrchr(filelist[i].fname, '/'); //파일 이름 상대경로 저장
		p++;
		sprintf(rsync_log, "\t\t%s %ldbytes\n", p, filelist[i].fsize); //파일 이름과 사이즈
		fwrite(rsync_log, strlen(rsync_log), 1, logfp); //logfile 작성
	}

	fclose(logfp);

	gettimeofday(&end_t, NULL); //시작 시간 기록
	ssu_runtime(&begin_t, &end_t); //실행 시간 출력
	exit(0);
}

void *rsync(void *arg){ //동기화 함수
	struct utimbuf time_buf;
	struct stat statbuf1, statbuf2;
	FILE *sfp, *dfp;
	struct file_info *src;
	char *dst;
	char buf[BUFFER_SIZE];
	char tmp[BUFFER_SIZE];
	int filesize, filebufsize;

	src = (struct file_info *)arg;
	dst = strrchr(src->fname, '/'); //파일 이름 구함
	dst++;
	printf("rsync %s\n", dst);

	if(stat(src->fname, &statbuf1) < 0){ //src파일 stat구조체 선언
		fprintf(stderr, "stat error rsync src %s\n", src->fname);
		exit(1);
	}

	if((sfp = fopen(src->fname, "r+")) == NULL){ //src file open
		fprintf(stderr, "fopen error for sfp %s\n", src->fname);
		exit(1);
	}

	if(chdir(dstpath) < 0){ //dst 디렉토리로 이동
		fprintf(stderr, "chdir error %s\n", dstpath);
		exit(1);
	}

	if((dfp = fopen(dst, "w+")) == NULL){ //src파일과 같은 이름의 파일 생성
		fprintf(stderr, "fopen error dfp\n");
		exit(1);
	}

	fseek(sfp, 0, SEEK_END);
	filesize = ftell(sfp); //파일 크기 구함
	fseek(sfp, 0, SEEK_SET);

	while(filesize > 0){ //파일 내용 복사
		memset(buf, 0, BUFFER_SIZE);
		fread(buf, BUFFER_SIZE, 1, sfp);
		if(filesize > BUFFER_SIZE)
			filebufsize = BUFFER_SIZE;
		else
			filebufsize = filesize;

		fwrite(buf, filebufsize, 1, dfp);
		filesize -= filebufsize;
	}

	fclose(sfp);
	fclose(dfp);

	/*src 수정시간으로 변경*/
	time_buf.actime = statbuf1.st_atime;
	time_buf.modtime = statbuf1.st_mtime;

	if(utime(dst, &time_buf) < 0){ //utime으로 dst디렉토리에 복사한 파일 수정시간 변경
		fprintf(stderr, "utime error\n");
		exit(1);
	}

	return NULL;
}

void ssu_runtime(struct timeval *begin_t, struct timeval *end_t){ //프로그램 실행시간 출력
	end_t->tv_sec -= begin_t->tv_sec;

	if(end_t->tv_usec < begin_t->tv_usec){
		end_t->tv_sec--;
		end_t->tv_usec += 1000000;
	}

	end_t->tv_usec -= begin_t->tv_usec;
	printf("Runtime: %ld:%06ld(sec:usec)\n", end_t->tv_sec, end_t->tv_usec);
}

void backup_tmpfile(char *filename){ //.tmp파일 생성 함수
	FILE *fp, *tmpfp;
	char tmpfname[BUFFER_SIZE];
	char buf[BUFFER_SIZE];
	int filesize, filebufsize;
	struct stat statbuf;
	struct utimbuf time_buf;
	
	if(chdir(dstpath) < 0){ //dst 디렉토리로 이동
		fprintf(stderr, "chdir error %s\n", dstpath);
		exit(1);
	}
	
	if(stat(filename, &statbuf) < 0){ //stat 구조체 선언
		fprintf(stderr, "backup stat error for %s\n", filename);
		exit(1);
	}

	if((fp = fopen(filename, "r")) == NULL){ //file open
		fprintf(stderr, "fopen error dfp\n");
		exit(1);
	}

	strcpy(tmpfname, filename);
	strcat(tmpfname, ".tmp"); //filename.tmp 파일 이름 저장

	if((tmpfp = fopen(tmpfname, "w+")) == NULL){ //.tmp 파일 생성
		fprintf(stderr, "fopen error for %s\n", tmpfname);
		exit(1);
	}

	fseek(fp, 0, SEEK_END);
	filesize = ftell(fp); //파일 크기 구함
	fseek(fp, 0, SEEK_SET);

	while(filesize > 0){ //파일 복사
		memset(buf, 0, BUFFER_SIZE);
		fread(buf, BUFFER_SIZE, 1, fp);
		if(filesize > BUFFER_SIZE)
			filebufsize = BUFFER_SIZE;
		else
			filebufsize = filesize;

		fwrite(buf, filebufsize, 1, tmpfp);
		filesize -= filebufsize;
	}
	fclose(fp);
	fclose(tmpfp);
	
	/*수정시간 변경*/
	time_buf.actime = statbuf.st_atime;
	time_buf.modtime = statbuf.st_mtime;

	if(utime(tmpfname, &time_buf) < 0){ //원본 파일과 같은 수정시간으로 변경
		fprintf(stderr, "utime error\n");
		exit(1);
	}

	tmpfilename[tmpcnt] = (char *)calloc(BUFFER_SIZE, sizeof(char));
	strcpy(tmpfilename[tmpcnt++], tmpfname); //.tmp파일 이름 구조체에 저장

	return;
}

static void ssu_signal_handler(int signo){ //SIGINT발생 시 dst디렉토리 복구 함수
	char *p;
	char buf[BUFFER_SIZE];
	char f_name[BUFFER_SIZE];
	int i;
	int filesize, filebufsize;
	struct stat statbuf, statbuf1;
	struct utimbuf time_buf;
	FILE *fp, *tempfp;

	//printf("SIGINT\n");

	if(chdir(dstpath) < 0){ //dst디렉토리로 이동
		fprintf(stderr, "chdir error %s\n", dstpath);
		exit(1);
	}

	/*동기화한 파일 삭제*/
	for(i=0; i<cnt; i++){
		if((p = strrchr(filelist[i].fname, '/')) == NULL)
			continue;
		p++;
		remove(p);
	}

	/*tmp file을 이용해 복구*/
	for(i=0; i<tmpcnt; i++){
		stat(tmpfilename[i], &statbuf);
		time_buf.actime = statbuf.st_atime;
		time_buf.modtime = statbuf.st_mtime;

		memset(f_name, 0, BUFFER_SIZE);
		p = strrchr(tmpfilename[i], '.');
		strncpy(f_name, tmpfilename[i], strlen(tmpfilename[i]) - strlen(p));

		rename(tmpfilename[i], f_name); //filename.tmp 파일을 filename으로 변경
	
		if(utime(f_name, &time_buf) < 0){ //수정시간 복구
			fprintf(stderr, "utime error\n");
			exit(1);
		}

	}

	gettimeofday(&end_t, NULL); //시작 시간 기록
	ssu_runtime(&begin_t, &end_t); //실행 시간 출력
	
	exit(0);
}
