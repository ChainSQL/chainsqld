/*
* File: swsds.h
* Copyright (c) SWXA 2009
*
*
*/

#ifndef _SW_SDS_H_
#define _SW_SDS_H_ 1

#ifdef __cplusplus
	extern "C"{
#endif

#define SGD_API_VERSION		0x01000000 /*api version*/

/*�������Ͷ���*/
typedef char				SGD_CHAR;
typedef char				SGD_INT8;
typedef short				SGD_INT16;
typedef int					SGD_INT32;
typedef long long			SGD_INT64;
typedef unsigned char		SGD_UCHAR;
typedef unsigned char		SGD_UINT8;
typedef unsigned short		SGD_UINT16;
typedef unsigned int		SGD_UINT32;
typedef unsigned long long	SGD_UINT64;
typedef unsigned int		SGD_RV;
typedef void*				SGD_OBJ;
typedef int					SGD_BOOL;
typedef void*				SGD_HANDLE;

/*�豸��Ϣ*/
typedef struct DeviceInfo_st{
	unsigned char IssuerName[40];
	unsigned char DeviceName[16];
	unsigned char DeviceSerial[16];
	unsigned int  DeviceVersion;
	unsigned int  StandardVersion;
	unsigned int  AsymAlgAbility[2];
	unsigned int  SymAlgAbility;
	unsigned int  HashAlgAbility;
	unsigned int  BufferSize;
} DEVICEINFO;

/*RSA��Կ*/
#define RSAref_MAX_BITS    2048
#define RSAref_MAX_LEN     ((RSAref_MAX_BITS + 7) / 8)
#define RSAref_MAX_PBITS   ((RSAref_MAX_BITS + 1) / 2)
#define RSAref_MAX_PLEN    ((RSAref_MAX_PBITS + 7)/ 8)

typedef struct RSArefPublicKey_st
{
	unsigned int  bits;
	unsigned char m[RSAref_MAX_LEN];
	unsigned char e[RSAref_MAX_LEN];
} RSArefPublicKey;

typedef struct RSArefPrivateKey_st
{
	unsigned int  bits;
	unsigned char m[RSAref_MAX_LEN];
	unsigned char e[RSAref_MAX_LEN];
	unsigned char d[RSAref_MAX_LEN];
	unsigned char prime[2][RSAref_MAX_PLEN];
	unsigned char pexp[2][RSAref_MAX_PLEN];
	unsigned char coef[RSAref_MAX_PLEN];
} RSArefPrivateKey;


/*ECC��Կ*/
#define ECCref_MAX_BITS			256 
#define ECCref_MAX_LEN			((ECCref_MAX_BITS+7) / 8)
#define ECCref_MAX_CIPHER_LEN	136

typedef struct ECCrefPublicKey_st
{
	unsigned int  bits;
	unsigned char x[ECCref_MAX_LEN]; 
	unsigned char y[ECCref_MAX_LEN]; 
} ECCrefPublicKey;

typedef struct ECCrefPrivateKey_st
{
    unsigned int  bits;
    unsigned char D[ECCref_MAX_LEN];
} ECCrefPrivateKey;

/*ECC ����*/
typedef struct ECCCipher_st
{
	unsigned int  clength;  //C����Ч����
	unsigned char x[ECCref_MAX_LEN]; 
	unsigned char y[ECCref_MAX_LEN]; 
	unsigned char C[ECCref_MAX_CIPHER_LEN];
	unsigned char M[ECCref_MAX_LEN];
} ECCCipher;

/*ECC ǩ��*/
typedef struct ECCSignature_st
{
	unsigned char r[ECCref_MAX_LEN];	
	unsigned char s[ECCref_MAX_LEN];	
} ECCSignature;

/*��������*/
#define SGD_TRUE		0x00000001
#define SGD_FALSE		0x00000000

/*�㷨��ʶ*/
#define SGD_SM1_ECB		0x00000101
#define SGD_SM1_CBC		0x00000102
#define SGD_SM1_CFB		0x00000104
#define SGD_SM1_OFB		0x00000108
#define SGD_SM1_MAC		0x00000110

#define SGD_SSF33_ECB	0x00000201
#define SGD_SSF33_CBC	0x00000202
#define SGD_SSF33_CFB	0x00000204
#define SGD_SSF33_OFB	0x00000208
#define SGD_SSF33_MAC	0x00000210

#define SGD_AES_ECB		0x00000401
#define SGD_AES_CBC		0x00000402
#define SGD_AES_CFB		0x00000404
#define SGD_AES_OFB		0x00000408
#define SGD_AES_MAC		0x00000410

#define SGD_3DES_ECB	0x00000801
#define SGD_3DES_CBC	0x00000802
#define SGD_3DES_CFB	0x00000804
#define SGD_3DES_OFB	0x00000808
#define SGD_3DES_MAC	0x00000810

#define SGD_SMS4_ECB	0x00002001
#define SGD_SMS4_CBC	0x00002002
#define SGD_SMS4_CFB	0x00002004
#define SGD_SMS4_OFB	0x00002008
#define SGD_SMS4_MAC	0x00002010

#define SGD_DES_ECB		0x00004001
#define SGD_DES_CBC		0x00004002
#define SGD_DES_CFB		0x00004004
#define SGD_DES_OFB		0x00004008
#define SGD_DES_MAC		0x00004010

#define SGD_RSA			0x00010000
#define SGD_RSA_SIGN	0x00010100
#define SGD_RSA_ENC		0x00010200
#define SGD_SM2_1		0x00020100
#define SGD_SM2_2		0x00020200
#define SGD_SM2_3		0x00020400

#define SGD_SM3			0x00000001
#define SGD_SHA1		0x00000002
#define SGD_SHA256		0x00000004
#define SGD_SHA512		0x00000008
#define SGD_SHA384		0x00000010
#define SGD_SHA224		0x00000020
#define SGD_MD5			0x00000080


/*��׼�����붨��*/
#define SDR_OK					0x0						   /*�ɹ�*/
#define SDR_BASE				0x01000000
#define SDR_UNKNOWERR			(SDR_BASE + 0x00000001)	   /*δ֪����*/
#define SDR_NOTSUPPORT			(SDR_BASE + 0x00000002)	   /*��֧��*/
#define SDR_COMMFAIL			(SDR_BASE + 0x00000003)    /*ͨ�Ŵ���*/
#define SDR_HARDFAIL			(SDR_BASE + 0x00000004)    /*Ӳ������*/
#define SDR_OPENDEVICE			(SDR_BASE + 0x00000005)    /*���豸����*/
#define SDR_OPENSESSION			(SDR_BASE + 0x00000006)    /*�򿪻Ự�������*/
#define SDR_PARDENY				(SDR_BASE + 0x00000007)    /*Ȩ�޲�����*/
#define SDR_KEYNOTEXIST			(SDR_BASE + 0x00000008)    /*��Կ������*/
#define SDR_ALGNOTSUPPORT		(SDR_BASE + 0x00000009)    /*��֧�ֵ��㷨*/
#define SDR_ALGMODNOTSUPPORT	(SDR_BASE + 0x0000000A)    /*��֧�ֵ��㷨ģʽ*/
#define SDR_PKOPERR				(SDR_BASE + 0x0000000B)    /*��Կ�������*/
#define SDR_SKOPERR				(SDR_BASE + 0x0000000C)    /*˽Կ�������*/
#define SDR_SIGNERR				(SDR_BASE + 0x0000000D)    /*ǩ������*/
#define SDR_VERIFYERR			(SDR_BASE + 0x0000000E)    /*��֤����*/
#define SDR_SYMOPERR			(SDR_BASE + 0x0000000F)    /*�Գ��������*/
#define SDR_STEPERR				(SDR_BASE + 0x00000010)    /*�������*/
#define SDR_FILESIZEERR			(SDR_BASE + 0x00000011)    /*�ļ���С������������ݳ��ȷǷ�*/
#define SDR_FILENOEXIST			(SDR_BASE + 0x00000012)    /*�ļ�������*/
#define SDR_FILEOFSERR			(SDR_BASE + 0x00000013)    /*�ļ�����ƫ��������*/
#define SDR_KEYTYPEERR			(SDR_BASE + 0x00000014)    /*��Կ���ʹ���*/
#define SDR_KEYERR				(SDR_BASE + 0x00000015)    /*��Կ����*/

/*============================================================*/
/*��չ������*/
#define SWR_BASE				(SDR_BASE + 0x00010000)	/*�Զ�����������ֵ*/
#define SWR_INVALID_USER		(SWR_BASE + 0x00000001)	/*��Ч���û���*/
#define SWR_INVALID_AUTHENCODE	(SWR_BASE + 0x00000002)	/*��Ч����Ȩ��*/
#define SWR_PROTOCOL_VER_ERR	(SWR_BASE + 0x00000003)	/*��֧�ֵ�Э��汾*/
#define SWR_INVALID_COMMAND		(SWR_BASE + 0x00000004)	/*�����������*/
#define SWR_INVALID_PARAMETERS	(SWR_BASE + 0x00000005)	/*����������������ݰ���ʽ*/
#define SWR_FILE_ALREADY_EXIST	(SWR_BASE + 0x00000006)	/*�Ѵ���ͬ���ļ�*/
#define SWR_SYNCH_ERR			(SWR_BASE + 0x00000007)	/*�࿨ͬ������*/
#define SWR_SYNCH_LOGIN_ERR		(SWR_BASE + 0x00000008)	/*�࿨ͬ�����¼����*/

#define SWR_SOCKET_TIMEOUT		(SWR_BASE + 0x00000100)	/*��ʱ����*/
#define SWR_CONNECT_ERR			(SWR_BASE + 0x00000101)	/*���ӷ���������*/
#define SWR_SET_SOCKOPT_ERR		(SWR_BASE + 0x00000102)	/*����Socket��������*/
#define SWR_SOCKET_SEND_ERR		(SWR_BASE + 0x00000104)	/*����LOGINRequest����*/
#define SWR_SOCKET_RECV_ERR		(SWR_BASE + 0x00000105)	/*����LOGINRequest����*/
#define SWR_SOCKET_RECV_0		(SWR_BASE + 0x00000106)	/*����LOGINRequest����*/

#define SWR_SEM_TIMEOUT			(SWR_BASE + 0x00000200)	/*��ʱ����*/
#define SWR_NO_AVAILABLE_HSM	(SWR_BASE + 0x00000201)	/*û�п��õļ��ܻ�*/
#define SWR_NO_AVAILABLE_CSM	(SWR_BASE + 0x00000202)	/*���ܻ���û�п��õļ���ģ��*/

#define SWR_CONFIG_ERR			(SWR_BASE + 0x00000301)	/*�����ļ�����*/

/*============================================================*/
/*���뿨������*/
#define SWR_CARD_BASE					(SDR_BASE + 0x00020000)			/*���뿨������*/
#define SWR_CARD_UNKNOWERR				(SWR_CARD_BASE + 0x00000001)	//δ֪����
#define SWR_CARD_NOTSUPPORT				(SWR_CARD_BASE + 0x00000002)	//��֧�ֵĽӿڵ���
#define SWR_CARD_COMMFAIL				(SWR_CARD_BASE + 0x00000003)	//���豸ͨ��ʧ��
#define SWR_CARD_HARDFAIL				(SWR_CARD_BASE + 0x00000004)	//����ģ������Ӧ
#define SWR_CARD_OPENDEVICE				(SWR_CARD_BASE + 0x00000005)	//���豸ʧ��
#define SWR_CARD_OPENSESSION			(SWR_CARD_BASE + 0x00000006)	//�����Ựʧ��
#define SWR_CARD_PARDENY				(SWR_CARD_BASE + 0x00000007)	//��˽Կʹ��Ȩ��
#define SWR_CARD_KEYNOTEXIST			(SWR_CARD_BASE + 0x00000008)	//�����ڵ���Կ����
#define SWR_CARD_ALGNOTSUPPORT			(SWR_CARD_BASE + 0x00000009)	//��֧�ֵ��㷨����
#define SWR_CARD_ALGMODNOTSUPPORT		(SWR_CARD_BASE + 0x00000010)	//��֧�ֵ��㷨����
#define SWR_CARD_PKOPERR				(SWR_CARD_BASE + 0x00000011)	//��Կ����ʧ��
#define SWR_CARD_SKOPERR				(SWR_CARD_BASE + 0x00000012)	//˽Կ����ʧ��
#define SWR_CARD_SIGNERR				(SWR_CARD_BASE + 0x00000013)	//ǩ������ʧ��
#define SWR_CARD_VERIFYERR				(SWR_CARD_BASE + 0x00000014)	//��֤ǩ��ʧ��
#define SWR_CARD_SYMOPERR				(SWR_CARD_BASE + 0x00000015)	//�Գ��㷨����ʧ��
#define SWR_CARD_STEPERR				(SWR_CARD_BASE + 0x00000016)	//�ಽ���㲽�����
#define SWR_CARD_FILESIZEERR			(SWR_CARD_BASE + 0x00000017)	//�ļ����ȳ�������
#define SWR_CARD_FILENOEXIST			(SWR_CARD_BASE + 0x00000018)	//ָ�����ļ�������
#define SWR_CARD_FILEOFSERR				(SWR_CARD_BASE + 0x00000019)	//�ļ���ʼλ�ô���
#define SWR_CARD_KEYTYPEERR				(SWR_CARD_BASE + 0x00000020)	//��Կ���ʹ���
#define SWR_CARD_KEYERR					(SWR_CARD_BASE + 0x00000021)	//��Կ����
#define SWR_CARD_BUFFER_TOO_SMALL		(SWR_CARD_BASE + 0x00000101)	//���ղ����Ļ�����̫С
#define SWR_CARD_DATA_PAD				(SWR_CARD_BASE + 0x00000102)	//����û�а���ȷ��ʽ��䣬����ܵõ����������ݲ���������ʽ
#define SWR_CARD_DATA_SIZE				(SWR_CARD_BASE + 0x00000103)	//���Ļ����ĳ��Ȳ�������Ӧ���㷨Ҫ��
#define SWR_CARD_CRYPTO_NOT_INIT		(SWR_CARD_BASE + 0x00000104)	//�ô������û��Ϊ��Ӧ���㷨���ó�ʼ������

//01/03/09�����뿨Ȩ�޹��������
#define SWR_CARD_MANAGEMENT_DENY		(SWR_CARD_BASE + 0x00001001)	//����Ȩ�޲�����
#define SWR_CARD_OPERATION_DENY			(SWR_CARD_BASE + 0x00001002)	//����Ȩ�޲�����
#define SWR_CARD_DEVICE_STATUS_ERR		(SWR_CARD_BASE + 0x00001003)	//��ǰ�豸״̬���������в���
#define SWR_CARD_LOGIN_ERR				(SWR_CARD_BASE + 0x00001011)	//��¼ʧ��
#define SWR_CARD_USERID_ERR				(SWR_CARD_BASE + 0x00001012)	//�û�ID��Ŀ/�������
#define SWR_CARD_PARAMENT_ERR			(SWR_CARD_BASE + 0x00001013)	//��������

//05/06�����뿨Ȩ�޹��������
#define SWR_CARD_MANAGEMENT_DENY_05		(SWR_CARD_BASE + 0x00000801)	//����Ȩ�޲�����
#define SWR_CARD_OPERATION_DENY_05		(SWR_CARD_BASE + 0x00000802)	//����Ȩ�޲�����
#define SWR_CARD_DEVICE_STATUS_ERR_05	(SWR_CARD_BASE + 0x00000803)	//��ǰ�豸״̬���������в���
#define SWR_CARD_LOGIN_ERR_05			(SWR_CARD_BASE + 0x00000811)	//��¼ʧ��
#define SWR_CARD_USERID_ERR_05			(SWR_CARD_BASE + 0x00000812)	//�û�ID��Ŀ/�������
#define SWR_CARD_PARAMENT_ERR_05		(SWR_CARD_BASE + 0x00000813)	//��������

/*============================================================*/
/*����������*/
#define SWR_CARD_READER_BASE				(SDR_BASE + 0x00030000)	//	���������ʹ���
#define SWR_CARD_READER_PIN_ERROR			(SWR_CARD_READER_BASE + 0x000063CE)  //�������
#define SWR_CARD_READER_NO_CARD				(SWR_CARD_READER_BASE + 0x0000FF01)	 //	ICδ����
#define SWR_CARD_READER_CARD_INSERT			(SWR_CARD_READER_BASE + 0x0000FF02)	 //	IC���뷽�����򲻵�λ
#define SWR_CARD_READER_CARD_INSERT_TYPE	(SWR_CARD_READER_BASE + 0x0000FF03)	 //	IC���ʹ���

/*�豸�����ຯ��*/
SGD_RV SDF_OpenDevice(SGD_HANDLE *phDeviceHandle);
SGD_RV SDF_CloseDevice(SGD_HANDLE hDeviceHandle);
SGD_RV SDF_OpenSession(SGD_HANDLE hDeviceHandle, SGD_HANDLE *phSessionHandle);
SGD_RV SDF_CloseSession(SGD_HANDLE hSessionHandle);
SGD_RV SDF_GetDeviceInfo(SGD_HANDLE hSessionHandle, DEVICEINFO *pstDeviceInfo);
SGD_RV SDF_GenerateRandom(SGD_HANDLE hSessionHandle, SGD_UINT32  uiLength, SGD_UCHAR *pucRandom);

SGD_RV SDF_GenerateRandomEx(SGD_HANDLE hSessionHandle, SGD_UINT32 uiCardNum, SGD_UINT32 uiHardwareFlag, SGD_UINT32 uiLength, SGD_UCHAR *pucRandom);

SGD_RV SDF_GetPrivateKeyAccessRight(SGD_HANDLE hSessionHandle, SGD_UINT32 uiKeyIndex,SGD_UCHAR *pucPassword, SGD_UINT32  uiPwdLength);
SGD_RV SDF_ReleasePrivateKeyAccessRight(SGD_HANDLE hSessionHandle, SGD_UINT32  uiKeyIndex);
SGD_RV SDF_GetFirmwareVersion(SGD_HANDLE hSessionHandle, SGD_UCHAR * sFirmware, SGD_UINT32 * ulFirmwareLen);
SGD_RV SDF_GetLibraryVersion(SGD_HANDLE hSessionHandle, SGD_UCHAR * sLibraryVersion, SGD_UINT32 * ulLibraryVersionLen);

/*��Կ�����ຯ��*/
SGD_RV SDF_GenerateKeyPair_RSA(SGD_HANDLE hSessionHandle, SGD_UINT32  uiKeyBits, RSArefPublicKey *pucPublicKey, RSArefPrivateKey *pucPrivateKey);
SGD_RV SDF_ExportSignPublicKey_RSA(SGD_HANDLE hSessionHandle, SGD_UINT32  uiKeyIndex, RSArefPublicKey *pucPublicKey);
SGD_RV SDF_ExportEncPublicKey_RSA(SGD_HANDLE hSessionHandle, SGD_UINT32  uiKeyIndex, RSArefPublicKey *pucPublicKey);
SGD_RV SDF_GenerateKeyWithIPK_RSA(SGD_HANDLE hSessionHandle, SGD_UINT32 uiIPKIndex, SGD_UINT32 uiKeyBits, SGD_UCHAR *pucKey, SGD_UINT32 *puiKeyLength, SGD_HANDLE *phKeyHandle);
SGD_RV SDF_GenerateKeyWithEPK_RSA(SGD_HANDLE hSessionHandle, SGD_UINT32 uiKeyBits, RSArefPublicKey *pucPublicKey, SGD_UCHAR *pucKey, SGD_UINT32 *puiKeyLength, SGD_HANDLE *phKeyHandle);
SGD_RV SDF_ImportKeyWithISK_RSA(SGD_HANDLE hSessionHandle, SGD_UINT32 uiISKIndex, SGD_UCHAR *pucKey, SGD_UINT32 uiKeyLength, SGD_HANDLE *phKeyHandle);
SGD_RV SDF_ExchangeDigitEnvelopeBaseOnRSA(SGD_HANDLE hSessionHandle, SGD_UINT32 uiKeyIndex, RSArefPublicKey *pucPublicKey, SGD_UCHAR *pucDEInput, SGD_UINT32 uiDELength, \
										  SGD_UCHAR *pucDEOutput, SGD_UINT32  *puiDELength);

SGD_RV SDF_ImportKey(SGD_HANDLE hSessionHandle, SGD_UCHAR *pucKey, SGD_UINT32 uiKeyLength, SGD_HANDLE *phKeyHandle);
SGD_RV SDF_DestroyKey(SGD_HANDLE hSessionHandle, SGD_HANDLE hKeyHandle);
SGD_RV SDF_GetSymmKeyHandle(SGD_HANDLE hSessionHandle, SGD_UINT32 uiKeyIndex, SGD_HANDLE *phKeyHandle);
SGD_RV SDF_GenerateKeyWithKEK(SGD_HANDLE hSessionHandle, SGD_UINT32 uiKeyBits, SGD_UINT32 uiAlgID,SGD_UINT32 uiKEKIndex, SGD_UCHAR *pucKey, SGD_UINT32 *puiKeyLength, SGD_HANDLE *phKeyHandle);
SGD_RV SDF_ImportKeyWithKEK(SGD_HANDLE hSessionHandle, SGD_UINT32 uiAlgID, SGD_UINT32 uiKEKIndex, SGD_UCHAR *pucKey, SGD_UINT32 uiKeyLength, SGD_HANDLE *phKeyHandle);

SGD_RV SDF_GenerateKeyPair_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiAlgID,SGD_UINT32  uiKeyBits, ECCrefPublicKey *pucPublicKey, ECCrefPrivateKey *pucPrivateKey);
SGD_RV SDF_ExportSignPublicKey_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32  uiKeyIndex, ECCrefPublicKey *pucPublicKey);
SGD_RV SDF_ExportEncPublicKey_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32  uiKeyIndex, ECCrefPublicKey *pucPublicKey);
SGD_RV SDF_GenerateAgreementDataWithECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiISKIndex, SGD_UINT32 uiKeyBits, SGD_UCHAR *pucSponsorID, SGD_UINT32 uiSponsorIDLength, \
										ECCrefPublicKey *pucSponsorPublicKey, ECCrefPublicKey *pucSponsorTmpPublicKey, SGD_HANDLE *phAgreementHandle);
