#pragma once
#if USE_M5STICK_FEATURE
#include <M5StickC.h>
#endif
#include <cstdint>

enum class blink_pattern_t : uint8_t {
	off = 0b0000,
	on = 0b1111,
	locked = 0b0001,
	unlocked = 0b0111,
	active = 0b0101,
	connecting = 0b0101,
	setting = 0b0011,
	error = 0b0101,
};

#if USE_M5STICK_FEATURE
enum class ind_color_t : int { on = TFT_RED, off = TFT_BLACK, locked = TFT_GREEN, unlocked = TFT_PURPLE, setting = TFT_BLUE };
#else
enum class ind_color_t : int {
	on,
	off,
	locked,
	unlocked,
	setting,
};
#endif

extern void set_ind_color(ind_color_t color, blink_pattern_t pattern);
