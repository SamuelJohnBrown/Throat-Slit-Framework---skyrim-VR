#include "ThroatSlitDetection.h"

#include "CrimeBounty.h"
#include "Geometry.h"
#include "ThroatSlitBloodEffect.h"
#include "ThroatSlitBloodPool.h"
#include "ThroatSlitSound.h"
#include "config.h"
#include "Engine.h"
#include "higgsinterface001.h"

#include "skse64/GameAPI.h"
#include "skse64/GameData.h"
#include "skse64/GameEvents.h"
#include "skse64/GameForms.h"
#include "skse64/GameObjects.h"
#include "skse64/GameReferences.h"
#include "skse64/GameRTTI.h"
#include "skse64/NiObjects.h"

#include <Windows.h>

namespace ThroatSlitVR
{
	namespace
	{
		constexpr int kSlitHistoryMax = 16;
		constexpr UInt32 kHealthActorValue = 24;

		struct GrabState
		{
			Actor* target = nullptr;
			bool isLeftHand = false;
			bool bladeHandIsLeft = false;
			bool neckContact = false;
			bool bladeOnNeckThisFrame = false;
			bool slitLogged = false;
			bool contactTimerActive = false;
			bool lastNeckContactValid = false;
			LARGE_INTEGER contactStartQpc{};
			LARGE_INTEGER lastNeckContactQpc{};
			int contactLossFrames = 0;
			UInt64 lastContactLogFrame = 0;
			UInt64 lastSlitDebugFrame = 0;
			UInt64 lastNearMissLogFrame = 0;
			float lastNearMissDist = 9999.0f;
			NiPoint3 currentBladeTrackPoint{};
			NiPoint3 slitPosRing[kSlitHistoryMax]{};
			int slitRingWriteIdx = 0;
			int slitHistoryCount = 0;
		};

		GrabState g_grabState;

		void KillActorFromThroatSlit(Actor* victim);

		void ResetSlitHistory()
		{
			g_grabState.slitRingWriteIdx = 0;
			g_grabState.slitHistoryCount = 0;
			g_grabState.contactLossFrames = 0;
			g_grabState.contactTimerActive = false;
		}

		void MarkNeckContactNow()
		{
			QueryPerformanceCounter(&g_grabState.lastNeckContactQpc);
			g_grabState.lastNeckContactValid = true;
		}

		double GetSecondsSinceQpc(const LARGE_INTEGER& then)
		{
			LARGE_INTEGER now{};
			LARGE_INTEGER freq{};
			QueryPerformanceCounter(&now);
			QueryPerformanceFrequency(&freq);
			if (freq.QuadPart <= 0) {
				return 9999.0;
			}

			return static_cast<double>(now.QuadPart - then.QuadPart) / static_cast<double>(freq.QuadPart);
		}

		double GetSecondsSinceLastNeckContact()
		{
			if (!g_grabState.lastNeckContactValid) {
				return 9999.0;
			}

			return GetSecondsSinceQpc(g_grabState.lastNeckContactQpc);
		}

		double GetContactDurationSeconds()
		{
			if (!g_grabState.contactTimerActive) {
				return 0.0;
			}

			LARGE_INTEGER now{};
			LARGE_INTEGER freq{};
			QueryPerformanceCounter(&now);
			QueryPerformanceFrequency(&freq);
			if (freq.QuadPart <= 0) {
				return 0.0;
			}

			return static_cast<double>(now.QuadPart - g_grabState.contactStartQpc.QuadPart) /
				static_cast<double>(freq.QuadPart);
		}

		void BeginContactTimer()
		{
			if (!g_grabState.contactTimerActive) {
				QueryPerformanceCounter(&g_grabState.contactStartQpc);
				g_grabState.contactTimerActive = true;
			}
		}

