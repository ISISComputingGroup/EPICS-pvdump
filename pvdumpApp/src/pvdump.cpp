///
/// @file pvdump.cpp
/// @author Freddie Akeroyd, STFC ISIS Facility
///
/// Dump all PV's in the IOC to a MySQL database file
///
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <exception>
#include <stdexcept>
#include <iostream>
#include <map>
#include <list>
#include <string>
#include <time.h>
#include <sstream>
#include <fstream>

#include "epicsStdlib.h"
#include "epicsString.h"
#include "dbDefs.h"
#include "epicsMutex.h"
#include "epicsGuard.h"
#include "dbBase.h"
#include "dbStaticLib.h"
#include "dbFldTypes.h"
#include "dbCommon.h"
#include "dbAccessDefs.h"
#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <iocsh.h>
#include <errlog.h>
#include "macLib.h"
#include "epicsExit.h"

#include "utilities.h"

// mysql
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/warning.h>
#include <cppconn/metadata.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/resultset_metadata.h>
#include <cppconn/statement.h>
#include "mysql_driver.h"
#include "mysql_connection.h"

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <epicsExport.h>

#include "pvdump.h"

static int get_pid()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static std::string get_path()
{
#ifdef _WIN32
    char buffer[MAX_PATH];
	buffer[0] = '\0';
    GetModuleFileName(NULL,buffer,sizeof(buffer));
#else
    char buffer[256];
    int n = readlink("/proc/self/exe", buffer, sizeof(buffer)-1);
	if (n >= 0)
	{
	   buffer[n] = '\0';
	}
	else
	{
	    strcpy(buffer,"<unknown>");
	}
#endif
    return std::string(buffer);
}

struct PVInfo
{
    std::string record_type;
    std::string record_desc;
    std::map<std::string,std::string> info_fields;
	PVInfo(const std::string& rt, const std::string& rd, const std::map<std::string,std::string>& inf) : 
        record_type(rt), record_desc(rd), info_fields(inf) { }
	PVInfo(const std::string& rt, const std::string& rd) : 
        record_type(rt), record_desc(rd) { }
	PVInfo() { }
};

static epicsMutex pv_map_mutex;
static std::map<std::string,PVInfo> pv_map;
static std::list<std::string> environ_list;

// based on iocsh dbl command from epics_base/src/db/dbTest.c 
// return an std map, currently key is pv and value is recordType (if that is defined)
static void dump_pvs(const char *precordTypename, const char *fields, std::map<std::string,PVInfo>& pvs)
{
    DBENTRY dbentry;
    DBENTRY *pdbentry=&dbentry;
    long status, status2;
    int nfields = 0;
    int ifield;
    char *fieldnames = 0;
    char **papfields = 0;
    pvs.clear();

    if (!pdbbase) {
        throw std::runtime_error("No database loaded\n");
    }
    if (precordTypename &&
        ((*precordTypename == '\0') || !strcmp(precordTypename,"*")))
        precordTypename = NULL;
    if (fields && (*fields == '\0'))
        fields = NULL;
    if (fields) {
        char *pnext;

        fieldnames = epicsStrDup(fields);
        nfields = 1;
        pnext = fieldnames;
        while (*pnext && (pnext = strchr(pnext,' '))) {
            nfields++;
            while (*pnext == ' ') pnext++;
        }
        papfields = static_cast<char**>(dbCalloc(nfields,sizeof(char *)));
        pnext = fieldnames;
        for (ifield = 0; ifield < nfields; ifield++) {
            papfields[ifield] = pnext;
            if (ifield < nfields - 1) {
                pnext = strchr(pnext, ' ');
                *pnext++ = 0;
                while (*pnext == ' ') pnext++;
            }
        }
    }
    dbInitEntry(pdbbase, pdbentry);
    if (!precordTypename)
        status = dbFirstRecordType(pdbentry);
    else
        status = dbFindRecordType(pdbentry,precordTypename);
    if (status) {
        printf("No record type\n");
    }
	std::map<std::string,std::string> info_fields;
    while (!status) {
        status = dbFirstRecord(pdbentry);
        while (!status) {
            char *pvalue = NULL;
            for (ifield = 0; ifield < nfields; ifield++) {
                status = dbFindField(pdbentry, papfields[ifield]);
                if (status) {
                    if (!strcmp(papfields[ifield], "recordType")) {
                        pvalue = dbGetRecordTypeName(pdbentry);
                    } else {
                        continue;
                    }
                } else {
                    pvalue = dbGetString(pdbentry);
                }
            }
            const char* recordType = dbGetRecordTypeName(pdbentry);
            const char* recordDesc = "";
            if (dbFindField(pdbentry, "DESC") == 0)
            {
                recordDesc = dbGetString(pdbentry);
            }
			// info fields
			info_fields.clear();
			status2 = dbFirstInfo(pdbentry);
			while(!status2)
			{
			    const char* info_name = dbGetInfoName(pdbentry);
				if (info_name != NULL)
				{
					const char* info_value = dbGetInfoString(pdbentry);
					info_fields[info_name] = (info_value != NULL ? info_value : "<error>");
				}
				else
				{
					printf("dbFirst/NextInfo() OK, but dbGetInfoName() returns NULL\n");
				}
			    status2 = dbNextInfo(pdbentry);
			}
			pvs[dbGetRecordName(pdbentry)] = PVInfo(recordType, recordDesc, info_fields);
            status = dbNextRecord(pdbentry);
        }
        if (precordTypename) break;
        status = dbNextRecordType(pdbentry);
    }
    if (nfields > 0) {
        free((void *)papfields);
        free((void *)fieldnames);
    }
    dbFinishEntry(pdbentry);
}

