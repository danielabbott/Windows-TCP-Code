#pragma once

#include <stdexcept>

template <typename T>
class Slice {
	T* p;
	uintptr_t n;

public:
	Slice(T* p_, uintptr_t n_) :p(p_), n(n_) {}

	T* ptr() {
		return p;
	}
	uintptr_t size() {
		return n;
	}

	T& operator[](long int i) const {
		return p[i];
	}

	Slice<T> slice(unsigned int offset, unsigned int new_length) {
		if (offset + new_length > n) {
			throw std::runtime_error("Slice out of range");
		}
		return Slice<T>(&p[offset], new_length);
	}

	template <typename U>
	constexpr Slice<U> cast() {
		static_assert(sizeof(T) == sizeof(U), "Slice sizes do not match");
		return Slice<U>(reinterpret_cast<U*>(p), n);
	}
};
