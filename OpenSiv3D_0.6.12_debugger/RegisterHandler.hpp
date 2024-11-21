#pragma once
#include <Windows.h>

class RegisterHandler
{
public:

	static bool getDebuggeeContext(CONTEXT* pContext, HANDLE thread)
	{
		pContext->ContextFlags = CONTEXT_FULL;

		if (not GetThreadContext(thread, pContext))
		{
			std::cout << "GetThreadContext failed: " << GetLastError() << std::endl;
			return false;
		}

		return true;
	}

	static bool setDebuggeeContext(CONTEXT* pContext, HANDLE thread)
	{
		if (not SetThreadContext(thread, pContext))
		{
			std::cout << "SetThreadContext failed: " << GetLastError() << std::endl;
			return false;
		}

		return true;
	}

	static void setTrapFlag(HANDLE thread)
	{
		CONTEXT context;

		if (not getDebuggeeContext(&context, thread))
		{
			return;
		}

		context.EFlags |= 0x100;

		if (not setDebuggeeContext(&context, thread))
		{
			return;
		}
	}

	static void rollbackIP(HANDLE thread)
	{
		CONTEXT context;

		if (not getDebuggeeContext(&context, thread))
		{
			return;
		}

		context.Rip -= 1;

		if (not setDebuggeeContext(&context, thread))
		{
			return;
		}
	}
};
