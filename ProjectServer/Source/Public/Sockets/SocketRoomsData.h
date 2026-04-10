// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

class FProjectEngine;
class FSocket;

class FSocketRoomsData
{
public:
    explicit FSocketRoomsData(FSocket* InSocket);

private:
    /** Pointer to main class */
    FSocket* Socket;

    /** Engine pointer */
    FProjectEngine* ProjectEngine;

};
