#pragma once

#ifndef RAND_CHECK_H_INCLUDE
#define RAND_CHECK_H_INCLUDE

//#include <ripple/beast/utility/Journal.h>
#include <peersafe/gmencrypt/hardencrypt/HardEncrypt.h>

extern "C" {
#include <peersafe/gmencrypt/randomcheck/matrix.h>
#include <peersafe/gmencrypt/randomcheck/log.h>

extern OneBit**   create_matrix(int, int);
extern void       def_matrix(OneBit*, int, int, OneBit**, int);
extern void       delete_matrix(int, OneBit**);
}

//#ifdef __cplusplus
//extern "C" {
//#endif

//显著性水平，用α表示
extern double g_dSigLevel;

//error code
#define SWR_OK					0x00000000
#define SWR_INVALID_PARAMS		0xF0000001
#define SWR_OPEN_FILE_ERROR		0xF0000002
#define SWR_HOST_MEMORY			0xF0000003
#define SWR_TEST_FAILED			0xF0000004

#define INDEX_MONOBIT_FREQUENCY 0
#define INDEX_BLOCK_FREQUENCY 1
#define INDEX_POKER 2
#define INDEX_SERIAL 3
#define INDEX_RUNS 4
#define INDEX_RUNS_DISTRIBUTION 5
#define INDEX_LONGEST_RUN_OF_ONES 6
#define INDEX_BINARY_DERIVATIVE 7
#define INDEX_AUTO_CORRELATION 8
#define INDEX_BINARY_MATRIX_RANK 9
#define INDEX_CUMULATIVE 10
#define INDEX_APPROXIMATE_ENTROPY 11
#define INDEX_LINEAR_COMPLEXITY 12
#define INDEX_MAURER 13
#define INDEX_DISCRETE_FOURIER_TRANSFORM 14

////ONE bit
//typedef struct {
//	unsigned char b : 1; //defines ONE bit
//}OneBit;

//二元序列
typedef struct {
	OneBit* bits; // Pointer to OneBit sequence
	int bitsNumber; // Number of bits in the 'bits' sequence
}BinarySequence;

class RandCheck
{
public:
	static RandCheck* getInstance();
	//void setLogJournal(beast::Journal* journal);

	int RandTest(HardEncrypt* hEObj, int randomTestSetCnt, int randomLen, bool isCycleCheck = false);
	int RandomnessTest(unsigned char* pbBuffer, int nBufferLen, int index);
	int RandomnessSingleCheck(unsigned char *pbBuffer, int nBufferLen);

private:
	static RandCheck* rcInstance;
	bool isCycleCheck_;
	//beast::Journal*	journal_;

private:
	RandCheck();
	double normal(double x);
	//单比特频数检测
	int MonobitFrequency(BinarySequence* pbsTestStream);
	//块内频数检测
	int BlockFrequency(BinarySequence* pbsTestStream, int m);
	//扑克检测
	int Poker(BinarySequence* pbsTestStream, int m);
	//重叠子序列检测
	int Serial(BinarySequence* pbsTestStream, int m);
	//游程总数检测
	int Runs(BinarySequence* pbsTestStream);
	//游程分布检测
	int RunsDistribution(BinarySequence* pbsTestStream);
	//块内最大“1”游程检测
	int LongestRunOfOnes(BinarySequence* pbsTestStream, int m);
	//二元推导检测
	int BinaryDerivative(BinarySequence* pbsTestStream, int k);
	//自相关检测
	int Autocorrelation(BinarySequence* pbsTestStream, int d);
	//矩阵秩检测
	int BinaryMatrixRank(BinarySequence* pbsTestStream, int m, int q);
	//累加和检测
	int Cumulative(BinarySequence* pbsTestStream);
	//近似熵检测
	int ApproximateEntropy(BinarySequence* pbsTestStream, int m);
	//线性复杂度检测
	int LinearComplexity(BinarySequence* pbsTestStream, int m);
	//Maurer通用统计检测
	int Maurer(BinarySequence* pbsTestStream, int L, int Q);
	//离散傅立叶检测
	int DiscreteFourierTransform(BinarySequence* pbsTestStream, int d);
};

//#ifdef __cplusplus  
//}
//#endif
#endif //#ifndef RAND_CHECK_H_INCLUDE
