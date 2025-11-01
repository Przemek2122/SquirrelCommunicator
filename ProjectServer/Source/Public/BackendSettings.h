// Created by Przemys³aw Wiewióra 2020-2025 https://github.com/Przemek2122/Engine
#pragma once

#include "CoreMinimal.h"

/** Class for managing backend settings */
class FBackendSettings
{
public:
    void LoadBackendSettings();

    std::shared_ptr<FIniObject> GetBackendSettingsIni() const { return BackendSettingsIniObject; }

protected:
    /** Settings ini object */
    std::shared_ptr<FIniObject> BackendSettingsIniObject;

};
