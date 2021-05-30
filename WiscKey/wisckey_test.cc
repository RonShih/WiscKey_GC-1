#include "lab2_common.h"
#include <fstream>
#include <algorithm> 
#include <vector>      
#include <ctime>       
#include <cstdlib>    



#ifndef GLOBAL_h
#define GLOBAL_h
bool compacted = false;
#endif

size_t value_size;
unsigned long GC = 0;
// Author: John Hsu
// Program: WiscKey Key Value Store with GC

typedef struct WiscKey {
  string dir;
  DB * leveldb;
  FILE * logfile;
} WK;
std::string headkey = "head";
std::string tailkey = "tail";
std::string GCkey = "GC";
void logfile_GC(WK * wk,std::string &accvalue,std::string &key,std::string &value){
	int goal = 1*1024*1024;// a chunk of logfile too big will crash
	bool found;
	std::string::size_type sz;
  	unsigned long head,tail;
  	key = headkey;
  	bool foundhead = leveldb_get(wk->leveldb,key,accvalue);
  	if(foundhead){
  		head = stol(accvalue,&sz);
  	}else{
  		head = 0;
  	}
  	key = GCkey;
  	bool foundtail = leveldb_get(wk->leveldb,key,accvalue);
  	if(foundtail){
  		GC = stol(accvalue,&sz);
  	}else{
  		GC = 0;
  	}
  	long offset = tail;//find the last pos in the logfile
  	long length = std::stol ("32",&sz);
  	long size = length;
	long i =  std::stol ("0",&sz);
	while(i<=goal&&tail+length*2<head)
	{
		
		GC++;
		//access value and key from logfile
		//access key
	  	{	
	  		fseek(wk->logfile,SEEK_SET,tail+i);
	  		fread(&accvalue,length,1,wk->logfile);
	  	}
	  	key = accvalue;
	  	i+=sizeof(key);
	  	//access value
	  	{
	  		fseek(wk->logfile,SEEK_SET,tail+i);
	  		fread(&accvalue,length,1,wk->logfile);
	  	}
	  	value = accvalue;
	  	i+=sizeof(value);
	  	//value = std::to_string(GC);
	  	key = GCkey;
	  	found = leveldb_get(wk->leveldb,GCkey,accvalue);
	  	if(found){
	  		
	  		//append to logfile
	  		size = sizeof(GCkey);
		  	{	
		  		fwrite(&GCkey,size,1,wk->logfile);
		  	}
	  		//insert to LSM tree
	  		offset = ftell(wk->logfile);
	  		value = std::to_string(GC);
	  		size = sizeof(value);
			leveldb_set(wk->leveldb,GCkey,value);//write to LSMTree
			{	
				fwrite(&value, size,1,wk->logfile);//寫到vlog裡面
			}
	  		//head tail update
	  		offset = ftell(wk->logfile);
	  		tail += sizeof(key)+sizeof(value);
	  		value = std::to_string(offset);
	  		leveldb_set(wk->leveldb,headkey,value);
	  		value = std::to_string(GC);
	  		leveldb_set(wk->leveldb,GCkey,value);
	  	}else{
	  		//do nothing
	  	}
	  	
	}
}
void do_logfileGC(WK *wk,std::string &accvalue,std::string &key,std::string &value){
	if(compacted){
		logfile_GC(wk,accvalue,key,value);
		compacted = false;
	}
}
static bool wisckey_get(WK * wk, string &key, string &value,string &accvalue)
{	std::string tmpkey = key;
	//compaction
	do_logfileGC(wk,accvalue,key,value);
	key = tmpkey;
	cout << "\n\t\tGet Function\n\n";
	cout << "Key Received: " << key << endl;

	string offsetinfo;
        bool found = leveldb_get(wk->leveldb, key, offsetinfo);
        if (found) {
       		cout << "Offset and Length: " << offsetinfo << endl;//call by address直接修改成可以用來取值的offset以及value size
        }
        else {
       	        cout << "Record:Not Found" << endl;
		return false;
        }
        //這裡做拆解offset資訊
	std::string value_offset;
	std::string value_length;
	std::string s = offsetinfo;
	std::string delimiter = "&&";
	size_t pos = 0;
	std::string token;
	while ((pos = s.find(delimiter)) != std::string::npos) {
    		token = s.substr(0, pos);
		value_offset = token;
    		s.erase(0, pos + delimiter.length());
	}
	value_length = s;
	//這裡做拆解offset資訊
	//cout << "Value Offset: " << value_offset << endl;
	//cout << "Value Length: " << value_length << endl;

  	std::string::size_type sz;
  	long offset = std::stol (value_offset,&sz);
	long length = std::stol (value_length,&sz);
	
	//rewind(wk->logfile);
	//cout << offset << length << endl;
	std::string value_record;
	//cout << ftell(wk->logread) << endl;
	
	fseek(wk->logfile,offset,SEEK_SET);
	fread(&value,length,1,wk->logfile);
	fseek(wk->logfile,offset-32,SEEK_SET);
	fread(&key,length,1,wk->logfile);
	
	//rewind(wk->logfile);
	cout << "Value Key: " <<key<<endl;
	cout << "LogFile Value: " << value << endl;
	return true;
}	

