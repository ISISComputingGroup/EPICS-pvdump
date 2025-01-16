// a mysql interface library for VS2010 galil ioc
// can only expose a pure C interface
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

#include "pvdump_mysql.h"

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

static SqlDriver* mysql_driver = NULL;

const char* SQLException::what()
{
    return "";
}

const char* SQLException::getSQLStateCStr()
{
    return "";
}

int SQLException::getErrorCode()
{
    return 0;
}

SqlPreparedStatement::SqlPreparedStatement(SQL_CONNECTION conn, const std::string& comm)
{
    m_pstatement = pvdump_mysql_prepareStatement(conn, comm.c_str());
	if (m_pstatement == nullptr) {
		throw std::runtime_error("PvdumpMysql: cannot create prepared statment");
	}
}

SqlPreparedStatement::~SqlPreparedStatement()
{
    pvdump_mysql_free_pstmt(m_pstatement);
}

void SqlPreparedStatement::setString(int idx, const std::string& value)
{
    pvdump_mysql_ps_setString(m_pstatement, idx, value.c_str());
}

void SqlPreparedStatement::setInt(int idx, int value)
{ 
    pvdump_mysql_ps_setInt(m_pstatement, idx, value);
}

void SqlPreparedStatement::executeUpdate()
{
    pvdump_mysql_ps_executeUpdate(m_pstatement);    
}

SqlStatement::SqlStatement(SQL_CONNECTION conn)
{
    m_statement = pvdump_mysql_createStatement(conn);
	if (m_statement == nullptr) {
		throw std::runtime_error("PvdumpMysql: cannot create statment");
	}
}

SqlStatement::~SqlStatement()
{
    pvdump_mysql_free_stmt(m_statement);    
}

void SqlStatement::execute(const std::string& comm)
{
    pvdump_mysql_stmt_execute(m_statement, comm.c_str());
}

SqlConnection::SqlConnection(SQL_DRIVER driver, const char* mysqlHost, const char* db, const char* pw)
{
    m_connection = pvdump_mysql_connect(driver, mysqlHost, db, pw);
	if (m_connection == nullptr) {
		throw std::runtime_error(std::string("PvdumpMysql: cannot connect to ") + mysqlHost);
	}
}

SqlConnection::~SqlConnection()
{
    pvdump_mysql_free_conn(m_connection);
}

void SqlConnection::setAutoCommit(int val)
{
    pvdump_mysql_conn_setAutoCommit(m_connection, val);
}

void SqlConnection::setSchema(const char* schema)
{
    pvdump_mysql_conn_setSchema(m_connection, schema);
}

SqlPreparedStatement* SqlConnection::prepareStatement(const std::string& stmt)
{
    return new SqlPreparedStatement(m_connection, stmt);
}

SqlStatement* SqlConnection::createStatement()
{
    return new SqlStatement(m_connection);
}

void SqlConnection::commit()
{
    pvdump_mysql_conn_commit(m_connection);
}

SqlDriver* SqlDriver::get_driver_instance()
{
    if (g_instance == NULL)
    {
        g_instance = new SqlDriver();
    }
    return g_instance;
}

SqlDriver::SqlDriver()
{
    m_driver = pvdump_mysql_get_driver_instance();
	if (m_driver == nullptr) {
		throw std::runtime_error("PvdumpMysql: cannot create driver");
	}
}

SqlConnection* SqlDriver::connect(const char* mysqlHost, const char* db, const char* pw)
{
    return new SqlConnection(m_driver, mysqlHost, db, pw);
}

SqlDriver* SqlDriver::g_instance = nullptr;
