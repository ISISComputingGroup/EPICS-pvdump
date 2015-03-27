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
#include <string>
#include <time.h>
#include <sstream>

#include "epicsStdlib.h"
#include "epicsString.h"
#include "dbDefs.h"
#include "epicsMutex.h"
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
#include <epicsMutex.h>
#include <iocsh.h>
#include <errlog.h>
#include "macLib.h"
#include "epicsExit.h"

#include "utilities.h"

// sqlite
#include "sqlite3.h"
#include "SqlDatabase.h"
#include "SqlField.h"
#include "SqlTable.h"

// mysql
#include <mysql_public_iface.h>

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

static std::map<std::string,PVInfo> pv_map;

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

// return 0 on success, -1 on error
static int execute_sql(sqlite3* db, const char* sql)
{
    char* errmsg = NULL;
    int ret = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK)
	{
		printf("pvdump: sqlite3 error code : %d\n", ret);
	}
	if (errmsg != NULL)
	{
		printf("pvdump: sqlite3: %s\n", errmsg);
	    sqlite3_free(errmsg);
	}
	return ret == SQLITE_OK ? 0 : -1;
}

static void pvdumpOnExit(void*);

static std::string ioc_name, db_name;    
static sql::Driver * mysql_driver;

static const int MAX_MACRO_VAL_LENGTH = 100; // should agree with length of macroval in iocenv MySQL table (iocdb_mysql_schema.txt)

static int dumpMysql(const std::map<std::string,PVInfo>& pv_map, int pid, const std::string& exepath)
{
	unsigned long npv = 0, ninfo = 0, nmacro = 0;
	try 
	{
        const clock_t begin_time = clock();
	    mysql_driver = sql::mysql::get_driver_instance();
	    std::auto_ptr< sql::Connection > con(mysql_driver->connect("localhost", "iocdb", "$iocdb"));
	    std::auto_ptr< sql::Statement > stmt(con->createStatement());
	    con->setAutoCommit(0); // we will create transactions ourselves via explicit calls to con->commit()
	    con->setSchema("iocdb");
		stmt->execute(std::string("DELETE FROM iocenv WHERE iocname='") + ioc_name + "'");
		std::ostringstream sql;
		sql << "DELETE FROM iocrt WHERE iocname='" << ioc_name << "' OR pid=" << pid; // remove any old record from iocrt with our current pid or name
		stmt->execute(sql.str());
		stmt->execute(std::string("DELETE FROM pvs WHERE iocname='") + ioc_name + "'"); // remove our PVS from last time, this will also delete records from pvinfo due to foreign key cascade action
		con->commit();
		
		std::auto_ptr< sql::PreparedStatement > iocrt_stmt(con->prepareStatement("INSERT INTO iocrt (iocname, pid, start_time, stop_time, running, exe_path) VALUES (?,?,NOW(),'0000-00-00 00:00:00',?,?)"));
		iocrt_stmt->setString(1,ioc_name);
		iocrt_stmt->setInt(2,pid);
		iocrt_stmt->setInt(3,1);
		iocrt_stmt->setString(4,exepath);
		iocrt_stmt->executeUpdate();
		con->commit();

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

        char **sp;
		char *envbit1, *envbit2;
		std::auto_ptr< sql::PreparedStatement > iocenv_stmt(con->prepareStatement("INSERT INTO iocenv (iocname, macroname, macroval) VALUES (?,?,?)"));
		iocenv_stmt->setString(1, ioc_name);
        for (sp = environ ; (sp != NULL) && (*sp != NULL) ; ++sp)
		{
		    envbit1 = strdup(*sp);   // name=value string  
			envbit2 = strchr(envbit1, '='); 
			if (NULL != envbit2)
			{
			    envbit1[envbit2-envbit1] = '\0';  // NULL out '='
			    ++envbit2;
				if (strlen(envbit2) < MAX_MACRO_VAL_LENGTH)  // ignore things with long values like PATH
				{
		            iocenv_stmt->setString(2, envbit1); // name
		            iocenv_stmt->setString(3, envbit2); // value
			        iocenv_stmt->executeUpdate();
				    ++nmacro;
				}
			}
			free(envbit1);
		}
		con->commit();

        std::cout << "pvdump: MySQL write of " << npv << " PVs with " << ninfo << " info entries, plus " << nmacro << " macros took " << float( clock () - begin_time ) /  CLOCKS_PER_SEC << " seconds" << std::endl;
    }
	catch (sql::SQLException &e) 
	{
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: %s (MySQL error code: %d, SQLState: %s)", e.what(), e.getErrorCode(), e.getSQLStateCStr());
        return -1;
	} 
	catch (std::runtime_error &e)
	{
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: %s", e.what());
        return -1;
	}
    catch(...)
    {
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: FAILED TRYING TO WRITE TO THE ISIS PV DB");
        return -1;
    }
	return 0;
}

