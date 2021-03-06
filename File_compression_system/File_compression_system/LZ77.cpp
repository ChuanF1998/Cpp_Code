#include "LZ77.h"

//curMatchlength的问题

#include <assert.h>
#include <iostream>
using namespace std;


const USH MIN_LOOKAHEAD = MAX_MATCH + MIN_MATCH + 1;
const USH MAX_DIST = WSIZE - MIN_LOOKAHEAD;

LZ77::LZ77()
: _pWin(new UCH[WSIZE * 2])
, _pH(WSIZE)
{}

LZ77::~LZ77()
{
	delete[] _pWin;
	_pWin = nullptr;
}

//压缩算法
void LZ77::CompressFile(const string& strFilePath, string& postFix)
{
    //1.如果源文件的大小小于 MIN_MATCH，则不进行处理
	//
	//获取文件大小
	FILE* fIn = fopen(strFilePath.c_str(), "rb");
	if (fIn == nullptr) {
		cout << "文件大小为空 ！" << endl;
		return;
	}

	//获取文件大小
	fseek(fIn, 0, SEEK_END);
	ULL FileSize= ftell(fIn);

	//1.文件大小小于MIN_MATCH，则不进行处理
	if (FileSize < MIN_MATCH) {
		cout << "文件太小" << endl;
		return;
	}

	//从压缩文件中读取一个缓冲区的数据到窗口中
	fseek(fIn, 0, SEEK_SET);
	size_t lookAhead = fread(_pWin, 1, 2 * WSIZE, fIn);

	//与写标记相关的变量
	UCH chFlag = 0;  
	USH bitCount = 0;
	bool isLen = false;
	

	USH start = 0;
	USH hashAddr = 0;
	USH matchHead = 0;
	USH curMatchLength = 0;  //匹配链长度
	USH curMatchDist = 0;    //距离

	//计算起始位置前2个字符的hashaddr值
	for (USH i = 0; i < MIN_MATCH - 1; ++i) {
		_pH.HashFunc(hashAddr, _pWin[i]);
	}

	//找出文件后缀
	size_t pos = strFilePath.find('.');
	postFix = strFilePath.substr(pos + 1);

	string compression = strFilePath.substr(0, pos);
	//压缩文件和标记文件
	string comFile = compression + ".lzp"; 
	string comFileFlag = compression + "_F.lzp";

	//打开输出文件
	FILE* fOut = fopen(comFile.c_str(), "wb");
	assert(fOut);
	//写标记的文件
	FILE* fOutFlag = fopen(comFileFlag.c_str(), "wb");

	//lookAhead表示先行缓冲区中剩余字节的个数
	while (lookAhead) {
		//匹配之前，将长度和距离清零
		curMatchDist = 0;
		curMatchLength = 0;

		//1.获取匹配头
	    _pH.Insert(matchHead, _pWin[start + 2], start, hashAddr);

		//2.验证在查找缓冲区中是否找到匹配，有匹配，找最长
		if (matchHead) {
			//顺着匹配链找最长,最终得到 <长度， 距离> 对
			curMatchLength = LongestMatch(matchHead, curMatchDist, start);
		}

		//3.验证是否找到匹配
		if (curMatchLength < MIN_MATCH) {
			//在查找缓冲区中没有找到匹配
			//将start位置的字符写入到压缩文件中
			fputc(_pWin[start], fOut);

			//写原字符对应的标记
			WriteFlag(fOutFlag, chFlag, bitCount, false);
			start++;
			lookAhead--;
		}
		else {
			//找到匹配，将<长度， 距离> 对插入

			//写长度
			UCH ch = (UCH)(curMatchLength - 3);
			fputc(ch, fOut);
            //写距离
			fwrite(&curMatchDist, sizeof(curMatchDist), 1, fOut);

			//写原字符对应的标记
			WriteFlag(fOutFlag, chFlag, bitCount, true);

			//更新先行缓冲区中剩余的字节数
			lookAhead -= curMatchLength;

			//将已经匹配的字符串按照三个一组插入到hash表中
			curMatchLength -= 1;
			while (curMatchLength) {
				start++;
				_pH.Insert(matchHead, _pWin[start + 2], start, hashAddr);
				curMatchLength--;				
			}
			start++;
		}

		//检测先行缓冲区中剩余字符个数
		if (lookAhead <= MIN_LOOKAHEAD) {
			FillWindow(fIn, lookAhead, start);
		}

	}

	//标记位不够8个比特位
	if (bitCount != 0 && bitCount < 8) {
		chFlag <<= (8 - bitCount);
		fputc(chFlag, fOutFlag);
	}
	fclose(fOutFlag);

	//合并压缩文件
	MergeFile(fOut, FileSize, comFileFlag);

	//关闭文件
	fclose(fOut);
	fclose(fIn);
	
	

}

//chFlag:该字节的每个比特位用来区分当前字符是原字符还是长度 0--原字符  1--长度
//islen:代表该字节是原字符还是长度
void LZ77::WriteFlag(FILE* fOut, UCH& chFlag, USH& bitCount, bool isLen)
{
	chFlag <<= 1;
	if (isLen) {
		chFlag |= 1;
	}
	bitCount++;
	if (bitCount == 8 && bitCount != 0) {
		//将该标记写入到压缩文件中
		fputc(chFlag, fOut);
		chFlag = 0;
		bitCount = 0;
	}
}


