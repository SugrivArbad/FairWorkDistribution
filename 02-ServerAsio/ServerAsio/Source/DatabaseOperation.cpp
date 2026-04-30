
#include <DatabaseOperation.hpp>
#include <sstream>
#include <iomanip>

extern std::string intToString(int iCnt);

using namespace nar;

extern std::string format_iso8601(const std::chrono::system_clock::time_point& tp);

void nar::verifyDatabaseName(SQLHDBC hDbc)
{
    SQLHSTMT hStmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);

    SQLRETURN ret = SQLExecDirectA(hStmt, (SQLCHAR*)"SELECT DB_NAME()", SQL_NTS);
    if (ret == SQL_SUCCESS) 
    {
        char dbName[128];
        SQLFetch(hStmt);
        SQLGetData(hStmt, 1, SQL_C_CHAR, dbName, sizeof(dbName), NULL);
        std::cout << "nar::VerifyDatabaseName() : Current DB : " << dbName << std::endl;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
}

void nar::selectQuery(SQLHDBC hDbc)
{
    SQLHSTMT hStmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);

    SQLRETURN ret = SQLExecDirectA(hStmt, (SQLCHAR*)"SELECT TOP 1 * FROM [dbo].[BSCSK_ImageInformation]", SQL_NTS);
    if (ret == SQL_SUCCESS) {
        std::cout << "nar::selectQuery() : Table exists, SELECT works." << std::endl;
    }
    else {
        std::cerr << "ERROR : nar::selectQuery() : SELECT failed — does table exist?" << std::endl;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
}

bool nar::createImageEntriesTable(SQLHDBC hDbc)
{
    try
    {
        SQLHSTMT hStmt = nullptr;
        SQLRETURN ret;

        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            std::cerr << "ERROR: Failed to allocate statement handle.\\n";
            return false;
        }

        std::string query = R"(
            IF OBJECT_ID('[dbo].[ImageEntries]', 'U') IS NULL
            BEGIN
                CREATE TABLE [dbo].[ImageEntries] (
                    [ID] INT IDENTITY(1,1) PRIMARY KEY,
                    [ImageName] varchar(500) NULL,
                    [ImageFilePath] varchar(100) NULL,
                    [ImageMarked] varchar(50) NULL,
                    [LiveScanMarked] varchar(50) NULL,
                    [TIP_Image] varchar(50) NULL,
                    [TIP_ImageFilePath] varchar(50) NULL,
                    [ProcessType] varchar(50) NULL,
                    [ScanOriginalDate] varchar(200) NULL,   -- changed here
                    [OperatorID] int NULL,
                    [BagCount] int NULL,
                    [SystemID] int NULL,
                    [TransferedToSharedLocation] varchar(50) NULL,
                    [BarCodeNo] varchar(50) NULL,
                    [ImageHeight] int NULL,
                    [ImageWidth] int NULL,
                    [ImageKV] int NULL,
                    [ImageMA] varchar(50) NULL,
                    [ImageOrganic] int NULL,
                    [ImageInorganic] int NULL,
                    [ImageHighDensity] int NULL,
                    [BarcodeGridVisiblity] bit NULL,
                    [BarcodeAlphanNumeric] bit NULL,
                    [TIPImageID] int NULL,
                    [MinAbsorptionLevel] int NULL,
                    [OrgAlarmsCount] int NULL,
                    [InorgAlarmsCount] int NULL,
                    [HighDensityAlarmsCount] int NULL,
                    [Acknowelege] int NULL,
                    [MessageFromSupervisor] varchar(500) NULL,
                    [Lock] bit NULL,
                    [Action] varchar(50) NULL
                )
            END
        )";

        ret = SQLExecDirectA(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) 
        {
            std::cerr << "Failed to create table.\\n";
            SQLCHAR sqlState[6], message[256];
            SQLINTEGER nativeError;
            SQLSMALLINT textLength;

            while (SQLGetDiagRecA(SQL_HANDLE_STMT, hStmt, 1, sqlState, &nativeError,
                message, sizeof(message), &textLength) == SQL_SUCCESS)
            {
                std::cerr << "[ODBC] SQL State: " << sqlState << " Message: " << message << "\\n";
                break;
            }

            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return false;
        }

        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return true;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "EXCEPTION : nar::createImageEntriesTable() : " << ex.what() << std::endl;
    }
}

