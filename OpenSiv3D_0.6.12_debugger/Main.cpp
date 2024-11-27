#include <Siv3D.hpp> // Siv3D v0.6.12
#include "ProcessDebugger.hpp"
#include "UserSourceFiles.hpp"

enum class CommandType
{
	Go,
	StepIn,
	StepOver,
	StepOut,
};

void Main()
{
	ProcessDebugger debugger;

	Optional<String> debugProcessPath;

	Optional<CommandType> command;

	bool isTerminate = false;

	auto updateDebugger = [&]() {

		while (not isTerminate)
		{
			if (debugProcessPath)
			{
				debugger.startDebugSession(debugProcessPath.value());
				debugProcessPath = none;
				command = none;
			}

			if (debugger)
			{
				debugger.continueDebugSession();

				command = none;

				while (not command && not isTerminate)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(0));
				}

				if (command)
				{
					switch (command.value())
					{
					case CommandType::Go:
						break;
					case CommandType::StepIn:
						debugger.stepIn();
						break;
					case CommandType::StepOver:
						debugger.stepOver();
						break;
					case CommandType::StepOut:
						debugger.stepOut();
						break;
					default:
						break;
					}
				}
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}
	};

	std::thread debuggerThread(updateDebugger);

	Font font(16);

	while (System::Update())
	{
		if (DragDrop::HasNewFilePaths())
		{
			debugProcessPath = DragDrop::GetDroppedFilePaths()[0].path;
			command = CommandType::Go;
		}

		if (SimpleGUI::Button(U"suspend", Vec2(100, 200)))
		{
			debugger.requestDebugBreak();
		}
		if (SimpleGUI::Button(U"resume", Vec2(300, 200)))
		{
			command = CommandType::Go;
		}
		if (SimpleGUI::Button(U"stepIn", Vec2(300, 250)))
		{
			command = CommandType::StepIn;
		}
		if (SimpleGUI::Button(U"stepOver", Vec2(300, 300)))
		{
			command = CommandType::StepOver;
		}
		if (SimpleGUI::Button(U"stepOut", Vec2(300, 350)))
		{
			command = CommandType::StepOut;
		}

		if (not command)
		{
			font(U"入力待機中…").draw();
		}

		if (debugger && debugger.status() != ProcessStatus::None)
		{
			font(U"メインスレッド：", debugger.mainThreadID()).draw(0, 30);
			font(U"ユーザースレッド：", debugger.userThreadID()).draw(0, 50);

			font(debugger.currentFilename()).draw(0, 100);
			font(debugger.currentLine()).draw(0, 120);

			if (auto optLineStr = UserSourceFiles::TryGetLine(debugger.currentFilename(), debugger.currentLine() - 1))
			{
				font(optLineStr.value().get()).draw(0, 140);
			}
		}
	}

	isTerminate = true;
	if (debugger && debugger.status() != ProcessStatus::None)
	{
		DebugBreakProcess(debugger.process().getHandle());
	}

	debuggerThread.join();
}
