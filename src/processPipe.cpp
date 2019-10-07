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
#include <fcntl.h>

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
#ifdef _WIN32
		if (WaitForSingleObject(pipe.get(), 1000) == WAIT_OBJECT_0)
#else
		while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()))// TODO:  Doesn't work properly (it blocks, so application never exits)
#endif// _WIN32
		{
			std::lock_guard<std::mutex> lock(stdOutMutex);
			stdOutBuffer += buffer.data();
		}

#ifdef _WIN32
		Sleep(100);// To avoid busy waiting
#else
		usleep(100000);
#endif
	}
}

std::string ProcessPipe::GetStdOutBuffer()
{
	std::lock_guard<std::mutex> lock(stdOutMutex);
	const std::string s(stdOutBuffer);
	stdOutBuffer.clear();
	return s;
}
