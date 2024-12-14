#pragma once
#include <vk_types.h>
#include "vk_descriptors.h"
#include <unordered_map>
#include <filesystem>
#include "config.h"
#include <TaskScheduler.h>
#include <future>
#include <logging.h>

namespace fastgltf
{
	class Asset;
	struct Image;
}

class VulkanEngine;

namespace lc
{
	enum class FileLoadRequestType
	{
		kURI,
		kVector,
		kBufferView
	};

	// file loading request
	struct FileLoadRequest
	{
		FileLoadRequestType type;
		char path[512];			 // URI type
		const void *memory_data; // vector and bufferview type
		size_t memory_size;
		size_t buffer_offset; // buffer view
		std::function<void(AllocatedImage)> callback;
	};

	// upload request
	struct UploadRequest
	{
		void *data;
		size_t size;
		VkFormat format;
		VkExtent3D extent;
		bool enable_mips;
		std::function<void(AllocatedImage)> callback;
	};


	struct AsyncLoaderState {
		enum class State {
			Running,
			Stopping,
			Stopped
		};
		std::atomic<State> state{ State::Stopped };
		std::mutex mutex;
		std::condition_variable cv;
	};

	class AsyncLoader
	{
	public:
		void Init(enki::TaskScheduler *task_scheduler);
		void Shutdown();

		void Update();

		void RequestFileLoad(const char *path, std::function<void(AllocatedImage)> callback);
		void RequestImageUpload(void *data, VkExtent3D extent, VkFormat format, std::function<void(AllocatedImage)> callback);

		void RequestVectorLoad(const void* data, size_t size, std::function<void(AllocatedImage)> callback);

		void RequestBufferViewLoad(const void* data, size_t size, size_t offset, std::function<void(AllocatedImage)> callback);

		std::shared_ptr<AsyncLoaderState> GetState() { return state_; }

	private:
		enki::TaskScheduler *task_scheduler_{nullptr};
		std::shared_ptr<AsyncLoaderState> state_ = std::make_shared<AsyncLoaderState>();


		std::mutex request_mutex_;
		std::vector<FileLoadRequest> file_load_requests_;
		std::vector<UploadRequest> upload_requests_;

		void ProcessFileRequests();
		void ProcessUploadRequests();
	};

	// I/O thread task
	struct RunPinnedTaskLoopTask : enki::IPinnedTask
	{
		void Execute() override
		{
			while (task_scheduler->GetIsRunning()&& execute)
			{
				task_scheduler->WaitForNewPinnedTasks();
				task_scheduler->RunPinnedTasks();
			}
		}

		enki::TaskScheduler *task_scheduler;
		std::atomic<bool> execute{ true };
	};

	// Asynchronous loading task
	struct AsyncLoadTask : enki::IPinnedTask
	{
		void Execute() override
		{
			while (state->state == AsyncLoaderState::State::Running) {
				{
					std::unique_lock<std::mutex> lock(state->mutex);
					state->cv.wait_for(lock, std::chrono::milliseconds(16),
						[this] { return state->state != AsyncLoaderState::State::Running; });
				}

				if (state->state == AsyncLoaderState::State::Running) {
					async_loader->Update();
				}
			}
		}

		AsyncLoader *async_loader;
		enki::TaskScheduler *task_scheduler;
		std::shared_ptr<AsyncLoaderState> state;
	};


	struct TextureID
	{
		uint32_t index;
	};

	class TextureCache
	{
	public:
		void SetDescriptorSet(VkDescriptorSet bindless_set)
		{
			bindless_set_ = bindless_set;
		}

		TextureID AddTexture(VkDevice device, const VkImageView &image_view, VkSampler sampler)
		{
			// 检查是否已存在
			for (uint32_t i = 0; i < cache_.size(); i++)
			{
				if (cache_[i].imageView == image_view && cache_[i].sampler == sampler)
				{
					return TextureID{i};
				}
			}
			// 检查是否超出最大数量
			if (cache_.size() >= kMAX_BINDLESS_RESOURCES)
			{
				throw std::runtime_error("Exceeded maximum bindless texture count");
			}

			uint32_t idx = static_cast<uint32_t>(cache_.size());

			// 添加到缓存
			VkDescriptorImageInfo image_info{
				.sampler = sampler,
				.imageView = image_view,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
			cache_.push_back(image_info);

			// 更新bindless descriptor set
			lc::DescriptorWriter writer;
			writer.WriteImageArray(kBINDLESS_TEXTURE_BINDING, idx, {image_info}, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
			writer.UpdateSet(device, bindless_set_);

			return TextureID{idx};
		}

	private:
		std::vector<VkDescriptorImageInfo> cache_;
		VkDescriptorSet bindless_set_;
	};

	struct GLTFMaterial
	{
		MaterialInstance data;
	};

	struct Bounds
	{
		glm::vec3 origin;
		float sphere_radius;
		glm::vec3 extents;
	};

	struct GeoSurface
	{
		uint32_t start_index;
		uint32_t count;
		Bounds bounds;
		uint32_t indirect_offset;
		std::shared_ptr<GLTFMaterial> material;
	};

	struct MeshAsset
	{
		std::string name;

		std::vector<GeoSurface> surfaces;
		GPUMeshBuffers mesh_buffers;
	};

	template <typename T>
	struct PerPassData
	{
	public:
		T &operator[](MeshPassType pass)
		{
			switch (pass)
			{
			case MeshPassType::kMainColor:
				return data[0];
			case MeshPassType::kTransparent:
				return data[1];
			case MeshPassType::kDirectionalShadow:
				return data[2];
			}
			assert(false);
			return data[0];
		}

		void Clear(T &&val)
		{
			for (int i = 0; i < 3; i++)
			{
				data[i] = val;
			}
		}

	private:
		std::array<T, 3> data;
	};

	struct LoadedGLTF : public IRenderable
	{
		// storage for all the data on a given glTF file
		std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
		std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
		std::unordered_map<std::string, AllocatedImage> images;
		std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

		// nodes that dont have a parent, for iterating through the file in tree order
		std::vector<std::shared_ptr<Node>> top_nodes;

		std::vector<VkSampler> samplers;

		lc::DescriptorAllocatorGrowable descriptor_pool;

		BufferHandle material_data_buffer_handle;

		//VulkanEngine *creator;
		GPUDevice* gpu_device;

		~LoadedGLTF()
		{
			ClearAll();
		}

		virtual void Draw(const glm::mat4 &top_matrix, DrawContext &ctx) override;

	private:
		void ClearAll();
	};

	void AddMeshBufferToGlobalBuffers(std::span<uint32_t> indices, std::span<Vertex> vertices);
	std::optional<std::shared_ptr<LoadedGLTF>> LoadGltf(GPUDevice* gpu_device, std::string_view file_path);
} // namespace lc
