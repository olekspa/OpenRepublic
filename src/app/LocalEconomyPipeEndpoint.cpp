#include "LocalEconomyPipeEndpoint.h"

#include "../economy_host/economy_protocol.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <limits>
#include <string_view>
#include <utility>

namespace openrepublic::economy_host
{
	namespace
	{
		constexpr std::size_t MaxPendingRequests = 32;
		constexpr std::size_t MaxResponseLineBytes = 1024 * 1024;
		constexpr DWORD PipeBufferBytes = 64 * 1024;
		constexpr wchar_t PipePrefix[] = L"\\\\.\\pipe\\";

		bool IsValidLocalPipeName(std::wstring_view name)
		{
			if (name.empty() || name.size() > 64)
			{
				return false;
			}
			for (wchar_t character : name)
			{
				const bool allowed = (character >= L'a' && character <= L'z') ||
					(character >= L'A' && character <= L'Z') ||
					(character >= L'0' && character <= L'9') ||
					character == L'.' || character == L'_' || character == L'-';
				if (!allowed)
				{
					return false;
				}
			}
			return true;
		}

		std::string WindowsErrorText(const char* operation, DWORD error)
		{
			return std::string(operation) + " failed with Windows error " + std::to_string(error);
		}
	}

	struct LocalEconomyPipeEndpoint::PendingRequest
	{
		std::uint64_t token = 0;
		std::string line;
		std::string response;
		bool completed = false;
	};

	LocalEconomyPipeEndpoint::LocalEconomyPipeEndpoint(std::wstring localName)
		: pipePath_(PipePrefix + std::move(localName))
	{
	}

	LocalEconomyPipeEndpoint::~LocalEconomyPipeEndpoint()
	{
		Stop();
	}

	bool LocalEconomyPipeEndpoint::Start(std::string& error)
	{
		error.clear();
		if (!IsValidLocalPipeName(std::wstring_view(pipePath_).substr(std::wstring_view(PipePrefix).size())))
		{
			error = "local economy pipe name must be 1-64 ASCII letters, digits, '.', '_' or '-'";
			return false;
		}
		if (worker_.joinable() || running_.load(std::memory_order_acquire) ||
			stopping_.load(std::memory_order_acquire))
		{
			error = "local economy pipe endpoint has already been started or stopped";
			return false;
		}

		const DWORD firstPipeFlag = FILE_FLAG_FIRST_PIPE_INSTANCE;
		const DWORD pipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
		HANDLE firstPipe = CreateNamedPipeW(pipePath_.c_str(), PIPE_ACCESS_DUPLEX | firstPipeFlag,
			pipeMode, 1, PipeBufferBytes, PipeBufferBytes, 0, nullptr);
		if (firstPipe == INVALID_HANDLE_VALUE)
		{
			error = WindowsErrorText("CreateNamedPipe", GetLastError());
			SetLastError(error);
			return false;
		}

		{
			std::lock_guard<std::mutex> lock(mutex_);
			lastError_.clear();
			workerDone_ = false;
		}
		stopping_.store(false, std::memory_order_release);
		running_.store(true, std::memory_order_release);
		try
		{
			worker_ = std::thread(&LocalEconomyPipeEndpoint::Worker, this,
				reinterpret_cast<std::uintptr_t>(firstPipe));
		}
		catch (const std::exception& exception)
		{
			running_.store(false, std::memory_order_release);
			{
				std::lock_guard<std::mutex> lock(mutex_);
				workerDone_ = true;
			}
			CloseHandle(firstPipe);
			error = std::string("could not start local economy pipe worker: ") + exception.what();
			SetLastError(error);
			return false;
		}
		return true;
	}

