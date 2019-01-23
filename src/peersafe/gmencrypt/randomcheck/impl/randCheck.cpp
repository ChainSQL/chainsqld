#include <peersafe/gmencrypt/randomcheck/randCheck.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

//#include <gmencrypt/randomcheck/matrix.h>
#include <peersafe/gmencrypt/randomcheck/swsds.h>
#include <ripple/basics/StringUtilities.h>

extern "C" {
//#include <gmencrypt/randomcheck/impl/cephes.c>
#include <peersafe/gmencrypt/randomcheck/cephes-protos.h>
#include <peersafe/gmencrypt/randomcheck/impl/dfft.c>

double igamc(double, double);
// void  __ogg_fdrffti(int, double*, double*);
// void  __ogg_fdrfftf(int, double*, double*, double*);
}

//#define RAN_SUCC_PRINTF
#ifdef RAN_SUCC_PRINTF
#define RanSuccPrint(fmt,...) printf(fmt"\n",##__VA_ARGS__)
#else
#define RanSuccPrint(fmt,...)
#endif // DEBUGLC_PRINTF

double g_dSigLevel = 0.01; //显著性水平，用α表示

//for test
int G_TotalSampleSize = 0;
int G_TotalErrno[15] = { 0 };
int G_errno[15] = { 0 };

char G_error_str[][50] = { "MonobitFrequency", "BlockFrequency", "Poker", \
"Serial", "Runs", "RunsDistribution", \
"LongestRunOfOnes", "BinaryDerivative", "AutoCorrelation", \
"BinaryMatrixRank", "Cumulative", "ApproximateEntropy", \
"LinearComplexity", "Maurer", "DiscreteFourierTransform",\
};
RandCheck* RandCheck::rcInstance = nullptr;
RandCheck * RandCheck::getInstance()
{
	if (rcInstance == nullptr)
	{
		rcInstance = new RandCheck();
	}
	return rcInstance;
}

//void RandCheck::setLogJournal(beast::Journal * journal)
//{
//	if (journal != nullptr && journal_ == nullptr)
//	{
//		journal_ = journal;
//	}
//}

int RandCheck::RandTest(HardEncrypt * hEObj, int randomTestSetCnt, int randomLen, bool isCycleCheck)
{
	int i = 0;
	int rv = 0;
	int iCount = 0; //ͨ通过检测的数量
	int iLevel = 0; //ͨ通过检测的标准数量
	bool checkResult = true;
	int iSampleSize = randomTestSetCnt; //样本数量为1000
	int iSampleLen = randomLen; //样本长度131072 = 10^6 bit ≈ 1M，128K bytes
	isCycleCheck_ = isCycleCheck;
	SGD_UCHAR *pbSampleBuffer = NULL;

	/*JLOG(journal_->info()) << "RandTest starting...";
	JLOG(journal_->info()) << ripple::stdStringFormat("RandTest: iSampleLength : [%d]", iSampleLen);
	JLOG(journal_->info()) << ripple::stdStringFormat("RandTest: iSampleSize : [%d]", iSampleSize);*/

	LOGP(LOG_INFO, 0, "RandTest starting...");
	LOGPX(LOG_INFO, 0, "RandTest: iSampleLength : [%d]", iSampleLen);
	LOGPX(LOG_INFO, 0, "RandTest: iSampleSize : [%d]", iSampleSize);

	iLevel = (int)(iSampleSize*(1 - g_dSigLevel - 3 * sqrt(g_dSigLevel*(1 - g_dSigLevel) / (double)iSampleSize)));

	if (iSampleSize >= 10)
	{
		iLevel -= iSampleSize / 10;
	}

	//JLOG(journal_->info()) << ripple::stdStringFormat("RandTest: iLevel : [%d]", iLevel);
	LOGPX(LOG_INFO, 0, "RandTest: iLevel : [%d]", iLevel);
	
	pbSampleBuffer = (SGD_UCHAR*)malloc(sizeof(SGD_UCHAR) * iSampleLen);
	if (pbSampleBuffer == NULL)
	{
		//JLOG(journal_->error()) << "RandTest: malloc sample buffer error";
		LOGP(LOG_ERROR, SWR_HOST_MEMORY, "RandTest: malloc sample buffer error");
		return SWR_HOST_MEMORY;
	}

	for (i = 0; i < iSampleSize; ++i)
	{
		//JLOG(journal_->info()) << ripple::stdStringFormat("RandTest: round:[%d] start......", i + 1);
		LOGPX(LOG_INFO, 0, "RandTest: round:[%d] start......", i + 1);
		//1 采样
		rv = hEObj->GenerateRandom(randomLen, pbSampleBuffer);
		//rv = GetSample(i+1, pbSampleBuffer, iSampleLen);
		if (rv != SDR_OK)
		{
			//free sample buffer
			if (pbSampleBuffer != NULL)
			{
				free(pbSampleBuffer);
				pbSampleBuffer = NULL;
			}
			//JLOG(journal_->error()) << "RandTest: GetSample error";
			LOGP(LOG_ERROR, rv, "RandTest: GetSample error");
			return rv;
		}

		//2 存储
		//rv = SaveSample(i, pbSampleBuffer, iSampleLen);

		//3 检测
		rv = RandomnessTest(pbSampleBuffer, iSampleLen, i + 1);
		if (rv == SWR_HOST_MEMORY)
		{
			//free sample buffer
			if (pbSampleBuffer != NULL)
			{
				free(pbSampleBuffer);
				pbSampleBuffer = NULL;
			}

			//JLOG(journal_->error()) << "RandTest: memory error";
			LOGP(LOG_ERROR, SWR_HOST_MEMORY, "RandTest: memory error");
			return SWR_HOST_MEMORY;
		}

		//JLOG(journal_->info()) << ripple::stdStringFormat("RandTest: round:[%d] ......end", i + 1);
		LOGPX(LOG_INFO, 0, "RandTest: round:[%d] ......end", i + 1);
	}


	//4 判定
	for (i = 0; i < 15; ++i)
	{
		if (G_errno[i] >= (iSampleSize - iLevel))
		{
			//未通过该项检测
			/*JLOG(journal_->error()) << ripple::stdStringFormat("RandTest: iSampleSize[%d], iLevel[%d]", iSampleSize, iLevel);
			JLOG(journal_->error()) << ripple::stdStringFormat("RandTest: G_errno[%d]:[%d] >= %d", i, G_errno[i], (iSampleSize - iLevel));
			JLOG(journal_->error()) << ripple::stdStringFormat("RandTest: failed %s test", G_error_str[i]);
			JLOG(journal_->error()) << ripple::stdStringFormat("RandTest: failed!!!!!!, do something");*/
			LOGPX(LOG_ERROR, 0, "RandTest: iSampleSize[%d], iLevel[%d]", iSampleSize, iLevel);
			LOGPX(LOG_ERROR, 0, "RandTest: G_errno[%d]:[%d] >= %d", i, G_errno[i], (iSampleSize - iLevel));
			LOGPX(LOG_ERROR, 0, "RandTest: failed %s test", G_error_str[i]);
			LOGP(LOG_ERROR, 0, "RandTest: failed!!!!!!, do something");

			rv = SWR_TEST_FAILED;
			checkResult = false;
		}
	}

	//5 记录测试信息
	{
		//JLOG(journal_->info()) << "RandTest result:";
		LOGP(LOG_INFO, 0, "RandTest result:");

		//记录总测试次数
		G_TotalSampleSize += iSampleSize;
		//JLOG(journal_->info()) << ripple::stdStringFormat("iSampleSize:[%d], total:[%d]", iSampleSize, G_TotalSampleSize);
		LOGPX(LOG_INFO, 0, "iSampleSize:[%d], total:[%d]", iSampleSize, G_TotalSampleSize);

		//JLOG(journal_->warn()) << "error info:";
		LOGP(LOG_WARNING, 0, "error info:");
		for (i = 0; i < 15; ++i)
		{
			G_TotalErrno[i] += G_errno[i];
			//JLOG(journal_->warn()) << ripple::stdStringFormat("%s error num:[%d], totalErrno:[%d]", G_error_str[i], G_errno[i], G_TotalErrno[i]);
			LOGPX(LOG_WARNING, 0, "%s error count:[%d], totalErrCnt:[%d]", G_error_str[i], G_errno[i], G_TotalErrno[i]);
		}
	}

	//clear error info
	for (i = 0; i < 15; ++i)
	{
		G_errno[i] = 0;
	}

	//free sample buffer
	if (pbSampleBuffer != NULL)
	{
		free(pbSampleBuffer);
	}

	//JLOG(journal_->info()) << "RandTest ...complete";
	LOGP(LOG_INFO, 0, "RandTest complete!");
	//return rv;
	return checkResult;
}

