﻿#pragma once
#include <Windows.h>
#include "StepHandler.hpp"

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

	void initializeBreakPointHelper()
	{
		m_breakPoints.clear();
		m_isFirstBpOccured = false;
		m_isSecondBpOccured = false;
		m_resetUserBpAddress = 0;

		m_isBeingSingleInstruction = false;
		m_isBeingStepOut = false;
		m_isBeingStepOver = false;
	}

	BreakPointType getBreakPointType(size_t address)
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

	bool setUserBreakPointAt(HANDLE process, size_t address)
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

	bool cancelUserBreakPointAt(HANDLE process, size_t address)
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

	void setStepOverBreakPointAt(HANDLE process, size_t address)
	{
		BreakPoint bp;
		bp.first = address;
		bp.second = setBreakPointAt(process, address);
		m_stepOverBp = bp;
	}

	void cancelStepOverBreakPoint(HANDLE process)
	{
		if (m_stepOverBp)
		{
			const auto& bp = m_stepOverBp.value();
			recoverBreakPoint(process, bp.first, bp.second);
			m_stepOverBp = none;
		}
		//m_isBeingStepOver = false;
	}

	void setStepOutBreakPointAt(HANDLE process, size_t address)
	{
		BreakPoint bp;
		bp.first = address;
		bp.second = setBreakPointAt(process, address);
		m_stepOutBp = bp;
	}

	void cancelStepOutBreakPoint(HANDLE process)
	{
		if (m_stepOutBp)
		{
			const auto& bp = m_stepOutBp.value();
			recoverBreakPoint(process, bp.first, bp.second);
			m_stepOutBp = none;
		}
		//m_isBeingStepOut = false;
	}

	bool recoverUserBreakPoint(HANDLE process, size_t address)
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

	void saveResetUserBreakPoint(size_t address)
	{
		m_resetUserBpAddress = address;
	}

	void resetUserBreakPoint(HANDLE process)
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

	uint8_t setBreakPointAt(HANDLE process, size_t address)
	{
		auto pAddress = reinterpret_cast<LPVOID>(address);
		size_t numOfBytes;

		uint8_t original;
		ReadProcessMemory(process, pAddress, &original, 1, &numOfBytes);

		uint8_t breakOp = 0xCC;
		WriteProcessMemory(process, pAddress, &breakOp, 1, &numOfBytes);

		return original;
	}

	void recoverBreakPoint(HANDLE process, size_t address, uint8_t original)
	{
		size_t numOfBytes;
		auto pAddress = reinterpret_cast<LPVOID>(address);
		WriteProcessMemory(process, pAddress, &original, 1, &numOfBytes);
	}

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
