/*
* File: ini.c
* Desc: Read and modify 'ini' config file, support predefine, such as '$','%'
* Copyright (c) sansec 2009
*
* Date        By who     Desc
* ----------  ---------  -------------------------------
* 2009.04.19  Yaahao     Created
* 2009.04.30  yaahao     ReadLine()������ļ�����޿���ʱ���һ���ַ���ʧ�Ĵ���
* 2009.08.20  ......     �޸�GetIniItem()�Ҳ�����������Ȼ��ȷ���صĴ���
* 2010.04.01  ......     Corrected GetIniItem() (not close(fp))
* 2012/3/25   GAO        �޸�Ϊͨ�ð汾 INI_H_COMMON_VERSION
*                        ����SetIniItem���ܣ�
*                            ��� value == NULL����ɾ�� item
*                            ��� item == NULL����ɾ�� section
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <peersafe/gmencrypt/randomcheck/swsds.h>
#include <peersafe/gmencrypt/randomcheck/ini.h>


/******************************************************************************
 * Function: GetItemValue
 * Description: ���������ֵ
 * Input: char *stConfigFileName �����ļ���
 *        char *item ����������
 * Output:char *value �������ֵ
 * Return: SDR_OK �ɹ� ���� ʧ��
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

	//û���ҵ�������
	fclose(fp);

	return -1;
}