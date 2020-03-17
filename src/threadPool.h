// File:  threadPool.h
// Date:  1/10/2018
// Auth:  K. Loux
// Desc:  Thread pool class.

#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

// Standard C++ headers
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <chrono>
#include <memory>
#include <cassert>
#include <type_traits>

class ThreadPool
{
public:
	ThreadPool(const unsigned int& threadCount, const unsigned int& rateLimit);
	virtual ~ThreadPool();

	void SetQueueSizeControl(const unsigned int& maxSize, const unsigned int& minSize);

	struct JobInfoBase
	{
		virtual ~JobInfoBase() = default;
		virtual void DoJob() = 0;
	};

public:
	void AddJob(std::unique_ptr<JobInfoBase> job);
	void WaitForAllJobsComplete() const;


private:
	mutable std::mutex queueMutex;
	std::condition_variable jobReadyCondition;
	mutable std::condition_variable jobCompleteCondition;

	unsigned int maxQueueSize = 0;
	unsigned int minQueueSize = 0;

	std::vector<std::thread> threads;
	const std::chrono::milliseconds minRequestDelta;

	void ThreadEntry();

	// These members are protected by queueMutex
	std::queue<std::unique_ptr<JobInfoBase>> jobQueue;
	std::chrono::steady_clock::time_point lastRequestTime = std::chrono::steady_clock::now();
	unsigned int pendingJobCount = 0;
};

#endif// THREAD_POOL_H_
