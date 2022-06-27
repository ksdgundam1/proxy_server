//////////////////////////////////////////////////////////////////////////
// File Name		: proxy_cache.c					//
// Date			: 2022/06/27					//
// Os			: Ubuntu 16.04 LTS 64bits			//
// Author		: Venti						//
// Student ID		: 2018706075					//
// ---------------------------------------------------------------------//
// Title : System Programming Assingment #1-3 (proxy server)		//
// Description : ???							//
// ///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <dirent.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>					//creat 함수를 사용하기 위해 include
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <openssl/sha.h>				//SHA1()

char *sha1_hash(char *, char *);
char *getHomeDir(char *home);

int Number_Of_Children = 0;				//프로그램이 종료될 때까지 생성한 자식 프로세스 수

void main(int argc, char argv[])
{							//입력받은 url을 저장하기 위한 변수
	time_t begin_time;
	time_t end_time;
	time(&begin_time);				//프로그램 실행과 동시에 시간을 체크

	time_t now;					//hit, miss 감지됐을 때 시간
	struct tm *ltime;
	
	char* home;
	getHomeDir(home);				//home directory 경로

	int how_much_hit = 0;				//HIT이 발생한 횟수
	int how_much_miss = 0;				//MISS 발생한 횟수
	char hash[41];
	char *hashed_url = hash;				//해싱된 url을 성저장하기 위한 변수
	char temp_dir[4];				//hashed_url에서 추출한 dir_name을 저장하기 위한 변수
	char *directory_name = temp_dir;
	char temp_file[39];				//hashed_url에서 추출한 file_name을 저장하기 위한 변수
	char *file_name = temp_file;

	pid_t pid;
	int status;

	//char command[100];
	//char url[30];
	//char *pUrl = url;

	char t_data[10];				//time 정보를 문자열로 변환하여 저장하기 위한 변수
	
	umask(0);					//이 프로그램을 통해 생성할 파일/디렉토리의 default 접근 권한 변경
	chdir(home);
	mkdir("cache", S_IRWXU | S_IRWXG | S_IRWXO);	//hashed_url을 통해 만든 파일/디렉토리를 저장할 cache 디렉토리 생성, 모든 권한 부여
	mkdir("logfile", S_IRWXU | S_IRWXG | S_IRWXO);	//logfile 디렉토리 생성
	chdir("./logfile");
	creat("log.txt", 0777);				//밑에 log.txt. 추가
	//FILE *log_file = fopen("log.txt", "w");		//log.txt. 파일을 쓰기 모드로 연다
	int log_file = open("log.txt", O_RDWR | O_RDWR | O_RDWR);	//log.txt 파일을 연다
	chdir(home);
	chdir("./cache/");				//해쉬된 url 정보들을 저장하기 위한 cache 디렉토리 생성

while(1)
{
	char command[100];

	printf("[%d]input CMD> ", getpid());
	scanf("%s", command);

if(strcmp(command, "connect") == 0)
{
	Number_Of_Children++;
	pid = fork();					//fork 시스템 콜을 이용해 자식 프로세스 생성
	if(pid == 0)					//자식 프로세스는 fork의 반환값이 0이므로, 자식 프로세스만 이 동작을 수행한다
{
	time_t child_begin_time;			//자식 프로세스의 동작 시간을 부모 프로세스와 독립적으로 측정하기 위해 새로운 time_t 구조체 선언
	time_t child_end_time;

	time(&child_begin_time);

	while(1)
	{	
		char url[30];				//입력받은 url을 저장하기 위한 변수
		char *pUrl = url;

		printf("[%d]input URL> ", getpid());
		scanf("%s", url);			//url을 입력받는다
				
		if(strcmp(url, "bye") == 0)		//bye를 입력받으면 루프 종료, 프로그램 종료
		{
			time(&child_end_time);
			child_end_time = child_end_time - child_begin_time;

			char c_sec[20];

			sprintf(c_sec, "%d", child_end_time);
			
			write(log_file, "[Terminated] run time: ", 23);
			write(log_file, c_sec, strlen(c_sec));
			write(log_file, " sec. #request hit : ", 21);

			sprintf(c_sec, "%d", how_much_hit);
			write(log_file, c_sec, strlen(c_sec));
			write(log_file, ", miss : ", 9);

			sprintf(c_sec, "%d", how_much_miss);
			write(log_file, c_sec, strlen(c_sec));
			write(log_file, "\n", 1);

			return 0;
		}

		else					//url이 입력되었을 때
		{
			hashed_url = sha1_hash(pUrl, hashed_url);			//입력받은 url을 hashing

			for(int i = 0; i <3; i++)
				temp_dir[i] = hashed_url[i];
			for(int j = 3; j < 41; j++)
				temp_file[j - 3] = hashed_url[j];
				
			int HIT = 0;							//HIT/MISS 여부를 판별하여 그 결과를 저장
			struct dirent *pCache;
			DIR *pCacheDir;
			
			pCacheDir = opendir(".");
			for(pCache = readdir(pCacheDir); pCache; pCache = readdir(pCacheDir))
			{	
				if(strcmp(pCache->d_name, directory_name) == 0)		//해싱된 url의 앞 세글자와 같은 이름의 디렉토리가 존재할 때
				{
					struct dirent *pFile;
					DIR *pDir;
					
					chdir("./cache/");
					pDir = opendir(directory_name);
					pFile = readdir(pDir);
					if((strcmp(pFile->d_name, "..") == 0) || (strcmp(pFile->d_name, ".") == 0))
						pFile = readdir(pDir);
					if((strcmp(pFile->d_name, "..") == 0) || (strcmp(pFile->d_name, ".") == 0))
						pFile = readdir(pDir);
					if(strcmp(pFile->d_name, file_name) == 0)		//해당 디렉토리 안에 나머지 url과 같은 이름의 파일이 존재할 때
						{HIT = 1; break;}			//HIT, for문 탈출
					closedir(pDir);
				}
			}
			closedir(pCacheDir);
			
			if(HIT == 1)				//HIT인 경우
			{
				time(&now);
				ltime = localtime(&now);
	
				write(log_file, "[HIT]", 5);
				write(log_file, directory_name, strlen(directory_name));
				write(log_file, "/", 1);
				write(log_file, file_name, strlen(file_name));
				write(log_file, "-[", 2);

				sprintf(t_data, "%d", 1900 + ltime->tm_year);
				write(log_file, t_data, strlen(t_data));
				t_data[0] = '\0';
				
				write(log_file, "/", 1);
				sprintf(t_data, "%d", 1 +  ltime->tm_mon);
				write(log_file, t_data, strlen(t_data));
				t_data[0] = '\0';

				write(log_file, "/", 1);
				sprintf(t_data, "%d", ltime->tm_mday);
				write(log_file, t_data, strlen(t_data));
				t_data[0] = '\0';

				write(log_file, ",", 1);
				sprintf(t_data, "%d", ltime->tm_hour);
				write(log_file, t_data, strlen(t_data));
				t_data[0] = '\0';

				write(log_file, ":", 1);
				sprintf(t_data, "%d", ltime->tm_min);
				write(log_file, t_data, strlen(t_data));
				t_data[0] = '\0';

				write(log_file, ":", 1);
				sprintf(t_data, "%d", ltime->tm_sec);
				write(log_file, t_data, strlen(t_data));
				t_data[0] = '\0';
				write(log_file, "]", 1);
				write(log_file, "\n", 1);

				write(log_file, "[HIT]", 5);
				write(log_file, url, strlen(url));
				write(log_file, "\n", 1);

				how_much_hit++;
			}

			else if(HIT != 1)			//MISS인 경우 해당 url을 디렉토리/파일 형태로 캐시 서버에 저장
			{
				mkdir(directory_name, S_IRWXU | S_IRWXG | S_IRWXO);	//cache 디렉토리 하위에 directory_name 디렉토리 생성
				chdir(directory_name);
				creat(file_name, 0777);	//그 밑에 file_name 파일 생성
				chdir("..");
				//로그로 이동, 로그에 출력
				//printf("[MISS]%s-[Time]\n", url);
				//fputs("[MISS]%s-[TIME]\n", log, url);
				
				time(&now);
				ltime = localtime(&now);

				write(log_file, "[MISS]", 6);
				write(log_file, url, strlen(url));
				write(log_file, "-[", 2);
				
				sprintf(t_data, "%d", 1900 + ltime->tm_year);
				write(log_file, t_data, strlen(t_data));
				t_data[0] = '\0';

				write(log_file, "/", 1);
				sprintf(t_data, "%d", 1 + ltime->tm_mon);
				write(log_file, t_data, strlen(t_data));
				t_data[0] = '\0';

				write(log_file, "/", 1);
				sprintf(t_data, "%d", ltime->tm_mday);
				write(log_file, t_data, strlen(t_data));
				t_data[0] = '\0';

				write(log_file, ",", 1);
				sprintf(t_data, "%d", ltime->tm_hour);
				write(log_file, t_data, strlen(t_data));
				t_data[0] = '\0';

				write(log_file, ":", 1);
				sprintf(t_data, "%d", ltime->tm_min);
				write(log_file, t_data, strlen(t_data));
				t_data[0] = '\0';

				write(log_file, ":", 1);
				sprintf(t_data, "%d", ltime->tm_sec);
				write(log_file, t_data, strlen(t_data));
				t_data[0] = '\0';

				write(log_file, "]\n", 2);

				how_much_miss++;
			}				//escape하지 않고 bye가 입력될 때 까지 반복*/
				
		}
	}
}
	else
	wait(&status);
}	//자식 프로세스 종료 지점

else if (strcmp(command, "quit") == 0) break;
else printf("CMD error: Command do not Exist!\n");
}
	
	time(&end_time);
	end_time = end_time - begin_time;

	char sec[10];
	sprintf(sec, "%d", end_time);

	write(log_file, "**SERVER** [Terminated] run time: ", 34);
	write(log_file, sec, strlen(sec));
	write(log_file, " sec. #sub process: ", 20);

	sprintf(sec, "%d", Number_Of_Children);
	write(log_file, sec, strlen(sec));
	write(log_file, "\n", 1);

	close(log_file);
}

char *sha1_hash(char *input_url, char *hashed_url)
{
	unsigned char hashed_160bits[20];
	char hashed_hex[41];
	int i;

	SHA1(input_url, strlen(input_url), hashed_160bits);	//////

	for(i = 0; i < sizeof(hashed_160bits); i++)
		sprintf(hashed_hex + i * 2, "%02x", hashed_160bits[i]);

	strcpy(hashed_url, hashed_hex);	/////

	return hashed_url;
}


char *getHomeDir(char *home)
{
	struct passwd *usr_info = getpwuid(getuid());
	strcpy(home, usr_info->pw_dir);

	return home;
}
