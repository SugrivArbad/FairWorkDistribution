#pragma once

// local headers
#include <Common.hpp>
#include <ImageObject.hpp>
#include <ImageServer.hpp>
#include <Session.hpp>
#include <BinaryProtocol.hpp>
#include <DatabaseOperation.hpp>

using namespace nar;

// preporcessors
#define CLIENT "Client"
#define OPERATOR "Operator"


#ifndef TRACE_LEVEL_1
#define TRACE_LEVEL_1
#endif TRACE_LEVEL_1
////#undef TRACE_LEVEL_1

#ifndef TRACE_LEVEL_2
#define TRACE_LEVEL_2
#endif TRACE_LEVEL_2
#undef TRACE_LEVEL_2

// global variables
const int t2 = 2000; // Time in milliseconds to wait when all operators are busy


// global functions ------------------- START

std::string format_iso8601(const std::chrono::system_clock::time_point& tp)
{
    std::time_t time_t_obj = std::chrono::system_clock::to_time_t(tp);
    //std::tm tm_obj = *std::gmtime(&time_t_obj); // convert to UTC
    std::tm tm_obj;
    std::ostringstream oss;

    errno_t err = gmtime_s(&tm_obj, &time_t_obj); // convert to UTC
    if (err)
    {
        if (err == EINVAL)
        {
            std::cerr << " ERROR : IN format_iso8601() : EINVAL – if either pointer is NULL." << std::endl;
        }
        else if (err == ERANGE)
        {
            std::cerr << " ERROR : IN format_iso8601() : ERANGE – if the time_t value cannot be represented as a struct tm." << std::endl;
        }
        else
        {
            std::cerr << " ERROR : IN format_iso8601() : Something else" << std::endl;
        }

        oss << "";
    }
    else
    {
        oss << std::put_time(&tm_obj, "%Y-%m-%dT%H:%M:%SZ");
        printf("  UTC time: %d-%02d-%02d %02d:%02d:%02d\n",
            tm_obj.tm_year + 1900, tm_obj.tm_mon + 1, tm_obj.tm_mday,
            tm_obj.tm_hour, tm_obj.tm_min, tm_obj.tm_sec);
    }

    return oss.str(); // e.g., 2025-05-19T14:30:00Z
}

std::string intToString(int iCnt)
{
    std::string strRet = "";

    if (iCnt > 0)
    {
        while (iCnt > 0)
        {
            char c = '0' + (iCnt % 10);
            strRet = c + strRet;
            iCnt /= 10;
        }
    }
    else
    {
        strRet = "0";
    }

    return strRet;
}

// global functions ------------------- END

/* ---------------------------------------------------------------------------------------------------------------- *\
 * #### Session class ####
 * Session is created here for client machine or operator
\* ---------------------------------------------------------------------------------------------------------------- */

// constructor
Session::Session(boost::asio::ip::tcp::socket socket, ImageServer* server)
    : socket_(std::move(socket)), serverRef_(server) { }

// Session : public member functions ------------------ START

void Session::destroy()
{
    imageHandler_ = nullptr;
    serverRef_ = nullptr;
    socket_.close();
}

