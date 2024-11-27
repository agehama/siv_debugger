#include <Windows.h>
#include <DbgHelp.h>
#include <Siv3D.hpp>
#include "ProcessHandle.hpp"
#include "ThreadHandle.hpp"
#include "UserSourceFiles.hpp"
#include "TypeHelper.hpp"
#include "ProcessHandle.hpp"

namespace
{
	// シンボルの仮想アドレスを取得する 
	// シンボルがローカル変数または引数の場合、 
	// pSymbol->AddressはRBPに対するオフセットであり、 
	// 両者を加算するとシンボルの仮想アドレスになる
	size_t GetSymbolAddress(PSYMBOL_INFO pSymbolInfo, HANDLE processHandle, const CONTEXT& context)
	{
		if ((pSymbolInfo->Flags & SYMFLAG_REGREL) == 0)
		{
			return pSymbolInfo->Address;
		}

		DWORD64 displacement;
		SYMBOL_INFO symbolInfo = {};
		symbolInfo.SizeOfStruct = sizeof(SYMBOL_INFO);

		SymFromAddr(
			processHandle,
			context.Rip,
			&displacement,
			&symbolInfo);

		// RIPが関数の最初の命令を指している場合、RBPの値は前の関数に属しているため使えない
		// 代わりにRSP-4をシンボルの基底アドレスとして使用する
		if (displacement == 0)
		{
			return context.Rsp - 4 + pSymbolInfo->Address;
		}

		return context.Rbp + pSymbolInfo->Address;
	}

	BOOL __stdcall SourceFilesProc(PSOURCEFILEW pSourceFile, PVOID)
	{
		std::wstring str(pSourceFile->FileName);
		const auto fileNmae = Unicode::FromWstring(str);
		if (fileNmae.ends_with(U".cpp") || fileNmae.ends_with(U".hpp"))
		{
			if (
				!fileNmae.contains(UR"(vctools)") &&
				!fileNmae.contains(UR"(minkernel)") &&
				!fileNmae.contains(UR"(onecore)") &&
				!fileNmae.contains(UR"(avcore)") &&
				!fileNmae.contains(UR"(\Program Files)") &&
				!fileNmae.contains(UR"(\a\_work\1\s\)") &&
				!fileNmae.contains(UR"(shared\inc\)") &&
				!fileNmae.contains(UR"(directx)") &&
				!fileNmae.contains(UR"(VCCRT)") &&
				!fileNmae.contains(UR"(Siv3D.hpp)") &&
				!fileNmae.contains(UR"(\include\Siv3D\)") &&
				!fileNmae.contains(UR"(\include\ThirdParty\)")
				)
			{
				UserSourceFiles::AddFile(Unicode::FromWstring(str));
				Console << Unicode::FromWstring(str);
			}
		}

		return TRUE;
	}

	struct EnumUserData
	{
		Array<VariableInfo> userVarInfoList;
		HashSet<String> systemVarNameList;
		HANDLE process;
		CONTEXT context;
	};

