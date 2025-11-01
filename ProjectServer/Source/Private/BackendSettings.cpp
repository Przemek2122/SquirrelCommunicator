#include "ProjectEngine.h"
#include "BackendSettings.h"
#include "Assets/IniReader/IniManager.h"
#include "Assets/IniReader/IniObject.h"

void FBackendSettings::LoadBackendSettings()
{
	FIniManager* IniManager = FGlobalDefines::GEngine->GetAssetsManager()->GetIniManager();
	BackendSettingsIniObject = IniManager->GetIniObject("BackendSettings");
	if (BackendSettingsIniObject->DoesIniExist())
	{
		BackendSettingsIniObject->LoadIni();
	}
	else
	{
		LOG_ERROR("Backend settings are missing!");
	}
}
