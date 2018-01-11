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

class ThreadPool
{
public:
	ThreadPool(const unsigned int& threadCount, const unsigned int& rateLimit);
	virtual ~ThreadPool();

	struct JobInfo;
	typedef void(*JobFunction)(const JobInfo&);

	struct JobInfo
	{
		JobInfo() = default;
		explicit JobInfo(JobFunction jobFunction) : jobFunction(jobFunction) {}
		virtual ~JobInfo() = default;

		JobFunction jobFunction;
	};

	void AddJob(std::unique_ptr<JobInfo> job);
	void WaitForAllJobsComplete() const;

private:
	mutable std::mutex queueMutex;
	std::condition_variable jobReadyCondition;
	mutable std::condition_variable jobCompleteCondition;

	std::vector<std::thread> threads;
	const std::chrono::milliseconds minRequestDelta;

	void ThreadEntry();

	// These members are protected by queueMutex
	std::queue<std::unique_ptr<JobInfo>> jobQueue;
	std::chrono::steady_clock::time_point lastRequestTime = std::chrono::steady_clock::now();
	unsigned int pendingJobCount = 0;
};

#endif// THREAD_POOL_H_
