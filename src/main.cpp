#include "main.h"
#include <Arduino.h>
#include <OneButton.h>
#include <Preferences.h>
#include <Sesame.h>
#include <SesameClient.h>
#include <SesameScanner.h>
#include <TaskManager.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "SettingServer.h"

using libsesame3bt::Sesame;
using libsesame3bt::SesameClient;
using libsesame3bt::SesameInfo;

constexpr int button_pin = 37;
constexpr int led_pin = 10;

constexpr int AUTH_TIMEOUT = 3000;

SesameClient client{};
Preferences prefs{};
SesameClient::Status sesame_status{};
SesameClient::state_t sc_state;
ind_color_t ind_color;
int ind_state;
blink_pattern_t ind_pattern;
OneButton button{button_pin};

bool
is_supported(Sesame::model_t model) {
	using model_t = Sesame::model_t;
	switch (model) {
		case model_t::sesame_3:
		case model_t::sesame_4:
		case model_t::sesame_5:
		case model_t::sesame_5_pro:
		case model_t::sesame_bike:
			return true;
		default:
			return false;
	}
}

const char*
model_name(Sesame::model_t model) {
	switch (model) {
		case Sesame::model_t::sesame_3:
			return "SESAME 3";
		case Sesame::model_t::wifi_2:
			return "Wi-Fi Module 2";
		case Sesame::model_t::sesame_bot:
			return "SESAME bot";
		case Sesame::model_t::sesame_bike:
			return "SESAME Cycle";
		case Sesame::model_t::sesame_4:
			return "SESAME 4";
		case Sesame::model_t::sesame_5:
			return "SESAME 5";
		case Sesame::model_t::sesame_5_pro:
			return "SESAME 5 PRO";
		case Sesame::model_t::sesame_touch:
			return "SESAME TOUCH";
		case Sesame::model_t::sesame_touch_pro:
			return "SESAME TOUCH PRO";
		case Sesame::model_t::sesame_bike_2:
			return "SESAME Cycle 2";
		default:
			return "UNKNOWN";
	}
}

void
reflect_color() {
	bool is_blank = ((1 << ind_state) & static_cast<uint8_t>(ind_pattern)) == 0;
	const int col = static_cast<int>(is_blank ? ind_color_t::off : ind_color);
#if USE_M5STICK_FEATURE
	M5.Lcd.fillScreen(col);
#endif
	digitalWrite(led_pin, is_blank ? HIGH : LOW);
	ind_state++;
	if (ind_state >= 4) {
		ind_state = 0;
	}
}

void
set_ind_color(ind_color_t color, blink_pattern_t pattern) {
	ind_color = color;
	ind_pattern = pattern;
	ind_state = 0;
	reflect_color();
}

TaskHandle_t intervl_task_hnd;

void
interval_task_loop(void* _) {
	for (;;) {
		Tasks.update();
		delay(10);
	}
}

std::optional<SettingServer> sserver;
bool is_locked;

void
status_update(SesameClient& client, SesameClient::Status status) {
	if (status != sesame_status) {
		Serial.printf_P(PSTR("Status in_lock=%u,in_unlock=%u,pos=%d,volt=%.2f,volt_crit=%u\n"), status.in_lock(), status.in_unlock(),
		                status.position(), status.voltage(), status.battery_critical());
		sesame_status = status;
		bool new_locked = status.in_lock() == status.in_unlock() ? is_locked : status.in_lock();
		if (new_locked != is_locked) {
			set_ind_color(new_locked ? ind_color_t::locked : ind_color_t::unlocked,
			              new_locked ? blink_pattern_t::locked : blink_pattern_t::unlocked);
			is_locked = new_locked;
		}
	}
}

void
button_clicked() {
	Serial.println(F("click"));
	if (client.is_session_active()) {
		if (!is_locked) {
			client.lock(u8"テストアプリ");
			set_ind_color(ind_color_t::unlocked, blink_pattern_t::active);
		}
	}
}

void
button_doubleclicked() {
	Serial.println(F("double click"));
	if (client.is_session_active()) {
		if (is_locked) {
			client.unlock(u8"テストアプリ");
			set_ind_color(ind_color_t::locked, blink_pattern_t::active);
		}
	}
}