//////void Session::handshake()
//////{
//////#ifdef TRACE_LEVEL_1
//////    std::cout << std::endl << " ************************************************** " << std::endl;
//////    std::cout << "\t Session::handshake() => " << std::endl;
//////#endif // TRACE_LEVEL_1
//////
//////    boost::asio::streambuf buf;
//////    std::size_t n = boost::asio::read_until(socket_, buf, '\n');  // blocking read until newline
//////
//////    std::istream is(&buf);
//////    std::string line;
//////    std::getline(is, line);  // remove newline
//////
//////#ifdef TRACE_LEVEL_2
//////    std::cout << "\t1Session::handshake() 1 =>" << std::endl;
//////#endif // TRACE_LEVEL_2
//////
//////    try
//////    {
//////        auto json = nlohmann::json::parse(line);
//////        role_ = json["Type"];
//////        // for client machine, systemId_ is system name (or ip address) for now and for operator, systemId_ is operator id (or login id)
//////        systemId_ = json["SystemId"];
//////
//////#ifdef TRACE_LEVEL_1
//////        std::cout << "\t Session::handshake() 2 => role_ = " << role_ << "  systemId_ = " << systemId_ << std::endl;
//////#endif // TRACE_LEVEL_1
//////
//////        if (role_ == CLIENT)
//////        {
//////            serverRef_->registerClient(shared_from_this(), systemId_);
//////            readClientMachine();
//////
//////            /* ---------------------------------------------------------------------------------------------------------------- *\
//////                * if not a single operator is available then do not start the machine
//////            \* ---------------------------------------------------------------------------------------------------------------- */
//////            if (serverRef_->operatorIdleMap_.empty())
//////            {
//////                serverRef_->pauseClients();
//////            }
//////        }
//////        else if (role_ == OPERATOR)
//////        {
//////            serverRef_->registerOperator(shared_from_this(), systemId_);
//////            readOperator();
//////
//////            /* ---------------------------------------------------------------------------------------------------------------- *\
//////                * check if client machine has stopped and if stopped then start those becuase new operator is ready for analysis
//////            \* ---------------------------------------------------------------------------------------------------------------- */
//////            if (serverRef_->imageQueue_.empty() && serverRef_->bClientsPaused && !serverRef_->clients_.empty())
//////            {
//////                serverRef_->resumeClients();
//////            }
//////            else
//////            {
//////                std::lock_guard<std::mutex> lock(serverRef_->mutex_);
//////                serverRef_->assignImages();
//////            }
//////        }
//////        else
//////        {
//////            socket_.close();
//////        }
//////    }
//////    catch (...)
//////    {
//////        std::cerr << std::endl << " *** ERROR : Failed to parse initial identity JSON." << std::endl;
//////        socket_.close();
//////    }
//////#ifdef TRACE_LEVEL_1
//////std::cout << " ************************************************** " << std::endl << std::endl;
//////#endif // TRACE_LEVEL_1
//////}


void Session::handshake()
{
#ifdef TRACE_LEVEL_1
    std::cout << std::endl << " ************************************************** " << std::endl;
    std::cout << "\t Session::handshake() => " << std::endl;
#endif // TRACE_LEVEL_1

    boost::asio::streambuf buf;
    std::size_t n = boost::asio::read_until(socket_, buf, '\n');  // blocking read until newline

    std::istream is(&buf);
    char roleByte;
    is.get(roleByte);

#ifdef TRACE_LEVEL_2
    std::cout << "\t1Session::handshake() 1 =>" << std::endl;
#endif // TRACE_LEVEL_2

    try
    {
        uint32_t idLen_be;
        is.read(reinterpret_cast<char*>(&idLen_be), sizeof(idLen_be));
        uint32_t idLen = ntohl(idLen_be);

        std::string id(idLen, '\0');
        is.read(&id[0], idLen);
        systemId_ = id;

        if (roleByte == 0x01)
            role_ = CLIENT;
        else if (roleByte == 0x02)
            role_ = OPERATOR;
        else {
            std::cerr << "ERROR : handshake() : Unknown role byte in handshake\n";
            destroy();
            return;
        }

#ifdef TRACE_LEVEL_1
        std::cout << "\t Session::handshake() 2 => role_ = " << role_ << "  systemId_ = " << systemId_ << std::endl;
#endif // TRACE_LEVEL_1

        if (role_ == CLIENT)
        {
            serverRef_->registerClient(shared_from_this(), systemId_);
            readClientMachine();

            /* ---------------------------------------------------------------------------------------------------------------- *\
                * if not a single operator is available then do not start the machine
            \* ---------------------------------------------------------------------------------------------------------------- */
            if (serverRef_->operatorIdleMap_.empty())
            {
                serverRef_->pauseClients();
            }
        }
        else if (role_ == OPERATOR)
        {
            serverRef_->registerOperator(shared_from_this(), systemId_);
            readOperator();

            /* ---------------------------------------------------------------------------------------------------------------- *\
                * check if client machine has stopped and if stopped then start those becuase new operator is ready for analysis
            \* ---------------------------------------------------------------------------------------------------------------- */
            if (serverRef_->imageQueue_.empty() && serverRef_->bClientsPaused && !serverRef_->clients_.empty())
            {
                serverRef_->resumeClients();
            }
            else
            {
                std::lock_guard<std::mutex> lock(serverRef_->mutex_);
                serverRef_->assignImages();
            }
        }
        else
        {
            socket_.close();
        }
    }
    catch (...)
    {
        std::cerr << std::endl << " *** ERROR : Failed to parse initial identity JSON." << std::endl;
        socket_.close();
    }
#ifdef TRACE_LEVEL_1
    std::cout << " ************************************************** " << std::endl << std::endl;
#endif // TRACE_LEVEL_1
}

