#pragma once

#ifndef IMAGE_SERVER_HPP_
#define IMAGE_SERVER_HPP_

#include <ImageObject.hpp>
#include <Session.hpp>
#include <DatabaseOperation.hpp>

namespace nar
{
    typedef enum class OperatorState : char
    {
        BUSY,
        AVAILABLE
    } operator_state_t;

    typedef struct OperatorInfo
    {
        operator_state_t state = operator_state_t::AVAILABLE;
        std::chrono::system_clock::time_point tpAvailableTime = std::chrono::system_clock::now();
    } operator_info_t;


    class ImageServer
    {
    public:
        // public member variables
        bool bClientsPaused = false;
        std::map<std::string, std::shared_ptr<Session>> clients_;
        std::map<std::string, operator_info_t> operatorIdleMap_;
        std::map<std::string, std::shared_ptr<Session>> operators_;
        std::priority_queue<ImageObject, std::vector<ImageObject>, std::greater<>> imageQueue_;
        std::mutex mutex_;

        DatabaseOperation db;

    private:
        // private member variables
        tcp::acceptor acceptor_;
        
        std::function<void(const ImageObject&)> dbHandler_;

    public :
        // constructor
        ImageServer(boost::asio::io_context& io_context, short port);
    
        // public member fuctions
        void registerClient(std::shared_ptr<Session> client, const std::string& systemId);
        void registerOperator(std::shared_ptr<Session> oper, const std::string& systemId);
        void unregisterClient(const std::string& systemId);
        void unregisterOperator(const std::string& systemId);

        void pauseClients();
        void resumeClients();
        void assignImages();

        void setDbHandler(std::function<void(const ImageObject&)> dbHandler);

    private:
        // private member functions
        void accept();
        void handleImage(const ImageObject& img);
        bool allOperatorsStillBusy();
        void sendImageToOperator(tcp::socket& operator_socket, const ImageObject& image);
    };
}

#endif IMAGE_SERVER_HPP_