	BOOL CALLBACK EnumVariablesCallBack(PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext)
	{
		auto pUserData = reinterpret_cast<EnumUserData*>(UserContext);

		if (pSymInfo->Tag == SymTagEnum::SymTagData)
		{
			const auto varName = Unicode::FromUTF8(std::string(pSymInfo->Name));
			if (!varName.starts_with(UR"(_)") &&
				!varName.starts_with(UR"(std::)") &&
				!varName.starts_with(UR"(DirectX::)") &&
				!varName.starts_with(UR"(s3d::)") &&
				!varName.starts_with(UR"(IID_)") &&
				!varName.starts_with(UR"(GUID_)") &&
				!varName.starts_with(UR"(CLSID_)") &&
				!varName.starts_with(UR"(WPD_)") &&
				!varName.starts_with(UR"(PKEY_)") &&
				!varName.starts_with(UR"(MF)") &&
				!varName.starts_with(UR"(L_)") &&
				!varName.starts_with(UR"(TID_)") &&
				!varName.starts_with(UR"(LIBID_)") &&
				!varName.starts_with(UR"(DIID_)") &&
				!varName.starts_with(UR"(s_f)") &&
				!varName.starts_with(UR"(MMS)") &&
				!varName.starts_with(UR"(MMI)") &&
				!varName.starts_with(UR"(MDE_)") &&
				!varName.starts_with(UR"(Concurrency::)") &&
				!varName.starts_with(UR"(ENHANCED_STORAGE_)") &&
				!varName.starts_with(UR"(MSBBUILDER_)") &&
				!varName.starts_with(UR"(SDPBUILDER_)") &&
				!varName.starts_with(UR"(ME_)") &&
				!varName.starts_with(UR"(MEDIACACHE_)") &&
				!varName.starts_with(UR"(SPROP_)") &&
				!varName.starts_with(UR"(PPM_)") &&
				!varName.starts_with(UR"(UnDecorator::)") &&
				!varName.starts_with(UR"(NETSTREAMSINK_)") &&
				!varName.starts_with(UR"(Dload)") &&
				!varName.starts_with(UR"(MPEG4_RTP_AU_)") &&
				!varName.starts_with(UR"(DPAID_)") &&
				!varName.starts_with(UR"(module_)") &&
				!varName.starts_with(UR"(DSDEVID_)") &&
				!varName.starts_with(UR"(DDVPTYPE_)") &&
				!varName.starts_with(UR"(DPSPGUID_)") &&
				!varName.starts_with(UR"(FIREWALL_PORT_)") &&
				!varName.contains(UR"($)")
				)
			{
				if (!pUserData->systemVarNameList.contains(varName))
				{
					Console << U"U\"" << varName << U"\",";

					VariableInfo varInfo;
					varInfo.address = GetSymbolAddress(pSymInfo, pUserData->process, pUserData->context);
					varInfo.modBase = pSymInfo->ModBase;
					varInfo.size = SymbolSize;
					varInfo.typeID = pSymInfo->TypeIndex;
					varInfo.name = varName;
					pUserData->userVarInfoList.push_back(varInfo);
				}
			}
		}

		return TRUE;
	}