void Session::start()
{
    handshake();
}


void Session::readClientMachine()
{
    auto self(shared_from_this());


    static int cnt = 0;
    ++cnt;

#ifdef TRACE_LEVEL_1
    std::cout << " *********** In readClientMachine() " << cnt << " *************************" << std::endl;
#endif // TRACE_LEVEL_1

    // Read 4 bytes (message length)
    boost::asio::async_read(socket_, boost::asio::buffer(&expectedLength_, sizeof(expectedLength_)),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
#ifdef TRACE_LEVEL_2
                std::cout << cnt << "] ******* CLIENT ******************************" << std::endl;
#endif // TRACE_LEVEL_1

                uint32_t len = ntohl(expectedLength_); // Convert from network byte order
                dataBuffer_.resize(len);

                // Now read the actual payload
                boost::asio::async_read(socket_, boost::asio::buffer(dataBuffer_.data(), dataBuffer_.size()),
                    [this, self](boost::system::error_code ec2, std::size_t /*length*/)
                    {
                        if (!ec2)
                        {
                            try
                            {
                                // Decode and process the image object
                                auto image = BinaryProtocol::deserializeImage(dataBuffer_);

#ifdef TRACE_LEVEL_1
                                std::cout << "\t " << systemId_ << "] role_ = " << role_ << " imageId = " << image.imageId << " systemId = " << image.systemId << " imageData = " << image.imageData[0] << " scannedTime = " << "" << std::endl;
#endif // TRACE_LEVEL_1

                                //ImageServer::handleImage();
                                imageHandler_(image); // call goes to ImageServer::handleImage()
                            }
                            catch (const std::exception& ex)
                            {
                                std::cerr << "ERROR : readClientMachine() : Failed to deserialize image: " << ex.what() << std::endl;
                            }


                            // Continue reading next image
                            readClientMachine();
                        }
                        else
                        {
                            std::cerr << "ERROR : readClientMachine() : Failed to read image data : " << ec2.message() << std::endl;

                            /* -------------------------------------------------- *\
                             * Log the disconnection
                             * Remove the client / operator from your server maps
                             * Clean up Session state
                            \* -------------------------------------------------- */
                            serverRef_->unregisterClient(systemId_);
                        }
                    });
            }
            else
            {
                std::cerr << "ERROR : readClientMachine() : Failed to read length prefix : " << ec.message() << std::endl;

                /* -------------------------------------------------- *\
                 * Log the disconnection
                 * Remove the client / operator from your server maps
                 * Clean up Session state
                \* -------------------------------------------------- */
                serverRef_->unregisterClient(systemId_);
            }
        });
}



void Session::readOperator()
{
    auto self(shared_from_this());

    static int cnt = 0;
    ++cnt;

#ifdef TRACE_LEVEL_1
    std::cout << " *********** In readOperator() " << cnt << " *************************" << std::endl;
#endif // TRACE_LEVEL_1

    boost::asio::async_read(socket_, boost::asio::buffer(&expectedLength_, sizeof(expectedLength_)),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                uint32_t len = ntohl(expectedLength_);
                dataBuffer_.resize(len);

                boost::asio::async_read(socket_, boost::asio::buffer(dataBuffer_.data(), dataBuffer_.size()),
                    [this, self](boost::system::error_code ec2, std::size_t /*length*/) {
                        if (!ec2) {
                            try {
                                auto result = BinaryProtocol::deserializeDecision(dataBuffer_);
#ifdef TRACE_LEVEL_1
                                std::cout << std::endl << "\t" << systemId_ << "] role_ = " << role_
                                    << " imageId = " << result.imageId
                                    << " operatorId = " << result.operatorId
                                    << " decision = " << result.decision
                                    << " decisionTime = " << result.decisionTime << std::endl << std::endl;
#endif
                                serverRef_->operatorIdleMap_[systemId_].tpAvailableTime = std::chrono::system_clock::now();
                                serverRef_->operatorIdleMap_[systemId_].state = nar::operator_state_t::AVAILABLE;

                                if (serverRef_->bClientsPaused) {
                                    if (!serverRef_->imageQueue_.empty()) {
                                        std::lock_guard<std::mutex> lock(serverRef_->mutex_);
                                        serverRef_->assignImages();
                                    }
                                    serverRef_->resumeClients();
                                }
                            }
                            catch (const std::exception& ex) {
                                std::cerr << "EXCEPTION : readOperator() : " << ex.what() << std::endl;
                            }
                            readOperator();
                        }
                        else {
                            std::cerr << "ERROR : readOperator() : Failed to read message body: " << ec2.message() << std::endl;
                            serverRef_->unregisterOperator(systemId_);
                        }
                    });
            }
            else {
                std::cerr << "ERROR : readOperator() : Failed to read length prefix: " << ec.message() << std::endl;
                serverRef_->unregisterOperator(systemId_);
            }
        });

}


