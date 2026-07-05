#include "Geometry.h"

#include "config.h"

#include "skse64/GameReferences.h"
#include "skse64/GameObjects.h"
#include "skse64/GameRTTI.h"
#include "skse64/NiNodes.h"
#include "skse64/NiObjects.h"

namespace ThroatSlitVR
{
	float PointLength(const NiPoint3& p)
	{
		return sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
	}

	float PointLengthXY(const NiPoint3& p)
	{
		return sqrtf(p.x * p.x + p.y * p.y);
	}

	NiPoint3 Normalize(const NiPoint3& p)
	{
		const float len = PointLength(p);
		if (len < 0.0001f) {
			return NiPoint3(0.0f, 0.0f, 0.0f);
		}
		return NiPoint3(p.x / len, p.y / len, p.z / len);
	}

	NiPoint3 ActorForwardHorizontal(const Actor* actor)
	{
		if (!actor) {
			return NiPoint3(0.0f, 1.0f, 0.0f);
		}

		const float yawRad = actor->rot.z * (MATH_PI / 180.0f);
		return Normalize(NiPoint3(-sinf(yawRad), cosf(yawRad), 0.0f));
	}

	NiPoint3 ActorRightHorizontal(const Actor* actor)
	{
		const NiPoint3 forward = ActorForwardHorizontal(actor);
		return NiPoint3(-forward.y, forward.x, 0.0f);
	}

	NiPoint3 DirectionHorizontal(const NiPoint3& from, const NiPoint3& to)
	{
		NiPoint3 dir = to - from;
		dir.z = 0.0f;
		return Normalize(dir);
	}

	static NiPoint3 ClosestPointOnSegment(const NiPoint3& point, const NiPoint3& segA, const NiPoint3& segB)
	{
		const NiPoint3 ab = segB - segA;
		const float abLenSq = VecDot(ab, ab);
		if (abLenSq < 0.00005f) {
			return segA;
		}

		float t = VecDot(point - segA, ab) / abLenSq;
		if (t <= 0.0f) {
			return segA;
		}
		if (t >= 1.0f) {
			return segB;
		}
		return segA + ab * t;
	}

	NiAVObject* FindBone(Actor* actor, const char* boneName)
	{
		if (!actor || !boneName || !boneName[0]) {
			return nullptr;
		}

		NiAVObject* root = actor->GetNiNode();
		if (!root) {
			return nullptr;
		}

		BSFixedString nodeName(boneName);
		return root->GetObjectByName(&nodeName.data);
	}

	SegmentDistResult DistPointToSegment(const NiPoint3& point, const NiPoint3& segA, const NiPoint3& segB)
	{
		SegmentDistResult result;
		result.contactPoint = ClosestPointOnSegment(point, segA, segB);
		result.throatPoint = point;
		result.dist = PointLength(result.contactPoint - point);
		return result;
	}

