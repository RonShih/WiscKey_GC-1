#include<iostream>
#include <map>
#include <vector>
#include "Flash/typedefine.h"
#include "Flash/main.h"
#include "Flash/flash.h"
#include "Flash/FTL.h"
#include <mutex>
#ifndef GLOBAL_H
#define GLOBAL_H

using leveldb::DB;
using namespace std;
extern bool compacted;
typedef struct Operation{
	AccessType_t AccessType;
	flash_size_t StartCluster;
	flash_size_t Length;
};
extern vector<Operation> Operations;
extern std::mutex Table_mutex;
extern std::mutex FTL_mutex;
#endif
