#include <Windows.h>
#include <DbgHelp.h>
#include <Siv3D.hpp>
#include "SymbolExplorer.hpp"
#include "RegisterHandler.hpp"

bool SymbolExplorer::init(const CREATE_PROCESS_DEBUG_INFO* pInfo, HANDLE process)
{
	//m_process = pInfo->hProcess;
	m_process = process;

	SymSetOptions(SYMOPT_LOAD_LINES);

	if (SymInitialize(m_process, NULL, FALSE))
	{
		DWORD64 moduleAddress = SymLoadModule64(
			m_process,
			pInfo->hFile,
			NULL,
			NULL,
			(DWORD64)pInfo->lpBaseOfImage,
			0
		);

		if (moduleAddress != 0)
		{
			return true;
		}
		else
		{
			Console << U"SymLoadModule64 failed: " << GetLastError();
			return false;
		}
	}
	else
	{
		Console << U"SymInitialize failed: " << GetLastError();
		return false;
	}
}

void SymbolExplorer::dispose()
{
	SymCleanup(m_process);
	m_process = NULL;
}

Optional<size_t> SymbolExplorer::findAddress(const String& symbolName) const
{
	SYMBOL_INFO symbolInfo = {};
	symbolInfo.SizeOfStruct = sizeof(SYMBOL_INFO);
	if (SymFromName(m_process, Unicode::ToUTF8(symbolName).c_str(), &symbolInfo))
	{
		return std::bit_cast<size_t>(symbolInfo.Address);
	}
	return none;
}

void SymbolExplorer::onDllLoaded(const LOAD_DLL_DEBUG_INFO* pInfo) const
{
	DWORD64 moduleAddress = SymLoadModule64(
		m_process,
		pInfo->hFile,
		NULL,
		NULL,
		(DWORD64)pInfo->lpBaseOfDll,
		0
	);

	if (moduleAddress == 0)
	{
		Console << U"SymLoadModule64 failed: " << GetLastError();
	}

	CloseHandle(pInfo->hFile);
}

void SymbolExplorer::onDllUnloaded(const UNLOAD_DLL_DEBUG_INFO* pInfo) const
{
	SymUnloadModule64(m_process, (DWORD64)pInfo->lpBaseOfDll);
}

Optional<size_t> SymbolExplorer::tryGetCallInstructionBytesLength(size_t address) const
{
	std::uint8_t instruction[10];

	size_t numOfBytes;
	auto pAddress = reinterpret_cast<LPVOID>(address);
	ReadProcessMemory(
		m_process,
		pAddress,
		instruction,
		sizeof(instruction) / sizeof(std::uint8_t),
		&numOfBytes);

	switch (instruction[0])
	{
	case 0xE8:
		return 5;

	case 0x9A:
		return 7;

	case 0xFF:
		switch (instruction[1])
		{
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x16:
		case 0x17:
		case 0xD0:
		case 0xD1:
		case 0xD2:
		case 0xD3:
		case 0xD4:
		case 0xD5:
		case 0xD6:
		case 0xD7:
			return 2;

		case 0x14:
		case 0x50:
		case 0x51:
		case 0x52:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57:
			return 3;

		case 0x15:
		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x95:
		case 0x96:
		case 0x97:
			return 6;

		case 0x94:
			return 7;
		}
	default:
		return none;
	}
}

Optional<size_t> SymbolExplorer::getRetInstructionAddress(HANDLE thread) const
{
	auto contextOpt = RegisterHandler::getDebuggeeContext(thread);
	if (not contextOpt)
	{
		return none;
	}

	const auto& context = contextOpt.value();

	DWORD64 displacement;
	SYMBOL_INFO symbol = {};
	symbol.SizeOfStruct = sizeof(SYMBOL_INFO);

	if (not SymFromAddr(
		m_process,
		context.Rip,
		&displacement,
		&symbol))
	{
		return none;
	}

	// symbol.Address: 関数の最初の命令アドレス, symbol.Size: 関数の全ての命令のバイト長
	const size_t endAddress = symbol.Address + symbol.Size;

	if (auto lenOpt = retInstructionLength(endAddress - 3); lenOpt && lenOpt.value() == 3)
	{
		return endAddress - lenOpt.value();
	}

	if (auto lenOpt = retInstructionLength(endAddress - 1); lenOpt && lenOpt.value() == 1)
	{
		return endAddress - lenOpt.value();
	}

	return none;
}

Optional<size_t> SymbolExplorer::retInstructionLength(size_t address) const
{
	std::uint8_t readByte;
	size_t numOfBytes;
	auto pAddress = reinterpret_cast<LPVOID>(address);
	ReadProcessMemory(m_process, pAddress, &readByte, 1, &numOfBytes);

	if (readByte == 0xC3 || readByte == 0xCB)
	{
		return 1;
	}

	if (readByte == 0xC2 || readByte == 0xCA)
	{
		return 3;
	}

	return none;
}

Optional<LINE_INFO> SymbolExplorer::GetCurrentLineInfo(HANDLE process, HANDLE thread)
{
	auto contextOpt = RegisterHandler::getDebuggeeContext(thread);
	if (not contextOpt)
	{
		return none;
	}

	const auto context = contextOpt.value();

	DWORD displacement;
	IMAGEHLP_LINE64 lineInfo = {};
	lineInfo.SizeOfStruct = sizeof(lineInfo);

	if (SymGetLineFromAddr64(
		process,
		context.Rip,
		&displacement,
		&lineInfo))
	{
		LINE_INFO currentLineInfo;
		currentLineInfo.fileName = Unicode::FromUTF8(std::string(lineInfo.FileName));
		currentLineInfo.lineNumber = lineInfo.LineNumber;
		if (not currentLineInfo.fileName.ends_with(U"Main.cpp"))
		{
			return none;
		}
		return currentLineInfo;
	}
	else
	{
		Console << U"error: " << GetLastError();
	}

	return none;
}
