/*
 * UNaviLog.h
 *
 *  Created on: 2014-12-18
 *      Author: li.lei
 */

#ifndef UNAVILOG_H_
#define UNAVILOG_H_

#include "unavi/UNaviCommon.h"
#include "unavi/util/UNaviRef.h"
#include "unavi/util/UNaviRWLock.h"
#include "unavi/util/UNaviUtil.h"
#include "unavi/core/UNaviEvent.h"
#include "unavi/core/UNaviHandler.h"
#include <ostream>

UNAVI_NMSP_BEGIN

enum UNaviTeeColor
{
	TEE_NONE,
	TEE_WHITE,
	TEE_RED,
	TEE_PINK,
	TEE_GREEN,
	TEE_BLUE,
	TEE_ORANGE
};

enum UNaviLogLevel
{
	LOG_DEBUG,
	LOG_INFO,
	LOG_NOTICE,
	LOG_WARN,
	LOG_ERROR,
	LOG_FATAL,
	LOG_UNKOWN
};

//todo: 跨平台时，需要不同的日志目录
#define DEFAULT_UNAVI_LOG_ROOT "unavi_log"

class UNaviLoggerConf
{
public:
	typedef UNaviRef<UNaviLoggerConf> Ref;
	friend class UNaviRef<UNaviLoggerConf>;

	static const unsigned int default_cron_size = 4096; //KB
	static const unsigned int default_line_max = 1024;

	UNaviLoggerConf();

	std::string name;
	int id;
	std::string dir_path; //默认在/var/log/unavi/目录下。

	UNaviLogLevel min_level;
	int cron_size_KB;
	int log_line_max;
	UNaviTeeColor tee_color;

	std::string cur_log_dir;
	UNaviRWLock lk;

	int fd; //todo: 跨平台问题。
	uint64_t cron_time;
	bool croning;

private:
	UNaviLoggerConf(const UNaviLoggerConf&);
	UNaviLoggerConf& operator=(const UNaviLoggerConf&);
};

class UNaviLogger
{
public:
	typedef UNaviRef<UNaviLogger> Ref;
	friend class UNaviRef<UNaviLogger>;

	UNaviLogger(UNaviLoggerConf& conf);

	void log(UNaviLogLevel level, const char* fmt,...) {
		va_list vl;
		va_start(vl,fmt);
		vlog(level,fmt,vl);
		va_end(vl);
	}

	void vlog(UNaviLogLevel level, const char* fmt, va_list vl);

	void flush();

	int get_id()const
	{
		return conf.id;
	}

	const char* get_name()const
	{
		return conf.name.c_str();
	}

private:
	~UNaviLogger();

	UNaviLoggerConf& conf;
	char *wr_buf;
	int wr_bufsz;
	bool log_file_checked;
	int pid;
	int workerid;

	struct LogChecker : public UNaviEvent
	{
		UNaviLogger& target;
		LogChecker(UNaviLogger& logger);
		virtual void process_event();
	};
	friend class LogChecker;
	LogChecker cronner;

	bool cron();

	UNaviLogger(const UNaviLogger&);
	UNaviLogger& operator=(const UNaviLogger&);
};

struct UNaviLogCronor : public UNaviHandler
{
	UNaviLogCronor(UNaviCycle* pcycle):
		UNaviHandler(pcycle)
	{}

	virtual void run_init();
	virtual void cleanup();
};

class UNaviLogInfra
{
public:
	UNaviLogInfra():cronor(NULL){};

	static UNaviLogger* get_logger(int id);
	static UNaviLogger* get_logger(const char* name);

	static int declare_logger(const char* name,
		const char* dir_path = DEFAULT_UNAVI_LOG_ROOT,
		UNaviLogLevel min_level = LOG_NOTICE,
		int cron_size = UNaviLoggerConf::default_cron_size,
		int max_line = UNaviLoggerConf::default_line_max,
		UNaviTeeColor tee = TEE_NONE);

	static void declare_logger_tee(int id, UNaviTeeColor tee);

