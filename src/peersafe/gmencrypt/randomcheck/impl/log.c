/*
* File: log.c
* Desc: 
* Copyright (c) SWXA 2009
*
* Date        By who     Desc
* ----------  ---------  -------------------------------
* 2009.04.20  Yaahao     Created
* 2010/11/11  GAO        Optimized
*/

#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#include <Windows.h>
#else
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <pthread.h>
#endif

#include <peersafe/gmencrypt/randomcheck/log.h>

#define DEFAULT_LOG_PATH "log"
#define DEFAULT_LOG_FILE "Random_Cycle_Test"

char apt_log_file[512] = "";              //全局变量
unsigned int apt_log_level = LOG_INFO;	  //全局变量
unsigned int apt_log_max_size = 0;        //全局变量

static char sLogStr[LOG_TRACE][20] = {
	"Error",  //LOG_ERROR
	"Warning",  //LOG_WARNING
	"Info",  //LOG_INFO
	"Trace"  //LOG_TRACE
};

/*Windows: 在系统安装盘符根目录*/
/*Linux/Unix: 在TMP目录*/


void APT_LogMessage(int nLogLevel, char *sModule, char *sFile, int nLine, unsigned int unErrCode, char *sMessage)
{
	FILE *fp;
	struct tm *newtime;
	time_t aclock;

	char sRealLogFile[300];
	char sLogPath[256];
	unsigned int nSessionID;

  	time( &aclock );                 
	newtime = localtime( &aclock ); 

	/*获取日志文件名称*/
	if(strlen(apt_log_file) == 0)
	{
#ifdef WIN32
		GetWindowsDirectoryA(sLogPath, sizeof(sLogPath)-1);
		sLogPath[2] = '\0'; /*只取系统盘符*/
		strcat(sLogPath, "\\");
		strcat(sLogPath, DEFAULT_LOG_PATH);
		strcat(sLogPath, "\\");
#else
		sprintf(sLogPath, "/tmp/%s/", DEFAULT_LOG_PATH);		
#endif
		sprintf(sRealLogFile, "%s%s_%4d%02d%02d.log", sLogPath, DEFAULT_LOG_FILE, newtime->tm_year+1900, newtime->tm_mon+1, newtime->tm_mday);
	
		/*打开日志文件*/
		fp = fopen(sRealLogFile, "a+");
		if(fp == NULL)
		{
			return;
		}
	}
	else
	{
		fp = fopen(apt_log_file, "a+");
		if(fp == NULL)
		{
			return;
		}
	}

#ifdef WIN32
	nSessionID = GetCurrentThreadId();
#else
	nSessionID = (unsigned int)pthread_self();
#endif
	
	/*写日志信息*/
	switch(nLogLevel)
	{
	case LOG_ERROR:
		fprintf(fp,"\n<%4d-%02d-%02d %02d:%02d:%02d><%s><%u><Error>[0x%08x]%s(%s:%d)",newtime->tm_year+1900,newtime->tm_mon+1,newtime->tm_mday,newtime->tm_hour,newtime->tm_min,newtime->tm_sec,sModule,nSessionID,unErrCode,sMessage,sFile,nLine);
		break;
	case LOG_WARNING:
		fprintf(fp,"\n<%4d-%02d-%02d %02d:%02d:%02d><%s><%u><Warning>%s<0x%08x>(%s:%d)",newtime->tm_year+1900,newtime->tm_mon+1,newtime->tm_mday,newtime->tm_hour,newtime->tm_min,newtime->tm_sec,sModule,nSessionID,sMessage,unErrCode,sFile,nLine);
		break;
	case LOG_INFO:
		fprintf(fp,"\n<%4d-%02d-%02d %02d:%02d:%02d><%s><%u><Info>%s(%d)(%s:%d)",newtime->tm_year+1900,newtime->tm_mon+1,newtime->tm_mday,newtime->tm_hour,newtime->tm_min,newtime->tm_sec,sModule,nSessionID,sMessage,unErrCode,sFile,nLine);
		break;
	case LOG_TRACE:
		fprintf(fp,"\n<%4d-%02d-%02d %02d:%02d:%02d><%s><%u><Trace>%s(%d)(%s:%d)",newtime->tm_year+1900,newtime->tm_mon+1,newtime->tm_mday,newtime->tm_hour,newtime->tm_min,newtime->tm_sec,sModule,nSessionID,sMessage,unErrCode,sFile,nLine);
		break;
	default:
		break;
	}

	/*关闭文件句柄*/
	fclose(fp);
}

