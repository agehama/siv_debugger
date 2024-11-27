#include <Siv3D.hpp>
#include "ThreadHandle.hpp"

Optional<CONTEXT> ThreadHandle::getContext() const
{
	CONTEXT context = {};
	context.ContextFlags = CONTEXT_FULL;

	if (not GetThreadContext(m_threadHandle, &context))
	{
		Console << U"GetThreadContext failed: " << GetLastError();
		return none;
	}

	return context;
}

bool ThreadHandle::setContext(const CONTEXT& context) const
{
	if (not SetThreadContext(m_threadHandle, &context))
	{
		Console << U"SetThreadContext failed: " << GetLastError();
		return false;
	}

	return true;
}

void ThreadHandle::setTrapFlag() const
{
	if (auto contextOpt = getContext())
	{
		auto context = contextOpt.value();
		context.EFlags |= 0x100; // TFビット
		setContext(context);
	}
}

void ThreadHandle::backRip() const
{
	if (auto contextOpt = getContext())
	{
		auto context = contextOpt.value();
		context.Rip -= 1;
		setContext(context);
	}
}
