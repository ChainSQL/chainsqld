//------------------------------------------------------------------------------
/*
 This file is part of chainsqld: https://github.com/chainsql/chainsqld
 Copyright (c) 2016-2018 Peersafe Technology Co., Ltd.
 
	chainsqld is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
 
	chainsqld is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
 */
//==============================================================================

#ifndef	__SWSDKEY_H
#define	__SWSDKEY_H

#ifdef __cplusplus
extern "C" {
#endif
    
    typedef void* HANDLE;
    typedef unsigned char BYTE;
    
#define IN
#define OUT
    
    //管理员/用户口令 类型
#define TYPE_SO				1	//管理员 pin.
#define TYPE_USER			2	//用户 pin.
#define TYPE_PIN_MINLEN		4	//Min Pin Len
#define TYPE_PIN_MAXLEN		16	//Max Pin Len
    
    //密钥/证书类型
#define TYPE_SIGN			2	//签名类型
#define TYPE_ENCRYPT		1	//加密类型
    
    //容器宏定义
#define MAX_KEY_NUMBER		4		//最大容器数
    
    
    //卷标
#define TYPE_LABEL_NAME_MINLEN		5		//最小卷标名称长度
#define TYPE_LABEL_NAME_MAXLEN		16		//最大卷标名称长度
    
    
    //
    //获取SDKEY设备信息宏定义
    //
#define EP_FREE_SIZE 					1	//SDKey 剩余空间
#define EP_SERIAL_NUMBER			2	//SDKey 硬件唯一序列号
#define EP_MAX_RETRY_TIMES		3	//获取当前SDKEY剩余的口令重试次数
    
    //
    //摘要算法类型
    //
#define HASH_ALG_MD2	1
#define HASH_ALG_MD5	2
#define HASH_ALG_SHA1_160	3
#define SGD_SHA1 3
#define SGD_SM3	4
    
    //
    //对称算法类型
    //
    
#define ALG_DES			1
#define ALG_3DES			2
#define ALG_AES			3
#define ALG_SSF33			4
#define ALG_SM1			5
#define ALG_SM4         6
    
    //补丁方式
#define PADDING_TYPE_NONE		0
#define PADDING_TYPE_PKCS5		1
    
    //
    //加密模式
    //
#define ALG_MOD_ECB			1
#define ALG_MOD_CBC			2
    
    
    //用户文件宏
#define EP_PUBLIC							0x00000001		//公有区 读取数据不受pin保护
#define EP_PRIVATE 						0x00000002		//私有区 读取数据受pin保护
#define EP_FILENAME_MAXLEN				24//20				//用户文件名称的最大长度
#define EP_FILE_MAX_NUM_PUB				10				//存储用户公有文件的最大数量
#define EP_FILE_MAX_NUM_PRI				10				//存储用户私有文件的最大数量
#define EP_FILE_MAX_NUM				 (EP_FILE_MAX_NUM_PUB+EP_FILE_MAX_NUM_PRI)	//存储用户文件的最大数量
    
    //定义 FILEINFO 结构主要列举时使用
    typedef struct
    {
        unsigned char 	pbFileName[EP_FILENAME_MAXLEN];	//文件名称
        unsigned long	dwFileNameLen;					//文件名称长度
        unsigned long	dwFileType;						//文件类型
        unsigned long	dwFileLen;						//文件大小
    }FILEINFO, *PFILEINFO;
    
    /*ECC*/
#define ECC_MAX_MODULUS_BITS_LEN 256
    /*ECC公钥数据结构*/
    typedef struct Struct_ECCPUBLICKEYBLOB{
        BYTE XCoordinate[ECC_MAX_MODULUS_BITS_LEN/8];
        BYTE YCoordinate[ECC_MAX_MODULUS_BITS_LEN/8];
    } ECCPUBLICKEYBLOB, *PECCPUBLICKEYBLOB;
    
    /*ECC私钥数据结构*/
    typedef struct Struct_ECCPRIVATEKEYBLOB{
        BYTE PrivateKey[ECC_MAX_MODULUS_BITS_LEN/8];
    } ECCPRIVATEKEYBLOB, *PECCPRIVATEKEYBLOB;
    
    /*ECC签名数据结构*/
    typedef struct Struct_ECCSIGNATUREBLOB{
        BYTE r[ECC_MAX_MODULUS_BITS_LEN/8];
        BYTE s[ECC_MAX_MODULUS_BITS_LEN/8];
    } ECCSIGNATUREBLOB, *PECCSIGNATUREBLOB;
    
    //-----------------------------------------------------------------------------
    //错误代码
    //-----------------------------------------------------------------------------
#define SDKEY_SUCCESS						0x00000000	//操作成功
#define SDKEY_FAILED						0x20000001	//操作失败
#define SDKEY_KEY_REMOVED					0x20000002	//未插入SDKEY
#define SDKEY_KEY_INVALID					0x20000003	//连接SDKEY失败/SDKEY无效
#define SDKEY_INVALID_PARAMETER  			0x20000004	//参数错误
#define SDKEY_VERIFIEDPIN_FAILED			0x20000005	//验证口令失败
#define SDKEY_USER_NOT_LOG_IN				0x20000006	//用户没有登陆，没有验证口令
#define SDKEY_BUFFER_TOO_SMALL       		0x20000007	//缓冲区太小
#define SDKEY_CONTAINER_TOOMORE 			0x20000008	//容器个数已满,大于10个
#define SDKEY_ERR_GETEKEYPARAM				0x20000009	//读取SDKEY信息失败
#define SDKEY_ERR_PINLOCKED					0x20000010	//密码已经锁死
#define SDKEY_ERR_CREATEFILE				0x20000011	//创建文件错误
#define SDKEY_ERR_EXISTFILE					0x20000012	//文件已存在错误
#define SDKEY_ERR_OPENFILE					0x20000013	//打开文件错误
#define SDKEY_ERR_READFILE					0x20000014	//读文件错误
#define SDKEY_ERR_WRITEFILE					0x20000015	//写文件错误
#define SDKEY_ERR_NOFILE					0x20000016	//没有找到文件错误
    
#define SDKEY_ERR_PARAMETER_NOT_SUPPORT				0x20000020	//不支持的参数
#define SDKEY_ERR_FUNCTION_NOT_SUPPORT				0x20000021	//不支持的函数
    
    
    /*
     函数功能：	打开SDKEY,得到操作句柄
     参数说明：	hKey:		返回操作句柄
     返回值：	SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long  SDKEY_OpenCard(IN OUT HANDLE* hKey);
    
    /*
     函数功能：	关闭SDKEY
     参数说明：	hKey:		操作句柄
     返回值：	SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long  SDKEY_CloseCard(IN HANDLE hKey);
    
    /*
     函数功能：	用户LogIn
     参数说明：	hKey:		操作句柄
     ulPINType:	管理员/用户口令类型
     pbPIN:		管理员/用户口令
     ulPINLen:	管理员/用户口令长度
     pulRetry:	口令可重试次数
     返回值：	SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long  SDKEY_LogIn(IN HANDLE	hKey,
                               IN unsigned long			ulPINType,
                               IN const unsigned char*		pbPIN,
                               IN unsigned long			ulPINLen,
                               OUT unsigned long		*pulRetry);
    
    /*
     函数功能：	用户LogOut
     参数说明：	hKey:		操作句柄
     返回值：	SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long  SDKEY_LogOut(IN HANDLE hKey);
    
    /*
     函数功能：	修改口令
     参数说明：	hKey:		操作句柄
     ulPINType:	管理员/用户口令类型
     pbOldPIN:		管理员/用户旧口令
     ulOldPINLen:	管理员/用户旧口令长度
     pbNewPIN:		管理员/用户新口令
     ulNewPINLen:	管理员/用户新口令长度
     pulRetry: 口令验证失败时返回可重试次数
     返回值：	SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long  SDKEY_ChangePIN(IN HANDLE	hKey,
                                   IN unsigned long	ulPINType,
                                   IN const unsigned char*	pbOldPIN,
                                   IN unsigned long		ulOldPINLen,
                                   IN const unsigned char*	pbNewPIN,
                                   IN unsigned long		ulNewPINLen,
                                   OUT unsigned long		*pulRetry);
    
    /*
     函数功能：	解锁口令
     参数说明：	hKey:		操作句柄
     pbPIN:		管理员口令
     ulPINLen:	管理员口令长度
     pbNewPIN:		用户新口令
     ulNewPINLen:	用户新口令长度
     pulRetry: 口令验证失败时返回管理员口令可重试次数
     返回值：	SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long  SDKEY_UnLock(IN HANDLE	hKey,
                                IN const unsigned char*	 pbPIN,    //管理员口令
                                IN unsigned long			ulPINLen,
                                IN const unsigned char*	     pbNewPIN,  //新的用户口令
                                IN unsigned long			ulNewPINLen,
                                OUT unsigned long		*pulRetry);
    
    
    /*
     函数功能：	取得SDKEY硬件参数
     参数说明：	hKey:		操作句柄
     dwParam:	获得信息类型，具体见 SDKEY设备信息宏定义
     pbData:			输出数据
     pdwDataLen:		输出数据长度
     dwFlags:		取值0，保留字节
     返回值：	SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long  SDKEY_GetKeyParam(IN HANDLE		hKey,
                                     IN unsigned long		dwParam,
                                     OUT unsigned char		*pbData,
                                     IN OUT unsigned long	*pdwDataLen);
    
    /*
     函数功能： 在SDKEY内创建文件
     参数说明：	hKey:   操作句柄
     *FileName    私有文件区名称
     FileNameLen  文件名称长度
     MaxFileLen   最大文件长度
     Flag         文件属性 (见用户文件宏),私有区文件还是共有区文件
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     备注:对sdkey内的文件操作无目录的概念。
     */
    unsigned long  SDKEY_CreateFile(IN HANDLE hKey,
                                    IN unsigned char *FileName,
                                    IN unsigned long FileNameLen,
                                    IN unsigned long MaxFileLen,
                                    IN unsigned long Flag    );
    
    /*
     函数功能： 写数据到SDKEY
     参数说明： hKey:   操作句柄
     *FileName    私有文件区名称
     FileNameLen  文件名称长度
     *pbData      数据内容
     pbDataLen    数据长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long  SDKEY_WriteData(IN HANDLE hKey,
                                   IN unsigned char *FileName,
                                   IN unsigned long FileNameLen,
                                   IN unsigned char *pbData,
                                   IN unsigned long pbDataLen);
    
    
    /*
     函数功能： 从SDKEY内读取数据
     参数说明： hKey:   操作句柄
     *FileName    私有文件区名称
     FileNameLen  文件名称长度
     *pbData      数据内容
     pbDataLen    数据长度
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long  SDKEY_ReadData(IN HANDLE hKey,
                                  IN unsigned char *FileName,
                                  IN unsigned long FileNameLen,
                                  OUT unsigned char *pbData,
                                  OUT unsigned long *pbDataLen);
    
    /*
     函数功能：  删除SDKEY内指定数据
     参数说明： hKey:   操作句柄
     *FileName    私有文件区名称
     FileNameLen  文件名称长度
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long  SDKEY_DelFile(IN HANDLE hKey,
                                 IN unsigned char *FileName,
                                 IN unsigned long FileNameLen);
    
    /*
     函数功能： 列举SDKEY内文件数据
     参数说明： hKey:   操作句柄
     *pbFileInfo    文件句柄
     pdwFileNum  文件个数
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long  SDKEY_ListFile(IN HANDLE hKey,
                                  OUT FILEINFO *pbFileInfo,
                                  OUT unsigned long *pdwFileNum);
    
    /*
     函数功能： 产生随机数
     参数说明： hKey:   操作句柄
     ulRandLen 待生产的随机数长度
     pRandom   输出随机数的缓冲区指针
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_GenRandom(IN HANDLE hKey,
                                  IN unsigned long ulRandLen,
                                  OUT unsigned char *pRandom);
    
    /*
     函数功能： 产生RSA密钥
     参数说明： hKey:   操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     ulModulusLen   rsa密钥模长，支持1024
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     备注：一个密钥容器下支持双密钥，即签名和加密(交货)2种。
     */
    unsigned long SDKEY_GenerateRsaKeyPair(IN HANDLE hKey,
                                           IN unsigned long ulAlias,
                                           IN unsigned long ulKeyUse,
                                           IN unsigned long ulModulusLen);
    
    /*
     函数功能： 获取公钥信息
     参数说明： hKey:   操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     pDerPubKey:der编码的公钥
     pulDerPubKeyLen：公钥长度，比如1024bits的rsa密钥,der编码的长度为140字节
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     备注：一个密钥容器下支持双密钥，即签名和加密(交货)2种。
     */
    unsigned long SDKEY_GetRsaPubLicKey(IN HANDLE hKey,
                                        IN unsigned long ulAlias,
                                        IN unsigned long ulKeyUse,
                                        OUT unsigned char *pDerPubKey,
                                        OUT unsigned long *pulDerPubKeyLen);
    
    /*
     函数功能： 导入证书到SDKEY内
     参数说明： hKey:   操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     pDerCert   Der编码的证书
     ulDerCertLen 证书长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_SetCertificate(IN HANDLE hKey,
                                       IN unsigned long ulAlias,
                                       IN unsigned long ulKeyUse,
                                       IN unsigned char *pDerCert,
                                       IN unsigned long ulDerCertLen);
    
    
    /*
     函数功能： 导出证书
     参数说明： hKey:   操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     pDerCert   Der编码的证书
     pulDerCertLen 证书长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_GetCertificate(IN HANDLE hKey,
                                       IN unsigned long ulAlias,
                                       IN unsigned long ulKeyUse,
                                       OUT unsigned char *pDerCert,
                                       OUT unsigned long *pulDerCertLen);
    
    /*
     函数功能： rsa签名（pkcs1格式）
     参数说明： hKey:   			操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     ulHashType  		摘要算法类型，HASH_ALG_MD2,HASH_ALG_MD5,HASH_ALG_SHA1_160
     
     pInData 			待签名的原文
     ulInDataLen    	待签名的原文长度
     pSignValue				签名值
     pulSignValueLen	签名值长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_RSASign(IN HANDLE hKey,
                                IN unsigned long ulAlias,
                                IN unsigned long ulKeyUse,
                                IN unsigned long ulHashType,
                                IN unsigned char *pInData,
                                IN unsigned long ulInDataLen,
                                OUT unsigned char *pSignValue,
                                OUT unsigned long *pulSignValueLen);
    
    /*
     函数功能： 验证签名（pkcs1格式）
     参数说明： hKey:操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     ulHashType摘要算法类型，
					HASH_ALG_MD2,HASH_ALG_MD5,HASH_ALG_SHA1_160
     pInData 	待签名的原文
     ulInDataLen待签名的原文长度
     pSignValue 签名值
     ulSignValueLen 签名值长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_RSAVerify(IN HANDLE hKey,
                                  IN unsigned long ulAlias,
                                  IN unsigned long ulKeyUse,
                                  IN unsigned long ulHashType,
                                  IN unsigned char *pInData,
                                  IN unsigned long ulInDataLen,
                                  IN unsigned char *pSignValue,
                                  IN unsigned long ulSignValueLen);
    
    
    /*
     函数功能： 公钥加密（pkcs1格式）
     参数说明： hKey:操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     pPlainData	明文
     ulPlainDataLen	明文长度
     pCipherData 	密文
     pulCipherDataLen密文长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_RSAPubKeyEncrypt(IN HANDLE hEkey,
                                         IN unsigned long ulAlias,
                                         IN unsigned long ulKeyUse,
                                         IN unsigned char * pPlainData,
                                         IN unsigned long ulPlainDataLen,
                                         OUT unsigned char * pCipherData,
                                         OUT unsigned long * pulCipherDataLen);
    
    
    /*
     函数功能： 私钥解密（pkcs1格式）
     参数说明： hEkey:   			操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     pCipherData 			密文
     ulCipherDataLen    	密文长度
     pPlainData				明文
     pulPlainDataLen	明文长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_RSAPriKeyDecrypt(IN HANDLE hEkey,
                                         IN unsigned long ulAlias,
                                         IN unsigned long ulKeyUse,
                                         IN unsigned char *pCipherData,
                                         IN unsigned long ulCipherDataLen,
                                         OUT unsigned char *pPlainData,
                                         OUT unsigned long *pulPlainDataLen);
    
    /*
     函数功能： 私钥加密（pkcs1格式）
     参数说明： hKey:操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     pPlainData	明文
     ulPlainDataLen	明文长度
     pCipherData 	密文
     pulCipherDataLen密文长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_RSAPriKeyEncrypt(IN HANDLE hEkey,
                                         IN unsigned long ulAlias,
                                         IN unsigned long ulKeyUse,
                                         IN unsigned char * pPlainData,
                                         IN unsigned long ulPlainDataLen,
                                         OUT unsigned char * pCipherData,
                                         OUT unsigned long * pulCipherDataLen);
    
    
    /*
     函数功能： 公钥解密（pkcs1格式）
     参数说明： hEkey:   			操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     pCipherData 			密文
     ulCipherDataLen    	密文长度
     pPlainData				明文
     pulPlainDataLen	明文长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_RSAPubKeyDecrypt(IN HANDLE hEkey,
                                         IN unsigned long ulAlias,
                                         IN unsigned long ulKeyUse,
                                         IN unsigned char *pCipherData,
                                         IN unsigned long ulCipherDataLen,
                                         OUT unsigned char *pPlainData,
                                         OUT unsigned long *pulPlainDataLen);
    
    /*
     函数功能： 对称算法加密
     参数说明： hEkey:   			操作句柄
     ulAlgFlag			算法类型，见 对称算法类型宏定义
     ulAlgMode		加密模式 ，见 加密模式宏定义
     iv						初始化向量
     pKey					密钥
     pPlainData			明文
     ulPlainDataLen	明文长度
     pCipherData 		密文
     pulCipherDataDataLen   密文长度
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_SymEncrypt(IN HANDLE hEkey,
                                   IN unsigned long ulAlgFlag,
                                   IN unsigned long ulAlgMode,
                                   IN unsigned char *iv,
                                   IN unsigned char *pKey,
                                   IN unsigned char *pPlainData,
                                   IN unsigned long ulPlainDataLen,
                                   OUT unsigned char *pCipherData,
                                   OUT unsigned long *pulCipherDataDataLen);
    
    /*
     函数功能： 对称算法解密
     参数说明： hEkey:   操作句柄
     ulAlgFlag				算法类型,见 对称算法类型宏定义
     ulAlgMode			加密模式 见 加密模式宏定义
     iv							初始化向量
     pKey						密钥
     pCipherData 			密文
     ulCipherDataDataLen    	密文长度
     pPlainData				明文
     pulPlainDataLen	明文长度
     
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_SymDecrypt(IN HANDLE hEkey,
                                   IN unsigned long ulAlgFlag,
                                   IN unsigned long ulAlgMode,
                                   IN unsigned char *iv,
                                   IN unsigned char *pKey,
                                   IN unsigned char *pCipherData ,
                                   IN unsigned long ulCipherDataDataLen,
                                   OUT unsigned char *pPlainData,
                                   OUT unsigned long *pulPlainDataLen);
    
    /*
     函数功能： 对称算法加密初始化
     参数说明： hEkey:   			操作句柄
     ulAlgFlag			算法类型，见 对称算法类型宏定义
     ulAlgMode		加密模式 ，见 加密模式宏定义
     ulPadding			补丁方式，见补丁方式宏定义
     iv						初始化向量
     pKey					密钥
     phKey				返回的算法处理句柄
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_SymEncryptInit(
                                       IN HANDLE hEkey,
                                       IN unsigned long ulAlgFlag,
                                       IN unsigned long ulAlgMode,
                                       IN unsigned long ulPadding,
                                       IN unsigned char *iv,
                                       IN unsigned char *pKey,
                                       OUT HANDLE	*phKey);
    
    /*
     函数功能： 对称算法加密中间处理函数
     参数说明： hEkey:   			操作句柄
     phKey				由SDKEY_SymEncryptInit生成的算法处理句柄
     pPlainData			明文
     ulPlainDataLen	明文长度
     pCipherData 		密文
     pulCipherDataLen   密文长度
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_SymEncryptUpdate(
                                         IN HANDLE hEkey,
                                         IN HANDLE phKey,
                                         IN unsigned char	*pPlainData,
                                         IN unsigned long ulPlainDataLen,
                                         OUT unsigned char *pCipherData,
                                         OUT unsigned long *pulCipherDataLen);
    
    /*
     函数功能： 对称算法加密结束函数
     参数说明： hEkey:   			操作句柄
     phKey				由SDKEY_SymEncryptInit生成的算法处理句柄
     pCipherData 		密文
     pulCipherDataLen   密文长度
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_SymEncryptFinal(
                                        IN HANDLE hEkey,
                                        IN HANDLE phKey,
                                        OUT unsigned char *pCipherData,
                                        OUT unsigned long *pulCipherDataLen);
    
    /*
     函数功能： 对称算法解密初始化
     参数说明： hEkey:   			操作句柄
     ulAlgFlag			算法类型，见 对称算法类型宏定义
     ulAlgMode		加密模式 ，见 加密模式宏定义
     ulPadding			补丁方式，见补丁方式宏定义
     iv						初始化向量
     pKey					密钥
     phKey				返回的算法处理句柄
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_SymDecryptInit(
                                       IN HANDLE hEkey,
                                       IN unsigned long ulAlgFlag,
                                       IN unsigned long ulAlgMode,
                                       IN unsigned long ulPadding,
                                       IN unsigned char *iv,
                                       IN unsigned char *pKey,
                                       OUT HANDLE	*phKey);
    
    /*
     函数功能： 对称算法解密中间处理函数
     参数说明： hEkey:   			操作句柄
     phKey				由SDKEY_SymDecryptInit生成的算法处理句柄
     pCipherData 		密文
     pulCipherDataLen   密文长度
     pPlainData			明文
     ulPlainDataLen	明文长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_SymDecryptUpdate(
                                         IN HANDLE hEkey,
                                         IN HANDLE phKey,
                                         IN unsigned char	*pCipherData,
                                         IN unsigned long ulCipherDataLen,
                                         OUT unsigned char *pPlainData,
                                         OUT unsigned long *pulPlainDataLen);
    
    /*
     函数功能： 对称算法解密结束函数
     参数说明： hEkey:   			操作句柄
     phKey				由SDKEY_SymDecryptInit生成的算法处理句柄
     pPlainData 		明文
     pulPlainDataLen 明文长度
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_SymDecryptFinal(
                                        IN HANDLE hEkey,
                                        IN HANDLE phKey,
                                        OUT unsigned char *pPlainData,
                                        OUT unsigned long *pulPlainDataLen);
    
    //ver2.2 add sm2 sm3 support
    /*
     函数功能： ECC签名（SM2）
     参数说明： hKey:   			操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     pInData 			待签名的HASH值
     ulInDataLen    	待签名的HASH值长度
     pSignValue			签名值
     pulSignValueLen	签名值长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_ECCSign(IN HANDLE hKey,
                                IN unsigned long ulAlias,
                                IN unsigned long ulKeyUse,
                                IN unsigned char *pInData,
                                IN unsigned long ulInDataLen,
                                OUT unsigned char *pSignValue,
                                OUT unsigned long *pulSignValueLen);
    
    /*
     函数功能： ECC验签（SM2）
     参数说明： hKey:操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     pInData 	待签名的HASH值
     ulInDataLen待签名的HASH值长度
     pSignValue 签名值
     ulSignValueLen 签名值长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_ECCVerify(IN HANDLE hKey,
                                  IN unsigned long ulAlias,
                                  IN unsigned long ulKeyUse,
                                  IN unsigned char *pInData,
                                  IN unsigned long ulInDataLen,
                                  IN unsigned char *pSignValue,
                                  IN unsigned long ulSignValueLen);
    
    
    /*
     函数功能： ECC公钥加密（SM2）
     参数说明： hKey:操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     pPlainData	明文
     ulPlainDataLen	明文长度
     pCipherData 	密文
     pulCipherDataLen密文长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     说明：密文长度为明文+96字节，顺序分别为C1C3C2，其中C1为一个点长度为64字节，C3长度固定为32字节，C2长度同明文
     */
    unsigned long SDKEY_ECCEncrypt(IN HANDLE hEkey,
                                   IN unsigned long ulAlias,
                                   IN unsigned long ulKeyUse,
                                   IN unsigned char * pPlainData,
                                   IN unsigned long ulPlainDataLen,
                                   OUT unsigned char * pCipherData,
                                   OUT unsigned long * pulCipherDataLen);
    
    
    /*
     函数功能： ECC私钥解密（SM2）
     参数说明： hEkey:   			操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     pCipherData 			密文
     ulCipherDataLen    	密文长度
     pPlainData				明文
     pulPlainDataLen	明文长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     说明：密文格式参考加密函数中说明
     */
    unsigned long SDKEY_ECCDecrypt(IN HANDLE hEkey,
                                   IN unsigned long ulAlias,
                                   IN unsigned long ulKeyUse,
                                   IN unsigned char *pCipherData,
                                   IN unsigned long ulCipherDataLen,
                                   OUT unsigned char *pPlainData,
                                   OUT unsigned long *pulPlainDataLen);
    
    /*
     函数功能： 外部ECC签名（SM2）
     参数说明： hKey:   			操作句柄
     pPriKey			外部输入的ECC私钥
     ulPriKeyLen		ECC私钥长度
     pInData 			待签名的HASH值
     ulInDataLen    	待签名的HASH值长度
     pSignValue			签名值
     pulSignValueLen	签名值长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_ExtECCSign(IN HANDLE hKey,
                                   IN unsigned char *pPriKey,
                                   IN unsigned long ulPriKeyLen,
                                   IN unsigned char *pInData,
                                   IN unsigned long ulInDataLen,
                                   OUT unsigned char *pSignValue,
                                   OUT unsigned long *pulSignValueLen);
    
    /*
     函数功能： 外部ECC验签（SM2）
     参数说明： hKey				操作句柄
     pPubKey		外部输入的ECC公钥
     ulPubKeyLen	公钥长度
     pInData 		待签名的HASH值
     ulInDataLen	待签名的HASH值长度
     pSignValue	签名值
     ulSignValueLen 签名值长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_ExtECCVerify(IN HANDLE hKey,
                                     IN unsigned char *pPubKey,
                                     IN unsigned long ulPubKeyLen,
                                     IN unsigned char *pInData,
                                     IN unsigned long ulInDataLen,
                                     IN unsigned char *pSignValue,
                                     IN unsigned long ulSignValueLen);
    
    
    /*
     函数功能： 外部ECC公钥加密（SM2）
     参数说明： hKey:操作句柄
     pPubKey		外部输入的ECC公钥
     ulPubKeyLen	公钥长度
     pPlainData		明文
     ulPlainDataLen	明文长度
     pCipherData 	密文
     pulCipherDataLen密文长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     说明：密文长度为明文+96字节，顺序分别为C1C3C2，其中C1为一个点长度为64字节，C3长度固定为32字节，C2长度同明文
     */
    unsigned long SDKEY_ExtECCEncrypt(IN HANDLE hEkey,
                                      IN unsigned char *pPubKey,
                                      IN unsigned long ulPubKeyLen,
                                      IN unsigned char * pPlainData,
                                      IN unsigned long ulPlainDataLen,
                                      OUT unsigned char * pCipherData,
                                      OUT unsigned long * pulCipherDataLen);
    
    
    /*
     函数功能： 外部ECC私钥解密（SM2）
     参数说明： hEkey:   			操作句柄
     pPriKey			外部输入的ECC私钥
     ulPriKeyLen		ECC私钥长度
     pCipherData 			密文
     ulCipherDataLen    	密文长度
     pPlainData				明文
     pulPlainDataLen	明文长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     说明：密文格式参考加密函数中说明
     */
    unsigned long SDKEY_ExtECCDecrypt(IN HANDLE hEkey,
                                      IN unsigned char *pPriKey,
                                      IN unsigned long ulPriKeyLen,
                                      IN unsigned char *pCipherData,
                                      IN unsigned long ulCipherDataLen,
                                      OUT unsigned char *pPlainData,
                                      OUT unsigned long *pulPlainDataLen);
    
    /*
     函数功能： 产生ECC密钥（SM2）
     参数说明： hKey:   操作句柄
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     ulModulusLen  ECC密钥模长，目前SM2为256bit
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     备注：一个密钥容器下支持双密钥，即签名和加密(交换)2种。
     */
    unsigned long SDKEY_GenECCKeyPair(IN HANDLE hKey,
                                      IN unsigned long ulAlias,
                                      IN unsigned long ulKeyUse,
                                      IN unsigned long ulModulusLen);
    
    /*
     函数功能： 产生卡外ECC密钥（SM2）
     参数说明： hKey:   操作句柄
     ulModulusLen  ECC密钥模长，目前SM2为256bit
     pPubKey ECC公钥
     pulPubKeyLen ECC公钥长度
     pPriKey ECC私钥
     pulPriKeyLen ECC私钥长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_GenExtECCKeyPair(IN HANDLE hKey,
                                         IN unsigned long ulModulusLen,
                                         OUT unsigned char *pPubKey,
                                         OUT unsigned long *pulPubKeyLen,
                                         OUT unsigned char *pPriKey,
                                         OUT unsigned long *pulPriKeyLen);
    
    /*
     函数功能： 获取公钥信息
     参数说明： hKey:   操作句柄  
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     pPubKey: ECC公钥
     pulPubKeyLen：公钥长度
     
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     备注：一个密钥容器下支持双密钥，即签名和加密(交换)2种。
     */ 
    unsigned long SDKEY_GetECCPublicKey(IN HANDLE hKey,
                                        IN unsigned long ulAlias,
                                        IN unsigned long ulKeyUse,
                                        OUT unsigned char *pPubKey,
                                        OUT unsigned long *pulPubKeyLen);
    
    /*
     函数功能：	Hash处理初始化
     参数说明：	hKey		操作句柄
     ulAlgo	Hash算法表示，目前支持SGD_SM3和SGD_SHA1
     hHash	Hash处理句柄
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_HashInit(IN HANDLE hKey,
                                 IN unsigned long ulAlgo,
                                 OUT HANDLE *hHash);
    
    /*
     函数功能：	Hash带ID的处理初始化
     参数说明：	hKey		操作句柄
     ulAlgo	Hash算法表示，带ID的初始化仅支持SGD_SM3
     pECCPubKey ECC公钥
     ulECCPubKeyLen ECC公钥长度
     pID ID数据
     ulIDLen ID的数据长度
     hHash	Hash处理句柄
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_HashInit_ID(IN HANDLE hKey,
                                    IN unsigned long ulAlgo,
                                    IN unsigned char *pECCPubKey,
                                    IN unsigned long ulECCPubKeyLen,
                                    IN unsigned char *pID,
                                    IN unsigned long ulIDLen,
                                    OUT HANDLE *hHash);
    
    /*
     函数功能：	Hash处理中间段函数
     参数说明：	hKey		操作句柄
     hHash	Hash处理句柄
     pInData 待处理数据
     ulInDataLen 待处理数据长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_HashUpdate(IN HANDLE hKey,
                                   IN HANDLE hHash,
                                   IN unsigned char *pInData,
                                   IN unsigned long ulInDataLen);
    
    /*
     函数功能：	Hash处理结束函数
     参数说明：	hKey		操作句柄
     hHash	Hash处理句柄
     pHashData Hash结果
     ulHashDataLen Hash结果长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_HashFinal(IN HANDLE hKey,
                                  IN HANDLE hHash,
                                  OUT unsigned char *pHashData,
                                  OUT unsigned long *pulHashDataLen);
    
    /*
     函数功能：	Hash处理函数
     参数说明：	hKey		操作句柄
     ulAlgo	Hash算法表示，目前支持SGD_SM3和SGD_SHA1
     pInData 待处理数据
     ulInDataLen 待处理数据长度
     pHashData Hash结果
     ulHashDataLen Hash结果长度
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_Hash(IN HANDLE hKey,
                             IN unsigned long ulAlgo,
                             IN unsigned char *pInData,
                             IN unsigned long ulInDataLen,
                             OUT unsigned char *pHashData,
                             OUT unsigned long *pulHashDataLen);
    
    //ver2.2.0.1 add import ecc key pair function
    /*
     函数功能： 导入ECC密钥对（SM2）
     参数说明： hKey:操作句柄  
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     pPubKey 	ECC公钥
     ulPubKeyLen ECC公钥长度
     pPriKey ECC私钥
     ulPriKeyLen 私钥长度          
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_ImportEccKeyPair(IN HANDLE hKey,
                                         IN unsigned long ulAlias,
                                         IN unsigned long ulKeyUse,
                                         IN unsigned char *pPubKey,
                                         IN unsigned long ulPubKeyLen,
                                         IN unsigned char *pPriKey,
                                         IN unsigned long ulPriKeyLen);
    
    
    
    //ver2.2.0.3以后  增加SM2密钥协商功能
    
    /*
     函数功能： 发起方第一步调用,生成密钥协商参数并输出。
     使用ECC密钥协商算法，为计算会话密钥而产生协商参数，返回临时ECC密钥对的公钥及协商句柄。
     参数说明：  hKey:操作句柄  
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT			
     pTempPubKey： 发起方临时公钥
     pulTempPubKeyLen： 发起方临时公钥长度   
     phAgreementHandle： 返回的密钥协商句柄
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_GenAgreementDataWithECC(IN HANDLE hKey,
                                                IN unsigned long ulAlias,
                                                IN unsigned long ulKeyUse,											
                                                OUT unsigned char *pTempPubKey,
                                                OUT unsigned long *pulTempPubKeyLen,
                                                OUT HANDLE *phAgreementHandle);
    /*
     函数功能：  响应方调用,产生协商数据并计算会话密钥。
     使用ECC密钥协商算法，产生协商参数并计算会话密钥，输出临时ECC密钥对公钥，并返回协商出来的密钥。
     参数说明：  hKey:操作句柄  
     ulAlias: 密钥容器号，从0 到 (MAX_KEY_NUMBER-1)
     ulKeyUse：密钥用途，TYPE_SIGN 或TYPE_ENCRYPT
     pSponsorPubKey：       发起方ECC公钥
     ulSponsorPubKeyLen：   发起方ECC公钥长度
     pSponsorTempPubKey：   发起方临时公钥
     ulSponsorTempPubKeyLen：发起方临时公钥长度
     ulAgreementKeyLen：   协商的会话密钥长度，不大于64字节
     pID：         响应方ID，不大于32字节
     ulIDLen：     响应方ID长度
     pSponsorID：  发起方ID，不大于32字节
     ulSponsorIDLen：发起方ID长度
     pTempPubKey：    响应方临时公钥
     pulTempPubKeyLen 响应方临时公钥长度 
     pAgreementKey：  协商出来的会话密钥
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_GenAgreementDataAndKeyWithECC(IN HANDLE hKey,
                                                      IN unsigned long ulAlias,
                                                      IN unsigned long ulKeyUse,
                                                      IN unsigned char *pSponsorPubKey,       
                                                      IN unsigned long ulSponsorPubKeyLen,
                                                      IN unsigned char *pSponsorTempPubKey,
                                                      IN unsigned long ulSponsorTempPubKeyLen,
                                                      IN unsigned long ulAgreementKeyLen,           
                                                      IN unsigned char *pID,
                                                      IN unsigned long  ulIDLen,
                                                      IN unsigned char *pSponsorID,
                                                      IN unsigned long  ulSponsorIDLen,
                                                      OUT unsigned char *pTempPubKey,
                                                      OUT unsigned long *pulTempPubKeyLen,
                                                      OUT unsigned char *pAgreementKey);
    
    /*
     函数功能：发起方第二步调用,计算会话密钥。
     使用ECC密钥协商算法，使用自身协商句柄和响应方的协商参数计算会话密钥，同时返回会话密钥。
     参数说明：  hKey:操作句柄  
     hAgreementHandle：密钥协商句柄
     pResponsePubKey： 响应方ECC公钥
     ulResponsePubKeyLen：响应方ECC公钥长度
     pResponseTempPubKey： 响应方临时ECC公钥
     ulResponseTempPubKeyLen：响应方临时ECC公钥长度
     ulAgreementKeyLen：要协商的会话密钥长度，不大于64字节
     pID： 	         发起方ID，不大于32字节
     ulIDLen：        发起方ID长度
     pResponseID：    响应方ID，不大于32字节
     ulResponseIDLen：响应方ID长度
     pAgreementKey：  协商出来的会话密钥
     返回值： SDKEY_SUCCESS:成功，其他见错误代码
     */
    unsigned long SDKEY_GenKeyWithECC(IN HANDLE hKey,
                                      IN HANDLE hAgreementHandle,
                                      IN unsigned char *pResponsePubKey,
                                      IN unsigned long ulResponsePubKeyLen,
                                      IN unsigned char *pResponseTempPubKey,
                                      IN unsigned long ulResponseTempPubKeyLen,
                                      IN unsigned long ulAgreementKeyLen,
                                      IN unsigned char *pID, 
                                      IN unsigned long ulIDLen,
                                      IN unsigned char *pResponseID,
                                      IN unsigned long ulResponseIDLen,
                                      OUT unsigned char *pAgreementKey);
    
#ifdef __cplusplus
}
#endif

#endif	 