	int BuildThroatZone(const Actor* npc, NiPoint3* outPoints, int maxPoints)
	{
		if (!npc || !outPoints || maxPoints <= 0) {
			return 0;
		}

		static const char* kNeckBoneNames[] = {
			"NPC Neck [Neck]",
			"Neck",
		};

		NiPoint3 neckPos{};
		bool hasNeck = false;
		for (const char* boneName : kNeckBoneNames) {
			NiAVObject* neck = FindBone(const_cast<Actor*>(npc), boneName);
			if (neck) {
				neckPos = neck->m_worldTransform.pos;
				hasNeck = true;
				break;
			}
		}

		if (!hasNeck) {
			static const char* kHeadBoneNames[] = {
				"NPC Head [Head]",
				"Head",
			};
			for (const char* boneName : kHeadBoneNames) {
				NiAVObject* head = FindBone(const_cast<Actor*>(npc), boneName);
				if (head) {
					const NiPoint3 headPos = head->m_worldTransform.pos;
					const NiPoint3 forward = ActorForwardHorizontal(npc);
					neckPos = headPos - forward * (BarebonesVR::fThroatForwardOffset * 0.55f) - NiPoint3(0.0f, 0.0f, 8.0f);
					hasNeck = true;
					break;
				}
			}
		}

		if (!hasNeck) {
			return 0;
		}

		const NiPoint3 forward = ActorForwardHorizontal(npc);
		const NiPoint3 right = ActorRightHorizontal(npc);
		const float fwd = BarebonesVR::fThroatForwardOffset;
		const float side = BarebonesVR::fThroatSideOffset;

		int count = 0;
		outPoints[count++] = neckPos;
		if (count >= maxPoints) {
			return count;
		}

		// Front of neck / Adam's apple — where the blade actually sits from behind.
		outPoints[count++] = neckPos + forward * fwd;
		if (count >= maxPoints) {
			return count;
		}

		outPoints[count++] = neckPos + forward * (fwd * 0.75f) + NiPoint3(0.0f, 0.0f, side);
		if (count >= maxPoints) {
			return count;
		}

		outPoints[count++] = neckPos + forward * (fwd * 0.75f) - NiPoint3(0.0f, 0.0f, side);
		if (count >= maxPoints) {
			return count;
		}

		outPoints[count++] = neckPos + right * side;
		if (count >= maxPoints) {
			return count;
		}

		outPoints[count++] = neckPos - right * side;
		if (count >= maxPoints) {
			return count;
		}

		NiAVObject* head = FindBone(const_cast<Actor*>(npc), "NPC Head [Head]");
		if (!head) {
			head = FindBone(const_cast<Actor*>(npc), "Head");
		}
		if (head && count < maxPoints) {
			const NiPoint3 headPos = head->m_worldTransform.pos;
			outPoints[count++] = headPos - forward * (fwd * 0.45f) - NiPoint3(0.0f, 0.0f, 6.0f);
		}

		return count;
	}

	ThroatContactResult DistBladeToThroatZone(
		const NiPoint3* throatPoints,
		int throatPointCount,
		const NiPoint3& segBottom,
		const NiPoint3& segTop)
	{
		ThroatContactResult best;
		if (!throatPoints || throatPointCount <= 0) {
			return best;
		}

		for (int i = 0; i < throatPointCount; ++i) {
			const SegmentDistResult segmentDist = DistPointToSegment(throatPoints[i], segBottom, segTop);
			if (segmentDist.dist < best.dist) {
				best.dist = segmentDist.dist;
				best.bladePoint = segmentDist.contactPoint;
				best.throatPoint = throatPoints[i];
				best.throatPointIndex = i;
				best.valid = true;
			}

			const float tipDist = PointLength(throatPoints[i] - segTop);
			if (tipDist < best.dist) {
				best.dist = tipDist;
				best.bladePoint = segTop;
				best.throatPoint = throatPoints[i];
				best.throatPointIndex = i;
				best.valid = true;
			}
		}

		return best;
	}

	static NiAVObject* FindWeaponNode(NiAVObject* root, bool isLeftHand)
	{
		if (!root) {
			return nullptr;
		}

		static const char* kLeftNodeNames[] = {
			"SHIELD",
			"NPC L Hand [RHnd]",
		};
		static const char* kRightNodeNames[] = {
			"WEAPON",
		};

		if (isLeftHand) {
			for (const char* nodeName : kLeftNodeNames) {
				BSFixedString nodeNameStr(nodeName);
				NiAVObject* node = root->GetObjectByName(&nodeNameStr.data);
				if (node) {
					return node;
				}
			}
			return nullptr;
		}

		for (const char* nodeName : kRightNodeNames) {
			BSFixedString nodeNameStr(nodeName);
			NiAVObject* node = root->GetObjectByName(&nodeNameStr.data);
			if (node) {
				return node;
			}
		}

		return nullptr;
	}

