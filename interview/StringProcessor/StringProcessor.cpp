// StringGenerator.cpp : Defines the entry point for the console application.
//
#ifdef _MSC_VER
#include <windows.h>
#endif
#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <cctype>
#include <boost/bind.hpp>
#include "boost/lexical_cast.hpp"
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <map>
#include <deque>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>

#ifdef _MSC_VER

typedef __int8  int8;
typedef __int16 int16;
typedef __int32 int32;
typedef __int64 int64;

typedef unsigned __int8  uint8;
typedef unsigned __int16 uint16;
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;


#else
typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
#endif

/* the bottleneck is the hard disk I/O(after use muti thread to compute random string),
 therefore I make a thread to write file. and another theads to generate*/
//Global define
uint64 g_max_memmory_size = 512*1000*1000; //asumming we have 512M mem
uint64 g_block_write_size = 10*1000*1000; //asumming we have 512M mem
uint64 g_piece_file_size = 10*1000*1000; //how much size of one piece file
float  g_max_memory_use = 0.6f; //define how much percent system memory we can use default to 60%

uint64 getTickTime()
{
#ifdef _MSC_VER
	return GetTickCount();
#else
	struct timespec time1 = {0, 0};
	clock_gettime(CLOCK_REALTIME, &time1);
	uint64 timestamp = ((long long)time1.tv_sec*1000000000LL + (long long)time1.tv_nsec)/1000;
	return timestamp/1000;
#endif
}

void expensiveFunc(std::string& str_in)
{
	return;
}

bool lessString(const std::string & str1, const std::string & str2) {
	return str1 < str2;
}

void sortStringArray(std::vector<std::string>& str_array)
{

	std::sort(str_array.begin(), str_array.end(),lessString);

}
//intput e.g. /home/ayres/tmp/_tmp ,2
//return the final path ,return 0 size string when failed
std::string mergeSortFiles(std::string path_prefix,int file_count)
{
	if(file_count <= 1)
	{
		return path_prefix + boost::lexical_cast<std::string>(0)+".tmp";;
	}

	//merge adjacent two files
	int merged_file_count = 0;

	for(int i = 0;i<file_count;i+=2)
	{
		if (i == file_count - 1) {
			//if just last file left just rename, no need to merge

			boost::system::error_code ec;
			std::string tmp_file_path_1 = path_prefix + boost::lexical_cast<std::string>(i)+".tmp";
			std::string tmp_out_file_path_1 = path_prefix +"_" + boost::lexical_cast<std::string>(merged_file_count)+".tmp";
 			boost::filesystem::rename(tmp_file_path_1,tmp_out_file_path_1,ec);
			merged_file_count++;
			break;
		}

		std::string tmp_file_path_1 = path_prefix + boost::lexical_cast<std::string>(i)+".tmp";;
		std::ifstream infile1(tmp_file_path_1.c_str(),std::ios::in);
		if (!infile1)
		{
			std::cout << "open temp file fail:"<< tmp_file_path_1<< std::endl;
			return "";
		}

		std::string tmp_file_path_2 = path_prefix + boost::lexical_cast<std::string>(i+1)+".tmp";;
		std::ifstream infile2(tmp_file_path_2.c_str(),std::ios::in);
		if (!infile2)
		{
			std::cout << "open temp file fail:"<< tmp_file_path_2<< std::endl;
			return "";
		}

		std::string  tmp_out_file_path_1= path_prefix +"_" + boost::lexical_cast<std::string>(merged_file_count)+".tmp";
		std::ofstream outfile(tmp_out_file_path_1.c_str(), std::ios::out);
		if(!outfile)
		{
			std::cout <<"can't create temp file!:"<<tmp_out_file_path_1<<std::endl;
			return "";
		}

		std::string string_line_file1;
		std::string string_line_file2;
		std::getline(infile1, string_line_file1, '\n');
		std::getline(infile2, string_line_file2, '\n');

		while (string_line_file1.size() > 0 || string_line_file2.size() >0)
		{
			//compare who is larger
			if (string_line_file1.size() > 0 && string_line_file2.size() >0)
			{
				bool mergeResult = lessString(string_line_file1,string_line_file2);
				if (mergeResult == true) {//string_line_file1 < string_line_file2
					outfile << string_line_file1 << std::endl;
					std::getline(infile1, string_line_file1, '\n');
				}
				else
				{
					outfile << string_line_file2 << std::endl;
					std::getline(infile2, string_line_file2, '\n');
				}
			}
			if (string_line_file1.size() > 0 && string_line_file2.size() == 0)
			{
				outfile << string_line_file1 << std::endl;
				std::getline(infile1, string_line_file1, '\n');
			}
			if (string_line_file2.size() > 0 && string_line_file1.size() == 0)
			{
				outfile << string_line_file2 << std::endl;
				std::getline(infile2, string_line_file2, '\n');
			}
		}
		infile1.close();
		infile2.close();
		outfile.close();
		merged_file_count++;

	}

    std::cout <<file_count<<" files merged to "<<merged_file_count<<" files."<<std::endl;

	if(merged_file_count > 1)
	{
		return mergeSortFiles(path_prefix + "_", merged_file_count);
	}
	else
	{
		return path_prefix +"_" + boost::lexical_cast<std::string>(0)+".tmp";
	}

}

