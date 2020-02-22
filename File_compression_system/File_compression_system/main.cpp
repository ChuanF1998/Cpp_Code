/*
文件压缩：
1.什么是文件压缩？
2.为什么要对文件进行压缩？
   文件太大，节省空间
   提高数据在网络上传输的效率
   对数据保护的作用--加密
3.文件压缩分类？
    无损压缩：
	有损压缩：解压缩之后不能将其还原成与源文件完全相同的格式
4.如何进行压缩？
*/

/*
压缩文件包含两部分：压缩信息：源文件后缀，编码行数, 字符次数信息
                    压缩数据：
*/

#include "FileCompressHuff.h"



int main()
{
	FileCompressHuff fc;
	fc.CompressFile("1.txt");
	fc.UncompressFile("2.txt");
	return 0;
}


/*
问题1：不能用char形，要用unsiged char
问题2：多写入了一个空格到压缩文件
*/