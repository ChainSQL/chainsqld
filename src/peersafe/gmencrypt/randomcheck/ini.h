/*
* File: ini.h
* Desc: 读取/修改 'ini' 配置文件, 
*       配置值支持'$'(Linux/UNIX),'%%'(Windows)等通配符
*       '#'为注释（建议放在行首）
* Copyright (c) sansec 2009
*
*/

#ifndef _SW_INI_H_
#define _SW_INI_H_ 1

/******************************************************************************
 * Function: GetItemValue
 * Description: 读配置项的值
 * Input: char *stConfigFileName 配置文件名
 *        char *item 配置项名称
 * Output:char *value 配置项的值
 * Return: SDR_OK 成功 其他 失败
 * Others:
******************************************************************************/
int GetItemValue(char *stConfigFileName, char *item, char *value);

#endif
