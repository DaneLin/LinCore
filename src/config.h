// Should we enable vertical sync during presentation? Worth setting to 0 when doing perf profiling to avoid GPU downclock during idle
#define CONFIG_VSYNC 1

// Should we enable synchronization validation? Worth running with 1 occasionally to check correctness.
#define CONFIG_SYNCVAL 0

// Maximum number of texture descriptors in the pool
#define DESCRIPTOR_LIMIT 65536

#define ASSET_PATH "../../"

constexpr uint32_t BINDLESS_TEXTURE_BINDING = 11;;

constexpr uint32_t MAX_BINDLESS_RESOURCES = 1024;

const std::string cacheFilePath = "pipeline_cache_data.bin";