static void pvdumpOnExit(void*);

static std::string ioc_name, db_name;    
static sql::Driver* mysql_driver = NULL;

static const int MAX_MACRO_VAL_LENGTH = 100; // should agree with length of macroval in iocenv MySQL table (iocdb_mysql_schema.txt)

struct MysqlThreadArgs
{
    const std::map<std::string,PVInfo>& pvm;
    const std::list<std::string>& evl;
    sql::Connection* con;
    MysqlThreadArgs(const std::map<std::string,PVInfo>& pvm_,
                    const std::list<std::string>& evl_,
                    sql::Connection* con_) : pvm(pvm_), evl(evl_), con(con_) { }
    ~MysqlThreadArgs() { delete con; }
};

static void dumpMysqlThread(void* arg)
{
	unsigned long npv = 0, ninfo = 0, nmacro = 0;
	const char* mysqlHost = macEnvExpand("$(MYSQLHOST=localhost)");
    MysqlThreadArgs* marg = (MysqlThreadArgs*)arg;
    sql::Connection* con = marg->con;
#ifndef PVDUMP_DUMMY
	try 
	{
        const clock_t begin_time = clock();
        // use DELETE and INSERT on pvs table as we may have the same pv name from a different IOC e.g. CAENSIM and CAEN
		std::auto_ptr< sql::PreparedStatement > pvs_dstmt(con->prepareStatement("DELETE FROM pvs WHERE pvname=?"));
        for(std::map<std::string,PVInfo>::const_iterator it = pv_map.begin(); it != pv_map.end(); ++it)
        {
            pvs_dstmt->setString(1, it->first);
			pvs_dstmt->executeUpdate();
        }
		con->commit();
        
		std::auto_ptr< sql::PreparedStatement > pvs_stmt(con->prepareStatement("INSERT INTO pvs (pvname, record_type, record_desc, iocname) VALUES (?,?,?,?)"));
		std::auto_ptr< sql::PreparedStatement > pvinfo_stmt(con->prepareStatement("INSERT INTO pvinfo (pvname, infoname, value) VALUES (?,?,?)"));
        for(std::map<std::string,PVInfo>::const_iterator it = pv_map.begin(); it != pv_map.end(); ++it)
        {
			++npv;
            pvs_stmt->setString(1, it->first);
            pvs_stmt->setString(2, it->second.record_type);
            pvs_stmt->setString(3, it->second.record_desc);
            pvs_stmt->setString(4, ioc_name);
			pvs_stmt->executeUpdate();
			const std::map<std::string,std::string>& imap = it->second.info_fields;
            for(std::map<std::string,std::string>::const_iterator itinf = imap.begin(); itinf != imap.end(); ++itinf)
			{
				++ninfo;
			    pvinfo_stmt->setString(1, it->first);
			    pvinfo_stmt->setString(2, itinf->first);
			    pvinfo_stmt->setString(3, itinf->second);
				pvinfo_stmt->executeUpdate();
			}
        }
		con->commit();

		std::auto_ptr< sql::PreparedStatement > iocenv_stmt(con->prepareStatement("INSERT INTO iocenv (iocname, macroname, macroval) VALUES (?,?,?)"));
		iocenv_stmt->setString(1, ioc_name);
        for(const std::string& s : environ_list)
        {
			size_t pos = s.find('=');
            if (pos != std::string::npos)
            {
				if ( (s.size() - pos) < MAX_MACRO_VAL_LENGTH )  // ignore things with long values like PATH
				{
		            iocenv_stmt->setString(2, s.substr(0, pos).c_str()); // name
		            iocenv_stmt->setString(3, s.substr(pos + 1).c_str()); // value
			        iocenv_stmt->executeUpdate();
				    ++nmacro;
				}
			}
		}
		con->commit();

        std::cout << "pvdump: MySQL write of " << npv << " PVs with " << ninfo << " info entries, plus " << nmacro << " macros took " << float( clock () - begin_time ) /  CLOCKS_PER_SEC << " seconds" << std::endl;
        delete marg;
    }
	catch (sql::SQLException &e) 
	{
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: %s (MySQL error code: %d, SQLState: %s)\n", e.what(), e.getErrorCode(), e.getSQLStateCStr());
	} 
	catch (std::runtime_error &e)
	{
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: %s\n", e.what());
	}
    catch(...)
    {
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: FAILED TRYING TO WRITE TO THE ISIS PV DB\n");
    }
#endif /* PVDUMP_DUMMY */
}


