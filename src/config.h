// config.h
#ifndef CONFIG_H
#define CONFIG_H
// Should we enable vertical sync during presentation? Worth setting to 0 when doing perf profiling to avoid GPU downclock during idle
#define CONFIG_VSYNC 1

// Should we enable synchronization validation? Worth running with 1 occasionally to check correctness.
#define CONFIG_SYNCVAL 0

// Maximum number of texture descriptors in the pool
#define DESCRIPTOR_LIMIT 65536

inline constexpr unsigned int kFRAME_OVERLAP = 2;

inline constexpr uint32_t kBINDLESS_TEXTURE_BINDING = 11;

inline constexpr uint32_t kMAX_BINDLESS_RESOURCES = 1024;

const std::string cache_file_path = "pipeline_cache_data.bin";

inline constexpr uint32_t kSTAGING_BUFFER_SIZE = 64 * 1024 * 1024;

inline constexpr uint32_t kMAX_DRAW_INDIRECT_COUNT = 1024;

#endif
