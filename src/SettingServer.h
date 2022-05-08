#pragma once
#include <WebServer.h>

class SettingServer {
 public:
	static inline constexpr const char* prefs_name = "libsesame3bt";
	SettingServer() {}
	void setup();
	void loop() {
		if (canRun) {
			server.handleClient();
		} else {
			delay(1);
		}
	}

 private:
	void hRoot();
	void hScan();
	bool canRun = false;
	WebServer server{80};
	void response_string(const char*);
	template <typename... Args>
	void response_text(const char* format, Args... args) {
		char buffer[128];
		snprintf(buffer, sizeof(buffer), format, args...);
		response_string(buffer);
	}
	template <typename... Args>
	void response_text(const __FlashStringHelper* fmt, Args... args) {
		char buffer[128];
		snprintf_P(buffer, sizeof(buffer), (PGM_P)fmt, args...);
		response_string(buffer);
	}

	static const char* start_page;
};
