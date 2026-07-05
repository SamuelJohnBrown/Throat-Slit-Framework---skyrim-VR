#include "ThroatSlitBloodEffect.h"

#include "Geometry.h"
#include "config.h"

#include "skse64/GameForms.h"
#include "skse64/GameReferences.h"
#include "skse64/GameRTTI.h"
#include "skse64/NiObjects.h"

namespace ThroatSlitVR
{
	namespace
	{
		static constexpr UInt32 kBloodImpactDataSetFormId = 0x0001F82A;  // Skyrim.esm

		static const char* kThroatBoneNames[] = {
			"NPC Neck [Neck]",
			"Neck",
		};

		class BGSImpactManager;

		typedef bool (*PlayImpactEffectFn)(
			BGSImpactManager* self,
			TESObjectREFR* obj,
			BGSImpactDataSet* impactData,
			const BSFixedString& asNodeName,
			NiPoint3* afPickDirection,
			float afPickLength,
			bool abApplyNodeRotation,
			bool abUseNodeLocalRotation);

		static RelocPtr<BGSImpactManager*> g_impactManagerSingleton(0x02FC52E0);
		static RelocAddr<PlayImpactEffectFn> PlayImpactEffect(0x05AA320);

		BGSImpactDataSet* g_bloodImpactDataSet = nullptr;

		const char* ResolveThroatBoneName(Actor* victim)
		{
			if (!victim) {
				return kThroatBoneNames[0];
			}

			for (const char* boneName : kThroatBoneNames) {
				if (FindBone(victim, boneName)) {
					return boneName;
				}
			}

			return kThroatBoneNames[0];
		}

	}  // namespace

	void InitThroatSlitBloodEffect()
	{
		g_bloodImpactDataSet = nullptr;

		TESForm* form = LookupFormByID(kBloodImpactDataSetFormId);
		if (!form) {
			LOG_ERR("[Blood] Failed to find blood impact dataset formId=%08X", kBloodImpactDataSetFormId);
			return;
		}

		g_bloodImpactDataSet = DYNAMIC_CAST(form, TESForm, BGSImpactDataSet);
		if (!g_bloodImpactDataSet) {
			LOG_ERR("[Blood] Form %08X is not a BGSImpactDataSet (type=%u)", kBloodImpactDataSetFormId, form->formType);
			return;
		}

		LOG_INFO("[Blood] Loaded blood impact dataset formId=%08X", kBloodImpactDataSetFormId);
	}

	void PlayThroatSlitBloodEffect(Actor* victim, Actor* attacker)
	{
		if (!victim || !g_bloodImpactDataSet) {
			return;
		}

		if (!victim->GetNiNode()) {
			LOG_ERR("[Blood] Victim has no 3D node; skipping throat blood effect");
			return;
		}

		BGSImpactManager* impactManager = *g_impactManagerSingleton;
		if (!impactManager) {
			LOG_ERR("[Blood] BGSImpactManager singleton unavailable; skipping throat blood effect");
			return;
		}

		const char* boneNameStr = ResolveThroatBoneName(victim);
		BSFixedString boneName(boneNameStr);

		NiPoint3 pickDirection(0.0f, 1.0f, 0.0f);
		if (attacker) {
			TESObjectREFR* victimRefr = static_cast<TESObjectREFR*>(victim);
			TESObjectREFR* attackerRefr = static_cast<TESObjectREFR*>(attacker);
			const float dx = victimRefr->pos.x - attackerRefr->pos.x;
			const float dy = victimRefr->pos.y - attackerRefr->pos.y;
			const float dz = victimRefr->pos.z - attackerRefr->pos.z;
			const float len = sqrtf(dx * dx + dy * dy + dz * dz);
			if (len > 0.001f) {
				pickDirection.x = dx / len;
				pickDirection.y = dy / len;
				pickDirection.z = dz / len;
			}
		}

		const bool played = PlayImpactEffect(
			impactManager,
			victim,
			g_bloodImpactDataSet,
			boneName,
			&pickDirection,
			100.0f,
			true,
			false);

		if (played) {
			LOG_INFO("[Blood] Played throat blood impact on formId=%08X at bone '%s'", victim->formID, boneNameStr);
		} else {
			LOG_ERR("[Blood] PlayImpactEffect failed for throat blood on formId=%08X bone '%s'", victim->formID, boneNameStr);
		}
	}

}  // namespace ThroatSlitVR