void Session::write(const std::string& message)
{
    try
    {
        boost::asio::write(socket_, boost::asio::buffer(message));
    }
    catch (const boost::system::system_error& e)
    {
        std::cerr << " EXCEPTION : Session::write() : Synchronous write failed: " << e.what() << std::endl;
    }
}


void Session::setImageHandler(std::function<void(const ImageObject&)> handler)
{
    imageHandler_ = handler;
}

// Session : public member functions ------------------ END


// Session : private member functions ------------------ START

std::chrono::system_clock::time_point Session::parse_iso8601_to_time_point(const std::string& timeStr)
{

    ////std::string raw_time = timeStr;
    ////size_t dot_pos = raw_time.find('.');
    ////if (dot_pos != std::string::npos)
    ////{
    ////    size_t z_pos = raw_time.find('Z', dot_pos);
    ////    if (z_pos != std::string::npos)
    ////    {
    ////        // Keep only up to 6 digits after the dot for microseconds
    ////        raw_time = raw_time.substr(0, dot_pos + 7) + "Z";
    ////    }
    ////}

    ////std::istringstream ss(raw_time);
    ////std::tm tm = {};
    ////ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    ////if (ss.fail()) {
    ////    throw std::runtime_error("Failed to parse scannedTime: " + raw_time);
    ////}
    ////std::time_t time_c = std::mktime(&tm);
    ////return std::chrono::system_clock::from_time_t(time_c);

    std::string raw_time = timeStr;
    size_t t_pos = raw_time.find('T');
    size_t dot_pos = raw_time.find('.', t_pos);
    if (dot_pos != std::string::npos)
    {
        raw_time = raw_time.substr(0, dot_pos) + "Z";
    }

    std::tm tm = {};
    std::istringstream ss(raw_time);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

    if (ss.fail())
    {
        throw std::runtime_error("Failed to parse scannedTime: " + raw_time);
    }

    // Use timegm (POSIX) or equivalent to interpret as UTC
#ifdef _WIN32
    std::time_t time_c = _mkgmtime(&tm);  // Windows-specific UTC conversion
#else
    std::time_t time_c = timegm(&tm);     // POSIX UTC version of mktime
#endif

    return std::chrono::system_clock::from_time_t(time_c);

}

// Session : private member functions ------------------ END







/* ---------------------------------------------------------------------------------------------------------------- *\
 * #### ImageServer class ####
\* ---------------------------------------------------------------------------------------------------------------- */

ImageServer::ImageServer(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
{
    /* -------------------------------------------------- *\
     * Creates a TCP acceptor bound to the given port.
     * Starts accepting connections by calling accept().
    \* -------------------------------------------------- */
    std::cout << " In ImageServer Constructor " << std::endl;

    accept();
}

void ImageServer::accept()
{
#ifdef TRACE_LEVEL_1
    std::cout << " In accept() 1" << std::endl;
#endif // TRACE_LEVEL_1

    /* -------------------------------------------------- *\
        * Waits asynchronously for new TCP connections.
        * When a connection is accepted:
        - Wraps it in a Session.
        - Adds it to the client list.
        - Sets the callback for handling incoming ImageObjects.
        - Starts reading from the socket.
        - Calls accept() again to accept more clients.
    \* -------------------------------------------------- */
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
#ifdef TRACE_LEVEL_2
            std::cout << " In acceptor_.async_accept()" << std::endl;
#endif // TRACE_LEVEL_2

            if (!ec)
            {
                static int cnt = 0;
                ++cnt;
#ifdef TRACE_LEVEL_2
                std::cout << " **** Client accepting ... " << cnt << std::endl << std::endl;
#endif // TRACE_LEVEL_2

                auto session = std::make_shared<Session>(std::move(socket), this);

                session->setImageHandler(
                    [this](const ImageObject& img)
                    {
                        handleImage(img);
                    });
                session->start();

#ifdef TRACE_LEVEL_1
                std::cout << " **** Session started for " << cnt << " client : " << cnt << std::endl << std::endl;
#endif // TRACE_LEVEL_1
            }
            else
            {
                std::cerr << "ERROR : ImageServer::accept() : Failed to establish connection with client machine or operator : MESSAGE => " << ec.message() << std::endl;
            }

#ifdef TRACE_LEVEL_2
            std::cout << " In acceptor_.async_accept() " << std::endl;
#endif // TRACE_LEVEL_2

            accept();
        });
}

