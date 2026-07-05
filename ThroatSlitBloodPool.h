#pragma once

class Actor;

namespace ThroatSlitVR
{
	void InitThroatSlitBloodPoolApi();
	void ScheduleThroatSlitBloodPool(Actor* victim);
	void UpdatePendingBloodPools();
}  // namespace ThroatSlitVR