bool nar::createDatabaseAndTableIfNotExists(const std::string& connectionStrNoDb, const std::string& dbName)
{
    SQLHENV hEnv = nullptr;
    SQLHDBC hDbc = nullptr;
    SQLHSTMT hStmt = nullptr;
    SQLRETURN ret;

    /*----------------------------------------------------*\
     * #### ODBC flow #####
        * 1. hEnv  -> Create Environment handle and Set
        * 2. hDbc  -> Create database handle under hEnv and Connect to DB using connection string
        * 3. hStmt -> Create statement handle under hDbc and prepare it
        * 4. Bind parameters
        * 5. Execute hStmt
    \*----------------------------------------------------*/
    
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
    
    SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
    ret = SQLDriverConnectA(hDbc, NULL, (SQLCHAR*)connectionStrNoDb.c_str(), SQL_NTS,
        NULL, 0, NULL, SQL_DRIVER_COMPLETE);

    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
        std::cerr << "ERROR : createDatabaseAndTableIfNotExists() : [Setup] Failed to connect to SQL Server (no DB)." << std::endl;
        return false;
    }

    // Allocate statement
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);

    // 1. Create database if not exists
    std::string createDbSQL = "IF DB_ID('" + dbName + "') IS NULL CREATE DATABASE [" + dbName + "]";
    SQLExecDirectA(hStmt, (SQLCHAR*)createDbSQL.c_str(), SQL_NTS);

    // 2. Use the newly created DB
    std::string useDbSQL = "USE [" + dbName + "]";
    SQLExecDirectA(hStmt, (SQLCHAR*)useDbSQL.c_str(), SQL_NTS);

    // 3. Create table if not exists
    bool bTableCreated = createImageEntriesTable(hDbc);

    if (!bTableCreated)
    {
        std::cerr << "ERROR : nar::createDatabaseAndTableIfNotExists() : failed to create ImageEntries table." << std::endl;
    }

    // Cleanup
    if (hStmt) 
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    if (hDbc)
    {
        SQLDisconnect(hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
    }
    if (hEnv) 
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);

    return true;
}

/* ------------------------------------------------------------------------ *\
 * Stores the connection string.
 * Calls connect() to initialize the ODBC connection.
\* ------------------------------------------------------------------------ */
DatabaseOperation::DatabaseOperation()
{
    try
    {
        ///*std::string user = "KVAdmin";
        //std::string pass = "sa";*/
        std::string user = "sa";
        std::string pass = "asdf@1234";
        std::string serverName = "localhost\\SQLEXPRESS";
        std::string dbName = "NAR";
        ////std::string dbName = "KennBSCSKDatabase";

        std::string connStrNoDb = "DRIVER={SQL Server};SERVER=" + serverName + ";UID=" + user + ";PWD=" + pass + ";";

        // Create DB and table if needed
        if (!createDatabaseAndTableIfNotExists(connStrNoDb, dbName))
        {
            std::cerr << "ERROR : createDatabaseAndTableIfNotExists() : Failed to set up database." << std::endl;
            throw "Failed to set up database.";
        }

        // Now create the real DB object
        std::string connStr = connStrNoDb + "DATABASE=" + dbName + ";";

        ////std::string connStr = "DRIVER={SQL Server};SERVER=" + serverName +
        ///*";UID=" + user +
        //";PWD=" + pass +
        //";DATABASE=" + dbName + ";";*/

        connectionString_ = connStr;
    }
    catch (const std::exception& ex)
    {
        std::cout << "EXCEPTION : DatabaseOperation::DatabaseOperation() : " << ex.what() << std::endl;
    }
}

/* ------------------------------------------------------------------------ *\
 * Ensures any active ODBC resources are released when the object goes out of scope
\* ------------------------------------------------------------------------ */
DatabaseOperation::~DatabaseOperation()
{
    disconnect();
}

void DatabaseOperation::connect()
{
    /* ------------------------------------------------------------------------ *\
     * Environment Handle : Allocated with SQLAllocHandle.
     * Set ODBC Version : Sets version 3.0 with SQLSetEnvAttr.
     * Connection Handle : Allocated to communicate with the DB.
    \* ------------------------------------------------------------------------ */
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv_);
    SQLSetEnvAttr(hEnv_, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, hEnv_, &hDbc_);

    /* ------------------------------------------------------------------------ *\
     * Connect to DB: Attempts connection using the connection string (e.g., DRIVER={SQL Server};...).
     * If ret fails:
     *   Logs the error.
     *   Frees resources via disconnect().
     *   Throws exception to stop further processing.
    \* ------------------------------------------------------------------------ */
    SQLCHAR outConnStr[1024];
    SQLSMALLINT outConnStrLen;
    SQLRETURN ret = SQLDriverConnectA(hDbc_, NULL,
        (SQLCHAR*)connectionString_.c_str(), SQL_NTS,
        outConnStr, sizeof(outConnStr), &outConnStrLen, SQL_DRIVER_COMPLETE);

    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
        std::cerr << "DatabaseOperation::connect() : Connection failed." << std::endl;
        disconnect();
        throw std::runtime_error("ODBC connection failed");
    }
}

