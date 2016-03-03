/*
 * UNaviLog.cpp
 *
 *  Created on: 2014-12-18
 *      Author: li.lei
 */

#include "unavi/core/UNaviLog.h"
#include "unavi/core/UNaviHandler.h"
#include "unavi/core/UNaviCycle.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <queue>
#include <list>
#include "unavi/frame/UOMBCommon.h"
#include "unavi/rtmfp/URtmfpCommon.h"
#include <assert.h>

using namespace unavi;
using namespace std;

static const char* level_str[LOG_UNKOWN+1] = {
	"debug",
	"info",
	"notice",
	"warning",
	"error",
	"fatal",
	"unknown"
};

UNaviLoggerConf::UNaviLoggerConf():
	id(-1),
	min_level(LOG_NOTICE),
	cron_size_KB(default_cron_size),
	log_line_max(default_line_max),
	fd(-1),
	cron_time(0),
	croning(false),
	tee_color(TEE_NONE)
{

}

UNaviLogger::UNaviLogger(UNaviLoggerConf& conf_):
	conf(conf_),
	wr_buf(NULL),
	wr_bufsz(0),
	log_file_checked(false),
	pid(-1),
	workerid(-1),
	cronner(*this)
{
	pid = ::getpid();
	if ( UNaviCycle::cycle() )
		workerid = UNaviCycle::worker_id();
	cronner.set_expire(60000000);
}

void UNaviLogger::vlog(UNaviLogLevel level, const char* fmt, va_list vl)
{
	if ( level < conf.min_level)
		return;

	if (log_file_checked==false) {
		log_file_checked = cron();
	}
	if (log_file_checked==false)
		return;

	if (wr_bufsz == 0 ) {
		wr_buf = new char[conf.log_line_max];
		wr_bufsz = conf.log_line_max;
	}
	else if ( wr_bufsz != conf.log_line_max) {
		delete []wr_buf;
		wr_bufsz = conf.log_line_max;
		wr_buf = new char[wr_bufsz];
	}

	struct timeval tv;
	int off;
	::gettimeofday(&tv,NULL);
	struct tm tmobj;
	::localtime_r(&tv.tv_sec, &tmobj);
	off = strftime(wr_buf, wr_bufsz, "%Y-%02m-%02d %02H:%02M:%02S", &tmobj);
	off += snprintf(wr_buf + off, wr_bufsz - off, ".%03d <p%05dw%03d %s>:  ", tv.tv_usec/1000,
		pid, workerid, level_str[level]);
	off += vsnprintf(wr_buf + off,wr_bufsz - off,fmt, vl);

	if (off >= wr_bufsz-1 ) {
		wr_buf[wr_bufsz-2] = '\n';
	}
	else {
		off += snprintf(wr_buf + off, wr_bufsz - off, "\n");
	}

	if ( off >= wr_bufsz ) {
		off = wr_bufsz - 1;
	}

	UNaviScopedRWLock lk(&conf.lk);
	if (conf.fd != -1) {
		assert(off == ::write(conf.fd, wr_buf, off));
	}

	if (conf.tee_color != TEE_NONE) {
		switch(conf.tee_color) {
		case TEE_WHITE:
			std::cout << "\033[37m";
			break;
		case TEE_BLUE:
			std::cout << "\033[34m";
			break;
		case TEE_GREEN:
			std::cout << "\033[32m";
			break;
		case TEE_ORANGE:
			std::cout << "\033[33m";
			break;
		case TEE_RED:
			std::cout << "\033[31m";
			break;
		case TEE_PINK:
			std::cout << "\033[35m";
			break;
		default:
			break;
		}
		std::cout << wr_buf;
		std::cout << "\033[37m";
		std::cout.flush();
	}
	return;
}

UNaviLogger::LogChecker::LogChecker(UNaviLogger& logger):
	UNaviEvent(0),
	target(logger)
{
	UNaviCycle* cycle = UNaviCycle::cycle();
	if ( cycle ) {
		cycle->log_cronner->event_regist(*this);
	}
}

void UNaviLogger::flush()
{
	if (log_file_checked==false) {
		log_file_checked = cron();
	}
	if (log_file_checked==false)
		return;

	UNaviScopedRWLock lk(&conf.lk);
	if (conf.fd != -1)
		::fsync(conf.fd);
	return;
}

void UNaviLogger::LogChecker::process_event()
{
	target.log_file_checked = target.cron();
	set_expire(60000000);
}

static std::string get_curtime_str()
{
	struct tm local_tm;
	time_t epoch;
	::time(&epoch);
	::localtime_r(&epoch,&local_tm);
	char buf[20];
	strftime(buf,sizeof(buf),"%Y-%02m-%02dT%02H:%02M:%02S", &local_tm);
	return buf;
}