SGD_RV SDF_GenerateKeyWithECC(SGD_HANDLE hSessionHandle, SGD_UCHAR *pucResponseID, SGD_UINT32 uiResponseIDLength, ECCrefPublicKey *pucResponsePublicKey, \
							  ECCrefPublicKey *pucResponseTmpPublicKey, SGD_HANDLE hAgreementHandle, SGD_HANDLE *phKeyHandle);
SGD_RV SDF_GenerateAgreementDataAndKeyWithECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiISKIndex, SGD_UINT32 uiKeyBits, SGD_UCHAR *pucResponseID, SGD_UINT32 uiResponseIDLength, \
											  SGD_UCHAR *pucSponsorID, SGD_UINT32 uiSponsorIDLength, ECCrefPublicKey *pucSponsorPublicKey, ECCrefPublicKey *pucSponsorTmpPublicKey, \
											  ECCrefPublicKey  *pucResponsePublicKey, ECCrefPublicKey  *pucResponseTmpPublicKey, SGD_HANDLE *phKeyHandle);
SGD_RV SDF_GenerateKeyWithIPK_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiIPKIndex, SGD_UINT32 uiKeyBits, ECCCipher *pucKey, SGD_HANDLE *phKeyHandle);
SGD_RV SDF_GenerateKeyWithEPK_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiKeyBits, SGD_UINT32 uiAlgID, ECCrefPublicKey *pucPublicKey, ECCCipher *pucKey, SGD_HANDLE *phKeyHandle);
SGD_RV SDF_ImportKeyWithISK_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiISKIndex, ECCCipher *pucKey, SGD_HANDLE *phKeyHandle);
SGD_RV SDF_ExchangeDigitEnvelopeBaseOnECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiKeyIndex, SGD_UINT32 uiAlgID, ECCrefPublicKey *pucPublicKey, ECCCipher *pucEncDataIn, \
										  ECCCipher *pucEncDataOut);