void ImageServer::handleImage(const ImageObject& img)
{
    std::lock_guard<std::mutex> lock(mutex_);

    /* -------------------------------------------------- *\
        * Adds image to imageQueue_(a min - heap based on scan time).
        * Calls assignImages() to try and assign it to an operator.
    \* -------------------------------------------------- */

#ifdef TRACE_LEVEL_2
    std::cout << "  In handleImage() " << std::endl;
#endif // TRACE_LEVEL_2

    ////if (dbHandler_)
    ////{
    ////    dbHandler_(img); // insert into DB immediately
    ////}

    imageQueue_.push(img);
    assignImages();

#ifdef TRACE_LEVEL_2
    std::cout << "  Out handleImage() " << std::endl;
#endif // TRACE_LEVEL_2
}

void ImageServer::assignImages()
{
#ifdef TRACE_LEVEL_2
    std::cout << "  In assignImages() : imageQueue_.size() =  " << imageQueue_.size() << std::endl;
#endif // TRACE_LEVEL_2

    bool bOperatorIsAvailable = true;
    std::string strSelectedOperator = "";
    ImageObject img;

    try
    {
        while (bOperatorIsAvailable && !imageQueue_.empty())
        {
            /* --------------------------------------------------------------------------------------- *\
                * If no operators are registered(empty map), break.
                * Finds the most idle operator (the one with earliest idle time).
                * Assigns the image to that operator's idle time to now and make its state as BUSY.
                * If no operator is idle :
                    - Reinsert the image into the queue.
                    - Sleep for t2 ms.
                    - If all operators still busy, call pauseClients().
            \* --------------------------------------------------------------------------------------- */

            img.empty();
#ifdef TRACE_LEVEL_1
            std::cout << "\t 1. assignImages() : operatorIdleMap_.empty() =  " << operatorIdleMap_.empty() << "   imageQueue_.size() = " << imageQueue_.size() << std::endl;
#endif // TRACE_LEVEL_1

            if (operatorIdleMap_.empty())
            {
                if (allOperatorsStillBusy())
                    pauseClients();

                bOperatorIsAvailable = false;
            }
            else
            {

#ifdef TRACE_LEVEL_2
                std::cout << "\t 2. assignImages() " << std::endl;
#endif // TRACE_LEVEL_2

                ImageObject img = imageQueue_.top();
                imageQueue_.pop();

                strSelectedOperator = "";
                auto maxIdle = std::chrono::system_clock::now();

                for (auto& [opId, idleState] : operatorIdleMap_)
                {
                    if (idleState.state == operator_state_t::AVAILABLE && idleState.tpAvailableTime < maxIdle)
                    {
                        maxIdle = idleState.tpAvailableTime;
                        strSelectedOperator = opId;
                    }
                }

                if (!strSelectedOperator.empty())
                {
                    operatorIdleMap_[strSelectedOperator].state = operator_state_t::BUSY;
                    operatorIdleMap_[strSelectedOperator].tpAvailableTime = std::chrono::system_clock::now();
                    sendImageToOperator(operators_[strSelectedOperator]->socket_, img);

#ifdef TRACE_LEVEL_1
                    std::cout << "\t** Assigned image to operator : " << strSelectedOperator << std::endl;
#endif // TRACE_LEVEL_1

                    std::thread thImgInsert(&DatabaseOperation::insertImage, &db, img, strSelectedOperator);
                    thImgInsert.detach();
                }
                else
                {
                    imageQueue_.push(img);
                    // THINK : removing t2 time wait as it is not neceesary, so commenting it
                    //////std::this_thread::sleep_for(std::chrono::milliseconds(t2));

                    /* -------------------------------------------------- *\
                        * Checks whether any operator has been idle in the last 1 ns(simulation).
                        * If all are busy, returns true.
                    \* -------------------------------------------------- */
                    if (allOperatorsStillBusy() && !clients_.empty())
                    {
                        pauseClients();
                        bOperatorIsAvailable = false;
                    }

#ifdef TRACE_LEVEL_1
                    std::cout << "\t** COULDN'T ABLE TO assing image to operator : " << strSelectedOperator << "   imageQueue_.size() = " << imageQueue_.size() << std::endl;
#endif // TRACE_LEVEL_1

                }
            }
        }
    }
    catch (const std::exception& ex)
    {
        /* -------------------------------------------------- *\
         * Log the disconnection
         * Remove the client / operator from your server maps
         * Remove the client / operator from your server maps
         * Clean up Session state
        \* -------------------------------------------------- */
        std::cerr << "EXCEPTION : ImageServer::assignImages() : Failed to send image : MESSAGE => " << ex.what() << std::endl;

        /* ----------------------------------------------------------------------- *\
         * if top image from imageQueue_ is not same as popped image
         * OR imageQueue_ is empty and popped image is not empty
         * it means after popping image from iimageQueue_
            then the operator connection has failed so again pushing image into imageQueue_
        \* ----------------------------------------------------------------------- */
        if ((!imageQueue_.empty() && imageQueue_.top() != img) || (imageQueue_.empty() && !img.IsEmpty()))
        {
            imageQueue_.push(img);
        }
        unregisterOperator(strSelectedOperator);

        std::cerr << "\t Operator [" << strSelectedOperator << "] disconnected: " << std::endl;
    }

#ifdef TRACE_LEVEL_2
    std::cout << "  OUT assignImages() : imageQueue_.size() =  " << imageQueue_.size() << std::endl;
#endif // TRACE_LEVEL_2
}