static int dumpMysql(const std::map<std::string,PVInfo>& pv_map, int pid, const std::string& exepath)
{
	unsigned long npv = 0, ninfo = 0, nmacro = 0;
	const char* mysqlHost = macEnvExpand("$(MYSQLHOST=localhost)");
#ifndef PVDUMP_DUMMY
	try 
	{
        const clock_t begin_time = clock();
        if (mysql_driver == NULL)
        {
	        mysql_driver = sql::mysql::get_driver_instance();
        }
	    sql::Connection* con = mysql_driver->connect(mysqlHost, "iocdb", "$iocdb");
        // the ORDER BY is to make deletes happen in a consistent primary key order, and so try and avoid deadlocks
        // but it may not be completely right. Additional indexes have also been added to database tables.
	    con->setAutoCommit(0); // we will create transactions ourselves via explicit calls to con->commit()
	    con->setSchema("iocdb");
        
        environ_list.clear();
        for (char** sp = environ ; (sp != NULL) && (*sp != NULL) ; ++sp)
		{
		    environ_list.push_back(*sp); // name=value string
        }
        
	    std::auto_ptr< sql::Statement > stmt(con->createStatement());
		stmt->execute(std::string("DELETE FROM iocenv WHERE iocname='") + ioc_name + "' ORDER BY iocname,macroname");
		std::ostringstream sql;
		sql << "DELETE FROM iocrt WHERE iocname='" << ioc_name << "' OR pid=" << pid << " ORDER BY iocname"; // remove any old record from iocrt with our current pid or name
		stmt->execute(sql.str());
		stmt->execute(std::string("DELETE FROM pvs WHERE iocname='") + ioc_name + "' ORDER BY pvname"); // remove our PVS from last time, this will also delete records from pvinfo due to foreign key cascade action
		con->commit();
		
		std::auto_ptr< sql::PreparedStatement > iocrt_stmt(con->prepareStatement("INSERT INTO iocrt (iocname, pid, start_time, stop_time, running, exe_path) VALUES (?,?,NOW(),'1970-01-01 00:00:01',?,?)"));
		iocrt_stmt->setString(1,ioc_name);
		iocrt_stmt->setInt(2,pid);
		iocrt_stmt->setInt(3,1);
		iocrt_stmt->setString(4,exepath);
		iocrt_stmt->executeUpdate();
		con->commit();
        MysqlThreadArgs* margs = new MysqlThreadArgs(pv_map, environ_list, con);
        epicsThreadCreate("pvdump", epicsThreadPriorityMedium, epicsThreadStackMedium, 
                           dumpMysqlThread, margs);
        std::cout << "pvdump: MySQL setup took " << float( clock () - begin_time ) /  CLOCKS_PER_SEC << " seconds" << std::endl;
    }
	catch (sql::SQLException &e) 
	{
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: %s (MySQL error code: %d, SQLState: %s)\n", e.what(), e.getErrorCode(), e.getSQLStateCStr());
        return -1;
	} 
	catch (std::runtime_error &e)
	{
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: %s\n", e.what());
        return -1;
	}
    catch(...)
    {
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: FAILED TRYING TO WRITE TO THE ISIS PV DB\n");
        return -1;
    }
#endif /* PVDUMP_DUMMY */
	return 0;
}

