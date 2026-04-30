#pragma once

#ifndef SESSION_HPP_
#define SESSION_HPP_

#include <ImageServer.hpp>

namespace nar
{
    class ImageServer; // Forward declaration

    class Session : public std::enable_shared_from_this<Session>
    {
    private:
        // private memeber variables
        std::string partialData_;
        std::function<void(const ImageObject&)> imageHandler_;
        std::string role_;
        std::string systemId_;
        std::string readBuffer_;
        ImageServer* serverRef_;  // Pointer to the owning server

        uint32_t expectedLength_;
        std::vector<char> dataBuffer_;

    public:
        // public member variables
        tcp::socket socket_;

    public:
        // constructor
        Session(boost::asio::ip::tcp::socket socket, ImageServer* server);
        ~Session() {};

        // public memeber functions
        void handshake();
        void start();
        void readClientMachine();
        void readOperator();
        void write(const std::string& message);
        void setImageHandler(std::function<void(const ImageObject&)> handler);

        void destroy();

    private:
        // private member functions
        std::chrono::system_clock::time_point parse_iso8601_to_time_point(const std::string& timeStr);
    };
}

#endif SESSION_HPP_

