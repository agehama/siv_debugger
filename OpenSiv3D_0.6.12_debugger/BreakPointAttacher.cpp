#include "BreakPointAttacher.hpp"
#include "ProcessHandle.hpp"

void BreakPointAttacher::initializeBreakPointHelper()
{
	m_breakPoints.clear();
	m_isFirstBpOccured = false;
	m_isSecondBpOccured = false;
	m_resetUserBpAddress = 0;

	m_isBeingSingleInstruction = false;
	m_isBeingStepOut = false;
	m_isBeingStepOver = false;
}

BreakPointType BreakPointAttacher::getBreakPointType(size_t address)
{
	// プロセス起動直後に自動で呼ばれるブレークポイント
	if (not m_isFirstBpOccured)
	{
		m_isFirstBpOccured = true;
		return BreakPointType::Init;
	}

	// Main()の先頭に張ったブレークポイント
	if (not m_isSecondBpOccured)
	{
		m_isSecondBpOccured = true;
		return BreakPointType::Entry;
	}

	if (m_stepOverBp && m_stepOverBp.value().first == address)
	{
		return BreakPointType::StepOver;
	}

	if (m_stepOutBp && m_stepOutBp.value().first == address)
	{
		return BreakPointType::StepOut;
	}

	for (const auto& breakPoint : m_breakPoints)
	{
		if (breakPoint.first == address)
		{
			return BreakPointType::User;
		}
	}

	return BreakPointType::Code;
}

bool BreakPointAttacher::setUserBreakPointAt(const ProcessHandle& process, size_t address)
{
	for (const auto& breakPoint : m_breakPoints)
	{
		if (breakPoint.first == address)
		{
			return false;
		}
	}

	m_breakPoints[address] = setBreakPointAt(process, address);
	return true;
}

bool BreakPointAttacher::cancelUserBreakPointAt(const ProcessHandle& process, size_t address)
{
	for (auto it = m_breakPoints.begin(); it != m_breakPoints.end(); ++it)
	{
		if (it->first == address)
		{
			recoverBreakPoint(process, it->first, it->second);
			m_breakPoints.erase(it);
			return true;
		}
	}

	//std::cout << "アドレス:";
	//printHex(address, false);
	//std::cout << "にはブレークポイントがセットされていません" << std::endl;
	return false;
}

void BreakPointAttacher::setStepOverBreakPointAt(const ProcessHandle& process, size_t address)
{
	BreakPoint bp;
	bp.first = address;
	bp.second = setBreakPointAt(process, address);
	m_stepOverBp = bp;
}

void BreakPointAttacher::cancelStepOverBreakPoint(const ProcessHandle& process)
{
	if (m_stepOverBp)
	{
		const auto& bp = m_stepOverBp.value();
		recoverBreakPoint(process, bp.first, bp.second);
		m_stepOverBp = none;
	}
	//m_isBeingStepOver = false;
}

void BreakPointAttacher::setStepOutBreakPointAt(const ProcessHandle& process, size_t address)
{
	BreakPoint bp;
	bp.first = address;
	bp.second = setBreakPointAt(process, address);
	m_stepOutBp = bp;
}

void BreakPointAttacher::cancelStepOutBreakPoint(const ProcessHandle& process)
{
	if (m_stepOutBp)
	{
		const auto& bp = m_stepOutBp.value();
		recoverBreakPoint(process, bp.first, bp.second);
		m_stepOutBp = none;
	}
	//m_isBeingStepOut = false;
}

bool BreakPointAttacher::recoverUserBreakPoint(const ProcessHandle& process, size_t address)
{
	for (const auto& breakPoint : m_breakPoints)
	{
		if (breakPoint.first == address)
		{
			recoverBreakPoint(process, breakPoint.first, breakPoint.second);
			//std::cout << "アドレス:";
			//printHex(address, false);
			//std::cout << "の命令を ";
			//printHex(bp.content, false);
			//std::cout << " に復元しました" << std::endl;

			return true;
		}
	}

	//std::cout << "アドレス:";
	//printHex(address, false);
	//std::cout << "にはブレークポイントがセットされていません" << std::endl;
	return false;
}

void BreakPointAttacher::saveResetUserBreakPoint(size_t address)
{
	m_resetUserBpAddress = address;
}

void BreakPointAttacher::resetUserBreakPoint(const ProcessHandle& process)
{
	for (const auto& breakPoint : m_breakPoints)
	{
		if (breakPoint.first == m_resetUserBpAddress)
		{
			setBreakPointAt(process, breakPoint.first);
			m_resetUserBpAddress = none;
		}
	}
}

uint8_t BreakPointAttacher::setBreakPointAt(const ProcessHandle& process, size_t address)
{
	uint8_t original;
	process.readMemory(address, original);

	const uint8_t breakOp = 0xCC;
	process.writeMemory(address, breakOp);

	return original;
}

void BreakPointAttacher::recoverBreakPoint(const ProcessHandle& process, size_t address, uint8_t original)
{
	process.writeMemory(address, 1, &original);
}
