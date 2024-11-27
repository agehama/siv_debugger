#pragma once
#include <Windows.h>

class ThreadHandle
{
public:

	ThreadHandle() = default;

	ThreadHandle(HANDLE thread) :m_threadHandle(thread) {}

	void suspend() const
	{
		SuspendThread(m_threadHandle);
	}

	void resume() const
	{
		ResumeThread(m_threadHandle);
	}

	Optional<CONTEXT> getContext() const;

	bool setContext(const CONTEXT& context) const;

	void setTrapFlag() const;

	void backRip() const;

	HANDLE getHandle() const { return m_threadHandle; }

private:

	HANDLE m_threadHandle = NULL;
};