/*�ǶԳ��������㺯��*/
SGD_RV SDF_ExternalPublicKeyOperation_RSA(SGD_HANDLE hSessionHandle, RSArefPublicKey *pucPublicKey, SGD_UCHAR *pucDataInput, SGD_UINT32 uiInputLength, SGD_UCHAR *pucDataOutput, \
										  SGD_UINT32 *puiOutputLength);
SGD_RV SDF_ExternalPrivateKeyOperation_RSA(SGD_HANDLE hSessionHandle, RSArefPrivateKey *pucPrivateKey, SGD_UCHAR *pucDataInput, SGD_UINT32 uiInputLength, SGD_UCHAR *pucDataOutput, \
										   SGD_UINT32 *puiOutputLength);
SGD_RV SDF_InternalPublicKeyOperation_RSA(SGD_HANDLE hSessionHandle, SGD_UINT32 uiKeyIndex, SGD_UINT32 uiKeyUsage, SGD_UCHAR *pucDataInput, SGD_UINT32 uiInputLength, SGD_UCHAR *pucDataOutput, \
										  SGD_UINT32 *puiOutputLength);
SGD_RV SDF_InternalPrivateKeyOperation_RSA(SGD_HANDLE hSessionHandle, SGD_UINT32 uiKeyIndex, SGD_UINT32 uiKeyUsage, SGD_UCHAR *pucDataInput, SGD_UINT32 uiInputLength, \
										   SGD_UCHAR *pucDataOutput, SGD_UINT32 *puiOutputLength);

