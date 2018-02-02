// File:  threadPool.cpp
// Date:  1/10/2018
// Auth:  K. Loux
// Desc:  Thread pool class.

// Local headers
#include "threadPool.h"

// Standard C++ headers
#include <iostream>
#include <iomanip>

ThreadPool::ThreadPool(const unsigned int& threadCount, const unsigned int& rateLimit)
	: minRequestDelta(rateLimit > 0 ? std::chrono::milliseconds(static_cast<long long>(1000.0 / rateLimit)) : std::chrono::milliseconds(0))
{
	threads.resize(threadCount);
	for (auto& t : threads)
		t = std::thread(&ThreadPool::ThreadEntry, this);
}

ThreadPool::~ThreadPool()
{
	{
		std::lock_guard<std::mutex> lock(queueMutex);
		while (!jobQueue.empty())
			jobQueue.pop();
	}

	unsigned int i;
	for (i = 0; i < threads.size(); ++i)
		AddJob(nullptr);

	for (auto& t : threads)
	{
		if (t.joinable())
			t.join();
	}
}

void ThreadPool::AddJob(std::unique_ptr<JobInfoBase> job)
{
	{
		std::lock_guard<std::mutex> lock(queueMutex);
		jobQueue.push(std::move(job));
	}

	jobReadyCondition.notify_one();
}

void ThreadPool::WaitForAllJobsComplete() const
{
	std::unique_lock<std::mutex> lock(queueMutex);
    unsigned int maxJobQueueSize(jobQueue.size());
    const auto originalPrecision(std::cout.precision());
    std::cout.precision(1);
	jobCompleteCondition.wait(lock, [this, &maxJobQueueSize]()
	{
        if (jobQueue.size() > maxJobQueueSize)
            maxJobQueueSize = jobQueue.size();
        const double fraction(static_cast<double>(maxJobQueueSize - jobQueue.size()) / maxJobQueueSize * 100.0);
        std::cout << std::fixed << '\r' << fraction << '%';
		return jobQueue.empty() && pendingJobCount == 0;
	});
    std::cout.precision(originalPrecision);
    std::cout << std::endl;
}

void ThreadPool::ThreadEntry()
{
	while (true)
	{
		std::unique_ptr<JobInfoBase> thisJob;
		std::chrono::milliseconds sleepTime(0);

		{
			std::unique_lock<std::mutex> lock(queueMutex);
			jobReadyCondition.wait(lock, [this]()
			{
				return jobQueue.size() > 0;
			});

			thisJob = std::move(jobQueue.front());
			++pendingJobCount;
			jobQueue.pop();

			if (minRequestDelta > std::chrono::milliseconds(0))
			{
				std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());
				if (now - lastRequestTime < minRequestDelta)
					sleepTime = minRequestDelta - std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRequestTime);
				else
					sleepTime = std::chrono::milliseconds(0);
				lastRequestTime = now + sleepTime;
			}
		}

		if (!thisJob)
			break;

		std::this_thread::sleep_for(sleepTime);
		thisJob->DoJob();

		{
			std::lock_guard<std::mutex> lock(queueMutex);
			--pendingJobCount;
		}

		jobCompleteCondition.notify_one();
	}

	std::lock_guard<std::mutex> lock(queueMutex);
	--pendingJobCount;
}