	static void GetReachAndHandle(TESObjectWEAP* weapon, float& reach, float& handle)
	{
		reach = 0.0f;
		handle = 0.0f;
		if (!weapon) {
			return;
		}

		switch (weapon->type()) {
			case TESObjectWEAP::GameData::kType_OneHandDagger:
				reach = 20.0f;
				handle = 10.0f;
				break;
			case TESObjectWEAP::GameData::kType_OneHandSword:
				reach = 90.0f;
				handle = 12.0f;
				break;
			case TESObjectWEAP::GameData::kType_OneHandAxe:
			case TESObjectWEAP::GameData::kType_OneHandMace:
				reach = 60.0f;
				handle = 12.0f;
				break;
			case TESObjectWEAP::GameData::kType_TwoHandSword:
				reach = 100.0f;
				handle = 20.0f;
				break;
			case TESObjectWEAP::GameData::kType_TwoHandAxe:
				reach = 70.0f;
				handle = 45.0f;
				break;
			case TESObjectWEAP::GameData::kType_Bow:
			case TESObjectWEAP::GameData::kType_Bow2:
			case TESObjectWEAP::GameData::kType_Staff:
			case TESObjectWEAP::GameData::kType_Staff2:
				return;
			default:
				reach = 70.0f;
				handle = 12.0f;
				break;
		}

		reach *= BarebonesVR::fWeaponRangeMulti;
		handle *= BarebonesVR::fWeaponRangeMulti;
	}

	bool IsThroatSlitEligibleWeapon(TESObjectWEAP* weapon)
	{
		if (!weapon) {
			return false;
		}

		switch (weapon->type()) {
			case TESObjectWEAP::GameData::kType_OneHandMace:
			case TESObjectWEAP::GameData::kType_1HM:
				return false;
			case TESObjectWEAP::GameData::kType_TwoHandAxe:
			case TESObjectWEAP::GameData::kType_2HA:
				return false;
			default:
				return true;
		}
	}

	bool GetWeaponSegment(Actor* actor, bool isLeftHand, WeaponSegment& outSegment)
	{
		outSegment = {};

		if (!actor) {
			return false;
		}

		TESForm* equipped = actor->GetEquippedObject(isLeftHand);
		if (!equipped || equipped->IsArmor()) {
			return false;
		}

		TESObjectWEAP* weapon = DYNAMIC_CAST(equipped, TESForm, TESObjectWEAP);
		if (!weapon || !IsThroatSlitEligibleWeapon(weapon)) {
			return false;
		}

		float reach = 0.0f;
		float handle = 0.0f;
		GetReachAndHandle(weapon, reach, handle);
		if (reach <= 0.0f) {
			return false;
		}

		NiAVObject* root = actor->GetNiNode();
		if (!root) {
			return false;
		}

		const char* nodeName = isLeftHand ? "SHIELD" : "WEAPON";
		BSFixedString nodeNameStr(nodeName);
		NiAVObject* weaponNode = root->GetObjectByName(&nodeNameStr.data);
		if (!weaponNode) {
			weaponNode = FindWeaponNode(root, isLeftHand);
		}
		if (!weaponNode) {
			return false;
		}

		const NiPoint3 weaponDir = NiPoint3{
			weaponNode->m_worldTransform.rot.data[0][1],
			weaponNode->m_worldTransform.rot.data[1][1],
			weaponNode->m_worldTransform.rot.data[2][1],
		};

		const float reachScale = BarebonesVR::fBladeTipReachBonus > 0.0f ? BarebonesVR::fBladeTipReachBonus : 1.0f;
		outSegment.bottom = weaponNode->m_worldTransform.pos - weaponDir * handle;
		outSegment.top = weaponNode->m_worldTransform.pos + weaponDir * (reach * reachScale);
		outSegment.valid = true;
		return true;
	}

}  // namespace ThroatSlitVR