		const char* ActorName(Actor* actor)
		{
			if (!actor) {
				return "<null>";
			}

			TESObjectREFR* refr = static_cast<TESObjectREFR*>(actor);
			const char* name = CALL_MEMBER_FN(refr, GetReferenceName)();
			if (name && name[0]) {
				return name;
			}

			if (actor->baseForm) {
				const char* baseName = actor->baseForm->GetFullName();
				if (baseName && baseName[0]) {
					return baseName;
				}
			}

			return "<unnamed>";
		}

		BGSKeyword* GetActorTypeUndeadKeyword()
		{
			static BGSKeyword* cached = nullptr;
			static bool resolved = false;
			if (resolved) {
				return cached;
			}

			resolved = true;
			DataHandler* dataHandler = DataHandler::GetSingleton();
			if (!dataHandler) {
				return nullptr;
			}

			for (UInt32 i = 0; i < dataHandler->keywords.count; ++i) {
				BGSKeyword* keyword = nullptr;
				if (!dataHandler->keywords.GetNthItem(i, keyword) || !keyword) {
					continue;
				}

				const char* editorId = keyword->keyword.c_str();
				if (editorId && _stricmp(editorId, "ActorTypeUndead") == 0) {
					cached = keyword;
					break;
				}
			}

			return cached;
		}

		bool FormHasKeyword(TESForm* form, BGSKeyword* keyword)
		{
			if (!form || !keyword) {
				return false;
			}

			BGSKeywordForm* keywordForm = DYNAMIC_CAST(form, TESForm, BGSKeywordForm);
			return keywordForm && keywordForm->HasKeyword(keyword);
		}

		bool IsUndeadActor(Actor* actor)
		{
			if (!actor) {
				return false;
			}

			BGSKeyword* undeadKeyword = GetActorTypeUndeadKeyword();
			if (!undeadKeyword) {
				return false;
			}

			if (FormHasKeyword(actor->baseForm, undeadKeyword)) {
				return true;
			}

			if (FormHasKeyword(actor->race, undeadKeyword)) {
				return true;
			}

			return false;
		}

		bool IsEssentialActor(Actor* actor)
		{
			if (!actor) {
				return false;
			}

			if (actor->flags2 & Actor::kFlag_kEssential) {
				return true;
			}

			TESNPC* npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
			if (npc && (npc->actorData.flags & (1 << 1))) {
				return true;
			}

			return false;
		}

		bool IsValidGrabTarget(TESObjectREFR* refr)
		{
			if (!refr) {
				return false;
			}

			Actor* actor = DYNAMIC_CAST(refr, TESObjectREFR, Actor);
			if (!actor) {
				return false;
			}

			PlayerCharacter* player = *g_thePlayer;
			if (!player || actor == static_cast<Actor*>(player)) {
				return false;
			}

			if (actor->IsDead(1)) {
				return false;
			}

			if (IsUndeadActor(actor)) {
				return false;
			}

			if (IsEssentialActor(actor)) {
				return false;
			}

			return true;
		}

		const char* GrabRejectReason(TESObjectREFR* refr)
		{
			if (!refr) {
				return "invalid reference";
			}

			Actor* actor = DYNAMIC_CAST(refr, TESObjectREFR, Actor);
			if (!actor) {
				return "non-actor";
			}

			PlayerCharacter* player = *g_thePlayer;
			if (!player || actor == static_cast<Actor*>(player)) {
				return "player";
			}

			if (actor->IsDead(1)) {
				return "dead";
			}

			if (IsUndeadActor(actor)) {
				return "undead";
			}

			if (IsEssentialActor(actor)) {
				return "essential";
			}

			return nullptr;
		}

