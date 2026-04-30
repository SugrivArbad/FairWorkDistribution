
// FILE: BinaryProtocol.hpp (updated to expect width * height imageData size)
#pragma once

#include <Common.hpp>
#include <ImageObject.hpp>
#include <boost/asio.hpp>

namespace BinaryProtocol
{
    using nar::ImageObject;
    using nar::DecisionResult;

    inline void writeString(std::ostream& os, const std::string& str)
    {
        uint32_t len = static_cast<uint32_t>(str.size());
        os.write(reinterpret_cast<const char*>(&len), sizeof(len));
        os.write(str.data(), len);
    }

    inline std::string readString(std::istream& is)
    {
        uint32_t len;
        is.read(reinterpret_cast<char*>(&len), sizeof(len));
        std::string str(len, ' ');
        is.read(&str[0], len);
        return str;
    }

    inline void serialize(const ImageObject& img, std::vector<char>& outBuffer)
    {
        std::ostringstream oss(std::ios::binary);
        writeString(oss, img.imageId);
        writeString(oss, img.systemId);

        auto ticks = std::chrono::duration_cast<std::chrono::milliseconds>(
            img.scannedTime.time_since_epoch()).count();
        oss.write(reinterpret_cast<const char*>(&ticks), sizeof(ticks));

        uint32_t size = static_cast<uint32_t>(img.imageData.size());
        oss.write(reinterpret_cast<const char*>(&size), sizeof(size));
        oss.write(reinterpret_cast<const char*>(img.imageData.data()), size * sizeof(uint16_t));

        std::string str = oss.str();
        uint32_t len = static_cast<uint32_t>(str.size());

        outBuffer.resize(sizeof(uint32_t) + len);
        uint32_t len_be = htonl(len);
        std::memcpy(outBuffer.data(), &len_be, sizeof(uint32_t));
        std::memcpy(outBuffer.data() + sizeof(uint32_t), str.data(), len);
    }

    inline ImageObject deserializeImage(const std::vector<char>& buffer)
    {
        std::istringstream iss(std::string(buffer.data(), buffer.size()), std::ios::binary);
        ImageObject img;
        img.imageId = readString(iss);
        img.systemId = readString(iss);

        int64_t ticks;
        iss.read(reinterpret_cast<char*>(&ticks), sizeof(ticks));
        img.scannedTime = std::chrono::system_clock::time_point(std::chrono::milliseconds(ticks));

        uint32_t size;
        iss.read(reinterpret_cast<char*>(&size), sizeof(size));
        img.imageData.resize(size);
        iss.read(reinterpret_cast<char*>(img.imageData.data()), size * sizeof(uint16_t));

        if (img.imageData.size() < 2)
        {
            throw std::runtime_error("Invalid imageData, less than 2 elements for dimensions");
        }

        uint16_t width = img.imageData[0];
        uint16_t height = img.imageData[1];

        if (img.imageData.size() != width * height)
        {
            throw std::runtime_error("Mismatch in imageData length vs dimensions: width=" + std::to_string(width) +
                ", height=" + std::to_string(height) +
                ", but imageData.size() = " + std::to_string(img.imageData.size()));
        }

        return img;
    }

    inline void serialize(const DecisionResult& res, std::vector<char>& outBuffer)
    {
        std::ostringstream oss(std::ios::binary);
        writeString(oss, res.imageId);
        writeString(oss, res.operatorId);
        writeString(oss, res.decision);
        writeString(oss, res.decisionTime);

        std::string str = oss.str();
        uint32_t len = static_cast<uint32_t>(str.size());

        outBuffer.resize(sizeof(uint32_t) + len);
        uint32_t len_be = htonl(len);
        std::memcpy(outBuffer.data(), &len_be, sizeof(uint32_t));
        std::memcpy(outBuffer.data() + sizeof(uint32_t), str.data(), len);
    }

    inline DecisionResult deserializeDecision(const std::vector<char>& buffer)
    {
        std::istringstream iss(std::string(buffer.data(), buffer.size()), std::ios::binary);
        DecisionResult res;
        res.imageId = readString(iss);
        res.operatorId = readString(iss);
        res.decision = readString(iss);
        res.decisionTime = readString(iss);
        return res;
    }

    inline std::vector<char> readLengthPrefixed(boost::asio::ip::tcp::socket& socket)
    {
        uint32_t len_be;
        boost::asio::read(socket, boost::asio::buffer(&len_be, sizeof(len_be)));
        uint32_t len = ntohl(len_be);

        std::vector<char> buffer(len);
        boost::asio::read(socket, boost::asio::buffer(buffer.data(), len));
        return buffer;
    }

    inline void writeBuffer(boost::asio::ip::tcp::socket& socket, const std::vector<char>& buffer)
    {
        boost::asio::write(socket, boost::asio::buffer(buffer));
    }

    inline void sendImageToOperator(boost::asio::ip::tcp::socket& socket, const ImageObject& image)
    {
        std::vector<char> buffer;
        serialize(image, buffer);
        writeBuffer(socket, buffer);
    }

    inline DecisionResult readOperatorDecision(boost::asio::ip::tcp::socket& socket)
    {
        std::vector<char> buffer = readLengthPrefixed(socket);
        return deserializeDecision(buffer);
    }

    inline std::string toHex(const std::string& binary)
    {
        std::ostringstream oss;
        for (unsigned char c : binary)
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
        return oss.str();
    }

    inline std::string toUuidString(const std::string& binary)
    {
        if (binary.size() != 16) return toHex(binary);  // fallback
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (int i = 0; i < 16; ++i)
        {
            if (i == 4 || i == 6 || i == 8 || i == 10) oss << "-";
            oss << std::setw(2) << (static_cast<unsigned int>(binary[i]) & 0xFF);
        }
        return oss.str();
    }
}
