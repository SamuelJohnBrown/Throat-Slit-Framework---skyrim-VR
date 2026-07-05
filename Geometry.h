#pragma once

#include "skse64/NiTypes.h"

class Actor;
class NiAVObject;
class TESObjectWEAP;

namespace ThroatSlitVR
{
	struct WeaponSegment
	{
		NiPoint3 bottom{};
		NiPoint3 top{};
		bool valid = false;
	};

	struct SegmentDistResult
	{
		float dist = 9999.0f;
		NiPoint3 contactPoint{};
		NiPoint3 throatPoint{};
	};

	struct ThroatContactResult
	{
		float dist = 9999.0f;
		NiPoint3 bladePoint{};
		NiPoint3 throatPoint{};
		int throatPointIndex = -1;
		bool valid = false;
	};

	constexpr int kThroatZoneMaxPoints = 8;

	float PointLength(const NiPoint3& p);
	float PointLengthXY(const NiPoint3& p);
	NiPoint3 Normalize(const NiPoint3& p);

	inline float VecDot(const NiPoint3& a, const NiPoint3& b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	NiPoint3 ActorForwardHorizontal(const Actor* actor);
	NiPoint3 ActorRightHorizontal(const Actor* actor);
	NiPoint3 DirectionHorizontal(const NiPoint3& from, const NiPoint3& to);

	SegmentDistResult DistPointToSegment(const NiPoint3& point, const NiPoint3& segA, const NiPoint3& segB);

	int BuildThroatZone(const Actor* npc, NiPoint3* outPoints, int maxPoints);
	ThroatContactResult DistBladeToThroatZone(
		const NiPoint3* throatPoints,
		int throatPointCount,
		const NiPoint3& segBottom,
		const NiPoint3& segTop);

	bool GetWeaponSegment(Actor* actor, bool isLeftHand, WeaponSegment& outSegment);
	bool IsThroatSlitEligibleWeapon(TESObjectWEAP* weapon);
	NiAVObject* FindBone(Actor* actor, const char* boneName);

}  // namespace ThroatSlitVR