/* ------------------------------------------------------------------------ *\
 * disconnect(): Clean Shutdown
 * Disconnects active DB connection and frees connection handle.
 * Frees the environment handle.
\* ------------------------------------------------------------------------ */
void DatabaseOperation::disconnect()
{
    if (hDbc_)
    {
        SQLDisconnect(hDbc_);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc_);
        hDbc_ = nullptr;
    }
    if (hEnv_)
    {
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv_);
        hEnv_ = nullptr;
    }
}


bool DatabaseOperation::insertImage(const ImageObject& img, std::string oper)
{
    /* ------------------------------------------------------------------------ *\
     * hStmt: ODBC statement handle for executing a query.
     * ret : Stores the result of each ODBC operation.
    \* ------------------------------------------------------------------------ */
    SQLHSTMT hStmt = nullptr;
    SQLRETURN ret;
    bool bInsertionSuccess = true;

    try
    {
        connect();

        /* ------------------------------------------------------------------------ *\
         * Allocate Statement Handle
         * Converts std::chrono::system_clock::time_point to a SQL-compatible time string like "2025-06-05T10:30:00Z".
        \* ------------------------------------------------------------------------ */

        SQLAllocHandle(SQL_HANDLE_STMT, hDbc_, &hStmt);


        /* ------------------------------------------------------------------------ *\
         * Prepare SQL Statement : Uses ? placeholders for parameter binding (prevents SQL injection).
         * Bind Parameters : ImageId (string), SystemId (string), ScannedTime (string timestamp), ImageData (binary)
         * Execute SQL Command
         * Check Execution Result
        \* ------------------------------------------------------------------------ */
        std::string query = R"(
            INSERT INTO [dbo].[ImageEntries]
            (
              [ImageName], [ImageFilePath], [ImageMarked], [LiveScanMarked], [TIP_Image],
              [TIP_ImageFilePath], [ProcessType], [ScanOriginalDate], [OperatorID], [BagCount],
              [SystemID], [TransferedToSharedLocation], [BarCodeNo], [ImageHeight], [ImageWidth],
              [ImageKV], [ImageMA], [ImageOrganic], [ImageInorganic], [ImageHighDensity],
              [BarcodeGridVisiblity], [BarcodeAlphanNumeric], [TIPImageID], [MinAbsorptionLevel],
              [OrgAlarmsCount], [InorgAlarmsCount], [HighDensityAlarmsCount], [Acknowelege],
              [MessageFromSupervisor], [Lock], [Action]
            )
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        //VALUES(NULL, ? , ? , ? , ? , ? , ? , NULL, ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? )
        
        SQLPrepareA(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);

        static int iImageCnt = 0;
        // Grey_op000_0
        std::string imageName = "Grey_" + oper + "_" + intToString(++iImageCnt);
        std::string imageFilePath = "E:\\ImageArchive\\" + imageName;
        std::string imageMarked = "Yes";
        std::string liveScanMarked = "Live";
        std::string tipImage = "TIP_123";
        std::string tipImageFilePath = "C:\\Images\\TIP.bmp";
        std::string processType = "Auto";
        ////SQL_TIMESTAMP_STRUCT scanDate = { 2025, 7, 1, 12, 0, 0, 0 }; // yyyy, mm, dd, hh, mm, ss
        std::string scanDate = "20250701120000"; //yyyymmddhhmmss
        int operatorId = 101;
        int bagCount = 3;
        int systemId = 55;
        std::string transferredToSharedLocation = "Y";
        std::string barCodeNo = "BC123456";
        int imageHeight = 1024;
        int imageWidth = 768;
        int imageKV = 80;
        std::string imageMA = "1.2";
        int imageOrganic = 5;
        int imageInorganic = 2;
        int imageHighDensity = 1;
        bool barcodeGridVisiblity = true;
        bool barcodeAlphanNumeric = false;
        int tipImageID = 9001;
        int minAbsorptionLevel = 7;
        int orgAlarmsCount = 2;
        int inorgAlarmsCount = 0;
        int highDensityAlarmsCount = 1;
        int acknowelege = 0;
        std::string messageFromSupervisor = "All clear";
        bool lockFlag = false;
        std::string action = "None";

        int iPar = 0;


        // VARCHAR bindings
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_LONGVARCHAR, 500, 0, (SQLPOINTER)imageName.c_str(), 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 100, 0, (SQLPOINTER)imageFilePath.c_str(), 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)imageMarked.c_str(), 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)liveScanMarked.c_str(), 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)tipImage.c_str(), 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)tipImageFilePath.c_str(), 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)processType.c_str(), 0, NULL);

        // DATETIME
        //SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, 0, 0, &scanDate, 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 200, 0, (SQLPOINTER)scanDate.c_str(), 0, NULL);

        // INTs
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &operatorId, 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &bagCount, 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &systemId, 0, NULL);

        // VARCHAR
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)transferredToSharedLocation.c_str(), 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)barCodeNo.c_str(), 0, NULL);

        // More INTs
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &imageHeight, 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &imageWidth, 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &imageKV, 0, NULL);

        // VARCHAR
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)imageMA.c_str(), 0, NULL);

        // More INTs
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &imageOrganic, 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &imageInorganic, 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &imageHighDensity, 0, NULL);

        // BITs
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT, 0, 0, &barcodeGridVisiblity, 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT, 0, 0, &barcodeAlphanNumeric, 0, NULL);

        // More INTs
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &tipImageID, 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &minAbsorptionLevel, 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &orgAlarmsCount, 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &inorgAlarmsCount, 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &highDensityAlarmsCount, 0, NULL);
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &acknowelege, 0, NULL);

        // VARCHAR
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 500, 0, (SQLPOINTER)messageFromSupervisor.c_str(), 0, NULL);

        // BIT
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT, 0, 0, &lockFlag, 0, NULL);

        // VARCHAR
        SQLBindParameter(hStmt, ++iPar, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0, (SQLPOINTER)action.c_str(), 0, NULL);


        ret = SQLExecute(hStmt);

        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
        {
            std::cerr << "ERROR : DatabaseOperation::inserImage() : Failed to insert image : " << img.imageId << std::endl;

            SQLCHAR sqlState[6], message[256];
            SQLINTEGER nativeError;
            SQLSMALLINT textLength;

            while (SQLGetDiagRecA(SQL_HANDLE_STMT, hStmt, 1, sqlState, &nativeError,
                message, sizeof(message), &textLength) == SQL_SUCCESS)
            {
                std::cerr << "[ODBC] SQL State: " << sqlState
                    << " Message: " << message << std::endl;
                break;
            }

            bInsertionSuccess = false;
        }

        // Cleanup
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

        disconnect();

        return bInsertionSuccess;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "EXCEPTION : DatabaseOperation::insertImage() : " << ex.what() << std::endl;
        if (hStmt) SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }
}


