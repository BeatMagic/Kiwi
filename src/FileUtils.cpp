#include <filesystem>
#include <fstream>
#include <string>
#include <kiwi/Types.h>
#include <kiwi/Utils.h>

using namespace std;

namespace kiwi
{
	ifstream& openFile(ifstream& f, const string& filePath, ios_base::openmode mode)
	{
		auto exc = f.exceptions();
		f.exceptions(ifstream::failbit | ifstream::badbit);
		try
		{
			// Use std::filesystem::u8path to properly handle UTF-8 paths on Windows
			f.open(std::filesystem::u8path(filePath), mode);
		}
		catch (const ios_base::failure&)
		{
			throw IOException{ "Cannot open file : " + filePath };
		}
		f.exceptions(exc);
		return f;
	}

	ofstream& openFile(ofstream& f, const string& filePath, ios_base::openmode mode)
	{
		auto exc = f.exceptions();
		f.exceptions(ofstream::failbit | ofstream::badbit);
		try
		{
			// Use std::filesystem::u8path to properly handle UTF-8 paths on Windows
			f.open(std::filesystem::u8path(filePath), mode);
		}
		catch (const ios_base::failure&)
		{
			throw IOException{ "Cannot open file : " + filePath };
		}
		f.exceptions(exc);
		return f;
	}

	bool isOpenable(const string& filePath)
	{
		ifstream ifs;
		try
		{
			openFile(ifs, filePath);
		}
		catch (const IOException&)
		{
			return false;
		}
		return true;
	}

}