	const char32_t* systemVarNames[] =
	{
		U"enable_percent_n",
		U"DOMAIN_LEAVE_GUID",
		U"MR_AUDIO_RENDER_SERVICE",
		U"init_atexit",
		U"encoded_function_pointers",
		U"w_tzdst_program",
		U"uninit_postmsg",
		U"c_dfDIJoystick2",
		U"errno_no_memory",
		U"DiGenreDeviceOrder",
		U"dststart",
		U"user_matherr",
		U"cereal::detail::StaticObject<cereal::detail::PolymorphicCasters>::instance",
		U"DS3DALG_HRTF_LIGHT",
		U"NAMED_PIPE_EVENT_GUID",
		U"mspdbName",
		U"doserrno_no_memory",
		U"pre_c_initializer",
		U"s_dwAbsMask",
		U"pre_cpp_initializer",
		U"atcount_cdecl",
		U"NO_SUBGROUP_GUID",
		U"pDNameNode::`vftable'",
		U"NETSERVER_UDP_PACKETPAIR_PACKET",
		U"pairNode::`vftable'",
		U"console_ctrl_handler_installed",
		U"g_tss_cv",
		U"type_info::`vftable'",
		U"more_info_string",
		U"term_action",
		U"NvOptimusEnablement",
		U"c",
		U"s",
		U"c_rgodfDIJoy2",
		U"GS_ContextRecord",
		U"NETSERVER_TCP_PACKETPAIR_PACKET",
		U"ProtectionSystemID_MicrosoftPlayReady",
		U"DS3DALG_HRTF_FULL",
		U"KSDATAFORMAT_SUBTYPE_MIDI",
		U"fSystemSet",
		U"c1",
		U"SPDFID_WaveFormatEx",
		U"is_initialized_as_dll",
		U"MR_VOLUME_CONTROL_SERVICE",
		U"USER_POLICY_PRESENT_GUID",
		U"RtlNtPathSeperatorString",
		U"g_tss_srw",
		U"tzname_states",
		U"w_tzstd_program",
		U"tz_api_used",
		U"w_tzname",
		U"AmdPowerXpressRequestHighPerformance",
		U"c23",
		U"KSDATAFORMAT_SUBTYPE_DIRECTMUSIC",
		U"LcidToLocaleNameTable",
		U"block_use_names",
		U"rttiTable",
		U"RtlAlternateDosPathSeperatorString",
		U"ProtectionSystemID_Hdcp2AudioID",
		U"post_pgo_initializer",
		U"errtable",
		U"ProtectionSystemID_Hdcp2VideoID",
		U"NETWORK_MANAGER_LAST_IP_ADDRESS_REMOVAL_GUID",
		U"abort_action",
		U"ctrlbreak_action",
		U"IndirectionName",
		U"CEACTIVATE_ATTRIBUTE_DEFAULT_HRESULT",
		U"EventTraceGuid",
		U"atfuns_cdecl",
		U"MESourceSupportedRatesChanged",
		U"MESourceNeedKey",
		U"LocaleNameToIndexTable",
		U"report_type_messages",
		U"GS_ExceptionPointers",
		U"DS3DALG_NO_VIRTUALIZATION",
		U"heap",
		U"debugCrtFileName",
		U"mspdb",
		U"NETWORK_MANAGER_FIRST_IP_ADDRESS_ARRIVAL_GUID",
		U"SPDFID_Text",
		U"CODECAPI_CAPTURE_SCENARIO",
		U"EventTraceConfigGuid",
		U"DPLPROPERTY_PlayerScore",
		U"MACHINE_POLICY_PRESENT_GUID",
		U"ln2",
		U"PrivateLoggerNotificationGuid",
		U"GS_ExceptionRecord",
		U"tz_info",
		U"RtlDosPathSeperatorsString",
		U"VIDEO_SINK_BROADCASTING_PORT",
		U"pterm",
		U"initlocks",
		U"initlocks",
		U"initlocks",
		U"initlocks",
		U"tzset_init_state",
		U"DPLPROPERTY_PlayerGuid",
		U"function_pointers",
		U"thread_local_exit_callback_func",
		U"tzstd_program",
		U"charNode::`vftable'",
		U"w_tzname_states",
		U"FH4::s_shiftTab",
		U"DOMAIN_JOIN_GUID",
		U"pcharNode::`vftable'",
		U"DNameNode::`vftable'",
		U"ProtectionSystemID_Hdcp2SystemID",
		U"DPLPROPERTY_LobbyGuid",
		U"tokenTable",
		U"heap_validation_pending",
		U"stack_premsg",
		U"FORMAT_MFVideoFormat",
		U"ctrlc_action",
		U"nameTable",
		U"hugexp",
		U"fmt::v8::detail::micro",
		U"dstend",
		U"DefaultTraceSecurityGuid",
		U"uninit_premsg",
		U"c_termination_complete",
		U"PrefixName",
		U"SPGDF_ContextFree",
		U"CUSTOM_SYSTEM_STATE_CHANGE_EVENT_GUID",
		U"SystemTraceControlGuid",
		U"FH4::s_negLengthTab",
		U"pinit",
		U"RPC_INTERFACE_EVENT_GUID",
		U"ndigs",
		U"ndigs",
		U"AM_MEDIA_TYPE_REPRESENTATION",
		U"tzdst_program",
		U"invln2",
		U"last_wide_tz",
		U"DNameStatusNode::`vftable'",
		U"NETMEDIASINK_SAMPLESWITHRTPTIMESTAMPS",
		U"CATID_MARSHALER",
		U"ALL_POWERSCHEMES_GUID",
		U"global_locale",
		U"PACKETIZATION_MODE",
		U"DPLPROPERTY_MessagesSupported",
		U"digits",
		U"digits",
		U"stack_postmsg",
	};
}

void ProcessHandle::reset()
{
	m_processHandle = NULL;
	m_userGlobalVariables.clear();
}

