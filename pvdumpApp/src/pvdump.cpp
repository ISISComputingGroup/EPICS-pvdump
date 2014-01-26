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

#include "pvdump.h"

#include "sqlite3.h"
#include "SqlDatabase.h"
#include "SqlField.h"
#include "SqlTable.h"

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#endif

#include <epicsExport.h>

static int get_pid()
{
    #ifdef _WIN32
        return _getpid();
    #endif
    return 0;
}

static std::string get_path()
{
    #ifdef _WIN32
        char buffer[MAX_PATH];
        GetModuleFileName(NULL,buffer,sizeof(buffer));
        std::string path(buffer);
    #else
         std::string path("");
    #endif

    return path;
}

// based on iocsh dbl command from epics_base/src/db/dbTest.c 
// return an std map, currently key is pv and value is recordType (if that is defined)
static void dump_pvs(const char *precordTypename, const char *fields, std::map<std::string,std::string>& pvs)
{
    DBENTRY dbentry;
    DBENTRY *pdbentry=&dbentry;
    long status;
    int nfields = 0;
    int ifield;
    char *fieldnames = 0;
    char **papfields = 0;

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
			pvs[dbGetRecordName(pdbentry)] = (pvalue ? pvalue : "");
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

extern "C" {

int pvdump(const char *dbName, const char *iocName)
{
    int pid = get_pid();
    std::string exepath = get_path();
        
    time_t currtime;
    time(&currtime);    
    

    sql::Field definition_tbPV[] = 
    {
        sql::Field(sql::FIELD_KEY),
        sql::Field("pvname", sql::type_text, sql::flag_not_null),
        sql::Field("record_type", sql::type_text),
        sql::Field("pid", sql::type_int),
        sql::Field("ioc", sql::type_text),
        sql::Field("start_time", sql::type_time),
        sql::Field("exe_path", sql::type_text),
        sql::Field(sql::DEFINITION_END),
    }; 
    
    sql::Field definition_tbIOC[] = 
    {
        sql::Field(sql::FIELD_KEY),
        sql::Field("ioc", sql::type_text, sql::flag_not_null),
        sql::Field("pid", sql::type_int),
        sql::Field("start_time", sql::type_time),
        sql::Field("exe_path", sql::type_text),
        sql::Field("running", sql::type_int),
        sql::Field(sql::DEFINITION_END),
    };    
    
    if (NULL == dbName)
	{
		dbName = "test.db";  // default database name
	}
    
    //PV stuff
	std::map<std::string,std::string> pv_map;
	try
	{
		dump_pvs(NULL, NULL, pv_map);
	}
	catch(const std::exception& ex)
	{
		printf("%s\n", ex.what());
	}
    
    sql::Database db;      
    
    try
    {
        db.open(dbName, 20000);

        sql::Table tbPV(db.getHandle(), "PVs", definition_tbPV);
        if (!tbPV.exists())
        {
            tbPV.create();
        }
        else
        {
            //Remove any previous entries for this IOC
            std::ostringstream s;
            s << "ioc = " << "'" << iocName << "'";
            tbPV.deleteRecords(s.str());
        }
        
        sql::Record record(tbPV.fields());

        const clock_t begin_time = clock();
        db.transactionBegin();
        for(std::map<std::string,std::string>::const_iterator it = pv_map.begin(); it != pv_map.end(); ++it)
        {
            record.setString("pvname", it->first);
            record.setString("record_type", it->second);
            record.setInteger("pid", pid);
            record.setString("ioc", iocName);
            record.setTime("start_time", currtime);
            record.setString("exe_path", exepath);
            tbPV.addRecord(&record);
        }
        db.transactionCommit();
        std::cout << "PV write took: " << float( clock () - begin_time ) /  CLOCKS_PER_SEC << std::endl;

        //IOC Stuff       
        sql::Table tbIOC(db.getHandle(), "IOCs", definition_tbIOC);
        
        if (!tbIOC.exists())
        {
            tbIOC.create();
        }
        else
        {
            //Remove any previous entries for this IOC
            std::ostringstream s;
            s << "ioc = " << "'" << iocName << "'";
            tbIOC.deleteRecords(s.str());
        }
               
        sql::Record ioc_record(tbIOC.fields());
        ioc_record.setString("ioc", iocName);
        ioc_record.setInteger("pid", pid);
        ioc_record.setTime("start_time", currtime);
        ioc_record.setString("exe_path", exepath);
        ioc_record.setInteger("running", 1);    
        tbIOC.addRecord(&ioc_record);
    }
    catch(...)
    {
        printf("FAILED TRYING TO WRITE TO THE ISIS PV DB\n");
        return -1;
    }    
    
    return 0;
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

static void pvdumpRegister(void)
{
    iocshRegister(&initFuncDef, initCallFunc);
}

epicsExportRegistrar(pvdumpRegister);

}

