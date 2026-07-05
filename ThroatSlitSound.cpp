#include "ThroatSlitSound.h"

#include "SkyrimVRESLAPI.h"
#include "config.h"

#include "skse64/GameForms.h"
#include "skse64/GameReferences.h"
#include "skse64/GameRTTI.h"
#include "skse64/NiNodes.h"

namespace ThroatSlitVR
{
	namespace
	{
		static constexpr const char* kThroatSlitEsp = "Throat Slit VR.esp";
		static constexpr UInt32 kThroatSlitSndrBaseFormId = 0x00000800;
		static constexpr UInt32 kThroatSlitSndrFallbackFormId = 0xFE000800;

		struct VRSoundHandle
		{
			UInt32 soundID = 0xFFFFFFFF;
			bool assumeSuccess = false;
			UInt8 pad05 = 0;
			UInt16 pad06 = 0;
			UInt32 state = 0;
		};

		typedef void* (*GetAudioManagerSingletonFn)();
		typedef bool (*BuildSoundDataFromDescriptorFn)(void* manager, VRSoundHandle* handle, void* soundDescriptor, UInt32 flags);
		typedef bool (*SoundHandlePlayFn)(VRSoundHandle* handle);
		typedef void (*SoundHandleSetObjectToFollowFn)(VRSoundHandle* handle, NiAVObject* node);

		static RelocAddr<GetAudioManagerSingletonFn> GetAudioManagerSingleton(0x00C29430);
		static RelocAddr<BuildSoundDataFromDescriptorFn> BuildSoundDataFromDescriptor(0x00C29F60);
		static RelocAddr<SoundHandlePlayFn> SoundHandlePlay(0x00C283E0);
		static RelocAddr<SoundHandleSetObjectToFollowFn> SoundHandleSetObjectToFollow(0x00C289C0);

		BGSSoundDescriptorForm* g_throatSlitSound = nullptr;
		UInt32 g_throatSlitSoundFormId = 0;

		BGSSoundDescriptorForm* ResolveSoundDescriptor(UInt32 formId)
		{
			if (formId == 0) {
				return nullptr;
			}

			TESForm* form = LookupFormByID(formId);
			if (!form) {
				return nullptr;
			}

			return DYNAMIC_CAST(form, TESForm, BGSSoundDescriptorForm);
		}

	}  // namespace

	void InitThroatSlitSound()
	{
		g_throatSlitSound = nullptr;
		g_throatSlitSoundFormId = 0;

		const UInt32 resolvedFormId =
			GetFullFormIdFromEspAndFormId(kThroatSlitEsp, kThroatSlitSndrBaseFormId);
		if (resolvedFormId != 0) {
			g_throatSlitSound = ResolveSoundDescriptor(resolvedFormId);
			if (g_throatSlitSound) {
				g_throatSlitSoundFormId = resolvedFormId;
			}
		}

		if (!g_throatSlitSound) {
			g_throatSlitSound = ResolveSoundDescriptor(kThroatSlitSndrFallbackFormId);
			if (g_throatSlitSound) {
				g_throatSlitSoundFormId = kThroatSlitSndrFallbackFormId;
			}
		}

		if (g_throatSlitSound) {
			LOG_INFO("[Sound] Loaded throat slit SNDR formId=%08X", g_throatSlitSoundFormId);
			return;
		}

		LOG_ERR(
			"[Sound] Failed to load throat slit SNDR from %s (base=%08X fallback=%08X)",
			kThroatSlitEsp,
			kThroatSlitSndrBaseFormId,
			kThroatSlitSndrFallbackFormId);
	}

	void PlayThroatSlitSound(Actor* victim)
	{
		if (!victim || !g_throatSlitSound) {
			return;
		}

		void* audioManager = GetAudioManagerSingleton();
		if (!audioManager) {
			LOG_ERR("[Sound] BSAudioManager unavailable for throat slit SNDR");
			return;
		}

		void* soundDescriptor = &g_throatSlitSound->soundDescriptor;
		VRSoundHandle handle{};
		if (!BuildSoundDataFromDescriptor(audioManager, &handle, soundDescriptor, 0x10)) {
			LOG_ERR("[Sound] BuildSoundDataFromDescriptor failed for throat slit SNDR");
			return;
		}

		if (NiNode* node = victim->GetNiNode()) {
			SoundHandleSetObjectToFollow(&handle, node);
		}

		if (SoundHandlePlay(&handle)) {
			LOG_INFO("[Sound] Playing throat slit SNDR on victim formId=%08X", g_throatSlitSoundFormId);
		} else {
			LOG_ERR("[Sound] Failed to play throat slit SNDR");
		}
	}

}  // namespace ThroatSlitVR