void
setup() {
	pinMode(led_pin, OUTPUT);
	digitalWrite(led_pin, LOW);
#if USE_M5STICK_FEATURE
	M5.begin();
	M5.Axp.ScreenBreath(8);
#else
	Serial.begin(115200);
#endif
	Serial.println(F("Starting ESP32SesameApp"));
	set_ind_color(ind_color_t::on, blink_pattern_t::on);
	BLEDevice::init("");

	Tasks.add([] { reflect_color(); })->startFps(4.0);
	xTaskCreateUniversal(interval_task_loop, "interval", 2048, nullptr, 1, &intervl_task_hnd, APP_CPU_NUM);

	bool force_init = false;
	if (digitalRead(button_pin) == LOW) {
		// 初期化操作の確認
		uint32_t start = millis();
		bool long_press = true;
		set_ind_color(ind_color_t::off, blink_pattern_t::off);
		while (millis() - start < 3000) {
			if (digitalRead(button_pin) == HIGH) {
				long_press = false;
				break;
			}
			delay(10);
		}
		if (long_press) {
			Serial.println(F("Force initialize"));
			force_init = true;
		}
	}
	set_ind_color(ind_color_t::on, blink_pattern_t::on);
	bool initialized = false;
	if (!force_init) {
		if (!prefs.begin(SettingServer::prefs_name, true)) {
			Serial.println(F("Failed to begin prefs"));
		} else {
			auto model = static_cast<Sesame::model_t>(prefs.getChar("model", -1));
			if (is_supported(model)) {
				size_t rsz;
				std::array<uint8_t, 6> bt_address;
				std::array<std::byte, Sesame::PK_SIZE> pk;
				std::array<std::byte, Sesame::SECRET_SIZE> secret;
				if ((rsz = prefs.getBytes("addr", bt_address.begin(), std::size(bt_address))) == std::size(bt_address) &&
				    (rsz = prefs.getBytes("pk", pk.begin(), std::size(pk))) == std::size(pk) &&
				    (rsz = prefs.getBytes("secret", secret.begin(), std::size(secret))) == std::size(secret)) {
					BLEAddress addr{bt_address.data(), BLE_ADDR_RANDOM};
					if (client.begin(addr, model) && client.set_keys(pk, secret)) {
						initialized = true;
					} else {
						Serial.println(F("Failed to SesameClient init"));
					}
				} else {
					Serial.println(F("Failed to load settings"));
				}
			} else {
				Serial.printf_P(PSTR("model = %s not suooprted\n"), model_name(model));
			}
			prefs.end();
		}
	}
	if (!initialized) {
		sserver.emplace();
	}

	if (sserver) {
		Serial.println(F("starting setting mode"));
		sserver->setup();
		return;
	}
	Serial.println(F("starting normal mode"));
	client.set_state_callback([](auto& client, auto state) { sc_state = state; });
	client.set_status_callback(status_update);
	button.attachClick(button_clicked);
	button.attachDoubleClick(button_doubleclicked);
}

enum class state_t { idle, connecting, active, abort };

state_t my_state = state_t::idle;
uint32_t last_operated = 0;

void
loop() {
	if (sserver) {
		sserver->loop();
		return;
	}
	button.tick();
	switch (my_state) {
		case state_t::idle:
			if (last_operated == 0 || millis() - last_operated > 2000) {
				Serial.println(F("connecting"));
				set_ind_color(ind_color_t::on, blink_pattern_t::active);
				if (client.connect()) {
					Serial.println(F("connected"));
					last_operated = millis();
					my_state = state_t::connecting;
				} else {
					Serial.println(F("Failed to connect"));
					my_state = state_t::abort;
					set_ind_color(ind_color_t::on, blink_pattern_t::error);
				}
			}
			break;
		case state_t::connecting:
			if (sc_state == SesameClient::state_t::active) {
				my_state = state_t::active;
			} else if (millis() - last_operated > AUTH_TIMEOUT || sc_state == SesameClient::state_t::idle) {
				Serial.println(F("Failed to authenticate"));
				set_ind_color(ind_color_t::on, blink_pattern_t::error);
				my_state = state_t::abort;
			} else {
				delay(100);
			}
			break;
		case state_t::active:
			delay(1);
			break;
		case state_t::abort:
		default:
			delay(500);
			break;
	}
}