		void OnHiggsGrabbed(bool isLeft, TESObjectREFR* grabbedRefr)
		{
			if (!IsValidGrabTarget(grabbedRefr)) {
				const char* reason = GrabRejectReason(grabbedRefr);
				if (reason) {
					Actor* actor = grabbedRefr ? DYNAMIC_CAST(grabbedRefr, TESObjectREFR, Actor) : nullptr;
					LOG_INFO(
						"[Grab] Ignored %s target%s%s on %s hand",
						reason,
						actor ? " " : "",
						actor ? ActorName(actor) : "",
						isLeft ? "left" : "right");
				}
				return;
			}

			Actor* npc = DYNAMIC_CAST(grabbedRefr, TESObjectREFR, Actor);
			PlayerCharacter* player = *g_thePlayer;
			if (!player || !npc) {
				return;
			}

			g_grabState.target = npc;
			g_grabState.isLeftHand = isLeft;
			g_grabState.neckContact = false;
			g_grabState.bladeOnNeckThisFrame = false;
			g_grabState.slitLogged = false;
			g_grabState.lastNeckContactValid = false;
			g_grabState.lastContactLogFrame = 0;
			g_grabState.lastSlitDebugFrame = 0;
			ResetSlitHistory();

			LOG_INFO("[Grab] %s grabbed with %s hand", ActorName(npc), isLeft ? "left" : "right");
		}

		void OnHiggsDropped(bool isLeft, TESObjectREFR* droppedRefr)
		{
			Actor* npc = droppedRefr ? DYNAMIC_CAST(droppedRefr, TESObjectREFR, Actor) : nullptr;
			if (g_grabState.target && npc == g_grabState.target) {
				LOG_INFO(
					"[Grab] Released %s with %s hand | hadNeckContact=%s",
					ActorName(npc),
					isLeft ? "left" : "right",
					g_grabState.neckContact ? "YES" : "NO");
				g_grabState = {};
				return;
			}

			if (npc) {
				LOG_INFO("[Grab] Dropped %s with %s hand", ActorName(npc), isLeft ? "left" : "right");
			}
		}

		void ExecuteThroatSlit(Actor* npc, const char* method)
		{
			if (!npc || g_grabState.slitLogged || IsUndeadActor(npc) || IsEssentialActor(npc)) {
				return;
			}

			g_grabState.slitLogged = true;
			LOG_INFO("%s's throat was slit", ActorName(npc));
			LOG_INFO("[ThroatSlit] %s", method);

			PlayThroatSlitSound(npc);
			PlayThroatSlitBloodEffect(npc, static_cast<Actor*>(*g_thePlayer));
			ApplyMurderBountyIfWitnessed(*g_thePlayer, npc);
			KillActorFromThroatSlit(npc);
			ScheduleThroatSlitBloodPool(npc);
		}

		bool IsRecentNeckContactForHitFallback()
		{
			if (!g_grabState.lastNeckContactValid) {
				return false;
			}

			const float windowSeconds = BarebonesVR::fHitFallbackWindowSeconds > 0.0f ?
				BarebonesVR::fHitFallbackWindowSeconds :
				1.0f;
			return GetSecondsSinceLastNeckContact() <= windowSeconds;
		}

		TESObjectWEAP* ResolveHitFallbackWeapon(const TESHitEvent* hitEvent)
		{
			if (hitEvent && hitEvent->sourceFormID != 0) {
				TESForm* sourceForm = LookupFormByID(hitEvent->sourceFormID);
				if (TESObjectWEAP* weapon = DYNAMIC_CAST(sourceForm, TESForm, TESObjectWEAP)) {
					return weapon;
				}
			}

			PlayerCharacter* player = *g_thePlayer;
			if (!player) {
				return nullptr;
			}

			const bool bladeHandIsLeft = !g_grabState.isLeftHand;
			TESForm* equipped = player->GetEquippedObject(bladeHandIsLeft);
			return DYNAMIC_CAST(equipped, TESForm, TESObjectWEAP);
		}

