// StringGenerator.cpp : Defines the entry point for the console application.
//

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
int g_max_queue_size = 10; //share how much size of queue can be used
float  g_max_memory_use = 0.6f; //define how much percent system memory we can use default to 60%
static bool s_if_thread_finshed = false;
boost::mutex g_mutex_sync_log; //for logging

typedef std::list<std::string> StringList;
typedef boost::shared_ptr<StringList> SharedPtrStringList;

std::deque<SharedPtrStringList> g_sptr_msgque_for_writing;
boost::mutex m_mutex_sync_msgque_for_writing;

#if 0
void Log(const char* str,...)
{
	char sztmp[4000]={0};
	int iReturn ;
	boost::mutex::scoped_lock lock(g_mutex_sync_log);

	va_list pArgs ;

	va_start (pArgs, str) ;

	iReturn = vsnprintf (sztmp,40000-2, str, pArgs);

	va_end (pArgs) ;

	std::cout <<sztmp;
}
#endif
void PostRequestToWrite(SharedPtrStringList sptr_string_list)
{
	m_mutex_sync_msgque_for_writing.lock();
	//test if memory is up to limit
    if(g_sptr_msgque_for_writing.size() >= g_max_queue_size)
    {
        m_mutex_sync_msgque_for_writing.unlock();
        //wait memory freed
        //we should able to avoid this contidion.
        // if memory is up to limit  we should reduce the thread count to do this
        //this part is just for safe
        while(1)
        {
            m_mutex_sync_msgque_for_writing.lock();
           if(g_sptr_msgque_for_writing.size() < g_max_queue_size)
            {
                break;
            }

            m_mutex_sync_msgque_for_writing.unlock();
            boost::this_thread::sleep(boost::posix_time::milliseconds(10));
        }

    }

	g_sptr_msgque_for_writing.push_back(sptr_string_list);
	m_mutex_sync_msgque_for_writing.unlock();
}

uint64 getTickTime()
{
    struct timespec time1 = {0, 0};
    clock_gettime(CLOCK_REALTIME, &time1);
    uint64 timestamp = ((long long)time1.tv_sec*1000000000LL + (long long)time1.tv_nsec)/1000;
    return timestamp/1000;
}

void StringGeneratorThread(int string_min_len, int string_max_len,uint64 max_size)
{
	try
	{
		SharedPtrStringList sptr_string_list;
		//compute random string length base;
		int random_string_range = string_max_len - string_min_len;

		uint64 str_size_remain = max_size;
		uint64 str_len_buffered = 0;


		char* ptr_tmp_str = new char[string_max_len+1];
		sptr_string_list.reset(new StringList());

		while (0 < str_size_remain)
		{
			//generate a random string size
			unsigned int str_len = string_min_len;
			if(random_string_range >0)
			{
				str_len = string_min_len + rand() % random_string_range;
			}

			if(str_len > str_size_remain)
			{
				str_len = str_size_remain;
				str_size_remain = 0;
			}
			else
			{
				str_size_remain -= str_len + 2; //including /r/n
			}

			str_len_buffered += str_len + 2;// including /r/n


			memset(ptr_tmp_str,0,string_max_len);


			for(unsigned int i = 0;i < str_len; i ++)
			{
				//generate alpha chars 0x41-0x5a A-Z 0X61-0X7A a-z

				int alpha = rand()%52;
				if(alpha > 25)
				{
					ptr_tmp_str[i] = 0x61 + alpha -26; //a-z
				}
				else
				{
					ptr_tmp_str[i] = 0x41 + alpha; //A-Z
				}
			}

			sptr_string_list->push_back(ptr_tmp_str);

			if(str_len_buffered > g_block_write_size)
			{
                PostRequestToWrite(sptr_string_list);
				str_len_buffered = 0;
				sptr_string_list.reset(new StringList());
			}
		}
		//sending the remain string list
		PostRequestToWrite(sptr_string_list);
		delete[] ptr_tmp_str;
		return;
	}
	catch (...)
	{

		return;
	}


}