bool ImageServer::allOperatorsStillBusy()
{
    auto now = std::chrono::system_clock::now();
    for (const auto& [opId, idleState] : operatorIdleMap_)
    {
        if (idleState.state == operator_state_t::AVAILABLE && (now - idleState.tpAvailableTime) > std::chrono::nanoseconds(10))
            return false;
    }
    return true;
}

void ImageServer::sendImageToOperator(tcp::socket& operator_socket, const ImageObject& image)
{
    try
    {
        BinaryProtocol::sendImageToOperator(operator_socket, image);

        std::cout << "[Server] Sent image to operator: " << BinaryProtocol::toUuidString(image.imageId) << std::endl;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "EXCEPTION : ImageServer::sendImageToOperator() : Failed to send image : MESSAGE => " << ex.what() << std::endl;
        // TODO : remove respective operator's session and references
        throw ex;
        // [Server] Failed to send image: write: An existing connection was forcibly closed by the remote host [system:10054]
    }
}

void ImageServer::pauseClients()
{
#ifdef TRACE_LEVEL_1
    std::cout << std::endl << "Pausing all clients..." << std::endl << std::endl;
#endif // TRACE_LEVEL_1

    for (auto& client : clients_)
    {
        /* ------------------------------------------------------------------------------------ *\
            * Sends a "PAUSE" message to all connected clients.
            * Clients would need to handle this message by stopping scans (handled in client app).
        \* ------------------------------------------------------------------------------------ */
        std::string strPauseMsg = "PAUSE\n";
        client.second->write(strPauseMsg);
    }

    bClientsPaused = true;
}

void ImageServer::resumeClients()
{
#ifdef TRACE_LEVEL_1
    std::cout << std::endl << "Resuming all clients..." << std::endl << std::endl;
#endif // TRACE_LEVEL_1

    for (auto& client : clients_)
    {
        /* ------------------------------------------------------------------------------------ *\
            * Sends a "RESUME" message to all connected clients.
            * Clients would need to handle this message by stopping scans (handled in client app).
        \* ------------------------------------------------------------------------------------ */
        std::string strResumeMsg = "RESUME\n";
        client.second->write(strResumeMsg);
    }

    bClientsPaused = false;
}


void ImageServer::registerClient(std::shared_ptr<Session> client, const std::string& systemId)
{
    try
    {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_[systemId] = client;

        bClientsPaused = false;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "EXCEPTION : ImageServer::registerClient() : Failed to register client : " << systemId << " : MESSAGE = > " << ex.what() << std::endl;
    }

#ifdef TRACE_LEVEL_2
    std::cout << " ImageServer::registerOperator => [Server] Registered client: " << systemId << std::endl;
#endif // TRACE_LEVEL_2
}