		void TryHitFallbackSlit(Actor* npc, const TESHitEvent* hitEvent)
		{
			if (!npc || !hitEvent || g_grabState.slitLogged || g_grabState.target != npc) {
				return;
			}

			if (!IsRecentNeckContactForHitFallback()) {
				return;
			}

			TESObjectWEAP* weapon = ResolveHitFallbackWeapon(hitEvent);
			if (!IsThroatSlitEligibleWeapon(weapon)) {
				return;
			}

			const float windowSeconds = BarebonesVR::fHitFallbackWindowSeconds > 0.0f ?
				BarebonesVR::fHitFallbackWindowSeconds :
				1.0f;
			const double secondsSinceNeckContact = GetSecondsSinceLastNeckContact();

			char method[256];
			sprintf_s(
				method,
				"Hit fallback | secondsSinceNeckContact=%.2f window=%.2f blocked=%s sourceForm=%08X",
				secondsSinceNeckContact,
				windowSeconds,
				(hitEvent->flags & TESHitEvent::kFlag_Blocked) ? "YES" : "NO",
				hitEvent->sourceFormID);

			ExecuteThroatSlit(npc, method);
		}

		class HitEventHandler : public BSTEventSink<TESHitEvent>
		{
		public:
			virtual EventResult ReceiveEvent(TESHitEvent* evn, EventDispatcher<TESHitEvent>* dispatcher) override
			{
				if (!evn || !g_grabState.target || g_grabState.slitLogged) {
					return kEvent_Continue;
				}

				PlayerCharacter* player = *g_thePlayer;
				if (!player || evn->caster != player) {
					return kEvent_Continue;
				}

				if (evn->projectileFormID != 0) {
					return kEvent_Continue;
				}

				Actor* hitTarget = evn->target ? DYNAMIC_CAST(evn->target, TESObjectREFR, Actor) : nullptr;
				if (!hitTarget || hitTarget != g_grabState.target) {
					return kEvent_Continue;
				}

				TryHitFallbackSlit(hitTarget, evn);
				return kEvent_Continue;
			}

			static HitEventHandler* GetSingleton()
			{
				static HitEventHandler instance;
				return &instance;
			}

		private:
			HitEventHandler() = default;
		};

		void KillActorFromThroatSlit(Actor* victim)
		{
			if (!victim || victim->IsDead(1)) {
				return;
			}

			const float maxHealth = victim->actorValueOwner.GetMaximum(kHealthActorValue);
			const float killDamage = maxHealth > 1.0f ? maxHealth * 100.0f : 99999.0f;
			victim->actorValueOwner.RestoreActorValue(Actor::kDamage, kHealthActorValue, -killDamage);
		}

		void LogThroatSlitContact(
			Actor* npc,
			bool isLeftHand,
			const ThroatContactResult& contact,
			int throatPointCount)
		{
			LOG_INFO(
				"[ThroatSlit] BLADE NECK CONTACT | target=%s hand=%s dist=%.2f zonePt=%d throat=(%.1f, %.1f, %.1f) blade=(%.1f, %.1f, %.1f)",
				ActorName(npc),
				isLeftHand ? "left" : "right",
				contact.dist,
				contact.throatPointIndex,
				contact.throatPoint.x,
				contact.throatPoint.y,
				contact.throatPoint.z,
				contact.bladePoint.x,
				contact.bladePoint.y,
				contact.bladePoint.z);
		}

		NiPoint3 BladeTrackPoint(const WeaponSegment& segment)
		{
			// Track the cutting edge (tip) — it moves the most during a slash.
			return segment.top;
		}

		void PushSlitSample(const NiPoint3& trackPoint)
		{
			g_grabState.slitPosRing[g_grabState.slitRingWriteIdx] = trackPoint;
			g_grabState.slitRingWriteIdx = (g_grabState.slitRingWriteIdx + 1) % kSlitHistoryMax;
			if (g_grabState.slitHistoryCount < kSlitHistoryMax) {
				++g_grabState.slitHistoryCount;
			}
		}

		NiPoint3 GetSlitSample(int framesAgo)
		{
			const int idx =
				(g_grabState.slitRingWriteIdx - 1 - framesAgo + kSlitHistoryMax * 2) % kSlitHistoryMax;
			return g_grabState.slitPosRing[idx];
		}

