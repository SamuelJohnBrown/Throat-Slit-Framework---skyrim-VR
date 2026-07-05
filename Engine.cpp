#include "Engine.h"

#include "OnFrame.h"
#include "ThroatSlitDetection.h"

#include <skse64/PapyrusActor.cpp>

namespace BarebonesVR
{
	SKSETrampolineInterface* g_trampolineInterface = nullptr;

	HiggsPluginAPI::IHiggsInterface001* higgsInterface;
	vrikPluginApi::IVrikInterface001* vrikInterface;

	SkyrimVRESLPluginAPI::ISkyrimVRESLInterface001* skyrimVRESLInterface;

	void StartMod()
	{
		ThroatSlitVR::InstallFrameHook();
		LOG_INFO("[Init] Throat Slit VR logging mod started");
	}

	void RegisterThroatSlitCallbacks()
	{
		ThroatSlitVR::RegisterHiggsCallbacks();
	}

	void ResetThroatSlitState()
	{
		ThroatSlitVR::ClearGrabState();
	}
}
