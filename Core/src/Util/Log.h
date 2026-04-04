#ifndef LOG_H
#define LOG_H

#include <spdlog/spdlog.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Log
{
public:
	// Meyer's singleton pattern: private ctor + static Get() method.
	static Log& Get();

	Log(const Log&) = delete;
	Log& operator=(const Log&) = delete;

	void Init();
	spdlog::logger* GetLogger(const std::string& name = "core");
private:
	Log() = default;

	// Default core and app loggers are created at startup; additional sub-loggers are created on demand.
	std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> m_Loggers;
	std::vector<spdlog::sink_ptr> m_Sinks; // shared across all loggers
};

// Macros - common log levels, routed to the "core" logger by default.  Pass a name as the first argument
// to route to a sub-logger: LOG_INFO_TO("render", "frame {}", n)
#define CORE_TRACE(...)    SPDLOG_LOGGER_TRACE  (Log::Get().GetLogger(), __VA_ARGS__)
#define CORE_DEBUG(...)    SPDLOG_LOGGER_DEBUG  (Log::Get().GetLogger(), __VA_ARGS__)
#define CORE_INFO(...)     SPDLOG_LOGGER_INFO   (Log::Get().GetLogger(), __VA_ARGS__)
#define CORE_WARN(...)     SPDLOG_LOGGER_WARN   (Log::Get().GetLogger(), __VA_ARGS__)
#define CORE_ERROR(...)    SPDLOG_LOGGER_ERROR  (Log::Get().GetLogger(), __VA_ARGS__)
#define CORE_FATAL(...)    SPDLOG_LOGGER_CRITICAL(Log::Get().GetLogger(), __VA_ARGS__)

#define APP_TRACE(...)     SPDLOG_LOGGER_TRACE  (Log::Get().GetLogger("app"), __VA_ARGS__)
#define APP_DEBUG(...)     SPDLOG_LOGGER_DEBUG  (Log::Get().GetLogger("app"), __VA_ARGS__)
#define APP_INFO(...)      SPDLOG_LOGGER_INFO   (Log::Get().GetLogger("app"), __VA_ARGS__)
#define APP_WARN(...)      SPDLOG_LOGGER_WARN   (Log::Get().GetLogger("app"), __VA_ARGS__)
#define APP_ERROR(...)     SPDLOG_LOGGER_ERROR  (Log::Get().GetLogger("app"), __VA_ARGS__)
#define APP_FATAL(...)     SPDLOG_LOGGER_CRITICAL(Log::Get().GetLogger("app"), __VA_ARGS__)

#define LOG_TRACE_TO(name, ...)    SPDLOG_LOGGER_TRACE  (Log::Get().GetLogger(name), __VA_ARGS__)
#define LOG_DEBUG_TO(name, ...)    SPDLOG_LOGGER_DEBUG  (Log::Get().GetLogger(name), __VA_ARGS__)
#define LOG_INFO_TO(name, ...)     SPDLOG_LOGGER_INFO   (Log::Get().GetLogger(name), __VA_ARGS__)
#define LOG_WARN_TO(name, ...)     SPDLOG_LOGGER_WARN   (Log::Get().GetLogger(name), __VA_ARGS__)
#define LOG_ERROR_TO(name, ...)    SPDLOG_LOGGER_ERROR  (Log::Get().GetLogger(name), __VA_ARGS__)
#define LOG_FATAL_TO(name, ...)    SPDLOG_LOGGER_CRITICAL(Log::Get().GetLogger(name), __VA_ARGS__)

#endif // LOG_H