SGD_RV SDF_ExternalSign_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiAlgID, ECCrefPrivateKey *pucPrivateKey, SGD_UCHAR *pucData, SGD_UINT32 uiDataLength, ECCSignature *pucSignature);
SGD_RV SDF_ExternalVerify_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiAlgID, ECCrefPublicKey *pucPublicKey, SGD_UCHAR *pucDataInput, SGD_UINT32 uiInputLength, ECCSignature *pucSignature);
SGD_RV SDF_InternalSign_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiISKIndex, SGD_UCHAR *pucData, SGD_UINT32 uiDataLength, ECCSignature *pucSignature);
SGD_RV SDF_InternalVerify_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiISKIndex, SGD_UCHAR *pucData, SGD_UINT32 uiDataLength, ECCSignature *pucSignature);
SGD_RV SDF_ExternalEncrypt_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiAlgID, ECCrefPublicKey *pucPublicKey, SGD_UCHAR *pucData, SGD_UINT32 uiDataLength, ECCCipher *pucEncData);
SGD_RV SDF_ExternalDecrypt_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiAlgID, ECCrefPrivateKey *pucPrivateKey, ECCCipher *pucEncData, SGD_UCHAR *pucData, SGD_UINT32 *puiDataLength);
SGD_RV SDF_InternalEncrypt_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiIPKIndex, SGD_UINT32 uiAlgID, SGD_UCHAR *pucData, SGD_UINT32 uiDataLength, ECCCipher *pucEncData);
SGD_RV SDF_InternalDecrypt_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiISKIndex, SGD_UINT32 uiAlgID, ECCCipher *pucEncData, SGD_UCHAR *pucData, SGD_UINT32 *puiDataLength);