static int pvdump(const char *dbName, const char *iocName)
{
    static int first_call = 1;
    int pid = get_pid();
    std::string exepath = get_path();
        
    time_t currtime;
    time(&currtime);
	if (NULL != iocName)
	{
	    ioc_name = iocName;
	}
	else
	{
	    ioc_name = getIOCName();
	}
	printf("pvdump: ioc name is \"%s\" pid %d\n", ioc_name.c_str(), pid);
    const char* epicsRoot = macEnvExpand("$(EPICS_ROOT)");
	if (NULL == epicsRoot)
	{
        errlogSevPrintf(errlogMinor, "pvdump: ERROR: EPICS_ROOT is NULL - cannot continue\n");
	    return -1;
	}
    
    //PV stuff
	try
	{
		dump_pvs(NULL, NULL, pv_map);
	}
	catch(const std::exception& ex)
	{
        errlogSevPrintf(errlogMinor, "pvdump: ERROR: %s\n", ex.what());
		return -1;
	}
	int ret = dumpMysql(pv_map, pid, exepath);
	if (ret == 0)
	{
	    // only install exit handler if we connect OK to mysql 
	    if (first_call)
	    {
            epicsAtExit(pvdumpOnExit, NULL); // register exit handler to change "running" state etc. in db
	    }
	}
	first_call = 0;
    return 0;
}

static void pvdumpOnExit(void*)
{
    time_t currtime;
    time(&currtime);
	printf("pvdump: calling exit handler for ioc \"%s\"\n", ioc_name.c_str());
    const char* mysqlHost = macEnvExpand("$(MYSQLHOST=localhost)");
#ifndef PVDUMP_DUMMY
	try
	{
        if (mysql_driver == NULL)
        {
	        mysql_driver = sql::mysql::get_driver_instance();
        }
		std::auto_ptr< sql::Connection > con(mysql_driver->connect(mysqlHost, "iocdb", "$iocdb"));
		std::auto_ptr< sql::Statement > stmt(con->createStatement());
		con->setSchema("iocdb");
	    std::ostringstream sql;
		sql << "UPDATE iocrt SET pid=NULL, start_time=start_time, stop_time=NOW(), running=0 WHERE iocname='" << ioc_name << "'";
		stmt->execute(sql.str());
	}
	// not sure of state of EPICS errlog during exit handlers, so use plain old stderr for safety
	catch (sql::SQLException &e) 
	{
		fprintf(stderr, "pvdump: MySQL ERR: %s (MySQL error code: %d, SQLState: %s)\n", e.what(), e.getErrorCode(), e.getSQLStateCStr());
        return;
	} 
	catch (std::runtime_error &e)
	{
		fprintf(stderr, "pvdump: MySQL ERR: %s\n", e.what());
        return;
	}
    catch(...)
    {
		fprintf(stderr, "pvdump: MySQL ERR: FAILED TRYING TO WRITE TO THE ISIS PV DB\n");
        return;
    }
#endif /* PVDUMP_DUMMY */
}