		float PeakDirectedLateralStep(bool bladeIsLeftHand, const NiPoint3& playerRight, int sampleSpan)
		{
			float peak = 0.0f;
			const int steps = sampleSpan < g_grabState.slitHistoryCount ? sampleSpan : g_grabState.slitHistoryCount - 1;
			for (int i = 1; i <= steps; ++i) {
				const NiPoint3 delta = GetSlitSample(i - 1) - GetSlitSample(i);
				const float lateral = delta.x * playerRight.x + delta.y * playerRight.y;
				if (bladeIsLeftHand) {
					if (lateral < peak) {
						peak = lateral;
					}
				} else if (lateral > peak) {
					peak = lateral;
				}
			}
			return peak;
		}

		struct SlitMetrics
		{
			float lateralAbs = 0.0f;
			float horizontalLenSq = 0.0f;
			float forwardAbs = 0.0f;
			float verticalAbs = 0.0f;
			float lateral = 0.0f;
			int sampleSpan = 0;
		};

		bool EvaluateSlitDelta(
			const NiPoint3& delta,
			const NiPoint3& playerRight,
			const NiPoint3& npcForward,
			bool bladeIsLeftHand,
			int sampleSpan,
			SlitMetrics& outMetrics)
		{
			const float lateralPlayer = delta.x * playerRight.x + delta.y * playerRight.y;

			outMetrics.lateral = lateralPlayer;
			outMetrics.lateralAbs = fabsf(lateralPlayer);
			outMetrics.forwardAbs = fabsf(delta.x * npcForward.x + delta.y * npcForward.y);
			outMetrics.horizontalLenSq = delta.x * delta.x + delta.y * delta.y;
			outMetrics.verticalAbs = fabsf(delta.z);

			const float minLateral = BarebonesVR::fSlitMinLateralDist;
			if (bladeIsLeftHand) {
				if (lateralPlayer > -minLateral) {
					return false;
				}
			} else if (lateralPlayer < minLateral) {
				return false;
			}

			if (outMetrics.horizontalLenSq < BarebonesVR::fSlitHorizontalSpeedThres) {
				return false;
			}

			const float minRatio = BarebonesVR::fSlitMinLateralRatio > 0.0f ? BarebonesVR::fSlitMinLateralRatio : 0.55f;
			if (outMetrics.lateralAbs * outMetrics.lateralAbs < outMetrics.horizontalLenSq * minRatio) {
				return false;
			}

			if (outMetrics.verticalAbs * outMetrics.verticalAbs > outMetrics.horizontalLenSq * 0.36f) {
				return false;
			}

			if (outMetrics.lateralAbs < outMetrics.forwardAbs * 0.85f) {
				return false;
			}

			const float peakStep = PeakDirectedLateralStep(bladeIsLeftHand, playerRight, sampleSpan);
			const float minPeak = BarebonesVR::fSlitPeakLateralStep > 0.0f ? BarebonesVR::fSlitPeakLateralStep : 3.5f;
			if (bladeIsLeftHand) {
				if (peakStep > -minPeak) {
					return false;
				}
			} else if (peakStep < minPeak) {
				return false;
			}

			return true;
		}

