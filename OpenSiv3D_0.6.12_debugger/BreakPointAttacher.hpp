#pragma once
#include <Windows.h>

enum class BreakPointType
{
	Init,
	Code,
	User,
};

class BreakPointAttacher
{
public:

	void initializeBreakPointHelper()
	{
		m_breakPoints.clear();
		m_isInitBpOccured = false;
		m_resetUserBpAddress = 0;
	}

	BreakPointType getBreakPointType(size_t address)
	{
		if (not m_isInitBpOccured)
		{
			m_isInitBpOccured = true;
			return BreakPointType::Init;
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

	HashTable<size_t, uint8_t> m_breakPoints; // アドレス→元のバイト値
	bool m_isInitBpOccured = false;
	Optional<size_t> m_resetUserBpAddress;
};
