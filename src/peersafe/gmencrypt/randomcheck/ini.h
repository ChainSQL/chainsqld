/*
* File: ini.h
* Desc: ��ȡ/�޸� 'ini' �����ļ�, 
*       ����ֵ֧��'$'(Linux/UNIX),'%%'(Windows)��ͨ���
*       '#'Ϊע�ͣ�����������ף�
* Copyright (c) sansec 2009
*
*/

#ifndef _SW_INI_H_
#define _SW_INI_H_ 1

/******************************************************************************
 * Function: GetItemValue
 * Description: ���������ֵ
 * Input: char *stConfigFileName �����ļ���
 *        char *item ����������
 * Output:char *value �������ֵ
 * Return: SDR_OK �ɹ� ���� ʧ��
 * Others:
******************************************************************************/
int GetItemValue(char *stConfigFileName, char *item, char *value);

#endif
