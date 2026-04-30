#pragma once

#ifndef DATABASE_OPERATION_HPP_
#define DATABASE_OPERATION_HPP_

#include <WinsockFix.hpp>

// Includes ODBC headers for interacting with SQL Server.
#include <sql.h>
#include <sqlext.h>
#include <string>
#include <vector>
#include <iostream>
#include <ImageObject.hpp>
#include <sqltypes.h>

namespace nar {

    bool createDatabaseAndTableIfNotExists(const std::string& connectionStrNoDb, const std::string& dbName);
    void verifyDatabaseName(SQLHDBC hDbc);
    void selectQuery(SQLHDBC hDbc);
    bool createImageEntriesTable(SQLHDBC hDbc);

    class DatabaseOperation {
    public:
        // contructor : opens DB connection
        DatabaseOperation();
        // destructor : safely closes connection and frees DB handles
        ~DatabaseOperation();

        // saves a scanned image record to the database
        bool insertImage(const ImageObject& img, std::string oper);
        int getImageRowCount();

        bool insertImageImageInformationTable(const ImageObject& img);

    private:
        // private member varaibles
        std::string connectionString_; // ODBC connection string stores DB credentials
        SQLHENV hEnv_ = nullptr; // Environment handle (like ODBC context)
        SQLHDBC hDbc_ = nullptr; // Database connection handle

        // Manage ODBC session lifecycle
        void connect();
        void disconnect();
    };
} // namespace nar

#endif DATABASE_OPERATION_HPP_
