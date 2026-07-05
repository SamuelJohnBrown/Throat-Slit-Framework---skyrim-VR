#include "config.h"

namespace BarebonesVR {
		
	int logging = 1;
    int leftHandedMode = 0;

	float fNeckContactDist = 20.0f;
	float fWeaponRangeMulti = 1.0f;
	float fThroatForwardOffset = 11.0f;
	float fThroatSideOffset = 4.0f;
	float fBladeTipReachBonus = 1.15f;

	int iSlitSampleFrames = 4;
	float fSlitHorizontalSpeedThres = 38.0f;
	float fSlitMinLateralDist = 8.5f;
	float fSlitMinLateralRatio = 0.50f;
	float fSlitPeakLateralStep = 3.0f;
	int iSlitContactGraceFrames = 4;
	float fSlitMinContactSeconds = 1.0f;
	float fHitFallbackWindowSeconds = 1.0f;
	int iEnableBloodPool = 1;
	std::string sBloodPoolProfileId = "ThroatSlitVR";

    void loadConfig() 
    {
        std::string runtimeDirectory = GetRuntimeDirectory();

        if (!runtimeDirectory.empty()) 
        {
            std::string filepath = runtimeDirectory + "Data\\SKSE\\Plugins\\ThroatSlitVR.ini";
            std::ifstream file(filepath);

            if (!file.is_open()) 
            {
                transform(filepath.begin(), filepath.end(), filepath.begin(), ::tolower);
                file.open(filepath);
            }

            if (file.is_open()) 
            {
                std::string line;
                std::string currentSection;

                while (std::getline(file, line)) 
                {
                    trim(line);
                    skipComments(line);

                    if (line.empty()) continue;

                    if (line[0] == '[') 
                    {
                        // New section
                        size_t endBracket = line.find(']');
                        if (endBracket != std::string::npos) 
                        {
                            currentSection = line.substr(1, endBracket - 1);
                            trim(currentSection);                            
                        }
                    }
                    else if (currentSection == "Settings") 
                    {
                        std::string variableName;
                        std::string variableValueStr = GetConfigSettingsStringValue(line, variableName);

                        if (variableName == "Logging") 
                        {
                            logging = std::stoi(variableValueStr);
                        }
                        else if (variableName == "NeckContactDistance")
                        {
                            fNeckContactDist = std::stof(variableValueStr);
                        }
                        else if (variableName == "WeaponRangeMultiply")
                        {
                            fWeaponRangeMulti = std::stof(variableValueStr);
                        }
                        else if (variableName == "SlitSampleFrames")
                        {
                            iSlitSampleFrames = std::stoi(variableValueStr);
                        }
                        else if (variableName == "SlitHorizontalSpeedThreshold")
                        {
                            fSlitHorizontalSpeedThres = std::stof(variableValueStr);
                        }
                        else if (variableName == "SlitMinLateralDistance")
                        {
                            fSlitMinLateralDist = std::stof(variableValueStr);
                        }
                        else if (variableName == "SlitMinLateralRatio")
                        {
                            fSlitMinLateralRatio = std::stof(variableValueStr);
                        }
                        else if (variableName == "SlitPeakLateralStep")
                        {
                            fSlitPeakLateralStep = std::stof(variableValueStr);
                        }
                        else if (variableName == "SlitMinContactSeconds")
                        {
                            fSlitMinContactSeconds = std::stof(variableValueStr);
                        }
                        else if (variableName == "SlitMinContactFrames")
                        {
                            fSlitMinContactSeconds = std::stof(variableValueStr) / 60.0f;
                        }
                        else if (variableName == "SlitContactGraceFrames")
                        {
                            iSlitContactGraceFrames = std::stoi(variableValueStr);
                        }
                        else if (variableName == "HitFallbackWindowSeconds")
                        {
                            fHitFallbackWindowSeconds = std::stof(variableValueStr);
                        }
                        else if (variableName == "BloodPoolProfileID")
                        {
                            sBloodPoolProfileId = variableValueStr;
                        }
                        else if (variableName == "ThroatForwardOffset")
                        {
                            fThroatForwardOffset = std::stof(variableValueStr);
                        }
                        else if (variableName == "ThroatSideOffset")
                        {
                            fThroatSideOffset = std::stof(variableValueStr);
                        }
                        else if (variableName == "BladeTipReachBonus")
                        {
                            fBladeTipReachBonus = std::stof(variableValueStr);
                        }
                    }
                    else if (currentSection == "Detection")
                    {
                        std::string variableName;
                        std::string variableValueStr = GetConfigSettingsStringValue(line, variableName);

                        if (variableName == "NeckContactDistance")
                        {
                            fNeckContactDist = std::stof(variableValueStr);
                        }
                        else if (variableName == "WeaponRangeMultiply")
                        {
                            fWeaponRangeMulti = std::stof(variableValueStr);
                        }
                        else if (variableName == "SlitSampleFrames")
                        {
                            iSlitSampleFrames = std::stoi(variableValueStr);
                        }
                        else if (variableName == "SlitHorizontalSpeedThreshold")
                        {
                            fSlitHorizontalSpeedThres = std::stof(variableValueStr);
                        }
                        else if (variableName == "SlitMinLateralDistance")
                        {
                            fSlitMinLateralDist = std::stof(variableValueStr);
                        }
                        else if (variableName == "SlitMinLateralRatio")
                        {
                            fSlitMinLateralRatio = std::stof(variableValueStr);
                        }
                        else if (variableName == "SlitPeakLateralStep")
                        {
                            fSlitPeakLateralStep = std::stof(variableValueStr);
                        }
                        else if (variableName == "SlitMinContactSeconds")
                        {
                            fSlitMinContactSeconds = std::stof(variableValueStr);
                        }
                        else if (variableName == "SlitMinContactFrames")
                        {
                            fSlitMinContactSeconds = std::stof(variableValueStr) / 60.0f;
                        }
                        else if (variableName == "SlitContactGraceFrames")
                        {
                            iSlitContactGraceFrames = std::stoi(variableValueStr);
                        }
                        else if (variableName == "HitFallbackWindowSeconds")
                        {
                            fHitFallbackWindowSeconds = std::stof(variableValueStr);
                        }
                        else if (variableName == "BloodPoolProfileID")
                        {
                            sBloodPoolProfileId = variableValueStr;
                        }
                        else if (variableName == "ThroatForwardOffset")
                        {
                            fThroatForwardOffset = std::stof(variableValueStr);
                        }
                        else if (variableName == "ThroatSideOffset")
                        {
                            fThroatSideOffset = std::stof(variableValueStr);
                        }
                        else if (variableName == "BladeTipReachBonus")
                        {
                            fBladeTipReachBonus = std::stof(variableValueStr);
                        }
                    }
                    else if (currentSection == "BloodPool")
                    {
                        std::string variableName;
                        std::string variableValueStr = GetConfigSettingsStringValue(line, variableName);

                        if (variableName == "EnableBloodPool")
                        {
                            iEnableBloodPool = std::stoi(variableValueStr);
                        }
                        else if (variableName == "BloodPoolProfileID")
                        {
                            sBloodPoolProfileId = variableValueStr;
                        }
                    }
                }
            }
            _MESSAGE("Config file is loaded successfully.");
            return;
        }
        return;
    }

	void Log(const int msgLogLevel, const char* fmt, ...)
	{
		if (msgLogLevel > logging)
		{
			return;
		}

		va_list args;
		char logBuffer[4096];

		va_start(args, fmt);
		vsprintf_s(logBuffer, sizeof(logBuffer), fmt, args);
		va_end(args);

		_MESSAGE(logBuffer);
	}

}