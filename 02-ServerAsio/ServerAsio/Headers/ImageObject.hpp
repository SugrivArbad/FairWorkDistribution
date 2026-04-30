#pragma once

#ifndef IMAGE_OBJECT_HPP_
#define IMAGE_OBJECT_HPP_

#include <Common.hpp>

namespace nar
{
    struct ImageObject
    {
        std::vector<USHORT> imageData;
        std::string imageId;
        std::string systemId;
        std::chrono::system_clock::time_point scannedTime;

        ImageObject()
        {
            imageData = std::vector<USHORT>();
            imageId = "";
            systemId = "";
            scannedTime = std::chrono::system_clock::time_point{};
        }

        ImageObject& operator=(const ImageObject& rhs)
        {
            if (this != &rhs) // protect against self-assignment
            {
                imageData = rhs.imageData;
                imageId = rhs.imageId;
                systemId = rhs.systemId;
                scannedTime = rhs.scannedTime;
            }
            return *this;
        }

        bool operator!=(const ImageObject& rhs) const
        {
            return imageData != rhs.imageData ||
                   imageId != rhs.imageId ||
                   systemId != rhs.systemId ||
                   scannedTime != rhs.scannedTime;
        }

        bool operator>(const ImageObject& other) const
        {
            return scannedTime > other.scannedTime;
        }

        bool IsEmpty()
        {
            return imageData.empty() &&
                   imageId == "" &&
                   systemId == "" &&
                   scannedTime == std::chrono::system_clock::time_point{};
        }

        void empty()
        {
            imageData = std::vector<USHORT>();
            imageId = "";
            systemId = "";
            scannedTime = std::chrono::system_clock::time_point{};
        }
    };


    struct DecisionResult 
    {
        std::string imageId;
        std::string operatorId;
        std::string decision;      // "Accepted" or "Rejected"
        std::string decisionTime;  // ISO8601 timestamp as string
    };
}

#endif IMAGE_OBJECT_HPP_
