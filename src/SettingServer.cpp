#include "SettingServer.h"
#include <Arduino.h>
#include <M5StickC.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <SesameClient.h>
#include <SesameScanner.h>
#include <optional>
#include "main.h"
#include "util.h"

using libsesame3bt::Sesame;
using libsesame3bt::SesameClient;
using libsesame3bt::SesameInfo;
using libsesame3bt::SesameScanner;

namespace {

const static char* SSID = "libsesame3bt";
const static char* AP_PASSWORD = "hirakegoma";

}  // namespace

const char* SettingServer::start_page =
    u8R"***(
<!doctype html>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>libsesame3bt sample</title>
<p>libsesame3bt sample setting</p>
<form action="/scan" method="post">
	<p>UUID: <input name="uuid"></p>
	<p>Secret Key: <input name="secret"></p>
	<p>Public Key: <input name="pk"></p>
	<p><input type="submit" value="BTスキャン実行"></p>
</form>
)***";

void
SettingServer::setup() {
	Preferences prefs{};
	if (!prefs.begin(prefs_name)) {
		Serial.println(F("Failed to open preference storage"));
		return;
	}
	prefs.clear();
	prefs.end();
	if (!WiFi.softAPConfig(IPAddress(10, 3, 3, 3), IPAddress(0, 0, 0, 0), IPAddress(255, 255, 255, 0))) {
		Serial.println(F("Failed to Wi-Fi AP setting"));
		return;
	}
	if (!WiFi.softAP(SSID, AP_PASSWORD)) {
		Serial.println(F("Failed to start Wi-Fi AP"));
		return;
	}
	server.on("/", [this] { hRoot(); });
	server.on("/scan", HTTPMethod::HTTP_POST, [this] { hScan(); });
	server.begin();
	canRun = true;
	Serial.printf_P(PSTR("Setting server ready on %s\n"), WiFi.softAPIP().toString().c_str());
	set_ind_color(ind_color_t::setting, blink_pattern_t::setting);
}

void
SettingServer::hRoot() {
	server.send(200, "text/html;charset=utf-8", start_page);
}

void
SettingServer::response_string(const char* text) {
	server.send(200, "text/plain;charset=utf-8", text);
}

const static char*
model_name(Sesame::model_t model) {
	switch (model) {
		case Sesame::model_t::sesame_3:
			return "SESAME 3";
		case Sesame::model_t::sesame_bot:
			return "SESAME bot";
		case Sesame::model_t::wifi_2:
			return "WiFi Module 2";
		case Sesame::model_t::sesame_4:
			return "SESAME 4";
		case Sesame::model_t::sesame_cycle:
			return "SESAME Cycle";
		default:
			return "UNKNOWN";
	}
}

void
SettingServer::hScan() {
	auto uuid_str = server.arg("uuid");
	NimBLEUUID uuid{std::string(uuid_str.c_str())};
	if (uuid.bitSize() == 0) {
		response_text(F(u8"UUIDの形式が正しくありません。"));
		return;
	}

	auto secret_str = server.arg("secret");
	std::array<uint8_t, SesameClient::SECRET_SIZE> secret;
	if (!util::hex2bin(secret_str, secret)) {
		response_text(F(u8"Secret Keyの形式が正しくありません。"));
		return;
	}

	auto pk_str = server.arg("pk");
	std::array<uint8_t, SesameClient::PK_SIZE> pk;
	if (!util::hex2bin(pk_str, pk)) {
		response_text(F(u8"Public Keyの形式が正しくありません。"));
		return;
	}
	set_ind_color(ind_color_t::setting, blink_pattern_t::connecting);
	std::optional<SesameInfo> found;
	SesameScanner::get().scan(20, [&uuid, &found](auto& _scanner, const auto* _info) {
		if (_info && _info->uuid == uuid) {
			found.emplace(*_info);
			_scanner.stop();
		}
	});
	if (!found) {
		response_text(F(u8"UUID: %s のデバイスが見つかりませんでした。"), uuid.toString().c_str());
		return;
	}
	if (found->model != Sesame::model_t::sesame_3 && found->model != Sesame::model_t::sesame_4) {
		response_text(F(u8"'%s' はサポートしていません。"), model_name(found->model));
		return;
	}
	if (!found->flags.registered) {
		response_text(F(u8"このデバイスは初期化されていません。"));
		return;
	}
	SesameClient client{};
	client.begin(found->address, found->model);
	if (!client.set_keys(pk, secret)) {
		response_text(F(u8"鍵情報の設定に失敗しました。"));
		return;
	}
	SesameClient::state_t cur_state;
	client.set_state_callback([&cur_state](auto& _client, auto state) { cur_state = state; });
	if (!client.connect(5)) {
		response_text(F(u8"SESAMEへの接続ができませんでした。"));
		return;
	}
	uint32_t started = millis();
	while (millis() - started < 10'000 && cur_state != SesameClient::state_t::active) {
		delay(100);
	}
	if (cur_state != SesameClient::state_t::active) {
		response_text(F(u8"SESAMEの認証に失敗しました。"));
		return;
	}
	client.disconnect();

	set_ind_color(ind_color_t::setting, blink_pattern_t::setting);
	Preferences prefs{};
	if (!prefs.begin(prefs_name)) {
		response_text(F(u8"設定保存領域を使用できません。"));
		return;
	}
	std::array<uint8_t, 6> btaddr;
	std::copy(found->address.getNative(), found->address.getNative() + 6, btaddr.begin());
	std::reverse(std::begin(btaddr), std::end(btaddr));
	if (prefs.putChar("model", static_cast<int8_t>(found->model)) != 1 ||
	    prefs.putBytes("addr", btaddr.data(), std::size(btaddr)) != std::size(btaddr) ||
	    prefs.putBytes("pk", pk.data(), std::size(pk)) != std::size(pk) ||
	    prefs.putBytes("secret", secret.data(), std::size(secret)) != std::size(secret)) {
		response_text(F(u8"設定の保存に失敗しました。"));
		return;
	}
	prefs.end();

	response_text(F(u8"設定が完了しました。再起動してください。"));
}
