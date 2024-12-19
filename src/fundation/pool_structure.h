#pragma once
#include <vector>
#include <cstdint>
#include <memory>
#include <cassert>
#include <type_traits>
#include "logging.h"

namespace lincore
{

	class ResourcePool {
	public:
		ResourcePool() = default;
		~ResourcePool() = default;

		// Prevent copying
		ResourcePool(const ResourcePool&) = delete;
		ResourcePool& operator=(const ResourcePool&) = delete;

		// Allow moving
		ResourcePool(ResourcePool&&) noexcept = default;
		ResourcePool& operator=(ResourcePool&&) noexcept = default;

		bool Init(uint32_t pool_size, uint32_t resource_size) {
			try {
				// 分配总内存：资源内存 + 索引内存
				uint32_t allocation_size = pool_size * resource_size;
				memory.resize(allocation_size);
				free_indices.resize(pool_size);

				// 初始化可用索引列表
				for (uint32_t i = 0; i < pool_size; ++i) {
					free_indices[i] = i;
				}

				free_indices_head = 0;
				pool_size_ = pool_size;
				resource_size_ = resource_size;
				used_indices = 0;

				return true;
			}
			catch (...) {
				Shutdown();
				return false;
			}
		}

		void Shutdown() {
			if (free_indices_head > 0) {
				// 打印未释放的资源信息
				for (uint32_t i = 0; i < free_indices_head; ++i) {
					LOGI("Resource %zu was not released", free_indices[i]);
				}
			}

			assert(used_indices == 0 && "Resources still in use during shutdown");

			memory.clear();
			free_indices.clear();
			free_indices_head = 0;
			pool_size_ = 0;
			resource_size_ = 0;
			used_indices = 0;
		}

		uint32_t ObtainResource() {
			if (free_indices_head < pool_size_) {
				const uint32_t free_index = free_indices[free_indices_head++];
				++used_indices;
				return free_index;
			}
			return k_invalid_index;
		}

		void ReleaseResource(uint32_t handle) {
			assert(handle != k_invalid_index && "Invalid handle");
			assert(free_indices_head > 0 && "No resources to release");
			assert(handle < pool_size_ && "Handle out of range");

			--free_indices_head;
			free_indices[free_indices_head] = handle;
			--used_indices;
		}

		void* AccessResource(uint32_t handle) {
			if (handle >= pool_size_) return nullptr;
			return memory.data() + handle * resource_size_;
		}

		const void* AccessResource(uint32_t handle) const {
			if (handle >= pool_size_) return nullptr;
			return memory.data() + handle * resource_size_;
		}

		static constexpr uint32_t k_invalid_index = UINT32_MAX;

	private:
		std::vector<uint8_t> memory;
		std::vector<uint32_t> free_indices;
		uint32_t free_indices_head{ 0 };
		uint32_t pool_size_{ 0 };
		uint32_t resource_size_{ 0 };
		uint32_t used_indices{ 0 };
	};

	template<typename T>
	class TypedResourcePool {
		static_assert(std::is_default_constructible_v<T>, "T must be default constructible");
	public:
		bool Init(uint32_t pool_size) {
			return pool.Init(pool_size, sizeof(T));
		}

		void Shutdown() {
			pool.Shutdown();
		}

		T* Obtain() {
			uint32_t index = pool.ObtainResource();
			if (index != ResourcePool::k_invalid_index) {
				T* resource = static_cast<T*>(pool.AccessResource(index));
				if (resource) {
					new (resource) T();
					if constexpr (has_pool_index_v<T>) {
						resource->pool_index = index;
					}
					return resource;
				}
			}
			return nullptr;
		}

		void Release(T* resource) {
			if (!resource) return;

			resource->~T();

			if constexpr (has_pool_index_v<T>) {
				pool.ReleaseResource(resource->pool_index);
			}
			else {
				uint8_t* mem_start = static_cast<uint8_t*>(pool.AccessResource(0));
				uint32_t index = (reinterpret_cast<uint8_t*>(resource) - mem_start) / sizeof(T);
				pool.ReleaseResource(index);
			}
		}

		T* Get(uint32_t index) {
			return static_cast<T*>(pool.AccessResource(index));
		}

		const T* Get(uint32_t index) const {
			return static_cast<const T*>(pool.AccessResource(index));
		}

	private:
		template<typename U>
		struct has_pool_index {
		private:
			template<typename V>
			static auto Check(V*) -> decltype(std::declval<V>().pool_index, std::true_type());
			static std::false_type Check(...);
		public:
			static constexpr bool value = decltype(Check(static_cast<U*>(nullptr)))::value;
		};

		template<typename U>
		static constexpr bool has_pool_index_v = has_pool_index<U>::value;

		ResourcePool pool;
	};

} // namespace lincore