		void TryDetectSlit(Actor* player, Actor* npc, UInt64 frameCounter)
		{
			if (g_grabState.slitLogged || !player || !npc) {
				return;
			}

			const float minContactSeconds =
				BarebonesVR::fSlitMinContactSeconds > 0.0f ? BarebonesVR::fSlitMinContactSeconds : 1.0f;
			if (GetContactDurationSeconds() < minContactSeconds) {
				return;
			}

			const int sampleFrames = BarebonesVR::iSlitSampleFrames > 1 ? BarebonesVR::iSlitSampleFrames : 3;
			if (g_grabState.slitHistoryCount <= sampleFrames) {
				return;
			}

			const NiPoint3 playerRight = ActorRightHorizontal(player);
			const NiPoint3 npcForward = ActorForwardHorizontal(npc);
			const bool bladeIsLeftHand = g_grabState.bladeHandIsLeft;

			SlitMetrics best{};
			bool found = false;

			for (int span = sampleFrames; span <= sampleFrames + 1; ++span) {
				if (span < 2 || span >= g_grabState.slitHistoryCount) {
					continue;
				}

				const NiPoint3 delta = GetSlitSample(0) - GetSlitSample(span);
				SlitMetrics metrics{};
				if (!EvaluateSlitDelta(delta, playerRight, npcForward, bladeIsLeftHand, span, metrics)) {
					continue;
				}

				if (!found || metrics.lateralAbs > best.lateralAbs) {
					best = metrics;
					best.sampleSpan = span;
					found = true;
				}
			}

			if (!found) {
				if (BarebonesVR::logging >= BarebonesVR::LOGLEVEL_DEBUG &&
					frameCounter - g_grabState.lastSlitDebugFrame >= 30) {
					const NiPoint3 delta = GetSlitSample(0) - GetSlitSample(sampleFrames);
					const float lateralPlayer = delta.x * playerRight.x + delta.y * playerRight.y;
					const float horizLenSq = delta.x * delta.x + delta.y * delta.y;
					LOG_DEBUG(
						"[ThroatSlit] Slit check | bladeHand=%s lateral=%.1f need=%s%.1f horiz2=%.1f needHoriz2=%.1f contactSec=%.2f needSec=%.2f",
						bladeIsLeftHand ? "left" : "right",
						lateralPlayer,
						bladeIsLeftHand ? "-" : "+",
						BarebonesVR::fSlitMinLateralDist,
						horizLenSq,
						BarebonesVR::fSlitHorizontalSpeedThres,
						GetContactDurationSeconds(),
						minContactSeconds);
					g_grabState.lastSlitDebugFrame = frameCounter;
				}
				return;
			}

			char method[256];
			sprintf_s(
				method,
				"Slit gesture | bladeHand=%s direction=%s lateral=%.1f forward=%.1f horizSpeed2=%.1f samples=%d",
				bladeIsLeftHand ? "left" : "right",
				best.lateral > 0.0f ? "right" : "left",
				best.lateral,
				best.forwardAbs,
				best.horizontalLenSq,
				best.sampleSpan);

			ExecuteThroatSlit(npc, method);
		}

	}  // namespace

	void RegisterHiggsCallbacks()
	{
		if (!BarebonesVR::higgsInterface) {
			LOG_ERR("[Init] HIGGS interface missing; grab detection disabled");
			return;
		}

		BarebonesVR::higgsInterface->AddGrabbedCallback(OnHiggsGrabbed);
		BarebonesVR::higgsInterface->AddDroppedCallback(OnHiggsDropped);
		LOG_INFO("[Init] Registered HIGGS grab/drop callbacks");
	}

	void RegisterHitEventHandler()
	{
		EventDispatcherList* eventDispatcher = GetEventDispatcherList();
		if (!eventDispatcher) {
			LOG_ERR("[Init] Hit event dispatcher unavailable");
			return;
		}

		eventDispatcher->unk630.AddEventSink(HitEventHandler::GetSingleton());
		LOG_INFO("[Init] Registered TESHitEvent handler for melee hit fallback");
	}

	void ClearGrabState()
	{
		g_grabState = {};
	}

