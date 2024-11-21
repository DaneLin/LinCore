#include <string>
#include <string_view>
#include <cstdint>

namespace StringUtils {

	// FNV-1a 32bit hashing algorithm.
	constexpr uint32_t Fnv1a32(char const* s, std::size_t count)
	{
		return ((count ? Fnv1a32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
	}

	constexpr size_t ConstStrLen(const char* s)
	{
		size_t size = 0;
		while (s[size]) { size++; };
		return size;
	}

	struct StringHash
	{
		uint32_t computed_hash;

		constexpr StringHash(uint32_t hash) noexcept : computed_hash(hash) {}

		constexpr StringHash(const char* s) noexcept : computed_hash(0)
		{
			computed_hash = Fnv1a32(s, ConstStrLen(s));
		}
		constexpr StringHash(const char* s, std::size_t count)noexcept : computed_hash(0)
		{
			computed_hash = Fnv1a32(s, count);
		}
		constexpr StringHash(std::string_view s)noexcept : computed_hash(0)
		{
			computed_hash = Fnv1a32(s.data(), s.size());
		}
		StringHash(const StringHash& other) = default;

		constexpr operator uint32_t()noexcept { return computed_hash; }
	};

}