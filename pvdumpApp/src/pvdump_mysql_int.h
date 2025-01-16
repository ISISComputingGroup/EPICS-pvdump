#ifndef PVDUMP_MYSQLINT_H
#define PVDUMP_MYSQLINT_H

extern "C" {

typedef void* SQL_DRIVER;
typedef void* SQL_CONNECTION;
typedef void* SQL_STATEMENT;
typedef void* SQL_PSTATEMENT;

#ifdef _WIN32
#ifdef BUILDING_MQSQL_INT
#define PVDUMP_EXPORT __declspec(dllexport)
#else
#define PVDUMP_EXPORT __declspec(dllimport)
#endif
#else
#define PVDUMP_EXPORT extern
#endif

PVDUMP_EXPORT SQL_CONNECTION pvdump_mysql_connect(SQL_DRIVER driver, const char* host, const char* db, const char* pw);
PVDUMP_EXPORT SQL_DRIVER pvdump_mysql_get_driver_instance();
PVDUMP_EXPORT int pvdump_mysql_conn_setAutoCommit(SQL_CONNECTION conn, int value);
PVDUMP_EXPORT int pvdump_mysql_conn_setSchema(SQL_CONNECTION conn, const char* schema);
PVDUMP_EXPORT int pvdump_mysql_conn_commit(SQL_CONNECTION conn);
PVDUMP_EXPORT int pvdump_mysql_stmt_execute(SQL_STATEMENT stm, const char* comm);
PVDUMP_EXPORT SQL_STATEMENT pvdump_mysql_createStatement(SQL_CONNECTION conn);
PVDUMP_EXPORT int pvdump_mysql_ps_executeUpdate(SQL_PSTATEMENT pst);
PVDUMP_EXPORT int pvdump_mysql_ps_setInt(SQL_PSTATEMENT pst, int idx, int value);
PVDUMP_EXPORT int pvdump_mysql_ps_setString(SQL_PSTATEMENT pst, int idx, const char* value);
PVDUMP_EXPORT SQL_PSTATEMENT pvdump_mysql_prepareStatement(SQL_CONNECTION conn, const char* comm);
PVDUMP_EXPORT void pvdump_mysql_free_conn(SQL_CONNECTION conn);
PVDUMP_EXPORT void pvdump_mysql_free_stmt(SQL_STATEMENT stm);
PVDUMP_EXPORT void pvdump_mysql_free_pstmt(SQL_PSTATEMENT pstm);

};

#endif /* PVDUMP_MYSQLINT_H */
