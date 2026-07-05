#pragma once

/*******************************************************************
 * DYNAMIC BLOODPOOL FRAMEWORK - API (Barebones SKSE adaptation)
 * Requires Dynamic Bloodpool Framework SKSE plugin installed.
 *******************************************************************/

#include "skse64/GameTypes.h"
#include "skse64/NiTypes.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>

class NiAVObject;
class TESObjectREFR;

namespace DBF_API
{
	inline void* g_Interface = nullptr;

	enum class InterfaceVersion : std::uint8_t
	{
		V1,
		Latest = V1
	};

	struct Version
	{
		std::array<std::uint16_t, 4> parts{ {0, 0, 0, 0} };

		Version() = default;

		Version(std::uint16_t major, std::uint16_t minor = 0, std::uint16_t patch = 0, std::uint16_t build = 0)
		{
			parts[0] = major;
			parts[1] = minor;
			parts[2] = patch;
			parts[3] = build;
		}
	};

	class Interface_V1
	{
	public:
		virtual ~Interface_V1() = default;

		struct Parameters
		{
			std::string profileID = "";
			TESObjectREFR* originRef = nullptr;

			std::variant<NiAVObject*, BSFixedString> originNodePos = static_cast<NiAVObject*>(nullptr);
			std::variant<NiAVObject*, BSFixedString> originNodeRot = static_cast<NiAVObject*>(nullptr);
			NiPoint3 nodeSpreadDirection{ 0.0f, 0.0f, 1.0f };
			bool waitForStableOrigin = true;

			struct Override
			{
				std::optional<NiPoint3> position = std::nullopt;
				std::optional<float> rotation = std::nullopt;
				std::optional<float> scale = std::nullopt;
				std::optional<float> spread = std::nullopt;
				std::optional<float> durationMult = std::nullopt;
			} override;

			std::function<void(bool, TESObjectREFR*)> callback = nullptr;
		};

		virtual Version GetVersion() noexcept = 0;
		virtual bool SpawnBloodpool(const Parameters parameters) noexcept = 0;
	};

	using Interface = Interface_V1;

	using RequestPluginAPIFn = void* (*)(InterfaceVersion version, const char* pluginName, Version pluginVersion);

	void* GetAPI(InterfaceVersion version = InterfaceVersion::Latest);
	bool IsAvailable();

}  // namespace DBF_API