	void LocalEconomyPipeEndpoint::Stop() noexcept
	{
		stopping_.store(true, std::memory_order_release);
		condition_.notify_all();
		if (!worker_.joinable())
		{
			running_.store(false, std::memory_order_release);
			return;
		}

		const HANDLE workerHandle = worker_.native_handle();
		for (;;)
		{
			std::unique_lock<std::mutex> lock(mutex_);
			if (workerDone_)
			{
				break;
			}
			condition_.wait_for(lock, std::chrono::milliseconds(25));
			const bool done = workerDone_;
			lock.unlock();
			if (done)
			{
				break;
			}
			// ConnectNamedPipe, ReadFile, and WriteFile are synchronous here.
			// Repeating cancellation closes the small race where Stop arrives just
			// before the worker enters its next blocking operation.
			CancelSynchronousIo(workerHandle);
		}
		worker_.join();
		{
			std::lock_guard<std::mutex> lock(mutex_);
			queuedRequests_.clear();
			pendingRequests_.clear();
		}
		running_.store(false, std::memory_order_release);
	}

	bool LocalEconomyPipeEndpoint::TryPop(Request& request)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (stopping_.load(std::memory_order_acquire) || queuedRequests_.empty())
		{
			return false;
		}
		const std::shared_ptr<PendingRequest> pending = std::move(queuedRequests_.front());
		queuedRequests_.pop_front();
		request.token = pending->token;
		request.line = pending->line;
		return true;
	}

	bool LocalEconomyPipeEndpoint::Complete(std::uint64_t token, std::string response)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		const auto found = pendingRequests_.find(token);
		if (found == pendingRequests_.end() || found->second->completed ||
			stopping_.load(std::memory_order_acquire))
		{
			return false;
		}

		const bool validResponse = !response.empty() && response.size() < MaxResponseLineBytes &&
			response.find_first_of("\r\n") == std::string::npos;
		if (!validResponse)
		{
			// An empty completion tells the transport worker to close this client
			// connection. It never truncates or splits a protocol response.
			response.clear();
		}
		found->second->response = std::move(response);
		found->second->completed = true;
		condition_.notify_all();
		return validResponse;
	}

	bool LocalEconomyPipeEndpoint::IsRunning() const noexcept
	{
		return running_.load(std::memory_order_acquire);
	}

	std::string LocalEconomyPipeEndpoint::LastError() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return lastError_;
	}

	std::wstring LocalEconomyPipeEndpoint::PipePath() const
	{
		return pipePath_;
	}

	void LocalEconomyPipeEndpoint::Worker(std::uintptr_t firstPipe) noexcept
	{
		HANDLE pipe = reinterpret_cast<HANDLE>(firstPipe);
		bool firstInstance = false;
		try
		{
			while (!IsStopping() && pipe != INVALID_HANDLE_VALUE)
			{
				const BOOL connected = ConnectNamedPipe(pipe, nullptr);
				const DWORD connectError = connected ? ERROR_SUCCESS : GetLastError();
				if (connected || connectError == ERROR_PIPE_CONNECTED)
				{
					ServeClient(reinterpret_cast<std::uintptr_t>(pipe));
				}
				else if (!(IsStopping() && connectError == ERROR_OPERATION_ABORTED))
				{
					SetLastError(WindowsErrorText("ConnectNamedPipe", connectError));
				}

				DisconnectNamedPipe(pipe);
				CloseHandle(pipe);
				pipe = INVALID_HANDLE_VALUE;
				if (IsStopping())
				{
					break;
				}

				std::string createError;
				pipe = CreatePipeInstance(firstInstance, createError);
				firstInstance = false;
				if (pipe == INVALID_HANDLE_VALUE)
				{
					SetLastError(std::move(createError));
					break;
				}
			}
		}
		catch (const std::exception& exception)
		{
			SetLastError(std::string("local economy pipe worker failed: ") + exception.what());
		}
		catch (...)
		{
			SetLastError("local economy pipe worker failed with an unknown exception");
		}

		if (pipe != INVALID_HANDLE_VALUE)
		{
			DisconnectNamedPipe(pipe);
			CloseHandle(pipe);
		}
		{
			std::lock_guard<std::mutex> lock(mutex_);
			workerDone_ = true;
		}
		running_.store(false, std::memory_order_release);
		condition_.notify_all();
	}

	bool LocalEconomyPipeEndpoint::ServeClient(std::uintptr_t pipeValue) noexcept
	{
		HANDLE pipe = reinterpret_cast<HANDLE>(pipeValue);
		try
		{
			std::string inputBuffer;
			std::array<char, 4096> readBuffer{};
			while (!IsStopping())
			{
				const std::size_t newline = inputBuffer.find('\n');
				if (newline != std::string::npos)
				{
					if (newline > MaxRequestLineBytes)
					{
						return false;
					}
					std::string line = inputBuffer.substr(0, newline);
					inputBuffer.erase(0, newline + 1);
					if (!line.empty() && line.back() == '\r')
					{
						line.pop_back();
					}

					std::string response;
					if (!SubmitAndWait(std::move(line), response) || response.empty())
					{
						return false;
					}
					if (response.size() >= MaxResponseLineBytes)
					{
						return false;
					}
					response.push_back('\n');
					std::size_t written = 0;
					while (written < response.size())
					{
						DWORD bytesWritten = 0;
						const DWORD requested = static_cast<DWORD>(std::min<std::size_t>(
							response.size() - written, std::numeric_limits<DWORD>::max()));
						if (!WriteFile(pipe, response.data() + written, requested, &bytesWritten, nullptr) ||
							bytesWritten == 0)
						{
							return false;
						}
						written += bytesWritten;
					}
					continue;
				}

				if (inputBuffer.size() > MaxRequestLineBytes)
				{
					return false;
				}
				DWORD bytesRead = 0;
				if (!ReadFile(pipe, readBuffer.data(), static_cast<DWORD>(readBuffer.size()), &bytesRead, nullptr) ||
					bytesRead == 0)
				{
					return false;
				}
				inputBuffer.append(readBuffer.data(), bytesRead);
			}
		}
		catch (...)
		{
			return false;
		}
		return false;
	}

	bool LocalEconomyPipeEndpoint::SubmitAndWait(std::string line, std::string& response)
	{
		std::unique_lock<std::mutex> lock(mutex_);
		if (stopping_.load(std::memory_order_acquire) || queuedRequests_.size() >= MaxPendingRequests)
		{
			return false;
		}
		const std::uint64_t token = nextToken_.fetch_add(1, std::memory_order_relaxed);
		if (token == 0)
		{
			return false;
		}
		std::shared_ptr<PendingRequest> pending = std::make_shared<PendingRequest>();
		pending->token = token;
		pending->line = std::move(line);
		queuedRequests_.push_back(pending);
		pendingRequests_.emplace(token, pending);
		condition_.notify_all();
		condition_.wait(lock, [&]() {
			return pending->completed || stopping_.load(std::memory_order_acquire);
		});
		pendingRequests_.erase(token);
		if (stopping_.load(std::memory_order_acquire) || !pending->completed)
		{
			return false;
		}
		response = std::move(pending->response);
		return true;
	}

	void LocalEconomyPipeEndpoint::SetLastError(std::string error) noexcept
	{
		try
		{
			std::lock_guard<std::mutex> lock(mutex_);
			lastError_ = std::move(error);
		}
		catch (...)
		{
		}
	}

	bool LocalEconomyPipeEndpoint::IsStopping() const noexcept
	{
		return stopping_.load(std::memory_order_acquire);
	}

	HANDLE LocalEconomyPipeEndpoint::CreatePipeInstance(bool firstInstance, std::string& error) const noexcept
	{
		const DWORD firstPipeFlag = firstInstance ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0;
		const DWORD pipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
		HANDLE pipe = CreateNamedPipeW(pipePath_.c_str(), PIPE_ACCESS_DUPLEX | firstPipeFlag,
			pipeMode, 1, PipeBufferBytes, PipeBufferBytes, 0, nullptr);
		if (pipe == INVALID_HANDLE_VALUE)
		{
			try
			{
				error = WindowsErrorText("CreateNamedPipe", GetLastError());
			}
			catch (...)
			{
			}
		}
		return pipe;
	}
}