void ImageServer::registerOperator(std::shared_ptr<Session> oper, const std::string& systemId)
{
    try
    {
        std::lock_guard<std::mutex> lock(mutex_);
        operators_[systemId] = oper;
        operatorIdleMap_[systemId].tpAvailableTime = std::chrono::system_clock::now();
    }
    catch (const std::exception& ex)
    {
        std::cerr << "EXCEPTION : ImageServer::registerOperator() : Failed to unregister client : " << systemId << " : MESSAGE = > " << ex.what() << std::endl;
    }

#ifdef TRACE_LEVEL_2
    std::cout << " ImageServer::registerOperator => [Server] Registered operator: " << systemId << std::endl;
#endif // TRACE_LEVEL_2
}

void ImageServer::unregisterClient(const std::string& systemId)
{
    try
    {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_[systemId]->destroy();
        clients_.erase(systemId);

        if (clients_.empty())
            bClientsPaused = true;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "EXCEPTION : ImageServer::unregisterClient() : Failed to unregister client : " << systemId << " : MESSAGE = > " << ex.what() << std::endl;
    }

#ifdef TRACE_LEVEL_1
    std::cout << " ImageServer::unregisterClient => [Server] Unregistered client machine : " << systemId << std::endl;
#endif // TRACE_LEVEL_1
}

void ImageServer::unregisterOperator(const std::string& systemId)
{
    try
    {
        std::lock_guard<std::mutex> lock(mutex_);
        operators_[systemId]->destroy();
        operators_.erase(systemId);
        operatorIdleMap_.erase(systemId);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "EXCEPTION : ImageServer::unregisterOperator() : Failed to unregister operator : " << systemId << " : MESSAGE = > " << ex.what() << std::endl;
    }

#ifdef TRACE_LEVEL_1
    std::cout << " ImageServer::unregisterOperator => [Server] Unregistered operator : " << systemId << std::endl;
#endif // TRACE_LEVEL_1

}

void ImageServer::setDbHandler(std::function<void(const ImageObject&)> dbHandler)
{
    dbHandler_ = dbHandler;
}










int main()
{
    try
    {
        std::cout << "***********************************************************************" << std::endl << std::endl;
        std::cout << "      SERVER STARTED " << std::endl;
        std::cout << "***********************************************************************" << std::endl << std::endl;
        
        ////nar::DatabaseOperation db(connStr);

        /* -------------------------------------------------- *\
         * Initializes Boost.Asio's io_context.
         * Creates an ImageServer object on port 12345.
         * Runs the I/O context (starts event loop).
        \* -------------------------------------------------- */
        std::cout << " In main() 1" << std::endl;
        boost::asio::io_context io_context;
        ImageServer server(io_context, 12345);

        //////// Simple lambda: pass the DB insert to your ImageServer logic
        //////server.setDbHandler([&db](const nar::ImageObject& img) {
        //////    db.insertImage(img);
        //////    });


        io_context.run();
        std::cout << " In main() 2" << std::endl;
    }
    catch (std::exception& e)
    {
        std::cerr << "EXCEPTION main() : " << e.what() << "\n";
    }
    return 0;
}






/* -------------------------------------------------------------------------------- *\

1] LABDA FUNCTION :
    Lambda functions in C++ are anonymous functions—functions without a name
        that can be defined in-line. They are useful for short, local operations,
        especially when passing functions as arguments
        (e.g., to algorithms like std::sort or std::for_each).

    Basic Syntax :
        [capture](parameters) -> return_type {
            // function body
        };

    Each part explained:
        Part            Description
        capture         Variables from the surrounding scope you want to use in the lambda.
        parameters      Function arguments (like any normal function).
        return_type     Optional. Inferred by default unless explicitly needed.
        {...}           The function body.


2] self var :
    is a common idiom in modern C++ to safely obtain a shared_ptr
    to the current object (this) from within a class that inherits from
        std::enable_shared_from_this<T>.

    stmt : auto self(shared_from_this());


    shared_from_this()
        Provided by std::enable_shared_from_this<T>
        Returns a std::shared_ptr<T> that shares ownership of *this
        Only works if the current object is already managed by a shared_ptr
    auto self(...)
        This creates a local variable named self that is a shared_ptr<T> (type deduced by auto)
        Useful when you want to:
        Keep the object alive during async operations, callbacks, etc.
        Capture a shared_ptr in a lambda
\* -------------------------------------------------------------------------------- */


