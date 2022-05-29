//////////////////////////////////////////////////////////////////////////
// File Name		: proxy_cache.c					//
// Date			: 2022/03/29					//
// Os			: Ubuntu 16.04 LTS 64bits			//
// Author		: Dong-Wook Kim					//
// Student ID		: 2018706075					//
// ---------------------------------------------------------------------//
// Title : System Programming Assingment #1-1 (proxy server)		//
// Description : ???							//
// ///////////////////////////////////////////////////////////////////////



#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <dirent.h>

#include <fcntl.h>					//creat 함수를 사용하기 위해 include
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/sha.h>				//SHA1()

char *sha1_hash(char *, char *);
char *getHomeDir(char *home);

void main(int argc, char argv[])
{				
	char home[50];					//home directory 경로 저장
	struct passwd *usr_info = getpwuid(getuid());
	strcpy(home, usr_info->pw_dir);
	chdir(home);

	char hash[41];
	char *hashed_url = hash;			//해싱된 url을 저장하기 위한 변수
	char temp_dir[4];				//hashed_url에서 추출한 dir_name을 저장하기 위한 변수
	char *directory_name = temp_dir;
	char temp_file[39];				//hashed_url에서 추출한 file_name을 저장하기 위한 변수
	char *file_name = temp_file;
	
	umask(0);					//이 프로그램을 통해 생성할 파일/디렉토리의 default 접근 권한 변경
	mkdir("cache", S_IRWXU | S_IRWXG | S_IRWXO);	//hashed_url을 통해 만든 파일/디렉토리를 저장할 cache 디렉토리 생성, 모든 권한 부여
	chdir("./cache/");

	while(1)
	{	
		char url[100];				//입력받은 url을 저장하기 위한 변수
		char *pUrl = url;

		printf("input url> ");
		scanf("%s", url);			//url을 입력받는다
				
		if(strcmp(url, "bye") == 0)		//bye를 입력받으면 루프 종료, 프로그램 종료
			break;

		else					//url이 입력되었을 때
		{
			hashed_url = sha1_hash(pUrl, hashed_url);			//입력받은 url을 hashing

			//char directory_name = strtok(hashed_url, );	//akke2213
			//char file_name = strtok(NULL, "\0");
			//printf("%s\n", hashed_url); //test
			for(int i = 0; i <3; i++)
				directory_name[i] = hashed_url[i];
			for(int j = 3; j < 41; j++)
				file_name[j - 3] = hashed_url[j];
				
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
					
					if((strcmp(pFile-> d_name, ".") == 0) || (strcmp(pFile->d_name, "..") == 0))
						pFile = readdir(pDir);
					if((strcmp(pFile-> d_name, ".") == 0) || (strcmp(pFile->d_name, "..") == 0))
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
	
			if(HIT == 1) printf("HIT\n");
			else if(HIT != 1)			//MISS인 경우 해당 url을 디렉토리/파일 형태로 캐시 서버에 저장
			{
				printf("MISS\n");

				mkdir(directory_name, S_IRWXU | S_IRWXG | S_IRWXO);	//cache 디렉토리 하위에 directory_name 디렉토리 생성
				chdir(directory_name);
				creat(file_name, 0777);	//그 밑에 file_name 파일 생성
				chdir("..");
			}				//escape하지 않고 bye가 입력될 때 까지 반복*/
				
		}
	}
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
