#pragma once

#ifndef GM_CHECK_H_INCLUDE
#define GM_CHECK_H_INCLUDE
#include <peersafe/gmencrypt/GmEncryptObj.h>
#include <peersafe/gmencrypt/GmEncrypt.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/basics/Log.h>
#include <ripple/beast/utility/Journal.h>
//#include <gmencrypt/randomcheck/randTest.h>

//5s per ledger,6h will create 4320 ledgers
const unsigned long randomCycheckLedgerNum = 4320;
const int MAX_LEN_4_GMSTD = 232;

#ifdef GM_ALG_PROCESS

class GMCheck {

public:
	static GMCheck* getInstance();
    static std::unique_ptr <ripple::Logs> logs;
private:
    static GMCheck* gmInstance;

public:
	bool sm2EncryptAndDecryptCheck(unsigned long plainDataLen=0);
	bool sm2SignedAndVerifyCheck();
	bool sm3HashCheck();
	bool sm4EncryptAndDecryptCheck(unsigned long plainDataLen=0);
	bool randomCheck(unsigned long dataLen=0, unsigned long cycleTimes=0);
	void tryRandomCycleCheck(unsigned long ledgerSeq);
	bool randomCycleCheck(unsigned long dataLen = 0, unsigned long cycleTimes = 0);
	bool randomSingleCheck(unsigned long dataLen);
	bool startAlgRanCheck(int checkType);

	bool generateRandom2File();
	bool handleGenerateRandom2File();
	int getDataSM4EncDec_CBC_Enc(int dataSetCnt, unsigned int plainLen);
	int getDataSM4EncDec_CBC_Dec(int dataSetCnt, unsigned int plainLen);
	int getDataSM4EncDec_ECB_Enc(int dataSetCnt, unsigned int plainLen);
	int getDataSM4EncDec_ECB_Dec(int dataSetCnt, unsigned int plainLen);
	int getDataSM2EncDec_Enc(int dataSetCnt, unsigned int plainLen);
	int getDataSM2EncDec_Dec(int dataSetCnt, unsigned int plainLen);
	int getDataSM2Sign(int dataSetCnt, unsigned int plainLen);
	int getDataSM2Verify(int dataSetCnt, unsigned int plainLen);
	int getDataSM2KeyPair(int dataSetCnt);
	int getDataSM3(int dataSetCnt, unsigned int plainLen);
	int getDataSMALL(int dataSetCnt, unsigned int plainLen);
	std::pair<bool, std::string> getAlgTypeData(int algType, int dataSetCnt, unsigned int plainDataLen);
	
public:
	enum rpcAlgType
	{
		SM2KEY,
		SM2ENC,
		SM2DEC,
		SM2SIG,
		SM2VER,
		SM3DAT,
		SM4CBCE,
		SM4CBCD,
		SM4ECBE,
		SM4ECBD,
		SM_ALL_DAT
	};
	enum algCheckType
	{
		SM2ED_CK,
		SM2SV_CK,
		SM3_CK,
		SM4_CK,
		RAN_CK,
		SM_ALL_CK
	};

private:
	GMCheck(beast::Journal gmCheckJournal);
	void cipherConstruct(ripple::Blob &cipher);
	void cipher2GMStand(unsigned char* cardCipher, unsigned char* gmStdCipher, unsigned int plainDataLen);
	int dataFolderCheck(std::string foldername);
	int FileWrite(char *filename, const char *mode, unsigned char *buffer, size_t size);
	int Data_Bin2Txt(unsigned char *binData, int binDataLen, char *txtData, int *txtDataLen);
	int PrintData(const char *itemName, unsigned char *sourceData, unsigned int dataLength, unsigned int rowCount);

	static void* randomCycheckThreadFun(void *arg);
	static void* randomGenerateThreadFun(void *arg);
	
private:
	GmEncrypt* hEObj;
	beast::Journal gmCheckJournal_;
	bool isRandomCycleCheckThread;
	bool isRandomGenerateThread;
	unsigned int parentTid;
};

#endif
#endif //GM_CHECK_H_INCLUDE
