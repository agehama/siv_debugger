#include <Siv3D.hpp> // Siv3D v0.6.12
#include "ProcessDebugger.hpp"

void Main()
{
	ProcessDebugger debugger;

	Optional<String> debugProcessPath;

	auto updateDebugger = [&]() {

		while (true)
		{
			if (debugProcessPath)
			{
				debugger.startDebugSession(debugProcessPath.value());
				debugProcessPath = none;
			}

			if (debugger)
			{
				debugger.continueDebugSession();
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(0));
		}
	};

	std::thread debuggerThread(updateDebugger);

	while (System::Update())
	{
		if (DragDrop::HasNewFilePaths())
		{
			debugProcessPath = DragDrop::GetDroppedFilePaths()[0].path;
		}

		if (SimpleGUI::Button(U"suspend", Vec2(100, 100)))
		{
			debugger.suspend();
		}
		if (SimpleGUI::Button(U"resume", Vec2(300, 100)))
		{
			debugger.resume();
		}

		Circle(Cursor::Pos(), 100).draw();
	}

	debuggerThread.join();
}
