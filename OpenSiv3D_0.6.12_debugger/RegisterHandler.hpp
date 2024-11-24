#pragma once
#include <Windows.h>
#include <Siv3D.hpp>

class RegisterHandler
{
public:

	static Optional<CONTEXT> getDebuggeeContext(HANDLE thread)
	{
		CONTEXT context = {};
		context.ContextFlags = CONTEXT_FULL;

		if (not GetThreadContext(thread, &context))
		{
			Console << U"GetThreadContext failed: " << GetLastError();
			return none;
		}

		return context;
	}

	static bool setDebuggeeContext(HANDLE thread, const CONTEXT& context)
	{
		if (not SetThreadContext(thread, &context))
		{
			Console << U"SetThreadContext failed: " << GetLastError();
			return false;
		}

		return true;
	}

	static void setTrapFlag(HANDLE thread)
	{
		if (auto contextOpt = getDebuggeeContext(thread))
		{
			auto context = contextOpt.value();
			context.EFlags |= 0x100; // TFビット
			setDebuggeeContext(thread, context);
		}
	}

	static void backDebuggeeRip(HANDLE thread)
	{
		if (auto contextOpt = getDebuggeeContext(thread))
		{
			auto context = contextOpt.value();
			context.Rip -= 1;
			setDebuggeeContext(thread, context);
		}
	}
};
