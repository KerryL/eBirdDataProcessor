// File:  processPipe.cpp
// Date:  7/19/2019
// Auth:  K. Loux
// Desc:  Wrapper for launching an application connected via a pipe.

// Local headers
#include "processPipe.h"

// OS headers
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define popen _popen
#define pclose _pclose
#define sleep Sleep
#else
#include <unistd.h>
#endif// _WIN32

// Standard C++ headers
#include <cassert>
#include <array>
#include <memory>
#include <stdexcept>

ProcessPipe::~ProcessPipe()
{
	stop = true;
	if (processThread.joinable())
		processThread.join();
}

bool ProcessPipe::Launch(const std::string& command)
{
	assert(stop);
	stop = false;
	try
	{
		processThread = std::thread(&ProcessPipe::PipeThread, this, command);
	}
	catch (...)
	{
		return false;
	}

	return true;
}

void ProcessPipe::PipeThread(const std::string& command)
{
	std::array<char, 128> buffer;
	std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);

	if (!pipe)
		throw std::runtime_error("popen() failed!");

	while (!stop)
	{
		while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()))
		{
			std::lock_guard<std::mutex> lock(stdOutMutex);
			stdOutBuffer += buffer.data();
		}

		Sleep(100);// To avoid busy waiting
	}
}

std::string ProcessPipe::GetStdOutBuffer()
{
	std::lock_guard<std::mutex> lock(stdOutMutex);
	const std::string s(stdOutBuffer);
	stdOutBuffer.clear();
	return s;
}