static bool check_dir(const char* dir, string& abs_path)
{
	char path[1024];
	memset(path,0x00,sizeof(path));
	std::list<string> path_token;
	if ( dir[0] != '/' ) {
		getcwd(path,sizeof(path));
		if (path[strlen(path)-1] != '/') {
			path[strlen(path)] = '/';
		}
		strcat(path,dir);
	}
	else {
		strcpy(path,dir);
	}

	if (path[strlen(path)-1] != '/') {
		path[strlen(path)] = '/';
	}

	char* p = path;
	char* tb = NULL, *te = NULL;
	while(*p) {
		if (*p == '/' ) {
			if (tb && tb != p) {
				*p = 0;
				if ( strcmp(tb,".") == 0) {
					tb = NULL;
				}
				else if ( strcmp(tb, "..") == 0) {
					tb = NULL;
					path_token.pop_back();
				}
				else {
					path_token.push_back(tb);
					tb = NULL;
				}
			}
		}
		else {
			if ( tb == NULL ) {
				tb = p;
			}
		}
		p++;
	}

	if (path_token.size() == 0) {
		abs_path = "/";
		return true;
	}

	std::list<string>::iterator it;
	abs_path = "";
	for(it = path_token.begin(); it!= path_token.end(); it++) {
		abs_path += "/";
		abs_path += *it;
	}
	abs_path += "/";

	int off = 0;

	do {
		string a = path_token.front();
		path_token.pop_front();
		off += snprintf( path + off, sizeof(path)-off, "/%s", a.c_str());
		if ( off >= sizeof(path) )
			return false;

		struct stat stbuf;
		if ( -1 == stat(path,&stbuf)) {
			if ( mkdir(path, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) == -1)
				return false;
		}
		else if ( !S_ISDIR(stbuf.st_mode)) {
			return false;
		}
	}while(!path_token.empty());

	return true;
}

