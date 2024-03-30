#pragma once
#include <WebServer.h>

class SettingServer {
 public:
	static inline constexpr const char* prefs_name = "libsesame3bt";
	SettingServer() {}
	void setup();
	void loop();

 private:
	void hRoot();
	void hConnect();
	void hScan();
	bool canRun = false;
	WebServer server{80};
	void response_string(const char*);
};
