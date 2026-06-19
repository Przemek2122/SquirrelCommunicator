// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#pragma once

#include "EngineCompat.h"

/** Class for global backend settings */
class FBackendSettings
{
public:
    FBackendSettings();

    void LoadBackendSettings();

    std::shared_ptr<FIniObject> GetBackendSettingsIni() const { return BackendSettingsIniObject; }

    /** Max number of letters per message */
    int32 GetMaxMessageSize() const { return MaxMessageSize; }

protected:
    /** Settings ini object */
    std::shared_ptr<FIniObject> BackendSettingsIniObject;

    /** Max number of letters per message */
    int32 MaxMessageSize;
};