bool ProcessHandle::init(const CREATE_PROCESS_DEBUG_INFO* pInfo)
{
	SymSetOptions(SYMOPT_LOAD_LINES);

	if (SymInitialize(m_processHandle, NULL, FALSE))
	{
		DWORD64 moduleAddress = SymLoadModule64(
			m_processHandle,
			pInfo->hFile,
			NULL,
			NULL,
			(DWORD64)pInfo->lpBaseOfImage,
			0
		);

		if (moduleAddress != 0)
		{
			IMAGEHLP_MODULE64 moduleInfo = {};
			moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
			if (SymGetModuleInfo64(m_processHandle, moduleAddress, &moduleInfo) && moduleInfo.SymType == SYM_TYPE::SymPdb)
			{
				if (not SymEnumSourceFilesW(m_processHandle, (DWORD64)pInfo->lpBaseOfImage, NULL, SourceFilesProc, this))
				{
					Console << U"SymEnumSourceFilesW failed: " << GetLastError();
				}

				// グローバル変数リストの取得
				{
					EnumUserData userData;
					const auto num = sizeof(systemVarNames) / sizeof(systemVarNames[0]);
					for (auto i : step(num))
					{
						userData.systemVarNameList.emplace(systemVarNames[i]);
					}
					if (SymEnumSymbols(m_processHandle, (DWORD64)pInfo->lpBaseOfImage, NULL, EnumVariablesCallBack, &userData))
					{
						m_userGlobalVariables = std::move(userData.userVarInfoList);
					}
					else
					{
						Console << U"SymEnumSymbols failed: " << GetLastError();
					}
				}
			}
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

void ProcessHandle::entryFunc(size_t /*address*/)
{
}

void ProcessHandle::dispose()
{
	SymCleanup(m_processHandle);
	m_processHandle = NULL;
}

void ProcessHandle::onDllLoaded(const LOAD_DLL_DEBUG_INFO* pInfo) const
{
	DWORD64 moduleAddress = SymLoadModule64(
		m_processHandle,
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

void ProcessHandle::onDllUnloaded(const UNLOAD_DLL_DEBUG_INFO* pInfo) const
{
	SymUnloadModule64(m_processHandle, (DWORD64)pInfo->lpBaseOfDll);
}

Optional<size_t> ProcessHandle::findAddress(const String& symbolName) const
{
	SYMBOL_INFO symbolInfo = {};
	symbolInfo.SizeOfStruct = sizeof(SYMBOL_INFO);
	if (SymFromName(m_processHandle, Unicode::ToUTF8(symbolName).c_str(), &symbolInfo))
	{
		return std::bit_cast<size_t>(symbolInfo.Address);
	}
	return none;
}

Optional<size_t> ProcessHandle::tryGetCallInstructionBytesLength(size_t address) const
{
	std::uint8_t instruction[10];

	size_t numOfBytes;
	auto pAddress = reinterpret_cast<LPVOID>(address);
	ReadProcessMemory(
		m_processHandle,
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

Optional<size_t> ProcessHandle::getRetInstructionAddress(const ThreadHandle& thread) const
{
	auto contextOpt = thread.getContext();
	if (not contextOpt)
	{
		return none;
	}

	const auto& context = contextOpt.value();

	DWORD64 displacement;
	SYMBOL_INFO symbol = {};
	symbol.SizeOfStruct = sizeof(SYMBOL_INFO);

	if (not SymFromAddr(
		m_processHandle,
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

Optional<size_t> ProcessHandle::retInstructionLength(size_t address) const
{
	std::uint8_t readByte;
	size_t numOfBytes;
	auto pAddress = reinterpret_cast<LPVOID>(address);
	ReadProcessMemory(m_processHandle, pAddress, &readByte, 1, &numOfBytes);

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

Optional<LineInfo> ProcessHandle::getCurrentLineInfo(const ThreadHandle& thread) const
{
	auto contextOpt = thread.getContext();
	if (not contextOpt)
	{
		return none;
	}

	const auto context = contextOpt.value();

	DWORD displacement;
	IMAGEHLP_LINE64 lineInfo = {};
	lineInfo.SizeOfStruct = sizeof(lineInfo);

	if (SymGetLineFromAddr64(
		//process,
		m_processHandle,
		context.Rip,
		&displacement,
		&lineInfo))
	{
		LineInfo currentLineInfo;
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

bool ProcessHandle::readMemory(size_t address, size_t size, LPVOID lpBuffer) const
{
	size_t numOfBytes;
	auto pAddress = reinterpret_cast<LPVOID>(address);
	return ReadProcessMemory(m_processHandle, pAddress, lpBuffer, size, &numOfBytes);
}

bool ProcessHandle::writeMemory(size_t address, size_t size, LPCVOID lpBuffer) const
{
	size_t numOfBytes;
	auto pAddress = reinterpret_cast<LPVOID>(address);
	auto success1 = WriteProcessMemory(m_processHandle, pAddress, lpBuffer, size, &numOfBytes);
	auto success2 = FlushInstructionCache(m_processHandle, pAddress, size);
	return success1 && success2;
}
