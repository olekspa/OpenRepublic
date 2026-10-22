#pragma once

#include <Windows.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace openrepublic::economy_host
{
	// Local JSONL transport for the app-owned economy API. The worker thread only
	// frames bytes and transfers requests; callers dispatch them on the app thread.
	class LocalEconomyPipeEndpoint final
	{
	public:
		struct Request
		{
			std::uint64_t token = 0;
			std::string line;
		};

		explicit LocalEconomyPipeEndpoint(std::wstring localName);
		~LocalEconomyPipeEndpoint();

		LocalEconomyPipeEndpoint(const LocalEconomyPipeEndpoint&) = delete;
		LocalEconomyPipeEndpoint& operator=(const LocalEconomyPipeEndpoint&) = delete;
		LocalEconomyPipeEndpoint(LocalEconomyPipeEndpoint&&) = delete;
		LocalEconomyPipeEndpoint& operator=(LocalEconomyPipeEndpoint&&) = delete;

		bool Start(std::string& error);
		void Stop() noexcept;
		bool TryPop(Request& request);
		bool Complete(std::uint64_t token, std::string response);
		bool IsRunning() const noexcept;
		std::string LastError() const;
		std::wstring PipePath() const;

	private:
		struct PendingRequest;

		void Worker(std::uintptr_t firstPipe) noexcept;
		bool ServeClient(std::uintptr_t pipe) noexcept;
		bool SubmitAndWait(std::string line, std::string& response);
		HANDLE CreatePipeInstance(bool firstInstance, std::string& error) const noexcept;
		void SetLastError(std::string error) noexcept;
		bool IsStopping() const noexcept;

		std::wstring pipePath_;
		std::atomic_bool stopping_{ false };
		std::atomic_bool running_{ false };
		std::atomic<std::uint64_t> nextToken_{ 1 };
		mutable std::mutex mutex_;
		std::condition_variable condition_;
		std::deque<std::shared_ptr<PendingRequest>> queuedRequests_;
		std::unordered_map<std::uint64_t, std::shared_ptr<PendingRequest>> pendingRequests_;
		std::string lastError_;
		std::thread worker_;
		bool workerDone_ = true;
	};
}