int main(int argc, char* argv[])
{
	char str_input_filepath[512] = {0};//"/home/ayres/Public/ubuntu-share/1.txt";
	char str_output_filepath[512] = {0};//"/home/ayres/Public/ubuntu-share/1.txt";
	char str_tmp_filepath[512] = {0};//"/home/ayres/Public/ubuntu-share/tmp";

	g_max_memmory_size = 100*1000*1000; //asumming we have 512M mem
    g_block_write_size = 5*1000*1000; //generate 5m block to save
    g_max_memory_use = 0.8f; //define max memeory percentage to use


    int max_mem = 512;
    std::cout <<"enter the max mem(MB,default to 512)"<<std::endl;
    std::cin >>max_mem;
	if(max_mem == 0)
	{
		max_mem = 512;
	}
    g_max_memmory_size = max_mem * 1000*1000;

    //
    float memory_use_percent = 0.8f;
    std::cout <<"enter how much percent mem can use [0.1-0.9](default to 0.8f)"<<std::endl;
    std::cin >>memory_use_percent;
    if(memory_use_percent == 0 || memory_use_percent >0.9f || memory_use_percent < 0.1f)
    {
        memory_use_percent = 0.8;
    }
    g_max_memory_use = memory_use_percent;
	g_piece_file_size = (double)g_max_memmory_size * g_max_memory_use;

    do
    {
        memset(str_input_filepath,0,512);
        std::cout << "please input input path, e.g.:/home/ayres/Public/ubuntu-share/1.txt"<<std::endl;
        std::cin >>str_input_filepath;
        if(strlen(str_input_filepath) == 0)
        {
            return -1;
        }

        if(strlen(str_input_filepath) > 0)
        {
           FILE * fp = fopen(str_input_filepath,"r");
           if(fp)
           {
                fclose(fp);
                break;
           }

        }

        std::cout << "file path is not corrcet, create file failed"<<std::endl;

    }while(1);

	do
	{
		memset(str_output_filepath,0,512);
		std::cout << "please input output file path, e.g.:/home/ayres/Public/ubuntu-share/2.txt"<<std::endl;
		std::cin >>str_output_filepath;
		if(strlen(str_output_filepath) == 0)
		{
			return -1;
		}

		if(strlen(str_output_filepath) > 0)
		{
			FILE * fp = fopen(str_output_filepath,"w");
			if(fp)
			{
				fclose(fp);
				break;
			}

		}

		std::cout << "file path is not corrcet, create file failed"<<std::endl;

	}while(1);

	do
	{
		memset(str_tmp_filepath,0,512);
		std::cout << "please input tempfile path, e.g.:/home/ayres/Public/ubuntu-share/tmp/"<<std::endl;
		std::cin >>str_tmp_filepath;
		if(strlen(str_tmp_filepath) == 0)
		{
			return -1;
		}

		if(strlen(str_tmp_filepath) > 0)
		{

			if(boost::filesystem::exists(str_tmp_filepath))
			{
				break;
			}
		}

		std::cout << "file path is not corrcet, create file failed"<<std::endl;

	}while(1);



	//start
    uint64 timestamp_start = getTickTime();


	//step.1 split the file to many piece file < max_mem * mem use percet.

	std::ifstream infile(str_input_filepath,std::ios::in);
	if (!infile)
	{
		std::cout << "open input file fail:"<<str_input_filepath << std::endl;
		return -1;
	}

	int tmp_file_count = 0;
	std::cout <<"spliting files...piece file size:"<<g_piece_file_size<<std::endl;



	std::vector<std::string> str_array;
	std::string text_line;
	uint64 memused = 0;
	int vector_size = 0;
	int vector_index = 0;

	while (std::getline(infile, text_line, '\n'))
	{
		expensiveFunc(text_line);

		memused += text_line.size();
		if(vector_index>=vector_size)
		{
			str_array.push_back(text_line);
			vector_size ++;
			vector_index++;
		}
		else
		{
			str_array[vector_index] = text_line;
			vector_index++;
		}


		if(memused > g_piece_file_size)
		{
			//sort
			sortStringArray(str_array);

			//write to piece file
			std::string  string_tmp_file_path= str_tmp_filepath;
			string_tmp_file_path += "tmp_" + boost::lexical_cast<std::string>(tmp_file_count)+".tmp";
			std::ofstream outfile(string_tmp_file_path.c_str(), std::ios::out);
			if(!outfile)
			{
				std::cout <<"can't create temp file!:"<<string_tmp_file_path<<std::endl;
				return -1;
			}

			for(int i = 0;i<vector_index;i++)
			{
				outfile << str_array[i] << std::endl;
			}
			tmp_file_count ++;
			memused = 0;
			outfile.close();
			vector_index = 0;

			if(tmp_file_count%50 == 0)
			{
                std::cout <<tmp_file_count<<" piece files created."<<std::endl;
			}
		}
	}

	if(vector_index > 0)
	{
		//sort
		sortStringArray(str_array);

		//write to piece file
		std::string  string_tmp_file_path = str_tmp_filepath;
		string_tmp_file_path += "tmp_" + boost::lexical_cast<std::string>(tmp_file_count)+".tmp";
		std::ofstream outfile(string_tmp_file_path.c_str(), std::ios::out);
		if(!outfile)
		{
			std::cout <<"can't create temp file!:"<<string_tmp_file_path<<std::endl;
			return -1;
		}

		for(int i = 0;i<vector_index;i++)
		{
			outfile << str_array[i] << std::endl;
		}

		tmp_file_count ++;
		outfile.close();
		str_array.clear();
	}

	infile.close();

    std::cout <<"merging piece files."<<std::endl;

	//step 2: MERGE-SORT the adjacent 2 piece file e.g. file 1,2,3,4 -> file 1+2 ,3+4 -> file 1+2+3+4
	std::string final_out = mergeSortFiles(std::string(str_tmp_filepath) + "tmp_",tmp_file_count);
	if(final_out.size() > 0)
	{
        std::cout <<"piece files mering complete."<<std::endl;
		boost::system::error_code ec;

		boost::filesystem::rename(final_out,str_output_filepath,ec);
		std::cout <<"file sorted:"<<str_output_filepath<<std::endl;
	}
	else
	{
	 std::cout <<"piece files mering failed."<<std::endl;
	}



    std::cout << "time spend:"<<((double)(getTickTime() - timestamp_start))/1000<<"(s)"<<std::endl;
	return 0;
}