	static void declare_universal_log(const char* nm,
		const char* dir_path = DEFAULT_UNAVI_LOG_ROOT,
		UNaviLogLevel min_level = LOG_NOTICE,
		int cron_size = UNaviLoggerConf::default_cron_size,
		int max_line = UNaviLoggerConf::default_line_max,
		UNaviTeeColor tee = TEE_NONE)
	{
		if ( !nm || strlen(nm) == 0)
			return;
		universal = nm;
		universal_id = declare_logger(nm, dir_path, min_level, cron_size, max_line, tee);
		universal_min_level = min_level;
	}

	static void set_universal_min_level(UNaviLogLevel level)
	{
		universal_min_level = level;
	}

	static const char* universal;
	static int universal_id;
	static UNaviLogLevel universal_min_level;

	static int sys_id;

	static UNaviLogInfra* thr_spec_run_init();
	static void thr_spec_run_cleanup();
private:
	friend class UNaviLogCronor;

	static pthread_once_t self_once;
	static pthread_key_t self_key;
	static void key_once(void);

	static UNaviRWLock *p_declare_lk;
	static hash_map<std::string, UNaviLoggerConf::Ref>* p_declare_config;
	static std::vector<UNaviLoggerConf::Ref>* p_conf_id_map;
	static int *p_log_id_gen;

	hash_map<std::string, UNaviLogger::Ref> log_name_map;
	std::vector<UNaviLogger::Ref> log_id_map;
	UNaviLogCronor* cronor;

	UNaviLogInfra(const UNaviLogInfra&);
	UNaviLogInfra& operator=(const UNaviLogInfra&);
};

UNAVI_NMSP_END

#define ulog(level,name,fmt, ...) do{\
	UNaviLogger* obj = NULL;\
	if ( unavi::UNaviLogInfra::universal )\
		obj = unavi::UNaviLogInfra::get_logger(unavi::UNaviLogInfra::universal_id);\
	else\
		obj = unavi::UNaviLogInfra::get_logger((name));\
	if ( !obj ) {\
		obj = unavi::UNaviLogInfra::get_logger(unavi::UNaviLogInfra::sys_id);\
	}\
	obj->log((level),(fmt),##__VA_ARGS__);\
}	while(0)

#define ulog_v(level,name,fmt, ...) do{\
	UNaviLogger* obj = NULL;\
	if ( unavi::UNaviLogInfra::universal )\
		obj = unavi::UNaviLogInfra::get_logger(unavi::UNaviLogInfra::universal_id);\
	else\
		obj = unavi::UNaviLogInfra::get_logger((name));\
	if ( !obj ) {\
		obj = unavi::UNaviLogInfra::get_logger(unavi::UNaviLogInfra::sys_id);\
	}\
	char xmt_buf[512];\
	int off=0;\
	off = snprintf(xmt_buf,sizeof(xmt_buf), "%s at<%%s:%%d>", (fmt));\
	if (off >=sizeof(xmt_buf)) {\
		std::string xmt_str = (fmt);\
		xmt_str+= " at<%s:%d>";\
		obj->log((level),xmt_str.c_str(), ##__VA_ARGS__,__FILE__,__LINE__);\
	}\
	else {\
		obj->log((level),xmt_buf, ##__VA_ARGS__,__FILE__,__LINE__);\
	}\
}while(0)

#define ulog_vv(level,name,fmt, ...) do{\
	UNaviLogger* obj = NULL;\
	if ( unavi::UNaviLogInfra::universal )\
		obj = unavi::UNaviLogInfra::get_logger(unavi::UNaviLogInfra::universal_id);\
	else\
		obj = unavi::UNaviLogInfra::get_logger((name));\
	if ( !obj ) {\
		obj = unavi::UNaviLogInfra::get_logger(unavi::UNaviLogInfra::sys_id);\
	}\
	char xmt_buf[512];\
	int off=0;\
	off = snprintf(xmt_buf,sizeof(xmt_buf), "%s at<%%s:%%d %%s>", (fmt));\
	if (off >=sizeof(xmt_buf)) {\
		std::string xmt_str = (fmt);\
		xmt_str+= " at<%s:%d %s>";\
		obj->log((level),xmt_str.c_str(), ##__VA_ARGS__,__FILE__,__LINE__,__PRETTY_FUNCTION__);\
	}\
	else {\
		obj->log((level),xmt_buf, ##__VA_ARGS__,__FILE__,__LINE__,__PRETTY_FUNCTION__);\
	}\
}while(0)

#define uexception_throw(e,level,name) do {\
	UNaviLogger* obj = NULL;\
	if ( unavi::UNaviLogInfra::universal )\
		obj = unavi::UNaviLogInfra::get_logger(unavi::UNaviLogInfra::universal_id);\
	else\
		obj = unavi::UNaviLogInfra::get_logger((name));\
	if ( !obj ) {\
		obj = unavi::UNaviLogInfra::get_logger(unavi::UNaviLogInfra::sys_id);\
	}\
	obj->log((level),"%s",e.what());\
	obj->flush();\
	throw (e);\
}while(0)

#define uexception_throw_v(e,level,name) do {\
	UNaviLogger* obj = NULL;\
	if ( unavi::UNaviLogInfra::universal )\
		obj = unavi::UNaviLogInfra::get_logger(unavi::UNaviLogInfra::universal_id);\
	else\
		obj = unavi::UNaviLogInfra::get_logger((name));\
	if ( !obj ) {\
		obj = unavi::UNaviLogInfra::get_logger(unavi::UNaviLogInfra::sys_id);\
	}\
	obj->log(level,"%s at<%s:%d %s>",e.what(),__FILE__,__LINE__,__FUNCTION__);\
	obj->flush();\
	throw (e);\
}while(0)