	void UpdateDetection()
	{
		if (!g_grabState.target) {
			return;
		}

		Actor* npc = g_grabState.target;
		if (npc->IsDead(1) || !npc->GetNiNode()) {
			LOG_INFO("[ThroatSlit] Grab target lost (dead or unloaded): %s", ActorName(npc));
			ClearGrabState();
			return;
		}

		if (BarebonesVR::higgsInterface) {
			TESObjectREFR* leftGrab = BarebonesVR::higgsInterface->GetGrabbedObject(true);
			TESObjectREFR* rightGrab = BarebonesVR::higgsInterface->GetGrabbedObject(false);
			if (leftGrab != npc && rightGrab != npc) {
				LOG_INFO("[ThroatSlit] Grab target no longer held by HIGGS: %s", ActorName(npc));
				ClearGrabState();
				return;
			}
		}

		PlayerCharacter* player = *g_thePlayer;
		if (!player) {
			return;
		}

		NiPoint3 throatPoints[kThroatZoneMaxPoints];
		const int throatPointCount = BuildThroatZone(npc, throatPoints, kThroatZoneMaxPoints);
		if (throatPointCount <= 0) {
			return;
		}

		static UInt64 frameCounter = 0;
		++frameCounter;

		g_grabState.bladeOnNeckThisFrame = false;
		float nearestMissDist = 9999.0f;
		bool hadWeaponSegment = false;

		const bool handsToCheck[] = {true, false};
		for (bool isLeftHand : handsToCheck) {
			if (isLeftHand == g_grabState.isLeftHand) {
				continue;
			}

			WeaponSegment segment;
			if (!GetWeaponSegment(static_cast<Actor*>(player), isLeftHand, segment) || !segment.valid) {
				continue;
			}

			hadWeaponSegment = true;
			const ThroatContactResult contact =
				DistBladeToThroatZone(throatPoints, throatPointCount, segment.bottom, segment.top);
			if (!contact.valid) {
				continue;
			}

			if (contact.dist < nearestMissDist) {
				nearestMissDist = contact.dist;
			}

			if (contact.dist > BarebonesVR::fNeckContactDist) {
				continue;
			}

			const bool wasContact = g_grabState.neckContact;
			g_grabState.neckContact = true;
			g_grabState.bladeOnNeckThisFrame = true;
			g_grabState.bladeHandIsLeft = isLeftHand;
			g_grabState.contactLossFrames = 0;
			BeginContactTimer();
			MarkNeckContactNow();
			g_grabState.currentBladeTrackPoint = BladeTrackPoint(segment);

			if (!wasContact || frameCounter - g_grabState.lastContactLogFrame >= 60) {
				LogThroatSlitContact(npc, isLeftHand, contact, throatPointCount);
				g_grabState.lastContactLogFrame = frameCounter;
			}

			break;
		}

		if (!g_grabState.bladeOnNeckThisFrame && hadWeaponSegment &&
			BarebonesVR::logging >= BarebonesVR::LOGLEVEL_DEBUG &&
			frameCounter - g_grabState.lastNearMissLogFrame >= 45 &&
			fabsf(nearestMissDist - g_grabState.lastNearMissDist) > 1.0f) {
			LOG_DEBUG(
				"[ThroatSlit] Near miss | target=%s closestDist=%.1f need=%.1f throatPts=%d",
				ActorName(npc),
				nearestMissDist,
				BarebonesVR::fNeckContactDist,
				throatPointCount);
			g_grabState.lastNearMissLogFrame = frameCounter;
			g_grabState.lastNearMissDist = nearestMissDist;
		}

		if (g_grabState.bladeOnNeckThisFrame) {
			PushSlitSample(g_grabState.currentBladeTrackPoint);
			TryDetectSlit(static_cast<Actor*>(player), npc, frameCounter);
		} else if (g_grabState.slitHistoryCount > 0) {
			const int graceFrames =
				BarebonesVR::iSlitContactGraceFrames > 0 ? BarebonesVR::iSlitContactGraceFrames : 1;
			++g_grabState.contactLossFrames;
			if (g_grabState.contactLossFrames >= graceFrames) {
				ResetSlitHistory();
			}
		}

		if (g_grabState.neckContact && frameCounter % 120 == 0) {
			LOG_INFO("[ThroatSlit] Sustained neck contact on %s", ActorName(npc));
		}
	}

}  // namespace ThroatSlitVR