int DatabaseOperation::getImageRowCount()
{
    SQLHSTMT hStmt = nullptr;
    SQLRETURN ret;
    int rowCount = -1;

    try
    {
        SQLAllocHandle(SQL_HANDLE_STMT, hDbc_, &hStmt);

        std::string query = "SELECT COUNT(*) FROM [dbo].[BSCSK_ImageInformation]";

        ret = SQLExecDirectA(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
        if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
        {
            ret = SQLFetch(hStmt);
            if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
            {
                SQLGetData(hStmt, 1, SQL_C_SLONG, &rowCount, 0, NULL);
            }
        }
        else
        {
            std::cerr << "ERROR : getImageRowCount() : Failed to execute query." << std::endl;

            SQLCHAR sqlState[6], message[256];
            SQLINTEGER nativeError;
            SQLSMALLINT textLength;

            while (SQLGetDiagRecA(SQL_HANDLE_STMT, hStmt, 1, sqlState, &nativeError,
                message, sizeof(message), &textLength) == SQL_SUCCESS)
            {
                std::cerr << "[ODBC] SQL State: " << sqlState
                    << " Message: " << message << std::endl;
                break;
            }
        }

        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "EXCEPTION : getImageRowCount() : " << ex.what() << std::endl;
        if (hStmt) SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    }

    return rowCount;
}


