//////////////////////////////////////////////////////////////////////////
// File Name		: proxy_cache.c					//
// Date			: 2022/05/28					//
// Os			: Ubuntu 16.04 LTS 64bits			//
// Author		: Venti						//
// Student ID		: 						//
// ---------------------------------------------------------------------//
// Title : System Programming Assingment #3-1 (proxy server)		//
// Description : ???							//
// ///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <dirent.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <fcntl.h>					
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <openssl/sha.h>				//SHA1()
#include <sys/ipc.h>
#include <sys/sem.h>
#include <netdb.h>

#define BUFFSIZE 1024
#define PORTNO 39999
#define WEBPORT 80

//functions
char *sha1_hash(char *, char *);				//hashing url
char *getHomeDir(char *home);					//get absolute path of home directory
char *getIPAddr(char* addr);					//get IP addr from url
void sig_alarm_handler(int signo) {printf("========== no response ==========\n");}
void sig_interrupt_handler(int signo);				//INT handler for main function
void sig_interrupt_handler_child(int signo);			//INT handler for child process
static void child_handler();					//

void p(int);
void v(int);

//variables
int Number_Of_Children = 0;
time_t begin;
time_t end;

int main(int argc, char argv[])
{	
	time(&begin);
	//				HIT/MISS 판별을 위한 변수			//	
	time_t now;					//hit, miss 감지됐을 때 시간
	struct tm *ltime;
	
	char home[50];
	struct passwd *usr_info = getpwuid(getuid());
	strcpy(home, usr_info->pw_dir);
	//home = getHomeDir(home);				//home directory 경로

	char hash[41];
	char *hashed_url = hash;				//해싱된 url을 성저장하기 위한 변수
	char temp_dir[4];				//hashed_url에서 추출한 dir_name을 저장하기 위한 변수
	char *directory_name = temp_dir;
	char temp_file[39];				//hashed_url에서 추출한 file_name을 저장하기 위한 변수
	char *file_name = temp_file;

	char t_data[200];				//time 정보를 문자열로 변환하여 저장하기 위한 변수

	pid_t pid;					//pid 정보를 저장하기 위한 변수
	int status;

	int semid, iter;

	union semun
	{
		int val;
		struct semid_ds *buf;
		unsigned short int *array;
	} arg;

	if((semid = semget((key_t)39999, 1, IPC_CREAT|0666)) == -1)		//세마포어 생성
	{
		perror("semget failed");
		exit(1);
	}

	arg.val = 1;
	if((semctl(semid, 0, SETVAL, arg)) == -1)
	{
		perror("semctl failed");
		exit(1);
	}

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
	///////////////////////////////////////////////////////////////////////////////////

	//				소켓						//
	struct sockaddr_in server_addr, client_addr;
	int socket_fd, client_fd;
	int len, len_out;

	if((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Server : Can't open stream socket\n");
		return 0;
	}

	bzero((char*)&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(PORTNO);

	int opt = 1;
	socket_fd = socket(PF_INET, SOCK_STREAM, 0);
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if(bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		printf("Server : Can't bind local address\n");
		return 0;
	}

	listen(socket_fd, 5);
	///////////////////////////////////////////////////////////////////////////////////

	signal(SIGINT, sig_interrupt_handler);
	signal(SIGCHLD, (void *)child_handler);

	while(1)
	{	
	//	if((pid = fork()) == 0){				//child 프로세스, 브라우저로부터 전달받은 http 정보 처리
	//	signal(SIGINT, sig_interrupt_handler_child);

		char url[BUFFSIZE] = {0, };
		char copy_url[BUFFSIZE] = {0, };
		char *pUrl = url;
		char method[20] = {0, };
		char tmp[BUFFSIZE] = {0, };
		char buf[BUFFSIZE];
		char* tok;
		int cache_fd;

		char response_header[BUFFSIZE] = {0, };
		char response_message[BUFFSIZE] = {0, };
		
		struct in_addr inet_client_address;

		len = sizeof(client_addr);
		client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &len);
		if(client_fd < 0)
		{
			printf("Server : acceptfailed\n");
			return 0;
		}

		inet_client_address.s_addr = client_addr.sin_addr.s_addr;

		if((pid = fork()) == 0)
		{
		signal(SIGINT, sig_interrupt_handler_child);

		read(client_fd, buf, BUFFSIZE);
		strcpy(tmp, buf);

		tok = strtok(tmp, " ");
		strcpy(method, tok);
		if(strcmp(method, "GET") == 0)
		{
			tok = strtok(NULL, " ");
			strcpy(url, tok);
			strcpy(copy_url, tok);
		}

			hashed_url = sha1_hash(pUrl, hashed_url);			//입력받은 url을 hashing

			for(int i = 0; i <3; i++)
				directory_name[i] = hashed_url[i];
			for(int j = 3; j < 41; j++)
				file_name[j - 3] = hashed_url[j];
			
			if((strcmp(url, "detectportal.firefox.com/canonical.html") == 0) || (strcmp(directory_name, "da3") == 0) || (strcmp(directory_name, "4a1") == 0) || (strcmp(url, "detectportal.firefox.com/success.txt?ipv4") == 0) || (strcmp(url, "detectportal.firefox.com/success.txt?ipv6") == 0))				    //Firefox 브라우저의 redirect 신호 제거
			{
				Number_Of_Children--;
				exit(0);
			}

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
					{
						HIT = 1; 
						break;
					}			//HIT, for문 탈출
					
					closedir(pDir);
			
				}
			}
			closedir(pCacheDir);

			if(HIT == 1)
			{
				chdir(directory_name);
				cache_fd = open(file_name, O_RDWR | O_RDWR | O_RDWR);
				chdir("..");
			}

			else if(HIT == 0)							//MISS인 경우 해당 url을 디렉토리/파일 형태로 캐시 서버에 저장
			{
				mkdir(directory_name, S_IRWXU | S_IRWXG | S_IRWXO);			//cache 디렉토리 하위에 directory_name 디렉토리 생성
				chdir(directory_name);
				creat(file_name, 0777);	//그 밑에 file_name 파일 생성
				cache_fd = open(file_name, O_RDWR | O_RDWR | O_RDWR);
				chdir("..");
				
				int web_sd, len;
				struct sockaddr_in web_server_addr;
				char web_buf[BUFFSIZE];

				char* IPAddr;
				char* tok = strtok(copy_url, "/");
				tok = strtok(NULL, "/");

				IPAddr = getIPAddr(tok);

				if((web_sd = socket(PF_INET, SOCK_STREAM, 0)) < 0)		//웹서버와 통신하기 위한 소켓 생성
				{
					printf("cant create socket for web server\n");
					return -1;
				}

				bzero(web_buf, sizeof(web_buf));
				bzero((char*)&web_server_addr, sizeof(web_server_addr));
				web_server_addr.sin_family = AF_INET;
				web_server_addr.sin_addr.s_addr = inet_addr(IPAddr);
				web_server_addr.sin_port = htons(WEBPORT);

				signal(SIGALRM, sig_alarm_handler);
				alarm(10);						//알람 생성
				if(connect(web_sd, (struct sockaddr*)&web_server_addr, sizeof(web_server_addr)) < 0)					//소켓을 통해 웹서버에 연결
				{
					printf("can't connect to web server\n");
					return -1;
				}

				write(web_sd, buf, sizeof(buf));			//프록시 서버가 받은 http request 웹서버에 전달
				while(1){
				if((len = read(web_sd, web_buf, sizeof(web_buf))) > 0)
				{
					alarm(0);					//response 받았으면 알람 종료
					write(cache_fd, web_buf, len);
					bzero((web_buf), sizeof(web_buf));
				}
				else break;
				}
				close(web_sd);
			}

		//로그 기록
		printf("*PID# %d is waiting for the semaphore.\n", getpid());
		p(semid);							//critical zone boundary

		printf("*PID# %d is in the critical zone.\n", getpid());	
		sleep(4);							//test code for semaphore
		time(&now);							//log를 남긴 시각
		ltime = localtime(&now);

		if(HIT == 1)
		{
			write(log_file, "[HIT]", 5);
			write(log_file, directory_name, strlen(directory_name));
			write(log_file, "/", 1);

			write(log_file, file_name, strlen(file_name));
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

			write(log_file, "/", 1);
			sprintf(t_data, "%d", ltime->tm_min);
			write(log_file, t_data, strlen(t_data));
			t_data[0] = '\0';

			write(log_file, "/", 1);
			sprintf(t_data, "%d", ltime->tm_sec);
			write(log_file, t_data, strlen(t_data));
			t_data[0] = '\0';

			write(log_file, "]\n", 2);
			write(log_file, "[HIT]", 5);
			write(log_file, pUrl, strlen(pUrl));
			write(log_file, "\n", 1);
		}

		else if (HIT == 0)
		{
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
		}
		
		printf("*PID# %d exited the critical zone.\n", getpid());
		v(semid);							//critical zone boundary

		int index;
		read(cache_fd, buf, BUFFSIZE);
		for(int i = 0; i < strlen(buf) - 1; i++)
			if((buf[i] == '\n') && (buf[i + 1] == '\n'))		//저장된 response에서 헤더와 메시지 분리
				{index = i; break;}
		//response to browser
		lseek(cache_fd, 0, SEEK_SET);
		bzero(buf, strlen(buf));
		read(cache_fd, buf, index + 1);
		//http header
		sprintf(response_header, buf);
		write(client_fd, response_header, strlen(response_header));

		//http message
		do{
		bzero(buf, strlen(buf));
		read(cache_fd, buf, BUFFSIZE);
		//메시지 읽어오기
		sprintf(response_message, buf);
		write(client_fd, response_message, strlen(response_message));
		}while((len = read(cache_fd, buf, BUFFSIZE)) > 0);

		close(client_fd);
		close(cache_fd);

		bzero(buf, strlen(buf));			//clear the buf

		exit(0);					//child process is done
		}

		//parent process is continue to another request
		else 
		{
			Number_Of_Children++;
			//continue;
		}
	}

	close(log_file);
	close(socket_fd);

	return 0;
}