static void wisckey_set(WK * wk, string &key, string &value,string &accvalue)
{
	std::string tmpkey = key;
	std::string tmpvalue = value;
	do_logfileGC(wk,accvalue,key,value);
	key = tmpkey;
	value = tmpvalue;
	long offset = ftell(wk->logfile);
	long size = sizeof(key);

	{	
		fwrite (&key, size,1,wk->logfile);
	}
	
	offset = ftell(wk->logfile);
	size = sizeof(value);
	
	std::string vlog_offset = std::to_string(offset);
	std::string vlog_size = std::to_string(size);
	std::stringstream vlog_value;
	vlog_value << vlog_offset << "&&" << vlog_size;//紀錄偏移量還有size等等就知道要從哪裡取value值(get會用到)
	std::string s = vlog_value.str();
	

	{	
		fwrite (&value, size,1,wk->logfile);//寫到vlog裡面
	}
	
	//更新head
	leveldb_set(wk->leveldb,key,s);//寫到levelDB裡面
	std::string headvalue = std::to_string(offset+size);
	leveldb_set(wk->leveldb,headkey,headvalue);
}

static void wisckey_del(WK * wk, string &key,string &value,string &accvalue)
{	
	std::string tmpkey = key;
	do_logfileGC(wk,accvalue,key,value);
	key = tmpkey;
	//純粹從level DB刪掉資料  gc？？？
 	cout << "Key: " << key << endl; 
	leveldb_del(wk->leveldb,key);
}

static WK * open_wisckey(const string& dirname)
{
	WK * wk = new WK;
	wk->logfile = fopen("logfile","wb+");
	wk->leveldb = open_leveldb(dirname,wk->logfile);
  	wk->dir = dirname;
  	return wk;
}
static void head_tail_insert(WK *wk,std::string &key,std::string &value){
	value = "0";
	key = headkey;
	leveldb_set(wk->leveldb,key,value);
	key = tailkey;
	leveldb_set(wk->leveldb,key,value);
	key = GCkey;
	leveldb_set(wk->leveldb,key,value);
}
static void close_wisckey(WK * wk)
{
	fclose(wk->logfile);
  	delete wk->leveldb;
  	delete wk;
}




