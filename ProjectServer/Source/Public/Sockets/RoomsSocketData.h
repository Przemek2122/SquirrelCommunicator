// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include "SocketData.h"
#include "nlohmann/json_fwd.hpp"

class FProjectEngine;
class FSocket;

class FRoomsSocketData
{
public:
    explicit FRoomsSocketData(FSocket* InSocket);

    /** Function for jumping into all other functions in this class */
    void PrimarySwitch(AnyWebSocket wsVariant, nlohmann::json& JsonMessage, uWS::OpCode opCode);

    void CreateRoom(AnyWebSocket wsVariant, uWS::OpCode opCode, const std::string& RoomName);

private:
    /** Pointer to main class */
    FSocket* Socket;

    /** Engine pointer */
    FProjectEngine* ProjectEngine;

    /** Key for generating tokens  */
    std::string EncryptionKey;

};
