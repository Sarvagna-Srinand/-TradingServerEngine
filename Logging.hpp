#pragma once

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>

enum class LogLevel {
	Debug,
	Information,
	Warning,
	Error,
	Critical
};

inline const char *LogLevelToString(LogLevel level) {
	switch (level) {
	case LogLevel::Debug:
		return "Debug";
	case LogLevel::Information:
		return "Information";
	case LogLevel::Warning:
		return "Warning";
	case LogLevel::Error:
		return "Error";
	case LogLevel::Critical:
		return "Critical";
	default:
		return "Unknown";
	}
}

class ILogger {
  public:
	virtual ~ILogger() = default;
	virtual void Debug(const std::string &module, const std::string &message) = 0;
	virtual void Information(const std::string &module, const std::string &message) = 0;
	virtual void Warning(const std::string &module, const std::string &message) = 0;
	virtual void Error(const std::string &module, const std::string &message) = 0;
};

class AbstractLogger : public ILogger {
  protected:
	virtual void Log(LogLevel level, const std::string &module, const std::string &message) = 0;

  public:
	void Debug(const std::string &module, const std::string &message) override {
		Log(LogLevel::Debug, module, message);
	}
	void Information(const std::string &module, const std::string &message) override {
		Log(LogLevel::Information, module, message);
	}
	void Warning(const std::string &module, const std::string &message) override {
		Log(LogLevel::Warning, module, message);
	}
	void Error(const std::string &module, const std::string &message) override {
		Log(LogLevel::Error, module, message);
	}
};

class TextLogger : public AbstractLogger {
	std::ofstream logFile;
	std::queue<std::string> logQueue;
	std::mutex queueMutex;
	std::condition_variable cv;
	std::atomic<bool> running;
	std::thread worker;

	void ProcessQueue() {
		while (running || !logQueue.empty()) {
			std::unique_lock<std::mutex> lock(queueMutex);
			cv.wait(lock, [this] { return !logQueue.empty() || !running; });
			while (!logQueue.empty()) {
				logFile << logQueue.front() << std::endl;
				logQueue.pop();
			}
		}
	}

  protected:
	void Log(LogLevel level, const std::string &module, const std::string &message) override {
		std::ostringstream oss;
		oss << "[" << ToString(level) << "] " << module << ": " << message;
		{
			std::lock_guard<std::mutex> lock(queueMutex);
			logQueue.push(oss.str());
		}
		cv.notify_one();
	}

  public:
	TextLogger(const std::string &filePath)
		: logFile(filePath), running(true), worker(&TextLogger::ProcessQueue, this) {}

	~TextLogger() {
		running = false;
		cv.notify_all();
		if (worker.joinable())
			worker.join();
		logFile.close();
	}

	static std::string ToString(LogLevel level) {
		switch (level) {
		case LogLevel::Debug:
			return "DEBUG";
		case LogLevel::Information:
			return "INFO";
		case LogLevel::Warning:
			return "WARN";
		case LogLevel::Error:
			return "ERROR";
		default:
			return "UNKNOWN";
		}
	}
};
