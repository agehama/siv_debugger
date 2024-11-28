#include <Siv3D.hpp> // Siv3D v0.6.12
#include "ProcessDebugger.hpp"
#include "UserSourceFiles.hpp"

enum class OperationCommandType
{
	Go,
	StepIn,
	StepOver,
	StepOut,
};

enum class ShowCommandType
{
	ShowGlovalVariables,
	ShowLocalVariables,
};

void Main()
{
	ProcessDebugger debugger;

	Optional<String> debugProcessPath;

	Optional<OperationCommandType> operationRequest;

	Optional<ShowCommandType> showRequest;

	bool isTerminate = false;

	auto updateDebugger = [&]() {

		while (not isTerminate)
		{
			if (debugProcessPath)
			{
				debugger.startDebugSession(debugProcessPath.value());
				debugProcessPath = none;
				operationRequest = none;
				showRequest = none;
			}

			if (debugger)
			{
				debugger.continueDebugSession();

				while (not operationRequest && not isTerminate)
				{
					if (showRequest)
					{
						switch (showRequest.value())
						{
						case ShowCommandType::ShowGlovalVariables:
							debugger.process().fetchGlobalVariables();
							break;
						case ShowCommandType::ShowLocalVariables:
							debugger.process().fetchLocalVariables(debugger.userThread());
							break;
						default: break;
						}

						showRequest = none;
					}

					std::this_thread::sleep_for(std::chrono::milliseconds(0));
				}

				if (operationRequest)
				{
					switch (operationRequest.value())
					{
					case OperationCommandType::Go:
						break;
					case OperationCommandType::StepIn:
						debugger.stepIn();
						break;
					case OperationCommandType::StepOver:
						debugger.stepOver();
						break;
					case OperationCommandType::StepOut:
						debugger.stepOut();
						break;
					default: break;
					}

					operationRequest = none;
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
			operationRequest = OperationCommandType::Go;
		}

		if (SimpleGUI::Button(U"suspend", Vec2(100, 200)))
		{
			debugger.requestDebugBreak();
		}
		if (SimpleGUI::Button(U"resume", Vec2(300, 200)))
		{
			operationRequest = OperationCommandType::Go;
		}
		if (SimpleGUI::Button(U"stepIn", Vec2(300, 250)))
		{
			operationRequest = OperationCommandType::StepIn;
		}
		if (SimpleGUI::Button(U"stepOver", Vec2(300, 300)))
		{
			operationRequest = OperationCommandType::StepOver;
		}
		if (SimpleGUI::Button(U"stepOut", Vec2(300, 350)))
		{
			operationRequest = OperationCommandType::StepOut;
		}
		if (SimpleGUI::Button(U"show global", Vec2(450, 200)))
		{
			showRequest = ShowCommandType::ShowGlovalVariables;
		}
		if (SimpleGUI::Button(U"show local", Vec2(450, 250)))
		{
			showRequest = ShowCommandType::ShowLocalVariables;
		}

		if (not operationRequest)
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

			font(debugger.process().getDebugString()).draw(0, 160);
		}
	}

	isTerminate = true;
	if (debugger && debugger.status() != ProcessStatus::None)
	{
		DebugBreakProcess(debugger.process().getHandle());
	}

	debuggerThread.join();
}
