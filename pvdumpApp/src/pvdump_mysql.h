
#include "pvdump_mysql_int.h"

class SQLException
{
    public:
    const char* what();
    int getErrorCode();
    const char* getSQLStateCStr();
};

class SqlPreparedStatement
{
    SQL_PSTATEMENT m_pstatement;
    public:
    SqlPreparedStatement(SQL_CONNECTION conn, const std::string& comm);
    ~SqlPreparedStatement();
    void setString(int idx, const std::string& value);
    void setInt(int idx, int value);
    void executeUpdate();
};

class SqlStatement
{
    SQL_STATEMENT m_statement;
    public:
    ~SqlStatement();
    SqlStatement(SQL_CONNECTION conn);
    void execute(const std::string& comm);
};

class SqlConnection
{
    private:
    SQL_CONNECTION m_connection;
    public:
    SqlConnection(SQL_DRIVER driver, const char* mysqlHost, const char* db, const char* pw);
    ~SqlConnection();
    void setAutoCommit(int val);
    void setSchema(const char* schema);
    SqlPreparedStatement* prepareStatement(const std::string& stmt);
    SqlStatement* createStatement();
    void commit();
};

class SqlDriver
{
    private:
    static SqlDriver* g_instance;
    SQL_DRIVER m_driver;
    public:
    SqlDriver();
    static SqlDriver* get_driver_instance();
    SqlConnection* connect(const char* mysqlHost, const char* db, const char* pw);
};
