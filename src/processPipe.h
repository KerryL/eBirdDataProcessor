// File:  processPipe.h
// Date:  7/19/2019
// Auth:  K. Loux
// Desc:  Wrapper for launching an application connected via a pipe.

#ifndef PROCESS_PIPE_H_
#define PROCESS_PIPE_H_

// Standard C++ headers
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

class ProcessPipe
{
public:
	~ProcessPipe();

	bool Launch(const std::string& command);
	std::string GetStdOutBuffer();

private:
	std::thread processThread;
	std::mutex stdOutMutex;
	std::string stdOutBuffer;
	std::atomic<bool> stop = true;

	void PipeThread(const std::string& command);
};

#endif// PROCESS_PIPE_H_
