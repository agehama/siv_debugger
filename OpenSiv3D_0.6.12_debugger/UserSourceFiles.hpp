#pragma once
#include <Siv3D.hpp>

class UserSourceFiles
{
public:

	static UserSourceFiles& i()
	{
		static UserSourceFiles instance;
		return instance;
	}

	static void AddFile(FilePathView path)
	{
		if (!FileSystem::Exists(path))
		{
			Print << U"not exist path: " << path;
		}

		TextReader reader(path);
		reader.readLines(i().m_sourceTable[path]);
	}

	static Optional<std::reference_wrapper<const String>> TryGetLine(FilePathView path, size_t lineNumber)
	{
		auto& instance = i();
		if (!instance.m_sourceTable.contains(path))
		{
			return none;
		}

		const auto& lines = instance.m_sourceTable[path];
		if (lines.size() <= lineNumber)
		{
			return none;
		}

		return std::reference_wrapper<const String>(lines[lineNumber]);
	}

private:

	HashTable<FilePath, Array<String>> m_sourceTable;
};