bool UNaviLogger::cron()
{
	static unsigned int seed = time(NULL);
	bool do_rename = true;
	off_t file_size = -1;
	UNaviScopedRWLock lk(&conf.lk);
	if (conf.fd != -1){
		if (conf.croning)
			return true;

		uint64_t cron_tm = conf.cron_time;
		bool need_cron = false;
		//todo: ¿çÆ½Ì¨
		struct stat stbuf;
		if (-1 ==::fstat(conf.fd, &stbuf) ) {
			need_cron = true;
		}
		else if ( stbuf.st_size >= conf.cron_size_KB*1024) {
			file_size = stbuf.st_size;
			need_cron = true;
		}

		if (conf.cur_log_dir != conf.dir_path) {
			do_rename = false;
			need_cron = true;
		}

		if (need_cron==false)
			return true;
	}
	lk.upgrade();

	conf.croning = true;
	if (conf.fd != -1) {
		::fsync(conf.fd);
		::close(conf.fd);
		conf.fd = -1;
	}

	string checked_abs;
	if ( !check_dir(conf.dir_path.c_str(), checked_abs) )
		return false;

	conf.dir_path = checked_abs;

	{
		ostringstream log_file_nm, cron_file_nm;
		log_file_nm << conf.dir_path << conf.name << ".log";

		if ( file_size == -1 ) {
			struct stat stbuf;
			if ( 0 == stat(log_file_nm.str().c_str(),&stbuf) ) {
				if (S_ISREG(stbuf.st_mode) ) {
					file_size = stbuf.st_size;
				}
			}
		}

		if ( do_rename && file_size >= conf.cron_size_KB*1024 ) {
			cron_file_nm << conf.dir_path << conf.name << "_" << get_curtime_str() <<"."
				<< ::rand_r(&seed)%1000 << ".log";
			::rename(log_file_nm.str().c_str(), cron_file_nm.str().c_str());
		}

		conf.fd = ::open(log_file_nm.str().c_str(),O_CREAT|O_RDWR|O_APPEND,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
		if (conf.fd==-1) {
			conf.croning = false;
			return false;
		}
		conf.cur_log_dir = conf.dir_path;
	}
	return true;
}

UNaviLogger::~UNaviLogger()
{
	if ( wr_buf)
		delete []wr_buf;

	UNaviScopedRWLock lk(&conf.lk,true);
	if (conf.fd != -1) {
		::fsync(conf.fd);
		::close(conf.fd);
		conf.fd = -1;
	}
}

unavi::UNaviRWLock* UNaviLogInfra::p_declare_lk = NULL;
hash_map<std::string, unavi::UNaviLoggerConf::Ref>* UNaviLogInfra::p_declare_config = NULL;
std::vector<unavi::UNaviLoggerConf::Ref>* UNaviLogInfra::p_conf_id_map = NULL;
pthread_key_t UNaviLogInfra::self_key;
pthread_once_t UNaviLogInfra::self_once = PTHREAD_ONCE_INIT;

UNaviLogInfra* UNaviLogInfra::thr_spec_run_init()
{
	pthread_once(&UNaviLogInfra::self_once,UNaviLogInfra::key_once);

	UNaviLogInfra* infra= reinterpret_cast<UNaviLogInfra*>
	(pthread_getspecific(UNaviLogInfra::self_key));
	if ( infra == NULL ) {
		infra = new UNaviLogInfra;
		pthread_setspecific(UNaviLogInfra::self_key, infra);
	}
	return infra;
}

void UNaviLogInfra::thr_spec_run_cleanup()
{
	UNaviLogInfra* infra= reinterpret_cast<UNaviLogInfra*>
		(pthread_getspecific(UNaviLogInfra::self_key));

	delete infra;
	pthread_setspecific(UNaviLogInfra::self_key,NULL);
}

UNaviLogger* UNaviLogInfra::get_logger(int id)
{
	UNaviLogInfra* infra= thr_spec_run_init();

	UNaviScopedRWLock dec_lk(p_declare_lk);
	if (id < 0 ||  id >= *p_log_id_gen)
		return NULL;
	dec_lk.reset();

	UNaviLogger* obj = NULL;
	if ( infra->log_id_map.size() > id ) {
		obj = infra->log_id_map[id].get();
	}

	if (obj == NULL) {
		UNaviLoggerConf* conf = (*p_conf_id_map)[id].get();
		obj = new UNaviLogger(*conf);
		UNaviLogger::Ref ref(*obj);
		infra->log_name_map[conf->name] = ref;
		infra->log_id_map.resize(*p_log_id_gen);
		infra->log_id_map[conf->id] = ref;
	}

	return obj;
}

UNaviLogger* UNaviLogInfra::get_logger(const char* name)
{
	UNaviLogInfra* infra= thr_spec_run_init();

	UNaviLogger* logger = NULL;
	hash_map<std::string, UNaviLogger::Ref>::iterator it = infra->log_name_map.find(name);
	if  (it == infra->log_name_map.end()) {
		UNaviScopedRWLock dec_lk(p_declare_lk);
		hash_map<std::string, UNaviLoggerConf::Ref>::iterator it2 = p_declare_config->find(name);
		if ( it2 == p_declare_config->end())
			return NULL;

		logger = new UNaviLogger(*it2->second.get());
		UNaviLogger::Ref ref(*logger);
		infra->log_name_map[name] = ref;
		infra->log_id_map.resize(*p_log_id_gen);
		infra->log_id_map[it2->second->id] = ref;
	}
	else {
		logger = it->second.get();
	}
	return logger;
}

int* UNaviLogInfra::p_log_id_gen = NULL;

int UNaviLogInfra::declare_logger(const char* name, const char* dir_path,
    UNaviLogLevel min_level,
    int cron_size,int max_line, UNaviTeeColor tee)
{
	static UNaviRWLock fs_declare_lk;
	static hash_map<std::string, UNaviLoggerConf::Ref> fs_declare_config;
	static std::vector<UNaviLoggerConf::Ref> fs_conf_id_map;
	static int fs_log_id_gen = 0;

	p_declare_lk = &fs_declare_lk;
	p_declare_config = &fs_declare_config;
	p_conf_id_map = &fs_conf_id_map;
	p_log_id_gen = &fs_log_id_gen;

	UNaviScopedRWLock dec_scope_lk(p_declare_lk,true);
	hash_map<std::string, UNaviLoggerConf::Ref>::iterator it = p_declare_config->find(name);
	UNaviLoggerConf* conf = NULL;
	if ( it == p_declare_config->end() ) {
		conf = new UNaviLoggerConf;
		conf->name = name;
		conf->id = fs_log_id_gen++;
		UNaviLoggerConf::Ref ref(*conf);
		(*p_declare_config)[string(name)] = ref;
		p_conf_id_map->resize(fs_log_id_gen);
		(*p_conf_id_map)[conf->id] = ref;
	}
	else {
		conf = it->second.get();
	}
	dec_scope_lk.reset();

	UNaviScopedRWLock lk(&conf->lk,true);
	check_dir(dir_path, conf->dir_path);
	conf->min_level = min_level;
	conf->cron_size_KB = cron_size;
	conf->tee_color = tee;
	if (max_line < 512)
		max_line = 512;
	conf->log_line_max = max_line;
	return conf->id;
}

void UNaviLogInfra::declare_logger_tee(int id, UNaviTeeColor tee)
{
	UNaviScopedRWLock lk(p_declare_lk);
	if (p_conf_id_map->size() <= id)
		return;
	(*p_conf_id_map)[id]->tee_color = tee;
}

void UNaviLogInfra::key_once(void)
{
	pthread_key_create(&UNaviLogInfra::self_key,NULL);
}

const char* UNaviLogInfra::universal = NULL;
int UNaviLogInfra::universal_id = -1;
unavi::UNaviLogLevel UNaviLogInfra::universal_min_level = unavi::LOG_NOTICE;

int UNaviLogInfra::sys_id = UNaviLogInfra::declare_logger("unavi_sys",
	DEFAULT_UNAVI_LOG_ROOT,
#ifdef DEBUG
	unavi::LOG_DEBUG,
#else
	unavi::LOG_NOTICE,
#endif
	30720,
	1024,
	unavi::TEE_BLUE);

void UNaviLogCronor::run_init()
{
	UNaviLogInfra::thr_spec_run_init();
}

void UNaviLogCronor::cleanup()
{
	UNaviLogInfra::thr_spec_run_cleanup();
}
