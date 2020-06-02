#pragma once

#include <functional>

namespace Ubpa::detail::DX12::FG::RsrcMngr_ {
	template <class T>
	inline void hash_combine(std::size_t& s, const T& v)
	{
		std::hash<T> h;
		s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
	}

	template<typename T>
	struct always_false : std::false_type {};
	template<typename T>
	static constexpr bool always_false_v = always_false<T>::value;
}

namespace std {
	template<>
	struct hash<D3D12_RESOURCE_DESC> {
		size_t operator()(const D3D12_RESOURCE_DESC& desc) const noexcept {
			size_t rst = 0;
			for (size_t i = 0; i < sizeof(D3D12_RESOURCE_DESC); i++)
				Ubpa::detail::DX12::FG::RsrcMngr_::hash_combine(rst, reinterpret_cast<const char*>(&desc)[i]);
			return rst;
		}
	};
}