// define funtion for this table as it is referenced in both pvdump and pvdumpOnExit
static sql::Table* openIocrtTable(sql::Database& db)
{
    return sql::Table::createFromDefinition(db.getHandle(), "iocrt", 
		    "iocname TEXT, pid INTEGER, start_time INTEGER, stop_time INTEGER, running INTEGER, exe_path TEXT");
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
        errlogSevPrintf(errlogMinor, "pvdump: ERROR: EPICS_ROOT is NULL - cannot continue");
	    return -1;
	}
    
    //PV stuff
	try
	{
		dump_pvs(NULL, NULL, pv_map);
	}
	catch(const std::exception& ex)
	{
        errlogSevPrintf(errlogMinor, "pvdump: ERROR: %s", ex.what());
		return -1;
	}

#if 0 /* old sqlite */   
    sql::Database db;      
    if (NULL != dbName)
	{
	    db_name = dbName;
	}
	else
	{
		db_name = std::string(epicsRoot) + "/iocs.sq3";  // default database name
	}
	printf("pvdump: sqlite db name is \"%s\"\n", db_name.c_str()); 
    
    try 
    {
        db.open(db_name, 20000);
		execute_sql(db.getHandle(), "PRAGMA foreign_keys = ON");
		// createFromDefinition counts commas so do not include constraints (i.e. foreign keys) in definitions. Also omit any other stuff like "primary key" or "unique"
		// as it can confus it.
		sql::Table* table_iocs = sql::Table::createFromDefinition(db.getHandle(), "iocs",
		    "iocname TEXT, dir TEXT, consoleport INT, logport INT, exe TEXT, cmd TEXT");
		sql::Table* table_pvs = sql::Table::createFromDefinition(db.getHandle(), "pvs", "pvname TEXT, record_type TEXT, record_desc TEXT, iocname TEXT"); 
		sql::Table* table_pvinfo = sql::Table::createFromDefinition(db.getHandle(), "pvinfo", "pvname TEXT, infoname TEXT, value TEXT");
		sql::Table* table_iocrt = openIocrtTable(db);

        db.transactionBegin();
		table_iocrt->deleteRecords(std::string("iocname='")+ioc_name+"'"); // delete our old entry from iocrt
		std::ostringstream oss;
		oss << "pid=" << pid;
		table_iocrt->deleteRecords(oss.str()); // remove any old record from iocrt with our current pid
		table_pvs->deleteRecords(std::string("iocname='")+ioc_name+"'"); // remove our PVS from last time, this will also delete records from pvinfo due to foreign key cascade action
        db.transactionCommit();
		
		sql::Record pvs_record(table_pvs->fields());
		sql::Record iocrt_record(table_iocrt->fields());
		sql::Record pvinfo_record(table_pvinfo->fields());
		
        iocrt_record.setString("iocname", ioc_name);
        iocrt_record.setInteger("pid", pid);
        iocrt_record.setTime("start_time", currtime);
        iocrt_record.setTime("stop_time", 0);
        iocrt_record.setInteger("running", 1);
        iocrt_record.setString("exe_path", exepath);
        
        const clock_t begin_time = clock();

        db.transactionBegin();
		table_iocrt->addRecord(&iocrt_record);
        for(std::map<std::string,PVInfo>::const_iterator it = pv_map.begin(); it != pv_map.end(); ++it)
        {
            pvs_record.setString("pvname", it->first);
            pvs_record.setString("record_type", it->second.record_type);
            pvs_record.setString("record_desc", it->second.record_desc);
            pvs_record.setString("iocname", ioc_name);
            table_pvs->addRecord(&pvs_record);
			const std::map<std::string,std::string>& imap = it->second.info_fields;
            for(std::map<std::string,std::string>::const_iterator itinf = imap.begin(); itinf != imap.end(); ++itinf)
			{
			    pvinfo_record.setString("pvname", it->first);
			    pvinfo_record.setString("infoname", itinf->first);
			    pvinfo_record.setString("value", itinf->second);
                table_pvinfo->addRecord(&pvinfo_record);
			}
        }
        db.transactionCommit();
		delete table_iocrt, table_pvinfo, table_pvs, table_iocs;
        std::cout << "pvdump: SQLite write took: " << float( clock () - begin_time ) /  CLOCKS_PER_SEC << std::endl;
    }
	catch(const std::exception& ex)
	{
		printf("pvdump: sqlite: ERROR: %s\n", ex.what());
	}
	catch(const sql::Exception& ex) // sql::Exception does not inherit from std::exception, so need to catch separately 
	{
		printf("pvdump: sqlite: sql ERROR: %s\n", ex.msg().c_str());
	}
    catch(...)
    {
        printf("pvdump: sqlite: ERROR: FAILED TRYING TO WRITE TO THE ISIS PV DB\n");
    }
