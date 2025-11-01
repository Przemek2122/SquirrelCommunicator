// Created by Przemys³aw Wiewióra 2020-2025 https://github.com/Przemek2122/Engine
#pragma once

#include "CoreMinimal.h"

/** This class could be way better, but for now, accept anything */
class FCORPolicy
{
public:
	FCORPolicy();

	CMap<std::string, std::string> GetCORHeaders() const { return CORHeaders; }

protected:
	CMap<std::string, std::string> CORHeaders;
};
