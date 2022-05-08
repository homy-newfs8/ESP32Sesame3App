#pragma once
#include <Arduino.h>

namespace util {

int8_t nibble(char c);

// template <size_t N>
// bool
// hex2bin(const String& str, uint8_t (&out)[N]) {
// 	if (str.length() != N * 2) {
// 		return false;
// 	}
// 	auto c_str = str.c_str();
// 	for (int i = 0; i < N; i++) {
// 		int n1 = nibble(c_str[i * 2]);
// 		int n2 = nibble(c_str[i * 2 + 1]);
// 		if (n1 < 0 || n2 < 0) {
// 			return false;
// 		}
// 		out[i] = (n1 << 4) + n2;
// 	}
// 	return true;
// }

template <size_t N>
bool
hex2bin(const String& str, std::array<uint8_t, N>& out) {
	if (str.length() != N * 2) {
		return false;
	}
	auto c_str = str.c_str();
	for (int i = 0; i < N; i++) {
		int n1 = nibble(c_str[i * 2]);
		int n2 = nibble(c_str[i * 2 + 1]);
		if (n1 < 0 || n2 < 0) {
			return false;
		}
		out[i] = (n1 << 4) + n2;
	}
	return true;
}

template <typename T, size_t S>
constexpr size_t
array_size(const T (&)[S]) {
	return S;
}

}  // namespace util
