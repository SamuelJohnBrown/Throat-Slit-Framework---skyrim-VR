#include "DBF_API.h"

#include "config.h"

#include <Windows.h>

namespace DBF_API
{
	void* GetAPI(InterfaceVersion version)
	{
		if (g_Interface) {
			return g_Interface;
		}

		const HMODULE handle = GetModuleHandleA("DynamicBloodpoolFramework.dll");
		if (!handle) {
			return nullptr;
		}

		const auto request = reinterpret_cast<RequestPluginAPIFn>(GetProcAddress(handle, "RequestPluginAPI"));
		if (!request) {
			return nullptr;
		}

		Version pluginVersion(1, 5, 5, 0);
		g_Interface = request(version, "ThroatSlitVR", pluginVersion);
		return g_Interface;
	}

	bool IsAvailable()
	{
		return GetAPI() != nullptr;
	}

}  // namespace DBF_API