int RandCheck::RandomnessTest(unsigned char * pbBuffer, int nBufferLen, int index)
{
	int rv = 0;
	int i = 0;
	int j = 0;
	unsigned int bit = 0;
	unsigned int mask = 1 << 7;
	unsigned int n0 = 0, n1 = 0; //记录转换过程中0和1的个数
	unsigned int nBitsRead = 0;
	OneBit* obStream = NULL;
	BinarySequence bsTestStream;

	//将随机数缓冲器转换为二元序列
	obStream = (OneBit *)calloc((nBufferLen * 8), sizeof(OneBit));
	if (obStream == NULL)
	{
		//JLOG(journal_->error()) << "RandomnessTest: calloc obStream error!";
		LOGP(LOG_ERROR, SWR_HOST_MEMORY, "RandomnessTest: calloc obStream error!");
		return SWR_HOST_MEMORY;
	}

	for (i = 0; i < nBufferLen; ++i)
	{
		unsigned char ucByte = pbBuffer[i];
		for (j = 0; j < 8; ++j)
		{
			if (ucByte & mask)
			{
				bit = 1;
				++n1;
			}
			else
			{
				bit = 0;
				++n0;
			}
			ucByte <<= 1;//左移1位
			obStream[nBitsRead].b = bit;
			++nBitsRead;
		}
	}

	bsTestStream.bits = obStream;
	bsTestStream.bitsNumber = nBitsRead;

	//for test, output bsTestStream

	/*JLOG(journal_->trace()) << ripple::stdStringFormat("RandomnessTest: bsTestStream.bitsNumber : [%d]", bsTestStream.bitsNumber);
	JLOG(journal_->trace()) << ripple::stdStringFormat("RandomnessTest: bsTestStream n0 : [%d]", n0);
	JLOG(journal_->trace()) << ripple::stdStringFormat("RandomnessTest: bsTestStream n1 : [%d]", n1);*/
	LOGPX(LOG_TRACE, 0, "RandomnessTest: bsTestStream.bitsNumber : [%d]", bsTestStream.bitsNumber);
	LOGPX(LOG_TRACE, 0, "RandomnessTest: bsTestStream n0 : [%d]", n0);
	LOGPX(LOG_TRACE, 0, "RandomnessTest: bsTestStream n1 : [%d]", n1);


	/*
	{
	unsigned char* sbsTestStream = (unsigned char*)calloc(nBitsRead, sizeof(unsigned char));
	if(sbsTestStream == NULL)
	{
	LOGEX(LOG_ERROR, rv, "RandomnessTest: calloc sbsTestStream error, size : [%d]", nBitsRead);
	return -1;
	}

	for(i = 0;i < nBitsRead; ++i)
	{
	sbsTestStream[i] = bsTestStream.bits[i].b + 0x30;
	}
	LOGDATA("sbsTestStream", sbsTestStream, nBitsRead);
	}
	*/

	//单比特频数检测
	rv = MonobitFrequency(&bsTestStream);
	if (rv != SWR_OK)
	{
		++G_errno[INDEX_MONOBIT_FREQUENCY];
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: MonobitFrequencyTest error, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "RandomnessTest: MonobitFrequencyTest error, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: MonobitFrequencyTest successful!");

	//块内频数检测
	rv = BlockFrequency(&bsTestStream, 100);
	if (rv != SWR_OK)
	{
		++G_errno[INDEX_BLOCK_FREQUENCY];
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: FrequencyTestWithinABlock error, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "RandomnessTest: FrequencyTestWithinABlock error, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: FrequencyTestWithinABlock successful!");

	//扑克检测
	rv = Poker(&bsTestStream, 2); //m = 2
	if (rv != SWR_OK)
	{
		if (rv == SWR_HOST_MEMORY)
		{
			//内存错误，立即退出
			if (obStream != NULL)
			{
				free(obStream);
			}
			return SWR_HOST_MEMORY;
		}

		++G_errno[INDEX_POKER];

		LOGPX(LOG_ERROR, index, "RandomnessTest: PokerTest error, m = 4, failed number:[0x%08x]", rv);
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: PokerTest error, m = 4, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: PokerTest successful! m = 4");

	//重叠子序列检测
	rv = Serial(&bsTestStream, 5);
	if (rv != SWR_OK)
	{
		++G_errno[INDEX_SERIAL];
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: SerialTest error, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "RandomnessTest: SerialTest error, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: SerialTest successful!");

	//游程总数检测
	rv = Runs(&bsTestStream);
	if (rv != SWR_OK)
	{
		++G_errno[INDEX_RUNS];

		//LOG(LOG_ERROR,index, "---------------RandomnessTest: RunsTest error");
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: RunsTest error, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "RandomnessTest: RunsTest error, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: RunsTest successful!");

	//游程分布检测
	rv = RunsDistribution(&bsTestStream);
	if (rv != SWR_OK)
	{
		if (rv == SWR_HOST_MEMORY)
		{
			//内存错误，立即退出
			if (obStream != NULL)
			{
				free(obStream);
			}
			return SWR_HOST_MEMORY;
		}

		++G_errno[INDEX_RUNS_DISTRIBUTION];
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: RunsDistributionTest error, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "RandomnessTest: RunsDistributionTest error, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: RunsDistributionTest successful!");

	//块内最大“1”游程检测
	rv = LongestRunOfOnes(&bsTestStream, 10000); //10000
	if (rv != SWR_OK)
	{
		++G_errno[INDEX_LONGEST_RUN_OF_ONES];
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: LongestRunOfOnesTest error, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "RandomnessTest: LongestRunOfOnesTest error, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: LongestRunOfOnesTest successful!");

	//二元推导检测
	rv = BinaryDerivative(&bsTestStream, 3); //3, 7
	if (rv != SWR_OK)
	{
		if (rv == SWR_HOST_MEMORY)
		{
			//内存错误，立即退出
			if (obStream != NULL)
			{
				free(obStream);
			}
			return SWR_HOST_MEMORY;
		}

		++G_errno[INDEX_BINARY_DERIVATIVE];
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: BinaryDerivativeTest error, k=3, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "RandomnessTest: BinaryDerivativeTest error, k=3, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: BinaryDerivativeTest successful! k=3");

	rv = BinaryDerivative(&bsTestStream, 7); //3, 7
	if (rv != SWR_OK)
	{
		if (rv == SWR_HOST_MEMORY)
		{
			//内存错误，立即退出
			if (obStream != NULL)
			{
				free(obStream);
			}
			return SWR_HOST_MEMORY;
		}

		++G_errno[INDEX_BINARY_DERIVATIVE];
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: BinaryDerivativeTest error, k= 7, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "RandomnessTest: BinaryDerivativeTest error, k= 7, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: PokerTest successful! k= 7");

	//自相关检测
	rv = Autocorrelation(&bsTestStream, 1); //1, 2, 8, 16
	if (rv != SWR_OK)
	{
		++G_errno[INDEX_AUTO_CORRELATION];
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: AutocorrelationTest error, d = 1, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "RandomnessTest: AutocorrelationTest error, d = 1, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: AutocorrelationTest successful! d = 1");

	rv = Autocorrelation(&bsTestStream, 2); //1, 2, 8, 16
	if (rv != SWR_OK)
	{
		++G_errno[INDEX_AUTO_CORRELATION];
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: AutocorrelationTest error, d = 2, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "RandomnessTest: AutocorrelationTest error, d = 2, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: AutocorrelationTest successful! d = 2");

	rv = Autocorrelation(&bsTestStream, 8); //1, 2, 8, 16
	if (rv != SWR_OK)
	{
		++G_errno[INDEX_AUTO_CORRELATION];
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: AutocorrelationTest error, d = 8, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "RandomnessTest: AutocorrelationTest error, d = 8, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: AutocorrelationTest successful! d = 8");

	rv = Autocorrelation(&bsTestStream, 16); //1, 2, 8, 16
	if (rv != SWR_OK)
	{
		++G_errno[INDEX_AUTO_CORRELATION];
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: AutocorrelationTest error, d = 16, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "RandomnessTest: AutocorrelationTest error, d = 16, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: AutocorrelationTest successful! d = 16");

	//矩阵秩检测
	rv = BinaryMatrixRank(&bsTestStream, 32, 32); //M = Q = 32
	if (rv != SWR_OK)
	{
		if (rv == SWR_HOST_MEMORY)
		{
			//内存错误，立即退出
			if (obStream != NULL)
			{
				free(obStream);
			}
			return SWR_HOST_MEMORY;
		}

		++G_errno[INDEX_BINARY_MATRIX_RANK];
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: BinaryMatrixRankTest error, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "RandomnessTest: BinaryMatrixRankTest error, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: BinaryMatrixRankTest successful!");

	//累加和检测
	rv = Cumulative(&bsTestStream);
	if (rv != SWR_OK)
	{
		++G_errno[INDEX_CUMULATIVE];
		//JLOG(journal_->error()) << ripple::stdStringFormat("--------RandomnessTest: CumulativeTest error, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "--------RandomnessTest: CumulativeTest error, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: BinaryMatrixRankTest successful!");

	//近似熵检测
	rv = ApproximateEntropy(&bsTestStream, 2); //m = 2, 5
	if (rv != SWR_OK)
	{
		if (rv == SWR_HOST_MEMORY)
		{
			//内存错误，立即退出
			if (obStream != NULL)
			{
				free(obStream);
			}
			return SWR_HOST_MEMORY;
		}

		++G_errno[INDEX_APPROXIMATE_ENTROPY];
		//JLOG(journal_->error()) << ripple::stdStringFormat("--------RandomnessTest: ApproximateEntropyTest error, m = 2, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "--------RandomnessTest: ApproximateEntropyTest error, m = 2, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: ApproximateEntropyTest successful! m = 2");

	rv = ApproximateEntropy(&bsTestStream, 5); //m = 2, 5
	if (rv != SWR_OK)
	{
		if (rv == SWR_HOST_MEMORY)
		{
			//内存错误，立即退出
			if (obStream != NULL)
			{
				free(obStream);
			}
			return SWR_HOST_MEMORY;
		}

		++G_errno[INDEX_APPROXIMATE_ENTROPY];
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: ApproximateEntropyTest error, m = 5, failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, index, "RandomnessTest: ApproximateEntropyTest error, m = 5, failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: ApproximateEntropyTest successful! m = 5");

	if (!isCycleCheck_)
	{
#if 1
		//线性复杂度检测
		rv = LinearComplexity(&bsTestStream, 500); //m = 500
		if (rv != SWR_OK)
		{
			if (rv == SWR_HOST_MEMORY)
			{
				//内存错误，立即退出
				if (obStream != NULL)
				{
					free(obStream);
				}
				return SWR_HOST_MEMORY;
			}

			++G_errno[INDEX_LINEAR_COMPLEXITY];
			//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: LinearComplexityTest error, failed number:[0x%08x]", rv);
			LOGPX(LOG_ERROR, index, "RandomnessTest: LinearComplexityTest error, failed number:[0x%08x]", rv);
			//goto RandTest_end;
			return rv;
		}
		else LOGP(LOG_TRACE, 0, "RandomnessTest: LinearComplexityTest successful!");
#endif

#if 1
		//Maurer通用统计检测
		rv = Maurer(&bsTestStream, 7, 1280); //L = 7, Q = 1280
		if (rv != SWR_OK)
		{
			if (rv == SWR_HOST_MEMORY)
			{
				//内存错误，立即退出
				if (obStream != NULL)
				{
					free(obStream);
				}
				return SWR_HOST_MEMORY;
			}

			++G_errno[INDEX_MAURER];
			//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: MaurerTest error, failed number:[0x%08x]", rv);
			LOGPX(LOG_ERROR, index, "RandomnessTest: MaurerTest error, failed number:[0x%08x]", rv);
			//goto RandTest_end;
			return rv;
		}
		else LOGP(LOG_TRACE, 0, "RandomnessTest: MaurerTest successful!");
#endif

#if 1
		//离散傅立叶检测
		rv = DiscreteFourierTransform(&bsTestStream, 8);
		if (rv != SWR_OK)
		{
			if (rv == SWR_HOST_MEMORY)
			{
				//内存错误，立即退出
				if (obStream != NULL)
				{
					free(obStream);
				}
				return SWR_HOST_MEMORY;
			}

			++G_errno[INDEX_DISCRETE_FOURIER_TRANSFORM];
			//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: DiscreteFourierTransformTest error, failed number:[0x%08x]", rv);
			LOGPX(LOG_ERROR, index, "RandomnessTest: DiscreteFourierTransformTest error, failed number:[0x%08x]", rv);
			//goto RandTest_end;
			return rv;
		}
		else LOGP(LOG_TRACE, 0, "RandomnessTest: DiscreteFourierTransformTest successful!");
#endif
	}

	//RandTest_end:

	//free obStream
	if (obStream != NULL)
	{
		free(obStream);
	}

	return SWR_OK;
}

int RandCheck::RandomnessSingleCheck(unsigned char * pbBuffer, int nBufferLen)
{
	int rv = 0;
	int i = 0;
	int j = 0;
	unsigned int bit = 0;
	unsigned int mask = 1 << 7;
	unsigned int n0 = 0, n1 = 0; //记录转换过程中0和1的个数
	unsigned int nBitsRead = 0;
	OneBit* obStream = NULL;
	BinarySequence bsTestStream;

	//将随机数缓冲器转换为二元序列
	obStream = (OneBit *)calloc((nBufferLen * 8), sizeof(OneBit));
	if (obStream == NULL)
	{
		//JLOG(journal_->error()) << "RandomnessTest: calloc obStream error!";
		LOGP(LOG_ERROR, SWR_HOST_MEMORY, "RandomnessTest: calloc obStream error!");
		return SWR_HOST_MEMORY;
	}

	for (i = 0; i < nBufferLen; ++i)
	{
		unsigned char ucByte = pbBuffer[i];
		for (j = 0; j < 8; ++j)
		{
			if (ucByte & mask)
			{
				bit = 1;
				++n1;
			}
			else
			{
				bit = 0;
				++n0;
			}
			ucByte <<= 1;//左移1位
			obStream[nBitsRead].b = bit;
			++nBitsRead;
		}
	}

	bsTestStream.bits = obStream;
	bsTestStream.bitsNumber = nBitsRead;
	//扑克检测
	rv = Poker(&bsTestStream, 2); //m = 2
	if (rv != SWR_OK)
	{
		if (rv == SWR_HOST_MEMORY)
		{
			//内存错误，立即退出
			if (obStream != NULL)
			{
				free(obStream);
			}
			return SWR_HOST_MEMORY;
		}

		++G_errno[INDEX_POKER];
		//JLOG(journal_->error()) << ripple::stdStringFormat("RandomnessTest: PokerTest error! failed number:[0x%08x]", rv);
		LOGPX(LOG_ERROR, 0, "RandomnessTest: PokerTest error! failed number:[0x%08x]", rv);
		//goto RandTest_end;
		return rv;
	}
	else LOGP(LOG_TRACE, 0, "RandomnessTest: PokerTest successful!");
	if (obStream != NULL)
	{
		free(obStream);
	}

	return SWR_OK;
}

RandCheck::RandCheck(): isCycleCheck_(false)
{
}

double RandCheck::normal(double x)
{
	double arg, result, sqrt2 = 1.414213562373095048801688724209698078569672;

	if (x > 0) {
		arg = x / sqrt2;
		result = 0.5 * (1 + erf(arg));
	}
	else {
		arg = -x / sqrt2;
		result = 0.5 * (1 - erf(arg));
	}
	return(result);
}

//单比特频数检测
//1,Conversion to +-1. The zeros and ones of the input sequence are converted to value of -1 and +1 and
//		are added together to produce Sn = X1 + X2 + ...+ Xn, where Xi = 2Ei - 1
//2,Compute the test statistic
//3,Compute P-value
int RandCheck::MonobitFrequency(BinarySequence* pbsTestStream)
{
	int i = 0;
	double sum = 0.0;
	double v = 0.0; //统计量V
	double dPValue = 0.0;
	double dSqrt2 = 1.41421356237309504880;

	if (pbsTestStream == NULL)
	{
		//JLOG(journal_->error()) << "MonobitFrequency pbsTestStream is null";
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "MonobitFrequency pbsTestStream is null");
		return SWR_INVALID_PARAMS;
	}

	for (i = 0; i < pbsTestStream->bitsNumber; ++i)
	{
		sum += pbsTestStream->bits[i].b ? (1) : (-1);
	}

	LOGPX(LOG_TRACE, 0, "MonobitFrequency: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("MonobitFrequency: sum : [%f]", sum);

	sum = fabs(sum);

	LOGPX(LOG_TRACE, 0, "MonobitFrequency: after fabs, sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("MonobitFrequency: after fabs, sum : [%f]", sum);

	v = sum / (sqrt(pbsTestStream->bitsNumber));

	LOGPX(LOG_TRACE, 0, "MonobitFrequency: v : [%f]", v);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("MonobitFrequency: v : [%f]", v);

	dPValue = erfc(v / dSqrt2);

	LOGPX(LOG_TRACE, 0, "MonobitFrequency: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("MonobitFrequency: dPValue : [%f]", dPValue);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "MonobitFrequency: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed MonobitFrequency~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("MonobitFrequency: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed MonobitFrequency~~~";

		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "MonobitFrequency: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed MonobitFrequency!!!");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("MonobitFrequency: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << ripple::stdStringFormat("pbsTestStream failed MonobitFrequency!!!");

		return SWR_TEST_FAILED;
	}
}
//块内频数检测
int RandCheck::BlockFrequency(BinarySequence * pbsTestStream, int m)
{
	int i = 0;
	int j = 0;
	int nBlocksNumber = 0;
	double sum = 0.0;
	double pi = 0.0; //每个子序列中1所占的比例
	double v = 0.0; //统计量V
	double dBlockSum = 0.0;
	double dPValue = 0.0;

	if (pbsTestStream == NULL)
	{
		//JLOG(journal_->error()) << "BlockFrequency pbsTestStream is null";
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "BlockFrequency pbsTestStream is null");
		return SWR_INVALID_PARAMS;
	}

	LOGPX(LOG_TRACE, 0, "BlockFrequency: m : [%d]", m);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BlockFrequency: m : [%d]", m);

	//
	nBlocksNumber = (int)floor((double)pbsTestStream->bitsNumber / (double)m);

	LOGPX(LOG_TRACE, 0, "BlockFrequency: nBlocksNumber : [%d]", nBlocksNumber);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BlockFrequency: nBlocksNumber: [%d]", nBlocksNumber);

	for (i = 0; i < nBlocksNumber; ++i)
	{
		dBlockSum = 0.0;
		for (j = 0; j < m; ++j)
		{
			dBlockSum += pbsTestStream->bits[j + i*m].b;
		}
		pi = (double)dBlockSum / (double)m;
		v = pi - 0.5;
		sum += v * v;
	}

	LOGPX(LOG_TRACE, 0, "BlockFrequency: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BlockFrequency: sum : [%f]", sum);

	v = 4.0 * m * sum;

	LOGPX(LOG_TRACE, 0, "BlockFrequency: v : [%f]", v);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BlockFrequency: v : [%f]", v);

	dPValue = igamc((double)nBlocksNumber / 2, v / 2);
	//dPValue = 1.0;

	LOGPX(LOG_TRACE, 0, "BlockFrequency: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BlockFrequency: dPValue : [%f]", dPValue);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "BlockFrequency: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed BlockFrequency~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("BlockFrequency: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed BlockFrequency~~~";

		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "BlockFrequency: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed BlockFrequency!!!");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("BlockFrequency: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream failed BlockFrequency!!!";

		return SWR_TEST_FAILED;
	}
}
//扑克检测
int RandCheck::Poker(BinarySequence * pbsTestStream, int m)
{
	int i = 0;
	int j = 0;
	int nBlocksNumber = 0;
	int nNArraySize = 0;
	long* n = NULL;
	long decRep;
	double sum = 0.0;
	double dPValue = 0.0;

	if (pbsTestStream == NULL)
	{
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "Poker pbsTestStream is null");
		//JLOG(journal_->error()) << "Poker pbsTestStream is null";
		return SWR_INVALID_PARAMS;
	}

	LOGPX(LOG_TRACE, 0, "Poker: m : [%d]", m);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Poker: m : [%d]", m);

	nBlocksNumber = (int)floor((double)pbsTestStream->bitsNumber / (double)m);

	LOGPX(LOG_TRACE, 0, "Poker: nBlocksNumber : [%d]", nBlocksNumber);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Poker: nBlocksNumber : [%d]", nBlocksNumber);

	nNArraySize = (int)pow(2, m);

	LOGPX(LOG_TRACE, 0, "Poker: nNArraySize : [%d]", nNArraySize);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Poker: nNArraySize : [%d]", nNArraySize);

	n = (long*)calloc(nNArraySize, sizeof(long));
	if (n == NULL)
	{
		LOGP(LOG_ERROR, SWR_HOST_MEMORY, "Poker calloc n error");
		//JLOG(journal_->error()) << "Poker calloc n error";
		return SWR_HOST_MEMORY;
	}

	for (i = 0; i < nNArraySize; ++i)
	{
		n[i] = 0;
	}

	for (i = 0; i < nBlocksNumber; ++i)
	{
		decRep = 0;
		for (j = 0; j < m; j++)
		{
			decRep += pbsTestStream->bits[(i - 1)*m + j].b * (long)pow(2, m - 1 - j);
		}
		++n[decRep];
	}

	for (i = 0; i < nNArraySize; ++i)
	{
		LOGPX(LOG_TRACE, 0, "Poker: n[%d] : [%u]", i, n[i]);
		LOGPX(LOG_TRACE, 0, "Poker: sum : [%f]", sum);
		//JLOG(journal_->trace()) << ripple::stdStringFormat("Poker: n[%d] : [%u]", i, n[i]);
		//JLOG(journal_->trace()) << ripple::stdStringFormat("Poker: sum : [%f]", sum);

		sum += pow(n[i], 2);
	}

	sum = nNArraySize / (double)pbsTestStream->bitsNumber * sum;
	sum = sum - (double)pbsTestStream->bitsNumber;

	LOGPX(LOG_TRACE, 0, "Poker: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Poker: sum : [%f]", sum);

	dPValue = igamc(((nNArraySize - 1) / 2), sum / 2);

	LOGPX(LOG_TRACE, 0, "Poker: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Poker: dPValue : [%f]", dPValue);

	//free
	if (n != NULL) free(n);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "Poker: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed Poker~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("Poker: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed Poker~~~";

		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "Poker: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed Poker!!!");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("Poker: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream failed Poker!!!";

		return SWR_TEST_FAILED;
	}
}
//重叠子序列检测
int RandCheck::Serial(BinarySequence* pbsTestStream, int nBlockSize)
{
	int i = 0;
	int j = 0;
	int nBlocksNumber = 0;
	double sum = 0.0;
	double pi = 0.0; //每个子序列中1所占的比例
	double v = 0.0; //统计量V
	double dBlockSum = 0.0;
	double dPValue = 0.0;

	if (pbsTestStream == NULL)
	{
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "Serial pbsTestStream is null");
		//JLOG(journal_->error()) << "Serial pbsTestStream is null";
		return SWR_INVALID_PARAMS;
	}

	LOGPX(LOG_TRACE, 0, "Serial: nBlockSize : [%d]", nBlockSize);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Serial: nBlockSize : [%d]", nBlockSize);

	//
	nBlocksNumber = (int)floor((double)pbsTestStream->bitsNumber / (double)nBlockSize);

	LOGPX(LOG_TRACE, 0, "Serial: nBlocksNumber : [%d]", nBlocksNumber);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Serial: nBlocksNumber : [%d]", nBlocksNumber);

	for (i = 0; i < nBlocksNumber; ++i)
	{
		dBlockSum = 0.0;
		for (j = 0; j < nBlockSize; ++j)
		{
			dBlockSum += pbsTestStream->bits[j + i*nBlockSize].b;
		}
		pi = (double)dBlockSum / (double)nBlockSize;
		v = pi - 0.5;
		sum += v * v;
	}

	LOGPX(LOG_TRACE, 0, "Serial: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Serial: sum : [%f]", sum);

	v = 4.0 * nBlockSize * sum;

	LOGPX(LOG_TRACE, 0, "Serial: v : [%f]", v);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Serial: v : [%f]", v);

	dPValue = igamc((double)nBlocksNumber / 2, v / 2);
	//dPValue = 1.0;

	LOGPX(LOG_TRACE, 0, "Serial: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Serial: dPValue : [%f]", dPValue);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "Serial: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed Serial~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("Serial: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed Serial~~~";
		
		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "BlockFrequencyTest: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed BlockFrequencyTest!!!");
		//JLOG(journal_->error()) << ripple::stdStringFormat("BlockFrequencyTest: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->error()) << "pbsTestStream failed BlockFrequencyTest!!!";

		return SWR_TEST_FAILED;
	}
}
//游程总数检测
int RandCheck::Runs(BinarySequence * pbsTestStream)
{
	int i = 0;
	double sum = 0.0;
	double v = 0.0; //Vn(obs)
	double pi = 0.0; //每个子序列中1所占的比例
	double dInterValue = 0.0; //中间值
	double dPValue = 0.0;

	if (pbsTestStream == NULL)
	{
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "Runs pbsTestStream is null");
		//JLOG(journal_->error()) << "Runs pbsTestStream is null";
		return SWR_INVALID_PARAMS;
	}

	for (i = 0; i < pbsTestStream->bitsNumber - 1; ++i)
	{
		//
		v += (pbsTestStream->bits[i].b == pbsTestStream->bits[i + 1].b) ? 0 : 1;
		//
		sum += pbsTestStream->bits[i].b;
	}

	++v;
	sum += pbsTestStream->bits[pbsTestStream->bitsNumber - 1].b;

	pi = sum / pbsTestStream->bitsNumber;

	LOGPX(LOG_TRACE, 0, "Runs: sum : [%f]", sum);
	LOGPX(LOG_TRACE, 0, "Runs: v : [%f]", v);
	LOGPX(LOG_TRACE, 0, "Runs: pi : [%f]", pi);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Runs: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Runs: v : [%f]", v);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Runs: pi : [%f]", pi);

	pi = pi * (1 - pi);
	dInterValue = fabs(v - 2 * pbsTestStream->bitsNumber * pi) / (2 * sqrt(2 * pbsTestStream->bitsNumber) * pi);
	dPValue = erfc(dInterValue);

	LOGPX(LOG_TRACE, 0, "Runs: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Runs: dPValue : [%f]", dPValue);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "Runs: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed Runs~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("Runs: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed Runs~~~";

		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "Runs: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed Runs!!!");
		//JLOG(journal_->error()) << ripple::stdStringFormat("Runs: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->error()) << "pbsTestStream failed Runs!!!";

		return SWR_TEST_FAILED;
	}
}
//游程分布检测
int RandCheck::RunsDistribution(BinarySequence * pbsTestStream)
{
	int i = 0;
	int j = 0;
	int num = 0;
	int last = 0; //前一位b值
	unsigned int k = 0; //ei >= 5的最大数
	unsigned int *b = NULL; //二元序列中长度为i的1游程的数目
	unsigned int *g = NULL; //二元序列中长度为i的0游程的数目
	double e = 0.0;
	double sum = 0.0;
	double dPValue = 0.0;

	if (pbsTestStream == NULL)
	{
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "RunsDistribution pbsTestStream is null");
		//JLOG(journal_->error()) << "RunsDistribution pbsTestStream is null";
		return SWR_INVALID_PARAMS;
	}

	for (i = 0; i < pbsTestStream->bitsNumber; ++i)
	{
		if (((pbsTestStream->bitsNumber - i + 3) / pow(2, i + 2)) >= 5)
		{
			k = i;
		}
	}

	LOGPX(LOG_TRACE, 0, "RunsDistribution: k : [%d]", k);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("RunsDistribution: k : [%d]", k);

	b = (unsigned int*)calloc(k, sizeof(unsigned int));
	if (b == NULL)
	{
		LOGP(LOG_ERROR, SWR_HOST_MEMORY, "RunsDistribution calloc b array error");
		//JLOG(journal_->error()) << "RunsDistribution calloc b array error";
		return SWR_HOST_MEMORY;
	}

	g = (unsigned int*)calloc(k, sizeof(unsigned int));
	if (g == NULL)
	{
		if (b != NULL) free(b);

		LOGP(LOG_ERROR, SWR_HOST_MEMORY, "RunsDistribution calloc g array error");
		//JLOG(journal_->error()) << "RunsDistribution calloc g array error";
		return SWR_HOST_MEMORY;
	}

	for (i = 1; i <= k; ++i)
	{
		b[i - 1] = 0;
		g[i - 1] = 0;
	}

	last = 0;

	for (j = 0; j < pbsTestStream->bitsNumber; ++j)
	{
		if (pbsTestStream->bits[j].b == 1)
		{
			LOGPX(LOG_TRACE, 0, "RunsDistribution: pbsTestStream->bits[%d].b:[%d]", j, 1);

			if (last != 1)
			{
				if (num >= 1 && num <= k)
				{
					++g[num - 1]; 
					LOGPX(LOG_TRACE, 0, "RunsDistribution: ++g, g:[%d], i:[%d], j:[%d]", g, i, j);
				}
				//1游程开始
				num = 1;
			}
			else
			{
				++num;
			}

			last = 1; //记录上一位值
		}
		else //pbsTestStream->bits[j].b == 0
		{
			LOGPX(LOG_TRACE, 0, "RunsDistribution: pbsTestStream->bits[%d].b:[%d]", j, 0);
			if (last != 0)
			{
				if (num >= 1 && num <= k)
				{
					++b[num - 1];
					LOGPX(LOG_TRACE, 0, "RunsDistribution: ++b, b:[%d], i:[%d], j:[%d]", b, i, j);
				}
				//0游程开始
				num = 1;
			}
			else
			{
				++num;
			}

			last = 0; //记录上一位值
		}
	}//end for(j = 0;j < pbsTestStream->bitsNumber; ++j)

	for (i = 1; i <= k; ++i)
	{
		//LOGEX(LOG_TRACE, 0, "RunsDistribution: i : [%d]", i);
		//LOGEX(LOG_TRACE, 0, "RunsDistribution: b[%d] : [%d]", i-1, b[i-1]);
		//LOGEX(LOG_TRACE, 0, "RunsDistribution: g[%d] : [%d]", i-1, g[i-1]);

		e = (double)(pbsTestStream->bitsNumber - i + 3) / pow(2, i + 2);

		sum += pow(b[i - 1] - e, 2) / e + pow(g[i - 1] - e, 2) / e;
	}//end for(i = 1;i <= k; ++i)

	LOGPX(LOG_TRACE, 0, "RunsDistribution: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("RunsDistribution: sum : [%f]", sum);

	dPValue = igamc(k - 1, sum / (double)2);
	//dPValue = 0;

	LOGPX(LOG_TRACE, 0, "RunsDistribution: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("RunsDistribution: dPValue : [%f]", dPValue);

	if (b != NULL) free(b);
	if (g != NULL) free(g);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "RunsDistribution: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed RunsDistribution~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("RunsDistribution: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed RunsDistribution~~~";

		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "RunsDistribution: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed RunsDistribution!!!");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("RunsDistribution: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream failed RunsDistribution!!!";

		return SWR_TEST_FAILED;
	}
}
//块内最大“1”游程检测
int RandCheck::LongestRunOfOnes(BinarySequence * pbsTestStream, int m)
{
	int i = 0;
	int j = 0;
	int nBlocksNumber = 0;
	int nu = 0; //该子序列中“1”游程的最大长度
	int run = 0; //当前“1”游程的长度
	int K = 0; //K的值，根据M值定，取自《随机数检测规范》 附录B.7 表B.1
	unsigned int v[7]; //{v0, v1,...v6}
	double sum = 0.0;
	double k[7]; //k[i]的值，取自《随机数检测规范》 附录B.7 表B.2
	double pi[7]; //pi的值，根据M值定，取自《随机数检测规范》 附录B.7 表B.3
	double dPValue = 0.0;

	if (pbsTestStream == NULL)
	{
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "LongestRunOfOnes pbsTestStream is null");
		//JLOG(journal_->error()) << "LongestRunOfOnes pbsTestStream is null";
		return SWR_INVALID_PARAMS;
	}

	LOGPX(LOG_TRACE, 0, "LongestRunOfOnes: m : [%d]", m);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("LongestRunOfOnes: m : [%d]", m);

	switch (m)
	{
	case 8:
	{
		K = 3;
		pi[0] = 0.2148;
		pi[1] = 0.3672;
		pi[2] = 0.2305;
		pi[3] = 0.1875;
		k[0] = 1;
		k[1] = 2;
		k[2] = 3;
		k[3] = 4;

		for (i = 0; i <= 6; ++i)
		{
			v[i] = 0;
		}
	}
	break;
	case 128:
	{
		K = 5;
		pi[0] = 0.1174;
		pi[1] = 0.2430;
		pi[2] = 0.2493;
		pi[3] = 0.1752;
		pi[4] = 0.1027;
		pi[5] = 0.1124;
		k[0] = 4;
		k[1] = 5;
		k[2] = 6;
		k[3] = 7;
		k[4] = 8;
		k[5] = 9;

		for (i = 0; i <= 6; ++i)
		{
			v[i] = 0;
		}
	}
	break;
	case 10000:
	{
		K = 6;
		pi[0] = 0.0882;
		pi[1] = 0.2092;
		pi[2] = 0.2483;
		pi[3] = 0.1933;
		pi[4] = 0.1208;
		pi[5] = 0.0675;
		pi[6] = 0.0727;
		k[0] = 10;
		k[1] = 11;
		k[2] = 12;
		k[3] = 13;
		k[4] = 14;
		k[5] = 15;
		k[6] = 16;

		for (i = 0; i <= 6; ++i)
		{
			v[i] = 0;
		}
	}
	break;
	default:
		//m value error
		LOGPX(LOG_ERROR, 0, "LongestRunOfOnes invalid m value : [%d]", m);
		//JLOG(journal_->error()) << ripple::stdStringFormat("LongestRunOfOnes invalid m value : [%d]", m);
		return -1;
	}

	nBlocksNumber = (int)floor((double)pbsTestStream->bitsNumber / (double)m);
	LOGPX(LOG_TRACE, 0, "LongestRunOfOnes: nBlocksNumber : [%d]", nBlocksNumber);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("LongestRunOfOnes: nBlocksNumber : [%d]", nBlocksNumber);

	for (i = 0; i < nBlocksNumber; ++i)
	{
		run = 0;
		nu = 0;
		for (j = i*m; j < (i + 1)*m; ++j)
		{
			if ((int)pbsTestStream->bits[j].b == 1)
			{
				++run;
				nu = nu > run ? nu : run;
			}
			else
			{
				run = 0;
			}
		}
		//LOGEX(LOG_TRACE, 0, "LongestRunOfOnes: block[%d] nu : %d", i, nu);

		for (j = 0; j <= K; ++j)
		{
			if (j == K)
			{
				//last
				++v[j];
				//LOGEX(LOG_TRACE, 0, "LongestRunOfOnes: k[%d] is : %f, ++v[%d], v[%d] now is %d", j, k[j], j, j, v[j]);
			}
			else
			{
				if (nu <= k[j])
				{
					++v[j];
					//LOGEX(LOG_TRACE, 0, "LongestRunOfOnes: k[%d] is : %f, ++v[%d], v[%d] now is %d", j, k[j], j, j, v[j]);
					break;
				}
			}
		}
	}

	LOGP(LOG_TRACE, 0, "LongestRunOfOnes: start count sum");
	//JLOG(journal_->trace()) << "LongestRunOfOnes: start count sum";
	sum = 0.0;
	for (i = 0; i <= K; ++i)
	{
		sum += pow(((double)v[i] - (double)nBlocksNumber*pi[i]), 2) / ((double)nBlocksNumber*pi[i]);
	}
	LOGPX(LOG_TRACE, 0, "LongestRunOfOnes: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("LongestRunOfOnes: sum : [%f]", sum);

	dPValue = igamc(3, sum / 2.0);
	//dPValue = 1.0;

	LOGPX(LOG_TRACE, 0, "LongestRunOfOnes: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("LongestRunOfOnes: dPValue : [%f]", dPValue);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "LongestRunOfOnes: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed LongestRunOfOnes~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("LongestRunOfOnes: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed LongestRunOfOnes~~~";

		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "LongestRunOfOnes: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed LongestRunOfOnes!!!");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("LongestRunOfOnes: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream failed LongestRunOfOnes!!!";

		return SWR_TEST_FAILED;
	}
}
//二元推导检测
int RandCheck::BinaryDerivative(BinarySequence * pbsTestStream, int k)
{
	int i = 0;
	int j = 0;
	double sum = 0.0;
	double dPValue = 0.0;
	double dSqrt2 = 1.41421356237309504880;
	OneBit* obTmpStream = NULL;
	BinarySequence bsTmpStream;


	if (pbsTestStream == NULL)
	{
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "BinaryDerivative pbsTestStream is null");
		//JLOG(journal_->error()) << "BinaryDerivative pbsTestStream is null";
		return SWR_INVALID_PARAMS;
	}

	LOGPX(LOG_TRACE, 0, "BinaryDerivative: k : [%d]", k);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BinaryDerivative: k : [%d]", k);

	//create new pbsStream
	obTmpStream = (OneBit *)calloc(pbsTestStream->bitsNumber, sizeof(OneBit));
	if (obTmpStream == NULL)
	{
		LOGP(LOG_ERROR, SWR_HOST_MEMORY, "BinaryDerivative: calloc obTmpStream error!");
		//JLOG(journal_->error()) << "BinaryDerivative: calloc obTmpStream error!";
		return SWR_HOST_MEMORY;
	}

	bsTmpStream.bits = obTmpStream;
	bsTmpStream.bitsNumber = pbsTestStream->bitsNumber;

	//copy binary stream
	for (i = 0; i < bsTmpStream.bitsNumber; ++i)
	{
		bsTmpStream.bits[i].b = pbsTestStream->bits[i].b;
	}

	//异或k次得到新序列
	for (i = 0; i < k; ++i)
	{
		for (j = 0; j < (bsTmpStream.bitsNumber - k - 1); ++j)
		{
			bsTmpStream.bits[j].b = bsTmpStream.bits[j].b ^ bsTmpStream.bits[j + 1].b;
		}
	}

	for (i = 0; i < pbsTestStream->bitsNumber - k; ++i)
	{
		sum += bsTmpStream.bits[i].b ? (1) : (-1);
	}

	//free tmp pbsStream
	if (obTmpStream != NULL)
	{
		free(obTmpStream);
		obTmpStream = NULL;
		bsTmpStream.bits = NULL;
	}

	LOGPX(LOG_TRACE, 0, "BinaryDerivative: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BinaryDerivative: sum : [%f]", sum);

	sum = fabs(sum);

	LOGPX(LOG_TRACE, 0, "BinaryDerivative: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BinaryDerivative: sum : [%f]", sum);

	sum = sum / sqrt(pbsTestStream->bitsNumber - k);

	LOGPX(LOG_TRACE, 0, "BinaryDerivative: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BinaryDerivative: sum : [%f]", sum);

	dPValue = erfc(fabs(sum) / dSqrt2);

	LOGPX(LOG_TRACE, 0, "BinaryDerivative: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BinaryDerivative: dPValue : [%f]", dPValue);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "BinaryDerivative: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed BinaryDerivative~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("BinaryDerivative: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed BinaryDerivative~~~";

		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "BinaryDerivative: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed BinaryDerivative!!!");
		//JLOG(journal_->error()) << ripple::stdStringFormat("BinaryDerivative: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->error()) << "pbsTestStream failed BinaryDerivative!!!";

		return SWR_TEST_FAILED;
	}
}
//自相关检测
int RandCheck::Autocorrelation(BinarySequence * pbsTestStream, int d)
{
	int i = 0;
	double sum = 0.0;
	double dPValue = 0.0;
	double dSqrt2 = 1.41421356237309504880;

	if (pbsTestStream == NULL)
	{
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "AutocorrelationTest pbsTestStream is null");
		//JLOG(journal_->error()) << "AutocorrelationTest pbsTestStream is null";
		return SWR_INVALID_PARAMS;
	}

	LOGPX(LOG_TRACE, 0, "AutocorrelationTest: d : [%d]", d);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("AutocorrelationTest: d : [%d]", d);

	//异或k次得到新序列
	for (i = 0; i < pbsTestStream->bitsNumber - d; ++i)
	{
		sum += pbsTestStream->bits[i].b ^ pbsTestStream->bits[i + d].b;
	}

	LOGPX(LOG_TRACE, 0, "AutocorrelationTest: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("AutocorrelationTest: sum : [%f]", sum);

	sum -= (pbsTestStream->bitsNumber - d) / 2;
	sum *= 2;
	sum /= sqrt(pbsTestStream->bitsNumber - d);

	LOGPX(LOG_TRACE, 0, "AutocorrelationTest: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("AutocorrelationTest: sum : [%f]", sum);

	dPValue = erfc(fabs(sum) / dSqrt2);

	LOGPX(LOG_TRACE, 0, "AutocorrelationTest: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("AutocorrelationTest: dPValue : [%f]", dPValue);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "AutocorrelationTest: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed AutocorrelationTest~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("AutocorrelationTest: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed AutocorrelationTest~~~";

		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "AutocorrelationTest: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed AutocorrelationTest!!!");
		//JLOG(journal_->error()) << ripple::stdStringFormat("AutocorrelationTest: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->error()) << "pbsTestStream failed AutocorrelationTest!!!";

		return SWR_TEST_FAILED;
	}
}
//矩阵秩检测
int RandCheck::BinaryMatrixRank(BinarySequence * pbsTestStream, int m, int q)
{
	int i = 0;
	int nMatrixNumber = 0;
	double sum = 0.0;
	double dPValue = 0.0;
	double R; // RANK
	double dFM32 = 0; //秩为M的个数（M=32）
	double dFM31 = 0; //秩为M-1的个数
	double dFM30 = 0; //秩小于M-1的个数
	double dP32 = 0.2888; //来自《随机数检测规范》 4.4.10 矩阵秩检测
	double dP31 = 0.5776; //来自《随机数检测规范》 4.4.10 矩阵秩检测
	double dP30 = 0.1336; //来自《随机数检测规范》 4.4.10 矩阵秩检测
						  //OneBit** matrix = create_matrix(m, q);
	OneBit** matrix = NULL;


	if (pbsTestStream == NULL)
	{
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "BinaryMatrixRank pbsTestStream is null");
		//JLOG(journal_->error()) << "BinaryMatrixRank pbsTestStream is null";
		return SWR_INVALID_PARAMS;
	}

	LOGPX(LOG_TRACE, 0, "BinaryMatrixRank: m : [%d], q : [%d]", m, q);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BinaryMatrixRank: m : [%d], q : [%d]", m, q);

	nMatrixNumber = (int)floor((double)pbsTestStream->bitsNumber / ((double)m * (double)q));

	LOGPX(LOG_TRACE, 0, "BinaryMatrixRank: nMatrixNumber : [%d]", nMatrixNumber);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BinaryMatrixRank: nMatrixNumber : [%d]", nMatrixNumber);

	if (nMatrixNumber <= 0)
	{
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "BinaryMatrixRank nMatrixNumber <= 0");
		//JLOG(journal_->error()) << "BinaryMatrixRank nMatrixNumber <= 0";
		return SWR_INVALID_PARAMS;
	}

	matrix = create_matrix(m, q);
	if (matrix == NULL)
	{
		LOGP(LOG_ERROR, SWR_HOST_MEMORY, "BinaryMatrixRank create_matrix error!!!");
		//JLOG(journal_->error()) << "BinaryMatrixRank create_matrix error!!!";
		return SWR_HOST_MEMORY;
	}

	for (i = 0; i < nMatrixNumber; ++i)
	{
		def_matrix(pbsTestStream->bits, m, q, matrix, i);
		R = computeRank(m, q, matrix);
		if (R == 32) ++dFM32;
		if (R == 31) ++dFM31;
	}

	dFM30 = nMatrixNumber - dFM32 - dFM31;

	LOGPX(LOG_TRACE, 0, "BinaryMatrixRank: dFM30 : [%f]", dFM30);
	LOGPX(LOG_TRACE, 0, "BinaryMatrixRank: dFM31 : [%f]", dFM31);
	LOGPX(LOG_TRACE, 0, "BinaryMatrixRank: dFM32 : [%f]", dFM32);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BinaryMatrixRank: dFM30 : [%f]", dFM30);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BinaryMatrixRank: dFM31 : [%f]", dFM31);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BinaryMatrixRank: dFM32 : [%f]", dFM32);

	sum = (pow(dFM32 - dP32*nMatrixNumber, 2) / (double)(nMatrixNumber * dP32)) +
		(pow(dFM31 - dP31*nMatrixNumber, 2) / (double)(nMatrixNumber * dFM31)) +
		(pow(dFM30 - dP30*nMatrixNumber, 2) / (double)(nMatrixNumber * dFM30));

	LOGPX(LOG_TRACE, 0, "BinaryMatrixRank: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BinaryMatrixRank: sum : [%f]", sum);

	dPValue = igamc((double)1, sum / (double)2);
	//dPValue = 1.0;

	LOGPX(LOG_TRACE, 0, "BinaryMatrixRank: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("BinaryMatrixRank: dPValue : [%f]", dPValue);

#if 0
	//free MATRIX
	for (i = 0; i < 32; i++)
		free(matrix[i]);
	free(matrix);
#endif

	//free MATRIX
	delete_matrix(m, matrix);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "BinaryMatrixRank: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed BinaryMatrixRank~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("BinaryMatrixRank: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed BinaryMatrixRank~~~";

		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "BinaryMatrixRank: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed BinaryMatrixRank!!!");
		//JLOG(journal_->error()) << ripple::stdStringFormat("BinaryMatrixRank: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->error()) << "pbsTestStream failed BinaryMatrixRank!!!";

		return SWR_TEST_FAILED;
	}
}
//累加和检测
int RandCheck::Cumulative(BinarySequence * pbsTestStream)
{
	int i = 0;
	int iStart = 0;
	int iFinish = 0;
	double sum = 0.0;
	double sum1 = 0.0;
	double sum2 = 0.0;
	double z = 0.0;
	double dPValue = 0.0;


	if (pbsTestStream == NULL)
	{
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "Cumulative pbsTestStream is null");
		//JLOG(journal_->error()) << "Cumulative pbsTestStream is null";
		return SWR_INVALID_PARAMS;
	}


	//异或k次得到新序列
	for (i = 0; i < pbsTestStream->bitsNumber; ++i)
	{
		sum += 2 * pbsTestStream->bits[i].b - 1;
		z = z > fabs(sum) ? z : fabs(sum);
	}

	LOGPX(LOG_TRACE, 0, "Cumulative: sum : [%f]", sum);
	LOGPX(LOG_TRACE, 0, "Cumulative: z : [%f]", z);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Cumulative: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Cumulative: z : [%f]", z);

	sum1 = 0.0;
	iStart = (-pbsTestStream->bitsNumber / (int)z + 1) / 4;
	iFinish = (pbsTestStream->bitsNumber / (int)z - 1) / 4;
	for (i = iStart; i <= iFinish; ++i)
		sum1 += (normal((4 * i + 1)*z / sqrt(pbsTestStream->bitsNumber)) - normal((4 * i - 1)*z / sqrt(pbsTestStream->bitsNumber)));
	sum2 = 0.0;
	iStart = (-pbsTestStream->bitsNumber / (int)z - 3) / 4;
	iFinish = (pbsTestStream->bitsNumber / (int)z - 1) / 4;
	for (i = iStart; i <= iFinish; ++i)
		sum2 += (normal((4 * i + 3)*z / sqrt(pbsTestStream->bitsNumber)) - normal((4 * i + 1)*z / sqrt(pbsTestStream->bitsNumber)));

	LOGPX(LOG_TRACE, 0, "Cumulative: sum1 : [%f]", sum1);
	LOGPX(LOG_TRACE, 0, "Cumulative: sum2 : [%f]", sum2);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Cumulative: sum1 : [%f]", sum1);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Cumulative: sum2 : [%f]", sum2);

	dPValue = 1.0 - sum1 + sum2;

	LOGPX(LOG_TRACE, 0, "Cumulative: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Cumulative: dPValue : [%f]", dPValue);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "Cumulative: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed Cumulative~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("Cumulative: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed Cumulative~~~";

		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "Cumulative: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed Cumulative!!!");
		//JLOG(journal_->error()) << ripple::stdStringFormat("Cumulative: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->error()) << "pbsTestStream failed Cumulative!!!";

		return SWR_TEST_FAILED;
	}
}
//近似熵检测
int RandCheck::ApproximateEntropy(BinarySequence * pbsTestStream, int m)
{
	int i = 0;
	int j = 0;
	int k = 0;
	int nBlockSize;
	unsigned int* v = NULL; //vi
	double dApEn[2];
	double sum = 0.0;
	double dPValue = 0.0;

	if (pbsTestStream == NULL)
	{
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "ApproximateEntropy pbsTestStream is null");
		//JLOG(journal_->error()) << "ApproximateEntropy pbsTestStream is null";
		return SWR_INVALID_PARAMS;
	}

	LOGPX(LOG_TRACE, 0, "ApproximateEntropy: m : [%d]", m);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("ApproximateEntropy: m : [%d]", m);

	v = (unsigned int*)calloc((int)pow(2, (m + 1)), sizeof(unsigned int));
	if (v == NULL)
	{
		LOGP(LOG_ERROR, SWR_HOST_MEMORY, "ApproximateEntropy calloc v error");
		//JLOG(journal_->error()) << "ApproximateEntropy calloc v error";
		return SWR_HOST_MEMORY;
	}

	for (i = 0; i < m + 1; ++i)
	{
		v[i] = 0;
	}

	for (nBlockSize = m; nBlockSize <= (m + 1); ++nBlockSize)
	{
		for (i = 0; i < pbsTestStream->bitsNumber; ++i)
		{
			k = 0;

			for (j = 0; j < nBlockSize; ++j)
			{
				k <<= 1;
				k += pbsTestStream->bits[(i + j) % pbsTestStream->bitsNumber].b;
			}

			++v[k];
		} //end for(i = 0;i < pbsTestStream->bitsNumber; ++i)

		sum = 0.0;

		for (i = 0; i < (int)pow(2, nBlockSize); ++i)
		{
			LOGPX(LOG_TRACE, 0, "ApproximateEntropy: v[%d] : [%u]", i, v[i]);
			//JLOG(journal_->trace()) << ripple::stdStringFormat("ApproximateEntropy: v[%d] : [%u]", i, v[i]);

			sum += ((double)v[i] / (double)pbsTestStream->bitsNumber)*log((double)v[i] / (double)pbsTestStream->bitsNumber);
		}

		dApEn[nBlockSize - m] = sum;

		LOGPX(LOG_TRACE, 0, "ApproximateEntropy: dApEn[%d] : [%f]", (nBlockSize - m), dApEn[nBlockSize - m]);
		//JLOG(journal_->trace()) << ripple::stdStringFormat("ApproximateEntropy: dApEn[%d] : [%f]", (nBlockSize - m), dApEn[nBlockSize - m]);
	} //end for(nBlockSize = m;nBlockSize <= (m+1); ++nBlockSize)

	sum = dApEn[0] - dApEn[1];

	LOGPX(LOG_TRACE, 0, "ApproximateEntropy: dApEn : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("ApproximateEntropy: dApEn : [%f]", sum);

	sum = 2.0 * pbsTestStream->bitsNumber * (log(2) - sum);

	LOGPX(LOG_TRACE, 0, "ApproximateEntropy: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("ApproximateEntropy: sum : [%f]", sum);

	dPValue = igamc(pow(2, m - 1), sum / 2.0);
	//dPValue = 1.0;

	LOGPX(LOG_TRACE, 0, "ApproximateEntropy: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("ApproximateEntropy: dPValue : [%f]", dPValue);

	//free
	if (v != NULL) free(v);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "ApproximateEntropy: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed ApproximateEntropy~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("ApproximateEntropy: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed ApproximateEntropy~~~";

		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "ApproximateEntropy: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed ApproximateEntropy!!!");
		//JLOG(journal_->error()) << ripple::stdStringFormat("ApproximateEntropy: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->error()) << "pbsTestStream failed ApproximateEntropy!!!";

		return SWR_TEST_FAILED;
	}
}
//线性复杂度检测
int RandCheck::LinearComplexity(BinarySequence* pbsTestStream, int m)
{
	int i = 0;
	int j = 0;
	int ii = 0;
	int sign = 0; //符号值
	int nBlocksNumber = 0;
	int nM = 0;
	int nN = 0;
	int nL = 0;
	int nD = 0;
	int K = 6;
	unsigned int v[7]; //{v0, v1,...v6}
					   //paii的值，取自《随机数检测规范》 附录B.13
	double pi[7] = { 0.010417, 0.03125, 0.12500, 0.5000, 0.25000, 0.06250, 0.020833 };
	double sum = 0.0;
	double dUValue = 0.0;
	double dTValue = 0.0;
	double dPValue = 0.0;
	OneBit* obB = NULL;
	OneBit* obC = NULL;
	OneBit* obT = NULL;
	OneBit* obP = NULL;


	if (pbsTestStream == NULL)
	{
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "LinearComplexity pbsTestStream is null");
		//JLOG(journal_->error()) << "LinearComplexity pbsTestStream is null";
		return SWR_INVALID_PARAMS;
	}

	LOGPX(LOG_TRACE, 0, "LinearComplexity: m : [%d]", m);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("LinearComplexity: m : [%d]", m);

	nBlocksNumber = (int)floor((double)pbsTestStream->bitsNumber / (double)m);

	LOGPX(LOG_TRACE, 0, "LinearComplexity: nBlocksNumber : [%d]", nBlocksNumber);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("LinearComplexity: nBlocksNumber : [%d]", nBlocksNumber);

	//计算u中-1指数值
	if (m % 2 == 0)
	{
		sign = -1;
	}
	else
	{
		sign = 1;
	}
	//u = (m/2) + ((9+(-1)^(m+1)/36) - 1/(2^m) * ((m/3)+(2/9)))
	dUValue = m / 2. + (9. + sign) / 36. - 1. / pow(2, m) * (m / 3. + 2. / 9.);

	LOGPX(LOG_TRACE, 0, "LinearComplexity: dUValue : [%f]", dUValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("LinearComplexity: dUValue : [%f]", dUValue);

	//计算Ti中-1指数值
	if (m % 2 == 0)
	{
		sign = 1;
	}
	else
	{
		sign = -1;
	}

	//{v0, v1,...v6}
	for (i = 0; i <= K; ++i)
	{
		v[i] = 0;
	}

	if (((obB = (OneBit*)calloc(m, sizeof(OneBit))) == NULL) ||
		((obC = (OneBit*)calloc(m, sizeof(OneBit))) == NULL) ||
		((obP = (OneBit*)calloc(m, sizeof(OneBit))) == NULL) ||
		((obT = (OneBit*)calloc(m, sizeof(OneBit))) == NULL))
	{
		if (obB != NULL) free(obB);
		if (obC != NULL) free(obC);
		if (obP != NULL) free(obP);
		if (obT != NULL) free(obT);

		LOGP(LOG_ERROR, SWR_HOST_MEMORY, "LinearComplexity calloc obB obC obP obT error");
		//JLOG(journal_->error()) << "LinearComplexity calloc obB obC obP obT error";
		return SWR_HOST_MEMORY;
	}

	for (ii = 0; ii < nBlocksNumber; ++ii)
	{
		for (i = 0; i < m; i++)
		{
			obB[i].b = 0;
			obC[i].b = 0;
			obT[i].b = 0;
			obP[i].b = 0;
		}

		nL = 0;
		nM = -1;
		nD = 0;
		obC[0].b = 1;
		obB[0].b = 1;
		// DETERMINE LINEAR COMPLEXITY
		nN = 0;
		while (nN < m)
		{
			nD = (int)pbsTestStream->bits[ii*m + nN].b;
			for (i = 1; i <= nL; i++)
				nD += (int)obC[i].b*(int)pbsTestStream->bits[ii*m + nN - i].b;
			nD = nD % 2;
			if (nD == 1) {
				for (i = 0; i < m; i++) {
					obT[i].b = obC[i].b;
					obP[i].b = 0;
				}
				for (j = 0; j < m; j++)
					if (obB[j].b == 1) obP[j + nN - nM].b = 1;
				for (i = 0; i < m; i++)
					obC[i].b = (obC[i].b + obP[i].b) % 2;
				if (nL <= nN / 2) {
					nL = nN + 1 - nL;
					nM = nN;
					for (i = 0; i < m; i++)
						obB[i].b = obT[i].b;
				}
			}//end if (nD == 1) {
			++nN;
		}//end while(nN < m)

		 //Ti = ((-1)^m) * (Li - u) + 2 / 9
		dTValue = sign * (nL - dUValue) + 2. / 9.;

		//LOGEX(LOG_TRACE, 0, "LinearComplexity: dTValue : [%f]", dTValue);

		if (dTValue <= -2.5)
			++v[0];
		else if (dTValue > -2.5 && dTValue <= -1.5)
			++v[1];
		else if (dTValue > -1.5 && dTValue <= -0.5)
			++v[2];
		else if (dTValue > -0.5 && dTValue <= 0.5)
			++v[3];
		else if (dTValue > 0.5 && dTValue <= 1.5)
			++v[4];
		else if (dTValue > 1.5 && dTValue <= 2.5)
			++v[5];
		else
			++v[6];
	}//end for(ii = 0;ii < nBlocksNumber; ++ii)

	for (i = 0; i <= K; ++i)
	{
		LOGPX(LOG_TRACE, 0, "LinearComplexity: pi[i] : [%f]", pi[i]);
		LOGPX(LOG_TRACE, 0, "LinearComplexity: v[i] : [%d]", v[i]);
		//JLOG(journal_->trace()) << ripple::stdStringFormat("LinearComplexity: pi[i] : [%f]", pi[i]);
		//JLOG(journal_->trace()) << ripple::stdStringFormat("LinearComplexity: v[i] : [%d]", v[i]);

		sum += pow((v[i] - nBlocksNumber*pi[i]), 2) / (nBlocksNumber*pi[i]);
	}

	//free
	if (obB != NULL) free(obB);
	if (obC != NULL) free(obC);
	if (obP != NULL) free(obP);
	if (obT != NULL) free(obT);

	dPValue = igamc(3, sum / 2);
	//dPValue = 1.0;

	LOGPX(LOG_TRACE, 0, "LinearComplexity: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("LinearComplexity: dPValue : [%f]", dPValue);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "LinearComplexity: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed LinearComplexity~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("LinearComplexity: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed LinearComplexity~~~";

		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "LinearComplexity: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed LinearComplexity!!!");
		//JLOG(journal_->error()) << ripple::stdStringFormat("LinearComplexity: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->error()) << "pbsTestStream failed LinearComplexity!!!";

		return SWR_TEST_FAILED;
	}
}
//Maurer通用统计检测
int RandCheck::Maurer(BinarySequence * pbsTestStream, int L, int Q)
{
	int i = 0;
	int j = 0;
	int K = 0;
	int p;
	long* T = NULL;
	long decRep;
	double c = 0.0;
	double sum = 0.0;
	double dPValue = 0.0;
	double dSqrt2 = 1.41421356237309504880;
	double   expected_value[17] = { 0, 0, 0, 0, 0, 0, //Var值，对应L(6-16)，取自NIST-RandomTests工程universal.c
		5.2177052, 6.1962507, 7.1836656,
		8.1764248, 9.1723243, 10.170032, 11.168765,
		12.168070, 13.167693, 14.167488, 15.167379 };
	double   variance[17] = { 0, 0, 0, 0, 0, 0, //Var值，对应L(6-16)，取自NIST-RandomTests工程universal.c
		2.954, 3.125, 3.238, 3.311, 3.356, 3.384,
		3.401, 3.410, 3.416, 3.419, 3.421 };


	if (pbsTestStream == NULL)
	{
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "Maurer pbsTestStream is null");
		//JLOG(journal_->error()) << "Maurer pbsTestStream is null";
		return SWR_INVALID_PARAMS;
	}

	if (L < 6 || L > 16)
	{
		LOGP(LOG_ERROR, L, "L out of range");
		//JLOG(journal_->error()) << "L out of range";
		return SWR_INVALID_PARAMS;
	}

	LOGPX(LOG_TRACE, 0, "Maurer: L : [%d]", L);
	LOGPX(LOG_TRACE, 0, "Maurer: Q : [%d]", Q);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Maurer: L : [%d]", L);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Maurer: Q : [%d]", Q);

	K = (int)floor((double)pbsTestStream->bitsNumber / (double)L) - Q;

	LOGPX(LOG_TRACE, 0, "Maurer: K : [%d]", K);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Maurer: K : [%d]", K);

	p = (int)pow(2, L);

	LOGPX(LOG_TRACE, 0, "Maurer: p : [%d]", p);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Maurer: p : [%d]", p);

	T = (long*)calloc(p, sizeof(long));
	if (T == NULL)
	{
		LOGP(LOG_ERROR, SWR_HOST_MEMORY, "Maurer calloc T array error");
		//JLOG(journal_->error()) << "Maurer calloc T array error";
		return SWR_HOST_MEMORY;
	}

	for (i = 0; i < p; ++i)
	{
		T[i] = 0;
	}

	// INITIALIZE TABLE
	for (i = 1; i <= Q; ++i)
	{
		decRep = 0;
		for (j = 0; j < L; j++)
		{
			decRep += pbsTestStream->bits[(i - 1)*L + j].b * (long)pow(2, L - 1 - j);
		}
		T[decRep] = i;
	}

	// PROCESS BLOCKS
	for (i = (Q + 1); i <= (Q + K); ++i)
	{
		decRep = 0;
		for (j = 0; j < L; j++)
		{
			decRep += pbsTestStream->bits[(i - 1)*L + j].b * (long)pow(2, L - 1 - j);
		}
		sum += log(i - T[decRep]) / log(2);
		T[decRep] = i;
	}

	LOGPX(LOG_TRACE, 0, "Maurer: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Maurer: sum : [%f]", sum);

	sum = sum / (double)K;

	LOGPX(LOG_TRACE, 0, "Maurer: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Maurer: sum : [%f]", sum);

	c = 0.7 - 0.8 / (double)L + (4 + 32 / (double)L)*pow(K, -3 / (double)L) / 15;

	LOGPX(LOG_TRACE, 0, "Maurer: c : [%f]", c);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Maurer: c : [%f]", c);

	c = c * sqrt(variance[L] / (double)K);

	LOGPX(LOG_TRACE, 0, "Maurer: c : [%f]", c);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Maurer: c : [%f]", c);

	//dPValue = 1.0;
	dPValue = erfc(fabs((sum - expected_value[L])) / (dSqrt2 * c));

	LOGPX(LOG_TRACE, 0, "Maurer: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("Maurer: dPValue : [%f]", dPValue);

	if (T != NULL) free(T);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "Maurer: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed Maurer~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("Maurer: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed Maurer~~~";

		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "Maurer: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed Maurer!!!");
		//JLOG(journal_->error()) << ripple::stdStringFormat("Maurer: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->error()) << "pbsTestStream failed Maurer!!!";

		return SWR_TEST_FAILED;
	}
}
//离散傅立叶检测
int RandCheck::DiscreteFourierTransform(BinarySequence * pbsTestStream, int d)
{
	int i = 0;
	int count = 0;
	double* dX = NULL;
	double* dm = NULL;
	double* dwsave = NULL;
	double* difac = NULL;
	double upperBound;
	double sum = 0.0;
	double N0;
	double dPValue = 0.0;
	double dSqrt2 = 1.41421356237309504880;


	if (pbsTestStream == NULL)
	{
		LOGP(LOG_ERROR, SWR_INVALID_PARAMS, "DiscreteFourierTransform pbsTestStream is null");
		//JLOG(journal_->error()) << "DiscreteFourierTransform pbsTestStream is null";
		return SWR_INVALID_PARAMS;
	}

	if (((dX = (double*)calloc(pbsTestStream->bitsNumber, sizeof(double))) == NULL) ||
		((dwsave = (double*)calloc(2 * pbsTestStream->bitsNumber + 15, sizeof(double))) == NULL) ||
		((difac = (double*)calloc(15, sizeof(double))) == NULL) ||
		((dm = (double*)calloc(pbsTestStream->bitsNumber / 2 + 1, sizeof(double))) == NULL))
	{
		if (dX != NULL) free(dX);
		if (dwsave != NULL) free(dwsave);
		if (difac != NULL) free(difac);
		if (dm != NULL) free(dm);

		LOGP(LOG_ERROR, SWR_HOST_MEMORY, "DiscreteFourierTransform calloc dX dwsave difac dm error");
		//JLOG(journal_->error()) << "DiscreteFourierTransform calloc dX dwsave difac dm error";
		return SWR_HOST_MEMORY;
	}

	for (i = 0; i < pbsTestStream->bitsNumber; ++i)
	{
		dX[i] = 2 * (int)pbsTestStream->bits[i].b - 1;
	}

	// INITIALIZE WORK ARRAYS
	__ogg_fdrffti(pbsTestStream->bitsNumber, dwsave, (int*)difac);
	// APPLY FORWARD FFT
	__ogg_fdrfftf(pbsTestStream->bitsNumber, dX, dwsave, (int*)difac);
	// COMPUTE MAGNITUDE
	dm[0] = sqrt(dX[0] * dX[0]);

	// DISPLAY FOURIER POINTS
	for (i = 0; i < pbsTestStream->bitsNumber / 2; i++)
	{
		dm[i + 1] = sqrt(pow(dX[2 * i + 1], 2) + pow(dX[2 * i + 2], 2));
	}

	// CONFIDENCE INTERVAL
	count = 0; // number of peaks less than h = sqrt(2.995732274*n)
	upperBound = sqrt(2.995732274 * pbsTestStream->bitsNumber);
	for (i = 0; i < pbsTestStream->bitsNumber / 2; i++)
		if (dm[i] < upperBound) count++;

	LOGPX(LOG_TRACE, 0, "DiscreteFourierTransform: count : [%d]", count);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("DiscreteFourierTransform: count : [%d]", count);

	N0 = (double)0.95*pbsTestStream->bitsNumber / 2.;

	LOGPX(LOG_TRACE, 0, "DiscreteFourierTransform: N0 : [%f]", N0);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("DiscreteFourierTransform: N0 : [%f]", N0);

	sum = (count - N0) / sqrt(pbsTestStream->bitsNumber / 2.*0.95*0.05);

	LOGPX(LOG_TRACE, 0, "DiscreteFourierTransform: sum : [%f]", sum);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("DiscreteFourierTransform: sum : [%f]", sum);

	//free
	if (dX != NULL) free(dX);
	if (dwsave != NULL) free(dwsave);
	if (difac != NULL) free(difac);
	if (dm != NULL) free(dm);

	dPValue = erfc(fabs(sum) / dSqrt2);

	LOGPX(LOG_TRACE, 0, "DiscreteFourierTransform: dPValue : [%f]", dPValue);
	//JLOG(journal_->trace()) << ripple::stdStringFormat("DiscreteFourierTransform: dPValue : [%f]", dPValue);

	if (dPValue >= g_dSigLevel)
	{
		LOGPX(LOG_TRACE, 0, "DiscreteFourierTransform: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_TRACE, 0, "pbsTestStream passed DiscreteFourierTransform~~~");
		//JLOG(journal_->trace()) << ripple::stdStringFormat("DiscreteFourierTransform: dPValue[%f] >= g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->trace()) << "pbsTestStream passed DiscreteFourierTransform~~~";

		return SWR_OK;
	}
	else
	{
		LOGPX(LOG_ERROR, SWR_TEST_FAILED, "DiscreteFourierTransform: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		LOGP(LOG_ERROR, SWR_TEST_FAILED, "pbsTestStream failed DiscreteFourierTransform!!!");
		//JLOG(journal_->error()) << ripple::stdStringFormat("DiscreteFourierTransform: dPValue[%f] < g_dSigLevel[%f]", dPValue, g_dSigLevel);
		//JLOG(journal_->error()) << "pbsTestStream failed DiscreteFourierTransform!!!";

		return SWR_TEST_FAILED;
	}
}