//匹配：是在查找缓冲区中进行的，查找缓冲区中可能会找到多个匹配
//输出：最长匹配
//注意事项：可能会遇到环状链，需设置最大匹配次数
//匹配在MAX_DIST范围内匹配，太远不进行匹配
USH LZ77::LongestMatch(USH matchHead, USH& curMatchDist, USH start)
{
	UCH curMatchLen = 0;   //一次匹配长度
	UCH maxMatchLen = 0;   //最大匹配长度
	UCH maxMatchCount = 255; //最大匹配次数
	USH curMatchStart = 0; //当前匹配在查找缓冲区中的位置
	
	//在先行缓冲区中查找匹配时，不能太远即不能超过MAX_DIST
	USH limit = start > MAX_DIST ? start - MAX_DIST : 0;

	do {
		//匹配范围
		UCH* pStart = _pWin + start;
		UCH* pEnd = pStart + MAX_MATCH;

		//查找缓冲区匹配串的起始
		UCH* pMatchStart = _pWin + matchHead;
		curMatchLen = 0;

		//进行匹配
		while (pStart < pEnd && *pStart == *pMatchStart) {
			curMatchLen++;
			pStart++;
			pMatchStart++;
		}

		//一次匹配结束
		if (curMatchLen > maxMatchLen) {
			maxMatchLen = curMatchLen;
			curMatchStart = matchHead;
		}

	} while ((matchHead = _pH.GetNext(matchHead)) > limit &&  maxMatchCount--);

	//距离
	curMatchDist = start - curMatchStart;

	return maxMatchLen;
}



//解压缩
void LZ77::UncompressFile(const string& strFilePath, string postFix)
{
	//打开压缩文件
	FILE* fIn = fopen(strFilePath.c_str(), "rb");
	if (fIn == nullptr) {
		cout << "打开文件失败" << endl;
		return;
	}
	//再次打开文件，为操作标记的指针
	FILE* fInF = fopen(strFilePath.c_str(), "rb");
	if (fInF == nullptr) {
		cout << "打开文件失败" << endl;
		return;
	}

	//获取源文件的大小
	ULL FileSize = 0;
	fseek(fInF, 0 - sizeof(FileSize), SEEK_END);
	fread(&FileSize, sizeof(FileSize), 1, fInF);

	//获取标记信息的大小
	size_t FlagSize = 0;
	fseek(fInF, 0 - sizeof(ULL) - sizeof(FlagSize), SEEK_END);
	fread(&FlagSize, sizeof(FlagSize), 1, fInF);

	//将读取标记信息的文件指针移动到保存标记数据的起始位置
	fseek(fInF, 0 - sizeof(FlagSize)- FlagSize - sizeof(FileSize), SEEK_END);
	
	size_t pos = strFilePath.find('.');
	string uncompression = strFilePath.substr(0, pos);
	//压缩文件和标记文件
	string uncomFile = uncompression + " _u." + postFix;
	//string comFileFlag = compression + "_F.lzp";

	FILE* fOut = fopen(uncomFile.c_str(), "wb");
	if (fOut == nullptr) {
		cout << "打开文件失败" << endl;
		return;
	}

	FILE* fOut_r = fopen(uncomFile.c_str(), "rb");

	UCH bitCount = 0;
	UCH chFlag = 0;
	ULL EncodeCount = 0;
	while (EncodeCount < FileSize) {
		if (bitCount == 0) {
			chFlag = fgetc(fInF);
			bitCount = 8;
		}


		//结果为1
		if (chFlag & 0x80) {
			//<距离,长度>对
			USH matchLen = fgetc(fIn) + 3;
			USH matchDist = 0;
			fread(&matchDist, sizeof(matchDist), 1, fIn);

			UCH ch;
			//清空缓冲区
			fflush(fOut);

			EncodeCount += matchLen;
			//fOut_r:读取前文匹配的内容
			fseek(fOut_r, 0 - matchDist, SEEK_END);
			while (matchLen != 0) {
				 ch = fgetc(fOut_r);
				 fputc(ch, fOut);
				 matchLen--;
				 fflush(fOut);
			}

		}
		else {
			//原字符
			UCH ch = fgetc(fIn);
			fputc(ch, fOut);
			EncodeCount += 1;
			
		}

		chFlag <<= 1;
		bitCount--;
	}

	fclose(fIn);
	fclose(fInF);
	fclose(fOut);
	fclose(fOut_r);

}

//合并压缩文件
void LZ77::MergeFile(FILE* fOut, ULL FileSize, string comFileFlag)
{
	//1.读取标记信息文件中内容，然后将结果写入到压缩文件中
	FILE* fInF = fopen(comFileFlag.c_str(), "rb");

    size_t flagSize = 0;
	UCH* pReadBuff = new UCH[1024];
	while (true) {
		size_t rdSize = fread(pReadBuff, 1, 1024, fInF);
		if (rdSize == 0) {
			break;
		}

		fwrite(pReadBuff, 1, rdSize, fOut);
		flagSize += rdSize;
	}

	fwrite(&flagSize, sizeof(flagSize), 1, fOut);
	fwrite(&FileSize, sizeof(FileSize), 1, fOut);
	fclose(fInF);
}

//滑动窗口
void LZ77::FillWindow(FILE* fIn, size_t& lookAhead, USH& start)
{
	if (start >= WSIZE) {
		//将右窗中的数据搬移到左窗
		memcpy(_pWin, _pWin + WSIZE, WSIZE);
		memset(_pWin + WSIZE, 0, WSIZE);
		start -= WSIZE;

		//更新hash表
		_pH.Update();

		//像右窗中补充一个窗口
		if (!feof(fIn)) {
			lookAhead += fread(_pWin + WSIZE, 1, WSIZE, fIn);
		}
	}
}