// allow a file of SQL commands to be executed from the IOC command line
// all lines are executed in a single transaction
static int sqlexec(const char *fileName)
{
	const char* mysqlHost = macEnvExpand("$(MYSQLHOST=localhost)");
	int nlines = 0;
	if (fileName == NULL || *fileName == '\0')
	{
        errlogSevPrintf(errlogMinor, "sqlexec: No filename given\n");
		return -1;
	}
#ifndef PVDUMP_DUMMY
	try 
	{
        const clock_t begin_time = clock();
        if (mysql_driver == NULL)
        {
	        mysql_driver = sql::mysql::get_driver_instance();
        }
	    std::auto_ptr< sql::Connection > con(mysql_driver->connect(mysqlHost, "iocdb", "$iocdb"));
	    con->setAutoCommit(0); // we will create transactions ourselves via explicit calls to con->commit()
	    con->setSchema("iocdb");
	    std::auto_ptr< sql::Statement > stmt(con->createStatement());
		std::fstream fs;
		char buffer[256];
		fs.open(fileName, std::ios::in);
		fs.getline(buffer, sizeof(buffer));
		while(fs.good())
		{
		    ++nlines;
			stmt->execute(std::string(buffer));
		    fs.getline(buffer, sizeof(buffer));
		}
        con->commit();
        std::cout << "sqlexec: executing " << nlines << " lines of SQL from \"" << fileName << "\" took " << float( clock () - begin_time ) /  CLOCKS_PER_SEC << " seconds" << std::endl;
    }
	catch (sql::SQLException &e) 
	{
        errlogSevPrintf(errlogMinor, "sqlexec: MySQL ERR: %s (MySQL error code: %d, SQLState: %s)\n", e.what(), e.getErrorCode(), e.getSQLStateCStr());
        return -1;
	} 
	catch (std::runtime_error &e)
	{
        errlogSevPrintf(errlogMinor, "sqlexec: MySQL ERR: %s\n", e.what());
        return -1;
	}
    catch(...)
    {
        errlogSevPrintf(errlogMinor, "sqlexec: MySQL ERR: FAILED TRYING TO WRITE TO THE ISIS PV DB\n");
        return -1;
    }
#endif /* PVDUMP_DUMMY */
    return 0;
}

// EPICS iocsh shell commands 

static const iocshArg pvdump_initArg0 = { "dbname", iocshArgString };			///< The name of the database
static const iocshArg pvdump_initArg1 = { "iocname", iocshArgString };

static const iocshArg sqlexec_initArg0 = { "filename", iocshArgString };			///< The name of the sql commands file

static const iocshArg * const pvdump_initArgs[] = { &pvdump_initArg0, &pvdump_initArg1 };
static const iocshArg * const sqlexec_initArgs[] = { &sqlexec_initArg0 };

static const iocshFuncDef pvdump_initFuncDef = {"pvdump", sizeof(pvdump_initArgs) / sizeof(iocshArg*), pvdump_initArgs};
static const iocshFuncDef sqlexec_initFuncDef = {"sqlexec", sizeof(sqlexec_initArgs) / sizeof(iocshArg*), sqlexec_initArgs};

static void pvdump_initCallFunc(const iocshArgBuf *args)
{
    pvdump(args[0].sval, args[1].sval);
}

static void sqlexec_initCallFunc(const iocshArgBuf *args)
{
    sqlexec(args[0].sval);
}

extern "C" 
{

static void pvdumpRegister(void)
{
    iocshRegister(&pvdump_initFuncDef, pvdump_initCallFunc);
    iocshRegister(&sqlexec_initFuncDef, sqlexec_initCallFunc);
}

epicsExportRegistrar(pvdumpRegister);

// these functions are for external non-IOC programs to add PVs to the database e.g. a c# channel access server
epicsShareFunc int pvdumpAddPV(const char* pvname, const char* record_type, const char* record_desc)
{
    epicsGuard<epicsMutex> _lock(pv_map_mutex);
    pv_map[pvname] = PVInfo(record_type, record_desc);
    return 0;
}

epicsShareFunc int pvdumpAddPVInfo(const char* pvname, const char* info_name, const char* info_value)
{
    epicsGuard<epicsMutex> _lock(pv_map_mutex);
    pv_map[pvname].info_fields[info_name] = info_value;
    return 0;
}

epicsShareFunc int pvdumpWritePVs(const char* iocname)
{
    int pid = get_pid();
    std::string exepath = get_path();
    if (iocname && *iocname)
    {
        setIOCName(iocname);
    }
    else
    {
        const char* exe_end = strrchr(exepath.c_str(), '\\');
        if (exe_end == NULL)
        {
            exe_end = strrchr(exepath.c_str(), '/');
        }
        if (exe_end != NULL)
        {
            setIOCName(exe_end + 1);
        }
        else
        {
            setIOCName(exepath.c_str());
        }
    }    
    ioc_name = getIOCName();
    printf("pvdump: IOC name is \"%s\"\n", ioc_name.c_str());
    return dumpMysql(pv_map, pid, exepath);
}

}

