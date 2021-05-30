#include <fstream>
#include "leveldb/db.h"
#include "db/db_impl.h"
#include "leveldb/filter_policy.h"
#include "leveldb/write_batch.h"



using leveldb::DB;

#ifndef Global_logfile
#define Global_logfile
extern FILE *logfile;
#endif
#ifndef Global_file_spinlock
#define Global_file_spinlock
extern bool file_spinlock;
#endif

#ifndef Global_db
#define Global_db
extern DB *db;
#endif
