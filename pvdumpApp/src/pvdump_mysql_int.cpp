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

#define BUILDING_MQSQL_INT
#include "pvdump_mysql_int.h"

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#define TRAP_ERROR \
    catch (const sql::SQLException &e)  \
	{  \
        fprintf(stderr, "MySQL ERR: %s (MySQL error code: %d, SQLState: %s)\n", e.what(), e.getErrorCode(), e.getSQLStateCStr()); \
	} \
	catch (const std::runtime_error &e) \
	{ \
        fprintf(stderr, "MySQL ERR: %s\n", e.what()); \
	} \
    catch(...) \
    { \
        fprintf(stderr, "MySQL ERR: FAILED\n"); \
    }

SQL_CONNECTION pvdump_mysql_connect(SQL_DRIVER driver, const char* host, const char* db, const char* pw)
{
    try {
        sql::Driver* mysql_driver = reinterpret_cast<sql::Driver*>(driver);
        sql::Connection* con = mysql_driver->connect(host, db, pw);
        return static_cast<SQL_CONNECTION>(con);
    }
    TRAP_ERROR;
    return nullptr;
}

SQL_DRIVER pvdump_mysql_get_driver_instance()
{
    try {
        sql::Driver* mysql_driver = sql::mysql::get_driver_instance();
        return static_cast<SQL_DRIVER>(mysql_driver);
    }
    TRAP_ERROR;
    return nullptr;
}

int pvdump_mysql_conn_setAutoCommit(SQL_CONNECTION conn, int value)
{
    try {
        sql::Connection* con = reinterpret_cast<sql::Connection*>(conn);
        con->setAutoCommit(value);
		return 0;
    }
    TRAP_ERROR;
    return -1;
}

int pvdump_mysql_conn_setSchema(SQL_CONNECTION conn, const char* schema)
{
    try {
        sql::Connection* con = reinterpret_cast<sql::Connection*>(conn);
        con->setSchema(schema);
		return 0;
    }
    TRAP_ERROR;
    return -1;
}

SQL_PSTATEMENT pvdump_mysql_prepareStatement(SQL_CONNECTION conn, const char* comm)
{
    try {
        sql::Connection* con = reinterpret_cast<sql::Connection*>(conn);
        sql::PreparedStatement* pstmt = con->prepareStatement(comm);
        return static_cast<SQL_PSTATEMENT>(pstmt);
    }
    TRAP_ERROR;
    return nullptr;
}

int pvdump_mysql_ps_setString(SQL_PSTATEMENT pst, int idx, const char* value)
{
    try {
        sql::PreparedStatement* pstmt = reinterpret_cast<sql::PreparedStatement*>(pst);
        pstmt->setString(idx, value);
        return 0;
    }
    TRAP_ERROR;
    return -1;
}
    
int pvdump_mysql_ps_setInt(SQL_PSTATEMENT pst, int idx, int value)
{
    try {
        sql::PreparedStatement* pstmt = reinterpret_cast<sql::PreparedStatement*>(pst);
        pstmt->setInt(idx, value);
        return 0;
    }
    TRAP_ERROR;
    return -1;
}

int pvdump_mysql_ps_executeUpdate(SQL_PSTATEMENT pst)
{
    try {
        sql::PreparedStatement* pstmt = reinterpret_cast<sql::PreparedStatement*>(pst);
        pstmt->executeUpdate();
        return 0;
    }
    TRAP_ERROR;
    return -1;
}

int pvdump_mysql_conn_commit(SQL_CONNECTION conn)
{
    try {
        sql::Connection* con = reinterpret_cast<sql::Connection*>(conn);
	    con->commit();
        return 0;
    }
    TRAP_ERROR;
    return -1;
}

SQL_STATEMENT pvdump_mysql_createStatement(SQL_CONNECTION conn)
{
    try {
        sql::Connection* con = reinterpret_cast<sql::Connection*>(conn);
        sql::Statement* stmt = con->createStatement();
        return static_cast<SQL_STATEMENT>(stmt);
    }
    TRAP_ERROR;
    return nullptr;
}

int pvdump_mysql_stmt_execute(SQL_STATEMENT stm, const char* comm)
{
    try {
        sql::Statement* stmt = reinterpret_cast<sql::Statement*>(stm);
	    stmt->execute(comm);
        return 0;
    }
    TRAP_ERROR;
    return -1;
}

void pvdump_mysql_free_conn(SQL_CONNECTION conn)
{
    sql::Connection* con = reinterpret_cast<sql::Connection*>(conn);
    delete con;
}

void pvdump_mysql_free_stmt(SQL_STATEMENT stm)
{
    sql::Statement* stmt = reinterpret_cast<sql::Statement*>(stm);
    delete stmt;
}

void pvdump_mysql_free_pstmt(SQL_PSTATEMENT pstm)
{
    sql::PreparedStatement* pstmt = reinterpret_cast<sql::PreparedStatement*>(pstm);
    delete pstmt;
}