void APT_LogMessageEx(int nLogLevel, char *sModule, char *sFile, int nLine, unsigned int unErrCode, char *sMessage, ...)
{
	FILE *fp;
	struct tm *newtime;
	time_t aclock;

	char sRealLogFile[300];
	char sLogPath[256];
	unsigned int nSessionID;
	va_list params;

  	time( &aclock );                 
	newtime = localtime( &aclock ); 

	/*获取日志文件名称*/
	if(strlen(apt_log_file) == 0)
	{
#ifdef WIN32
		GetWindowsDirectoryA(sLogPath, sizeof(sLogPath)-1);
		sLogPath[2] = '\0'; /*只取系统盘符*/
		strcat(sLogPath, "\\");
		strcat(sLogPath, DEFAULT_LOG_PATH);
		strcat(sLogPath, "\\");
#else
		sprintf(sLogPath, "/tmp/%s/", DEFAULT_LOG_PATH);		
#endif
		sprintf(sRealLogFile, "%s%s_%4d%02d%02d.log", sLogPath, DEFAULT_LOG_FILE, newtime->tm_year+1900, newtime->tm_mon+1, newtime->tm_mday);
	
		/*打开日志文件*/
		fp = fopen(sRealLogFile, "a+");
		if(fp == NULL)
		{
			return;
		}
	}
	else
	{
		fp = fopen(apt_log_file, "a+");
		if(fp == NULL)
		{
			return;
		}
	}

#ifdef WIN32
	nSessionID = GetCurrentThreadId();
#else
	nSessionID = (unsigned int)pthread_self();
#endif

	/*Write log message*/
	switch(nLogLevel)
	{
	case LOG_ERROR:
	case LOG_WARNING:
	case LOG_INFO:
	case LOG_TRACE:

		va_start(params, sMessage);

		//date, time, module, threadID
		fprintf(fp,"\n<%4d-%02d-%02d %02d:%02d:%02d><%s><%u>",
				newtime->tm_year+1900,
				newtime->tm_mon+1,
				newtime->tm_mday,
				newtime->tm_hour,
				newtime->tm_min,
				newtime->tm_sec,
				sModule, nSessionID);
		//level
		fprintf(fp,"<%s>", sLogStr[nLogLevel - 1]);
		//message
		vfprintf(fp, sMessage, params);
		//no.
		fprintf(fp,"<0x%08x>", unErrCode);
		//file, line
		fprintf(fp,"(%s:%d)", sFile,nLine);

		va_end(params);

		break;
	default:

		break;
	}

	/*关闭文件句柄*/
	fclose(fp);
}

void logPrint(int nLogLevel, char * sModule, char * sFile, int nLine, unsigned int unErrCode, char * sMessage)
{
	struct tm *newtime;
	time_t aclock;

	time(&aclock);
	newtime = localtime(&aclock);

	/*写日志信息*/
	switch (nLogLevel)
	{
	case LOG_ERROR:
		//printf("<%4d-%02d-%02d %02d:%02d:%02d><%s><Error> errCode:[0x%08x]%s(%s:%d)\n", 
		printf("<%4d-%02d-%02d %02d:%02d:%02d><%s><Error> errCode:[0x%08x]%s\n",
			newtime->tm_year + 1900, newtime->tm_mon + 1, newtime->tm_mday, newtime->tm_hour, newtime->tm_min, newtime->tm_sec, 
			sModule, unErrCode, sMessage);
		//sModule, unErrCode, sMessage, sFile, nLine);
		break;
	case LOG_WARNING:
		printf("<%4d-%02d-%02d %02d:%02d:%02d><%s><Warning> errCode:[0x%08x]%s\n", 
			newtime->tm_year + 1900, newtime->tm_mon + 1, newtime->tm_mday, newtime->tm_hour, newtime->tm_min, newtime->tm_sec, 
			sModule, unErrCode, sMessage);
		break;
	case LOG_INFO:
		printf("<%4d-%02d-%02d %02d:%02d:%02d><%s><Info> %s\n", 
			newtime->tm_year + 1900, newtime->tm_mon + 1, newtime->tm_mday, newtime->tm_hour, newtime->tm_min, newtime->tm_sec, 
			sModule, sMessage);
		break;
	case LOG_TRACE:
		printf("<%4d-%02d-%02d %02d:%02d:%02d><%s><Trace> %s\n", 
			newtime->tm_year + 1900, newtime->tm_mon + 1, newtime->tm_mday, newtime->tm_hour, newtime->tm_min, newtime->tm_sec, 
			sModule, sMessage);
		break;
	default:
		break;
	}
}

void logPrintEx(int nLogLevel, char * sModule, char * sFile, int nLine, unsigned int unErrCode, char * sMessage, ...)
{
	struct tm *newtime;
	time_t aclock;
	va_list params;

	time(&aclock);
	newtime = localtime(&aclock);
	//date, time, module
	printf("<%4d-%02d-%02d %02d:%02d:%02d><%s><%s> ",
		newtime->tm_year + 1900,newtime->tm_mon + 1,newtime->tm_mday,newtime->tm_hour,newtime->tm_min,newtime->tm_sec,
		sModule, sLogStr[nLogLevel - 1]);
	/*Write log message*/
	switch (nLogLevel)
	{
	case LOG_ERROR:
	case LOG_WARNING:
		printf("errCode:[0x%08x]", unErrCode);
	case LOG_INFO:
	case LOG_TRACE:
		{
			va_start(params, sMessage);
			int tempCharLen = 512;
			char *tempChar = (char *)malloc(tempCharLen);
			int size = vsnprintf(tempChar, tempCharLen, sMessage, params);
			va_end(params);
			printf("%s\n", tempChar);
			free(tempChar);

			//file, line
			//printf("(%s:%d)\n", sFile, nLine);
			break;
		}
	default:
		break;
	}
}
