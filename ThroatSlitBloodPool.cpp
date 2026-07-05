#include "ThroatSlitBloodPool.h"

#include "DBF_API.h"
#include "config.h"

#include "skse64/GameReferences.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameTypes.h"
#include "skse64/NiObjects.h"

namespace ThroatSlitVR
{
	namespace
	{
		static constexpr int kBloodPoolDelayFrames = 90;
		static constexpr const char* kDefaultBloodPoolProfileId = "ThroatSlitVR";

		struct PendingBloodPool
		{
			UInt32 refHandle = 0;
			int framesRemaining = 0;
		};

		PendingBloodPool g_pendingBloodPool;

		const char* ResolveBloodPoolProfileId()
		{
			if (!BarebonesVR::sBloodPoolProfileId.empty()) {
				return BarebonesVR::sBloodPoolProfileId.c_str();
			}
			return kDefaultBloodPoolProfileId;
		}

		Actor* ResolveActorFromHandle(UInt32 refHandle)
		{
			if (refHandle == 0 || refHandle == *g_invalidRefHandle) {
				return nullptr;
			}

			UInt32 handle = refHandle;
			NiPointer<TESObjectREFR> refr;
			if (!LookupREFRByHandle(handle, refr) || !refr) {
				return nullptr;
			}

			return DYNAMIC_CAST(refr, TESObjectREFR, Actor);
		}

		bool SpawnGroundBloodPool(Actor* victim, const char* profileId)
		{
			if (!victim || !profileId || !profileId[0]) {
				return false;
			}

			auto* api = static_cast<DBF_API::Interface_V1*>(DBF_API::GetAPI());
			if (!api) {
				return false;
			}

			if (!victim->GetNiNode()) {
				LOG_ERR("[BloodPool] Victim has no 3D node; skipping ground blood pool");
				return false;
			}

			DBF_API::Interface_V1::Parameters params{};
			params.originRef = static_cast<TESObjectREFR*>(victim);
			params.profileID = profileId;
			params.waitForStableOrigin = true;

			const bool spawned = api->SpawnBloodpool(params);
			if (spawned) {
				LOG_INFO(
					"[BloodPool] Spawned ground blood pool on refHandle=%08X profile='%s' dead=%s",
					GetOrCreateRefrHandle(static_cast<TESObjectREFR*>(victim)),
					profileId,
					victim->IsDead(1) ? "YES" : "NO");
			}

			return spawned;
		}

		void SpawnThroatSlitBloodPoolNow(Actor* victim)
		{
			if (!victim) {
				return;
			}

			const char* profileId = ResolveBloodPoolProfileId();
			if (SpawnGroundBloodPool(victim, profileId)) {
				return;
			}

			LOG_ERR(
				"[BloodPool] SpawnBloodpool failed for refHandle=%08X profile='%s' dead=%s (install Data/SKSE/DynamicBloodpoolFramework/ThroatSlitVR_BloodPool.json)",
				GetOrCreateRefrHandle(static_cast<TESObjectREFR*>(victim)),
				profileId,
				victim->IsDead(1) ? "YES" : "NO");
		}

	}  // namespace

	void InitThroatSlitBloodPoolApi()
	{
		DBF_API::g_Interface = nullptr;
		g_pendingBloodPool = {};

		if (!BarebonesVR::iEnableBloodPool) {
			LOG_INFO("[BloodPool] Ground blood pools disabled in INI");
			return;
		}

		auto* api = static_cast<DBF_API::Interface_V1*>(DBF_API::GetAPI());
		if (!api) {
			LOG_INFO("[BloodPool] Dynamic Bloodpool Framework not loaded (optional)");
			return;
		}

		const DBF_API::Version version = api->GetVersion();
		LOG_INFO(
			"[BloodPool] Dynamic Bloodpool Framework API v%u.%u.%u registered (profile default='%s')",
			version.parts[0],
			version.parts[1],
			version.parts[2],
			kDefaultBloodPoolProfileId);
	}

	void ScheduleThroatSlitBloodPool(Actor* victim)
	{
		if (!BarebonesVR::iEnableBloodPool || !victim) {
			return;
		}

		if (!DBF_API::GetAPI()) {
			return;
		}

		const UInt32 refHandle = GetOrCreateRefrHandle(static_cast<TESObjectREFR*>(victim));
		if (refHandle == 0 || refHandle == *g_invalidRefHandle) {
			LOG_ERR("[BloodPool] Failed to create ref handle for ground blood pool");
			return;
		}

		g_pendingBloodPool.refHandle = refHandle;
		g_pendingBloodPool.framesRemaining = kBloodPoolDelayFrames;
		LOG_INFO(
			"[BloodPool] Scheduled ground blood pool for refHandle=%08X in %d frames profile='%s'",
			refHandle,
			kBloodPoolDelayFrames,
			ResolveBloodPoolProfileId());
	}

	void UpdatePendingBloodPools()
	{
		if (!BarebonesVR::iEnableBloodPool || g_pendingBloodPool.framesRemaining <= 0) {
			return;
		}

		--g_pendingBloodPool.framesRemaining;
		if (g_pendingBloodPool.framesRemaining > 0) {
			return;
		}

		const UInt32 refHandle = g_pendingBloodPool.refHandle;
		g_pendingBloodPool.refHandle = 0;

		Actor* victim = ResolveActorFromHandle(refHandle);
		if (!victim) {
			LOG_ERR("[BloodPool] Pending spawn aborted; refHandle=%08X no longer valid", refHandle);
			return;
		}

		SpawnThroatSlitBloodPoolNow(victim);
	}

}  // namespace ThroatSlitVR