#endif /* old sqlite */
	dumpMysql(pv_map, pid, exepath);
	if (first_call)
	{
        epicsAtExit(pvdumpOnExit, NULL); // register exit handler to change "running" state etc. in db
	}
	first_call = 0;
    return 0;
}

static void pvdumpOnExit(void*)
{
    time_t currtime;
    time(&currtime);
	printf("pvdump: calling exit handler for ioc \"%s\"\n", ioc_name.c_str());
#if 0 /* old sqlite */	 
    sql::Database db;          
    try 
    {
        db.open(db_name, 20000);
	    std::ostringstream sql;
		execute_sql(db.getHandle(), "PRAGMA foreign_keys = ON");
		sql << "UPDATE iocrt SET pid=NULL, stop_time=" << currtime << ", running=0 WHERE iocname='" << ioc_name << "'";
		execute_sql(db.getHandle(), sql.str().c_str());
	}
	catch(const std::exception& ex)
	{
		printf("pvdump: ERROR: %s\n", ex.what());
	}
	catch(const sql::Exception& ex) // sql::Exception does not inherit from std::exception, so need to catch separately 
	{
		printf("pvdump: sql ERROR: %s\n", ex.msg().c_str());
	}
    catch(...)
    {
        printf("pvdump: ERROR: FAILED TRYING TO WRITE TO THE ISIS PV DB\n");
    }
#endif /* old sqlite */
	try
	{
		std::auto_ptr< sql::Connection > con(mysql_driver->connect("localhost", "iocdb", "$iocdb"));
		std::auto_ptr< sql::Statement > stmt(con->createStatement());
		con->setSchema("iocdb");
	    std::ostringstream sql;
		sql << "UPDATE iocrt SET pid=NULL, start_time=start_time, stop_time=NOW(), running=0 WHERE iocname='" << ioc_name << "'";
		stmt->execute(sql.str());
	}
	catch (sql::SQLException &e) 
	{
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: %s (MySQL error code: %d, SQLState: %s)", e.what(), e.getErrorCode(), e.getSQLStateCStr());
        return;
	} 
	catch (std::runtime_error &e)
	{
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: %s", e.what());
        return;
	}
    catch(...)
    {
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: FAILED TRYING TO WRITE TO THE ISIS PV DB");
        return;
    }
}

// EPICS iocsh shell commands 

static const iocshArg initArg0 = { "dbname", iocshArgString };			///< The name of the database
static const iocshArg initArg1 = { "iocname", iocshArgString };

static const iocshArg * const initArgs[] = { &initArg0, &initArg1 };

static const iocshFuncDef initFuncDef = {"pvdump", sizeof(initArgs) / sizeof(iocshArg*), initArgs};

static void initCallFunc(const iocshArgBuf *args)
{
    pvdump(args[0].sval, args[1].sval);
}

extern "C" 
{

static void pvdumpRegister(void)
{
    iocshRegister(&initFuncDef, initCallFunc);
}

epicsExportRegistrar(pvdumpRegister);

// these functions are for external non-IOC programs to add PVs e.g. c# channell access server
epicsShareFunc int pvdumpAddPV(const char* pvname, const char* record_type, const char* record_desc)
{
    pv_map[pvname] = PVInfo(record_type, record_desc);
    return 0;
}

epicsShareFunc int pvdumpAddPVInfo(const char* pvname, const char* info_name, const char* info_value)
{
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
    return dumpMysql(pv_map, pid, exepath);
}

}