#define uexception_throw_vv(e,level,name) do {\
	UNaviLogger* obj = NULL;\
	if ( unavi::UNaviLogInfra::universal )\
		obj = unavi::UNaviLogInfra::get_logger(unavi::UNaviLogInfra::universal_id);\
	else\
		obj = unavi::UNaviLogInfra::get_logger(name);\
	if ( !obj ) {\
		obj = unavi::UNaviLogInfra::get_logger(unavi::UNaviLogInfra::sys_id);\
	}\
	obj->log(level,"%s at<%s:%d %s>",e.what(),__FILE__,__LINE__,__PRETTY_FUNCTION__);\
	obj->flush();\
	throw (e);\
}while(0)

#define udebug_log(name,fmt,...) ulog_v(unavi::LOG_DEBUG,name,fmt,##__VA_ARGS__)
#define udebug_log_vv(name,fmt,...) ulog_vv(unavi::LOG_DEBUG,name,fmt,##__VA_ARGS__)

#define uinfo_log(name,fmt,...) ulog(unavi::LOG_INFO,name,fmt,##__VA_ARGS__)
#define uinfo_log_v(name,fmt,...) ulog_v(unavi::LOG_INFO,name,fmt,##__VA_ARGS__)
#define uinfo_log_vv(name,fmt,...) ulog_vv(unavi::LOG_INFO,name,fmt,##__VA_ARGS__)

#define unotice_log(name,fmt,...) ulog(unavi::LOG_NOTICE,name,fmt,##__VA_ARGS__)
#define unotice_log_v(name,fmt,...) ulog_v(unavi::LOG_NOTICE,name,fmt,##__VA_ARGS__)
#define unotice_log_vv(name,fmt,...) ulog_vv(unavi::LOG_NOTICE,name,fmt,##__VA_ARGS__)

#define uwarn_log(name,fmt,...) ulog(unavi::LOG_WARN,name,fmt,##__VA_ARGS__)
#define uwarn_log_v(name,fmt,...) ulog_v(unavi::LOG_WARN,name,fmt,##__VA_ARGS__)
#define uwarn_log_vv(name,fmt,...) ulog_vv(unavi::LOG_WARN,name,fmt,##__VA_ARGS__)

#define uerror_log(name,fmt,...) ulog(unavi::LOG_ERROR,name,fmt,##__VA_ARGS__)
#define uerror_log_v(name,fmt,...) ulog_v(unavi::LOG_ERROR,name,fmt,##__VA_ARGS__)
#define uerror_log_vv(name,fmt,...) ulog_vv(unavi::LOG_ERROR,name,fmt,##__VA_ARGS__)

#define ufatal_log(name,fmt,...) ulog(unavi::LOG_FATAL,name,fmt,##__VA_ARGS__)
#define ufatal_log_v(name,fmt,...) ulog_v(unavi::LOG_FATAL,name,fmt,##__VA_ARGS__)
#define ufatal_logn_vv(name,fmt,...) ulog_vv(unavi::LOG_FATAL,name,fmt,##__VA_ARGS__)
#endif /* UNAVILOG_H_ */
