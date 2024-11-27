#pragma once
#include <Windows.h>
#include "StepHandler.hpp"

class ProcessHandle;

enum class BreakPointType
{
	Init,
	Entry,
	Code,
	User,
	StepOver,
	StepOut,
};

class BreakPointAttacher
{
public:

	void initializeBreakPointHelper();

	BreakPointType getBreakPointType(size_t address);

	bool setUserBreakPointAt(const ProcessHandle& process, size_t address);

	bool cancelUserBreakPointAt(const ProcessHandle& process, size_t address);

	void setStepOverBreakPointAt(const ProcessHandle& process, size_t address);

	void cancelStepOverBreakPoint(const ProcessHandle& process);

	void setStepOutBreakPointAt(const ProcessHandle& process, size_t address);

	void cancelStepOutBreakPoint(const ProcessHandle& process);

	bool recoverUserBreakPoint(const ProcessHandle& process, size_t address);

	void saveResetUserBreakPoint(size_t address);

	void resetUserBreakPoint(const ProcessHandle& process);

	bool needResetBreakPoint()
	{
		return m_resetUserBpAddress.has_value();
	}

	const HashTable<size_t, uint8_t>& getUserBreakPoints()
	{
		return m_breakPoints;
	}

	bool isBeingStepOver() const { return m_isBeingStepOver; }
	void setBeingStepOver(bool value) { m_isBeingStepOver = value; }

	bool isBeingSingleInstruction() const { return m_isBeingSingleInstruction; }
	void setBeingSingleInstruction(bool value) { m_isBeingSingleInstruction = value; }

	bool isBeingStepOut() const { return m_isBeingStepOut; }
	void setBeingStepOut(bool value) { m_isBeingStepOut = value; }

private:

	uint8_t setBreakPointAt(const ProcessHandle& process, size_t address);

	void recoverBreakPoint(const ProcessHandle& process, size_t address, uint8_t original);

	using BreakPoint = std::pair<size_t, uint8_t>;

	HashTable<size_t, uint8_t> m_breakPoints; // アドレス→元のバイト値
	Optional<BreakPoint> m_stepOverBp;
	Optional<BreakPoint> m_stepOutBp;
	bool m_isFirstBpOccured = false;
	bool m_isSecondBpOccured = false;
	Optional<size_t> m_resetUserBpAddress;

	bool m_isBeingStepOver = false;
	bool m_isBeingStepOut = false;
	bool m_isBeingSingleInstruction = false;
};