char *sha1_hash(char *input_url, char *hashed_url)
{
	unsigned char hashed_160bits[20];
	char hashed_hex[41];
	int i;

	SHA1(input_url, strlen(input_url), hashed_160bits);	

	for(i = 0; i < sizeof(hashed_160bits); i++)
		sprintf(hashed_hex + i * 2, "%02x", hashed_160bits[i]);

	strcpy(hashed_url, hashed_hex);	

	return hashed_url;
}


char *getHomeDir(char *home)
{
	struct passwd *usr_info = getpwuid(getuid());
	strcpy(home, usr_info->pw_dir);

	return home;
}

char *getIPAddr(char* addr)
{
	struct hostent* hent;
	char* haddr;
	int len = strlen(addr);

	if((hent = (struct hostent*)gethostbyname(addr)) != NULL)
		haddr = inet_ntoa(*((struct in_addr*)hent->h_addr_list[0]));

	return haddr;
}

void sig_interrupt_handler(int signo)
{
	time(&end);

	char home[50];
	char no[5];
	struct passwd* usr_info = getpwuid(getuid());
	strcpy(home, usr_info->pw_dir);
	chdir(home);
	chdir("./logfile");
	int log_fd = open("log.txt", O_RDWR | O_RDWR | O_RDWR);
	lseek(log_fd, 0, SEEK_END);

	end = end - begin;
	
	write(log_fd, "** Server Terminated ** run time: ", 34);
	sprintf(home, "%ld", end);
	write(log_fd, home, strlen(home));

	Number_Of_Children--;
	write(log_fd, " sec. # sub process: ", 21);
	sprintf(home, "%d", Number_Of_Children);
	write(log_fd, home, strlen(home));
	write(log_fd, "\n", 1);

	close(log_fd);
		
	exit(0);
}

void sig_interrupt_handler_child(int signo)
{
	Number_Of_Children--;
	exit(0);
}

void p(int semid)
{
	struct sembuf pbuf;
	pbuf.sem_num = 0;
	pbuf.sem_op = -1;
	pbuf.sem_flg = SEM_UNDO;

	if((semop(semid, &pbuf, 1)) == -1)
	{
		perror("p : semop failed");
		exit(1);
	}
}

void v(int semid)
{
	struct sembuf vbuf;
	vbuf.sem_num = 0;
	vbuf.sem_op = 1;
	vbuf.sem_flg = SEM_UNDO;

	if((semop(semid, &vbuf, 1)) == -1)
	{
		perror("v : semop failed");
		exit(1);
	}
}

static void child_handler()
{
	pid_t pid;
	int status;
	while((pid = waitpid(-1, &status, WNOHANG)) > 0);
}
