// Created by https://www.linkedin.com/in/przemek2122/ 2020-2026

#include "ProjectEngine.h"
#include "BackendSettings.h"
#include "SQRLLIniObject.h"
#include "Logger/Logger.h"

FBackendSettings::FBackendSettings()
	: MaxMessageSize(1024)
{
}

void FBackendSettings::LoadBackendSettings()
{
	// Standalone: construct ini object directly with path (no IniManager needed)
	BackendSettingsIniObject = std::make_shared<SQRLLIniObject>("./Assets/Config/BackendSettings.ini");
	BackendSettingsIniObject->LoadIni();
	if (BackendSettingsIniObject->IsLoaded())
	{
		const FIniField MaxMessageSizeField = BackendSettingsIniObject->FindFieldByName("MaxMessageSize");
		if (MaxMessageSizeField.IsValid())
		{
			MaxMessageSize = MaxMessageSizeField.GetValueAsInt();
		}
	}
	else
	{
		LOG_ERROR("Backend settings are missing!");
	}
}
