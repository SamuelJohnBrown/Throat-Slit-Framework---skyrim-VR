#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <skse64/NiProperties.h>
#include <skse64/NiNodes.h>

#include "skse64\GameSettings.h"
#include "Utility.hpp"

#include <skse64/GameData.h>

#include "higgsinterface001.h"
#include "vrikinterface001.h"
#include "SkyrimVRESLAPI.h"

namespace BarebonesVR {

	const UInt32 MOD_VERSION = 0x10000;
	const std::string MOD_VERSION_STR = "1.5.5-blood-pool";
	extern int leftHandedMode;

	extern int logging;

	// Throat slit detection tuning (WeaponCollision-style units ~1.4 cm)
	extern float fNeckContactDist;
	extern float fWeaponRangeMulti;
	extern float fThroatForwardOffset;
	extern float fThroatSideOffset;
	extern float fBladeTipReachBonus;

	// Slit gesture: sharp horizontal blade movement while on neck
	extern int iSlitSampleFrames;
	extern float fSlitHorizontalSpeedThres;
	extern float fSlitMinLateralDist;
	extern float fSlitMinLateralRatio;
	extern float fSlitPeakLateralStep;
	extern int iSlitContactGraceFrames;
	extern float fSlitMinContactSeconds;
	extern float fHitFallbackWindowSeconds;
	extern int iEnableBloodPool;
	extern std::string sBloodPoolProfileId;
    

	void loadConfig();
	
	void Log(const int msgLogLevel, const char* fmt, ...);
	enum eLogLevels
	{
		LOGLEVEL_ERR = 0,
		LOGLEVEL_WARN,
		LOGLEVEL_INFO,
		LOGLEVEL_DEBUG,
	};


#define LOG(fmt, ...) BarebonesVR::Log(BarebonesVR::LOGLEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERR(fmt, ...) BarebonesVR::Log(BarebonesVR::LOGLEVEL_ERR, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) BarebonesVR::Log(BarebonesVR::LOGLEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) BarebonesVR::Log(BarebonesVR::LOGLEVEL_DEBUG, fmt, ##__VA_ARGS__)


}