/*�Գ��������㺯��*/
SGD_RV SDF_Encrypt(SGD_HANDLE hSessionHandle, SGD_HANDLE hKeyHandle, SGD_UINT32 uiAlgID, SGD_UCHAR *pucIV, SGD_UCHAR *pucData, SGD_UINT32 uiDataLength, SGD_UCHAR *pucEncData, \
				   SGD_UINT32 *puiEncDataLength);
SGD_RV SDF_Decrypt(SGD_HANDLE hSessionHandle, SGD_HANDLE hKeyHandle, SGD_UINT32 uiAlgID, SGD_UCHAR *pucIV, SGD_UCHAR *pucEncData, SGD_UINT32 uiEncDataLength, SGD_UCHAR *pucData, \
				   SGD_UINT32 *puiDataLength);
SGD_RV SDF_CalculateMAC(SGD_HANDLE hSessionHandle, SGD_HANDLE hKeyHandle, SGD_UINT32 uiAlgID, SGD_UCHAR *pucIV, SGD_UCHAR *pucData, SGD_UINT32 uiDataLength, SGD_UCHAR *pucMAC, \
						SGD_UINT32 *puiMACLength);

/*�Ӵ����㺯��*/
SGD_RV SDF_HashInit(SGD_HANDLE hSessionHandle, SGD_UINT32 uiAlgID, ECCrefPublicKey *pucPublicKey, SGD_UCHAR *pucID, SGD_UINT32 uiIDLength);
SGD_RV SDF_HashUpdate(SGD_HANDLE hSessionHandle, SGD_UCHAR *pucData, SGD_UINT32 uiDataLength);
SGD_RV SDF_HashFinal(SGD_HANDLE hSessionHandle, SGD_UCHAR *pucHash, SGD_UINT32 *puiHashLength);
SGD_RV SDF_HashInit_ECC(SGD_HANDLE hSessionHandle, SGD_UINT32 uiAlgID, SGD_UCHAR *pucParamID);

/*�û��ļ���������*/
SGD_RV SDF_CreateFile(SGD_HANDLE hSessionHandle, SGD_UCHAR *pucFileName, SGD_UINT32 uiNameLen, SGD_UINT32 uiFileSize);
SGD_RV SDF_ReadFile(SGD_HANDLE hSessionHandle, SGD_UCHAR *pucFileName, SGD_UINT32 uiNameLen, SGD_UINT32 uiOffset, SGD_UINT32 *puiReadLength, SGD_UCHAR *pucBuffer);
SGD_RV SDF_WriteFile(SGD_HANDLE hSessionHandle, SGD_UCHAR *pucFileName, SGD_UINT32 uiNameLen, SGD_UINT32 uiOffset, SGD_UINT32 uiWriteLength, SGD_UCHAR *pucBuffer);
SGD_RV SDF_DeleteFile(SGD_HANDLE hSessionHandle, SGD_UCHAR *pucFileName, SGD_UINT32 uiNameLen);

#ifdef __cplusplus
}
#endif

#endif /*#ifndef _SW_SDS_H_*/