void WriteStringThread(char * file_path,uint64 max_size)
{
	bool has_started = false;
	FILE * fp = fopen(file_path,"w+");
	if(fp == NULL)
	{
		return;
	}
	uint64 size_writed = 0;
	while(true)
	{
		try
		{

			SharedPtrStringList sptr_string_list;
			m_mutex_sync_msgque_for_writing.lock();
			if(!g_sptr_msgque_for_writing.empty())
			{

				sptr_string_list = g_sptr_msgque_for_writing.front();
				g_sptr_msgque_for_writing.pop_front();
				m_mutex_sync_msgque_for_writing.unlock();
			}
			else
			{


				m_mutex_sync_msgque_for_writing.unlock();

				if(has_started == true && s_if_thread_finshed == true)
				{
					break;
				}
				else
				{
					//go sleep 10 ms
					boost::this_thread::sleep(boost::posix_time::milliseconds(10));
					continue;
				}
			}

			StringList::iterator it;

			for(it=sptr_string_list->begin();it!=sptr_string_list->end();it++)
			{
				fwrite((*it).c_str(),1,(*it).size(),fp);
				fputs("\r\n",fp);
				size_writed += (*it).size() + 2;//including /r/n
			}

			fflush(fp);
			std::cout << (((double)size_writed)/(double)max_size)*100<<"% writed"<<std::endl;
			has_started = true;
			if(size_writed>= max_size)
			{
				fclose(fp);
				return;
			}
		}
		catch (...)
		{

			break;
		}
	}

	fclose(fp);
	return;

}

int main(int argc, char* argv[])
{
	char str_path[512] = {0};//"/home/ayres/Public/ubuntu-share/1.txt";
	uint64 max_size = 200*1000*1000;
	g_max_memmory_size = 100*1000*1000; //asumming we have 512M mem
    g_block_write_size = 5*1000*1000; //generate 5m block to save
    g_max_memory_use = 0.8f; //define max memeory percentage to use

	int min_len = 10;
	int max_len = 100;

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
	g_max_queue_size = ((double)g_max_memmory_size * g_max_memory_use)/ g_block_write_size;

    int thread_count = 2;
    std::cout <<"enter the process thread num(usually set to (core num)*2 default to 2)"<<std::endl;
    std::cin >>thread_count;
    if(thread_count == 0)
    {
        thread_count = 512;
    }

    int process_thread_count = thread_count; //core number *2



    std::cout <<"min len of string(default to 10)"<<std::endl;
    std::cin >>min_len;
    if(min_len == 0)
    {
        min_len = 10;
    }

    std::cout <<"max len of string(must >= min len,default to 100)"<<std::endl;
    std::cin >>max_len;
    if(max_len == 0 || min_len >= max_len)
    {
		std::cout <<"max len is not correct set to same value with min_len"<<std::endl;
        max_len = min_len;
    }

    int max_size_mb = 0;
    std::cout <<"total lens of string(MB,default to 200)"<<std::endl;
    std::cin >>max_size_mb;
    if(max_size_mb == 0)
    {
        max_size_mb = 200;
    }

    max_size = max_size_mb*1000*1000;


    do
    {
        memset(str_path,0,512);
        std::cout << "please input outputfile path, e.g.:/home/ayres/Public/ubuntu-share/1.txt"<<std::endl;
        std::cin >>str_path;
        if(strlen(str_path) == 0)
        {
            return -1;
        }

        if(strlen(str_path) > 0)
        {
           FILE * fp = fopen(str_path,"w+");
           if(fp)
           {
                fclose(fp);
                break;
           }

        }

        std::cout << "file path is not corrcet, create file failed"<<std::endl;

    }while(1);

	srand(time(NULL));


    std::vector<boost::shared_ptr<boost::thread> > thread_pool;

    uint64 timestamp_start = getTickTime();

    //create computing random string thread
    for(int i = 0; i < process_thread_count ;i++ )
    {
        boost::shared_ptr<boost::thread> sptr_thread;
        sptr_thread.reset(new boost::thread(boost::bind(&StringGeneratorThread, min_len, max_len, max_size/process_thread_count)));
        thread_pool.push_back(sptr_thread);
    }

	boost::thread th2(boost::bind(&WriteStringThread, str_path,max_size));

    for(int i = 0; i < process_thread_count ;i++ )
    {
        thread_pool[i]->join();
    }

	s_if_thread_finshed = true;
	th2.join();

	uint64 timestamp_end = getTickTime();

    std::cout << "time spend:"<<((double)(timestamp_end - timestamp_start))/1000<<"(s)"<<std::endl;
	return 0;
}

