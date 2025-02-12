#pragma once

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#define LOGGER_FORMAT "[%^%l%$] %v"
#define PROJECT_NAME "LinCore"

// Mainly for IDEs
#ifndef ROOT_PATH_SIZE
#define ROOT_PATH_SIZE 0
#endif

#define __FILENAME__ (static_cast<const char *>(__FILE__) + ROOT_PATH_SIZE)

#define LOGI(...) spdlog::info(fmt::format(__VA_ARGS__))
#define LOGW(...) spdlog::warn(fmt::format(__VA_ARGS__))
#define LOGE(...) spdlog::error("[{}:{}] {}", __FILENAME__, __LINE__, fmt::format(__VA_ARGS__))
#define LOGD(...) spdlog::debug(fmt::format(__VA_ARGS__))

#define VK_CHECK(x)                                                          \
	do                                                                       \
	{                                                                        \
		VkResult err = x;                                                    \
		if (err)                                                             \
		{                                                                    \
			LOGE("Detected Vulkan error: {}", string_VkResult(err));		 \
			abort();                                                         \
		}                                                                    \
	} while (0)