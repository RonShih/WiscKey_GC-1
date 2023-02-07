#pragma once
#include <assert.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <string>
#include <ctime>
#include <algorithm>
#include <cstdlib>
#include <boost/algorithm/string.hpp>
#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "leveldb/write_batch.h"
#include "global.h"


using std::string;
using std::vector;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::stringstream;
using leveldb::ReadOptions;
using leveldb::Options;
using leveldb::Status;
using leveldb::WriteBatch;
using leveldb::WriteOptions;
using leveldb::DB;

/*MODIFIED
typedef struct valueinfo{
	bool hot;
	unsigned long version;
	string value;
};
*/

typedef struct valueinfo{
  string key;
  string value;//convert the address into string type
	bool hot;
	unsigned long version;
  size_t size;
};

//APPENDED
typedef struct VTable_entry{
  unsigned long latest_ver;
  //... record some metadata
  vector<valueinfo> val_vec;
};

//static map<string,vector<valueinfo>> VTable;//MODIFIED
static map<string, VTable_entry> VTable;//MODIFIED
std::mutex VTable_mutex;

/*Invoked by wisckey_set.cc*/
static void leveldb_set(DB * db, string &key, string &value)
{
  VTable_mutex.lock();

  valueinfo valinfo;//for each entry of VTable
  valinfo.key = key;
  valinfo.hot = false;
  valinfo.version = 1;
  valinfo.value = value;//address of value: "offset&&length"
  valinfo.size = sizeof(value);
  size_t idx = 0; //record the index of the vector of this entry

  if(VTable.find(key)==VTable.end()){
    //VTable[key].push_back(valinfo);//first key insertion
    VTable[key].val_vec.push_back(valinfo);
    VTable[key].latest_ver = valinfo.version;//initialize latest_ver
  }else{
    valinfo.version = VTable[key].latest_ver + 1;//the version of incoming valinfo increases by 1
    VTable[key].latest_ver = valinfo.version;//update latest_ver
    idx = VTable[key].val_vec.size();
    VTable[key].val_vec.push_back(valinfo);
  }

  VTable_mutex.unlock();

  WriteBatch wb;
  Status s;
  WriteOptions wopt;

  valueinfo *value_addr = &VTable[key].val_vec[idx];

  ostringstream addr_to_str;//convert the address into string
  addr_to_str << value_addr;
  string value_addr_str = addr_to_str.str();

  cout << "Put key : " << key << ", and value: " << value_addr_str << " into DB." << endl;
  s = db->Put(wopt, key, value_addr_str);//put into db

  assert(s.ok());
}

/*Invoked by wisckey_test.cc*/
static bool leveldb_get(DB * db, string &key, string &value)
{
  assert(lldb);
  ReadOptions ropt;
  Status s = db->Get(ropt, key, &value);

  assert(s.ok());
  if (s.IsNotFound()) {
    return false;
  } else {
    return true;
  }
}

/*Invoked by leveldb_test.cc*/
static void leveldb_vset(DB * db, string key, string value)
{
    WriteBatch wb;
    Status s;
    WriteOptions wopt;
    //wb.Put(key, value);
    s = db->Put(wopt, key,value);
    assert(s.ok());
    
    VTable_mutex.lock();


    valueinfo valinfo;
    valinfo.hot = false;
    valinfo.version = 1;
    valinfo.value = value;
    vector<valueinfo> temp = VTable[key].val_vec;
    if(VTable.find(key)==VTable.end()){
      //first key insertion
      temp.push_back(valinfo);
    }else{
      valinfo.version = temp.size()+1;
      temp.push_back(valinfo);
    }
    VTable[key].val_vec = temp;
    VTable_mutex.unlock();
}

/*Invoked by leveldb_test.cc*/
static bool leveldb_vget(DB * db, string key, string &value)
{
  assert(lldb);
  ReadOptions ropt;
  Status s = db->Get(ropt, key, &value);
  assert(s.ok());
  if (s.IsNotFound()) {
    return false;
  } else {
    return true;
  }
}

static void leveldb_del(DB * db, string &key)
{
  WriteOptions wopt;
  Status s;
  s = db->Delete(wopt, key);
  assert(s.ok());
  VTable_mutex.lock();
  if(VTable.find(key)==VTable.end()){
  	//do nothing
  }else{
  	//del all version
  	VTable.erase(VTable.find(key));
  }
  VTable_mutex.unlock();
}

  static void
destroy_leveldb(const string &dirname)
{
  Options options;
  leveldb::DestroyDB(dirname, options);
}

  static DB *
open_leveldb(const string &dirname)
{
  Options options;
  options.create_if_missing = true;
  options.filter_policy = leveldb::NewBloomFilterPolicy(10);
  options.write_buffer_size = 1u << 21;
  destroy_leveldb(dirname);
  DB * db = NULL;
  Status s = DB::Open(options, dirname, &db);
  return db;
}
  static DB *
open_leveldb(const string &dirname,FILE *fl)
{
  Options options;
  options.create_if_missing = true;
  options.filter_policy = leveldb::NewBloomFilterPolicy(10);
  options.write_buffer_size = 1u << 21;
  destroy_leveldb(dirname);
  DB * db = NULL;
  Status s = DB::Open(options, dirname, &db,fl);
  return db;
}

/*MODIFIED

//V1
static void leveldb_set(DB * db, string &key, string &value)
{
  VTable_mutex.lock();

  valueinfo valinfo;//for each entry of VTable
  valinfo.key = key;
  valinfo.hot = false;
	valinfo.version = 1;
	valinfo.value = value;//address of value
  valinfo.size = sizeof(value);
  size_t idx = 0; //record the index of the vector of this entry

  if(VTable.find(key)==VTable.end()){
 	  VTable[key].push_back(valinfo);//first key insertion
  }else{
  	valinfo.version = VTable[key].size()+1;//???
    idx = VTable[key].size();
  	VTable[key].push_back(valinfo);
  }

  VTable_mutex.unlock();

  WriteBatch wb;
  Status s;
  WriteOptions wopt;

  valueinfo *table_address = &VTable[key][idx];

  ostringstream addr_to_str;//convert the address into string
  addr_to_str << table_address;
  string table_address_str = addr_to_str.str();

  s = db->Put(wopt, key, table_address_str);//put into db
  assert(s.ok());
}

//Original one 
static void leveldb_set(DB * db, string &key, string &value)
{
  WriteBatch wb;
  Status s;
  WriteOptions wopt;
  //wb.Put(key, value);
  s = db->Put(wopt, key,value);
  assert(s.ok());
  
  VTable_mutex.lock();
  valueinfo valinfo;
  	valinfo.hot = false;
	valinfo.version = 1;
	valinfo.value = value;
  vector<valueinfo> temp = VTable[key];
  if(VTable.find(key)==VTable.end()){
  	//first key insertion
 	temp.push_back(valinfo);
  }else{
  	valinfo.version = temp.size()+1;
  	temp.push_back(valinfo);
  }
  VTable[key] = temp;
  VTable_mutex.unlock();
}
*/