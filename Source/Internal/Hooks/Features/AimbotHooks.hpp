#pragma once

#include "pch.h"

class AimbotHooks
{
private:
	bool m_bInstalled = false;

public:
	bool Setup();
	void Destroy();
	bool IsInstalled() const { return m_bInstalled; };
	static bool IsSilentReady();
	static uint64_t GetOverrideHitCount();
};
