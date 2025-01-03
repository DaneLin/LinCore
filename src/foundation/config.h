// config.h
#ifndef CONFIG_H
#define CONFIG_H

#if NDEBUG
constexpr bool bUseValidationLayers = false;
#else
constexpr bool bUseValidationLayers = true;
#endif

const std::vector<const char*> required_extensions = {
	VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
	VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
	VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
	VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME };

// Should we enable vertical sync during presentation? Worth setting to 0 when doing perf profiling to avoid GPU downclock during idle
#define CONFIG_VSYNC 1

// Should we enable synchronization validation? Worth running with 1 occasionally to check correctness.
#define CONFIG_SYNCVAL 0

// Maximum number of texture descriptors in the pool
#define DESCRIPTOR_LIMIT 65536

constexpr unsigned int kFRAME_OVERLAP = 2;
constexpr uint32_t kINITIAL_DESCRIPTOR_POOL_SIZE = 1024;

constexpr uint32_t kBINDLESS_TEXTURE_SET_ID = 1;
constexpr uint32_t kBINDLESS_TEXTURE_BINDING = 0;
constexpr uint32_t kMAX_BINDLESS_RESOURCES = 1024;

constexpr uint8_t kMAX_IMAGE_OUTPUT = 8;
constexpr uint8_t kMAX_DESCRIPTOR_SET_LAYOUTS = 8;
constexpr uint8_t kMAX_SHADER_STAGES = 8;
constexpr uint8_t kMAX_DESCRIPTOR_PER_SET = 16;

const std::string cache_file_path = "pipeline_cache_data.bin";

constexpr uint32_t kSTAGING_BUFFER_SIZE = 64 * 1024 * 1024;

constexpr uint32_t kNUM_RENDER_THREADS = 4;

#define LC_DRAW_INDIRECT 0

#endif
