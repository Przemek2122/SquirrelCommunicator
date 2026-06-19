// Created by https://www.linkedin.com/in/przemek2122/ 2020-2025 https://github.com/Przemek2122/Engine
#pragma once

#include "EngineCompat.h"

/** This class could be way better, but for now, accept anything */
class FCORPolicy
{
public:
	FCORPolicy();

	CUnorderedMap<std::string, std::string> GetCORHeaders() const { return CORHeaders; }

protected:
	CUnorderedMap<std::string, std::string> CORHeaders;
};
