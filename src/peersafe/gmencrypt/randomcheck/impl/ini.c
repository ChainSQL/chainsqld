/*
* File: ini.c
* Desc: Read and modify 'ini' config file, support predefine, such as '$','%'
* Copyright (c) sansec 2009
*
* Date        By who     Desc
* ----------  ---------  -------------------------------
* 2009.04.19  Yaahao     Created
* 2009.04.30  yaahao     ReadLine()中如果文件最后无空行时最后一个字符丢失的错误
* 2009.08.20  ......     修改GetIniItem()找不到配置项依然正确返回的错误
* 2010.04.01  ......     Corrected GetIniItem() (not close(fp))
* 2012/3/25   GAO        修改为通用版本 INI_H_COMMON_VERSION
*                        增加SetIniItem功能：
*                            如果 value == NULL，则删除 item
*                            如果 item == NULL，则删除 section
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gmencrypt/randomcheck/swsds.h>
#include <gmencrypt/randomcheck/ini.h>


/******************************************************************************
 * Function: GetItemValue
 * Description: 读配置项的值
 * Input: char *stConfigFileName 配置文件名
 *        char *item 配置项名称
 * Output:char *value 配置项的值
 * Return: SDR_OK 成功 其他 失败
 * Others:
******************************************************************************/
int GetItemValue(char *stConfigFileName, char *item, char *value)
{
	FILE *fp = NULL;
	char *ptr = NULL;
	char line[128];
	int i = 0;
	
	if(stConfigFileName == NULL)
	{
		return -1;
	}
	
	memset(line, 0, sizeof(line));
	
	fp = fopen(stConfigFileName, "r");
	if(fp == NULL)
	{
		return -1;
	}
	
	while(fgets(line, sizeof(line), fp) != NULL)
	{
		if(line[0] == '#')
		{
			continue;
		}

		i = strlen(line);

		if(line[i-1] == '\n')
		{
			line[i-1] = '\0';
		}

		i = strlen(item);

		if(strncmp(item, line,strlen(item))  == 0 && (line[i] == '=' || line[i] == ' '))
		{
			ptr = strstr(line, "=");
			if(ptr != NULL)
			{
				*ptr = 0;

				do
				{
					++ptr;
				} while(*ptr == ' ' || *ptr == '\t');
			}

			strcpy(value, ptr);

			fclose(fp);

			return SDR_OK;
		}
	}

	//没有找到配置项
	fclose(fp);

	return -1;
}