/*
static void testing_function2(WK * wk,string &key,string &value,string &accvalue) 
{
// Setting Value and Testing it      
	
	cout << "\n\n\t\tInput Received\n" << endl;
	cout << "Key: " << key << endl;
        cout << "Value: " << value << endl;
        key = "test_v1";
        value = "value_v1";
	wisckey_set(wk,key,value,accvalue);
	key = "test_v2";
        value = "value_v2";
	wisckey_set(wk,key,value,accvalue);

	const bool found = leveldb_get(wk->leveldb,headkey,accvalue);
	if (found) {
		cout << "Wisckey head :"<< accvalue << endl;
	}
	key = "test_v2";
	const bool found2 = wisckey_get(wk,key,value,accvalue);
	if (found2) {
		cout << "Record Matched :"<< value << endl;
	}
// Deleting Value 
	cout << "\n\n\t\tDelete Operation\n" << endl;
	key = "test_v2";
	wisckey_del(wk,key,accvalue);
	key = "test_v3";
	value = "value_v3";
	wisckey_set(wk,key,value,accvalue);
	key = "test_v4";
	value = "value_v4";
	wisckey_set(wk,key,value,accvalue);
	key = "test_v3";
	const bool found3 = wisckey_get(wk,key,value,accvalue);
	if (found3) {
		cout << "Record Matched :"<< value << endl;
	}
	key = "test_v4";
	const bool found4 = wisckey_get(wk,key,value,accvalue);
	if (found4) {
		cout << "Record Matched :"<< value << endl;
	}
	const bool hfound = leveldb_get(wk->leveldb,headkey,accvalue);
	if (hfound) {
		cout << "head now is:"<< accvalue << endl;
	}

}*/

static void testing_compaction(WK * wk,string &key,string &value,string &accvalue) 
{
/* Setting Value and Testing it */     
        for(int i=0;i<200000;i++){
        	//cout << i <<endl;
        	key = "key_v_"+std::to_string(i+1);
        	value = "value_v_"+std::to_string(i+1);
        	wisckey_set(wk,key,value,accvalue);
        }
        key = "key_v_857";
        bool found2 = wisckey_get(wk,key,value,accvalue);
	if (found2) {
		cout << "Record Matched :"<< value << endl;
	}
        //std::string input;
        //scanf("stop!!!!%s",input);
}
static void datainsert(WK *wk,std::string &key,std::string &value,std::string &accvalue){
	char * vbuf = new char[value_size];
  	for (size_t i = 0; i < value_size; i++) {
    		vbuf[i] = rand();
  	}
  	value = string(vbuf, value_size);
  	
 	size_t nfill = 1000000000 / (value_size + 8);
  	clock_t t0 = clock();
  	size_t p1 = nfill / 40;
  	for (size_t j = 0; j < nfill; j++) {
    		key = std::to_string(((size_t)rand())*((size_t)rand()));
    		wisckey_set(wk, key, value,accvalue);
   		if (j >= p1) {
      			clock_t dt = clock() - t0;
      			cout << "progress: " << j+1 << "/" << nfill << " time elapsed: " << dt * 1.0e-6 << endl << std::flush;
      			p1 += (nfill / 40);
    		}    
  	}
  	
  	clock_t dt = clock() - t0;
  	cout << "time elapsed: " << dt * 1.0e-6 << " seconds" << endl;
}
void reopenlogfile(WK * wk){
	fclose(wk->logfile);
  	wk->logfile = fopen("logfile","wb+");
}
int main(int argc, char ** argv)
{
	if (argc < 2) {
    		cout << "Usage: " << argv[0] << " <value-size>" << endl;
    		exit(0);
  	}
  	value_size = std::stoull(argv[1], NULL, 10);
  	if (value_size < 1 || value_size > 100000) {
    		cout << "  <value-size> must be positive and less then 100000" << endl;
   	 	exit(0);
  	}
  	
  	std::string testkey;
  	std::string testvalue;
  	std::string accvalue;

  	WK * wk = open_wisckey("wisckey_test_dir");
  	head_tail_insert(wk,testkey,testvalue);
	

  	
  	if (wk == NULL) {
    		cerr << "Open WiscKey failed!" << endl;
    		exit(1);
  	}
  	
  	/*
  	這裡是測試get set delete 都OK
  	*/

  	testing_compaction(wk,testkey,testvalue,accvalue);
  	/*
  	這裡是測試
  	*/
  	/*Without reopen, it will cause segamentation fault*/
  	reopenlogfile(wk);
  	datainsert(wk,testkey,testvalue,accvalue);
        close_wisckey(wk);
        destroy_leveldb("wisckey_test_dir");       
        remove("logfile");
        exit(0);
}
