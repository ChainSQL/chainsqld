/*
* File: log.h
* Desc: 
* Copyright (c) SWXA 2009
*
* Date        By who     Desc
* ----------  ---------  -------------------------------
* 2009.04.20  Yaahao     Created
* 2010/11/11  GAO        Optimized
*/


/************************************************************************/
/************************************************************************/
/************************************************************************/
/* Error log API                                                        */
/* Copy from SWSDS API                                                  */
/************************************************************************/
/************************************************************************/
/************************************************************************/

#ifndef _SW_LOG_H_
#define _SW_LOG_H_ 1 //兼容原版本的LOG模块

#define LOG_ERROR		1
#define LOG_WARNING		2
#define LOG_INFO		3
#define LOG_TRACE		4

#define DEF_LOG_MODULE   "RandomCheck"

extern char apt_log_file[512]; //全局变量
extern unsigned int apt_log_level;//全局变量
extern unsigned int apt_log_max_size; //全局变量

void APT_LogMessage(int nLogLevel, char *sModule, char *sFile, int nLine, unsigned int unErrCode, char *sMessage);
void APT_LogMessageEx(int nLogLevel, char *sModule, char *sFile, int nLine, unsigned int unErrCode, char *sMessage, ...);
void logPrint(int nLogLevel, char *sModule, char *sFile, int nLine, unsigned int unErrCode, char *sMessage);
void logPrintEx(int nLogLevel, char *sModule, char *sFile, int nLine, unsigned int unErrCode, char *sMessage, ...);

#define LOGP(lvl, rv, msg) \
	do { \
	if ((lvl) <= apt_log_level) {\
	logPrint(lvl, DEF_LOG_MODULE, __FILE__, __LINE__, rv, msg);} \
	} while (0)
#define LOGPX(lvl, rv, msg, ...) \
	do { \
	if ((lvl) <= apt_log_level) {\
	logPrintEx(lvl, DEF_LOG_MODULE, __FILE__, __LINE__, rv, msg, __VA_ARGS__);} \
	} while (0)

#define LOG(lvl, rv, msg) \
	do { \
	if ((lvl) <= apt_log_level) {\
	APT_LogMessage(lvl, DEF_LOG_MODULE, __FILE__, __LINE__, rv, msg);} \
	} while (0)

#define LOGEX(lvl, rv, msg, ...) \
	do { \
	if ((lvl) <= apt_log_level) {\
	APT_LogMessageEx(lvl, DEF_LOG_MODULE, __FILE__, __LINE__, rv, msg, __VA_ARGS__);} \
	} while (0)

#endif //#ifndef _SW_LOG_H_




