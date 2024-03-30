#define FMT_HEADER_ONLY
#include <fmt/format.h>
// fmt must be included before Arduino.h
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <SesameClient.h>
#include <SesameScanner.h>
#include <libsesame3bt/util.h>
#include <cstring>
#include <set>
#include <string_view>
#include <vector>
#include "SettingServer.h"
#include "main.h"

using libsesame3bt::Sesame;
using libsesame3bt::SesameClient;
using libsesame3bt::SesameInfo;
using libsesame3bt::SesameScanner;
namespace util = libsesame3bt::core::util;

namespace {

const static char* SSID = "libsesame3bt";
const static char* AP_PASSWORD = "hirakegoma";

}  // namespace

constexpr const char* scan_result_pre =
    u8R"***(
<!doctype html>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>libsesame3bt sample scanning</title>
<form action="/connect" method="post">
<select name="sesame">
)***";

constexpr const char* scan_result_post =
    u8R"***()
</select>
<p>Secret Key: <input name="secret"></p>
<p>Public Key: <input name="pk"></p>
<p><input type="submit" value="接続テスト"></p>
</form>
<br/>
<form action="/scan" method="post">
<button type="submit" value="rescan">再スキャン</button>
</form>
)***";

constexpr const char* start_page =
    u8R"***(
<!doctype html>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>libsesame3bt sample</title>
<form action="/scan" method="post">
	<p><input type="submit" value="BTスキャン実行"></p>
</form>
)***";

void
SettingServer::loop() {
	if (canRun) {
		server.handleClient();
	} else {
		delay(1);
	}
}
void
SettingServer::setup() {
	Preferences prefs{};
	if (!prefs.begin(prefs_name)) {
		Serial.println("Failed to open preference storage");
		return;
	}
	prefs.clear();
	prefs.end();
	if (!WiFi.softAPConfig(IPAddress(10, 3, 3, 3), IPAddress(0, 0, 0, 0), IPAddress(255, 255, 255, 0))) {
		Serial.println("Failed to Wi-Fi AP setting");
		return;
	}
	if (!WiFi.softAP(SSID, AP_PASSWORD)) {
		Serial.println("Failed to start Wi-Fi AP");
		return;
	}
	server.on("/", [this]() { hRoot(); });
	server.on("/scan", [this]() { hScan(); });
	server.on("/connect", [this]() { hConnect(); });
	server.begin();
	canRun = true;
	Serial.printf("Setting server ready on %s\n", WiFi.softAPIP().toString().c_str());
	set_ind_color(ind_color_t::setting, blink_pattern_t::setting);
}

void
SettingServer::hRoot() {
	server.send(200, "text/html;charset=utf8", start_page);
}

void
SettingServer::response_string(const char* text) {
	server.send(200, "text/plain;charset=utf-8", text);
}

void
SettingServer::hScan() {
	set_ind_color(ind_color_t::setting, blink_pattern_t::connecting);
	std::set<std::string> uuids{};
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);  // prepare chunk transfer
	server.send(200, "text/html;charset=utf-8", scan_result_pre);
	SesameScanner::get().scan(20, [this, &uuids](auto& scanner, const auto* info) {
		if (info && is_supported(info->model) && info->flags.registered) {
			auto ustr = std::string{info->uuid};
			if (uuids.count(ustr) == 0) {
				uuids.insert(ustr);
				server.sendContent(fmt::format("<option value=\"{:02x};{};{}\">{}: {}</option>", static_cast<uint8_t>(info->model),
				                               std::string(info->address).c_str(), ustr.c_str(), model_name(info->model), ustr.c_str())
				                       .c_str());
			}
		}
	});
	server.sendContent(scan_result_post);
	server.sendContent("");  // FINISH chunk
}

void
SettingServer::hConnect() {
	auto sesame = server.arg("sesame");
	auto secret_p = server.arg("secret");
	auto pk_p = server.arg("pk");
	if (!sesame.length() || !secret_p.length() || sesame.length() != 2 + 1 + 6 * 2 + 5 + 1 + 36) {
		response_string(u8"デバイスが指定されいていません。");
		return;
	}
	std::array<std::byte, 1> model_a;
	if (!util::hex2bin(std::string_view{sesame.c_str(), 2}, model_a)) {
		return;
	}
	auto model = static_cast<Sesame::model_t>(model_a[0]);
	if (Sesame::get_os_ver(model) == Sesame::os_ver_t::unknown) {
		response_string(fmt::format("'{}' はサポートしていません。", model_name(model)).c_str());
		return;
	}
	std::array<std::byte, Sesame::SECRET_SIZE> secret;
	if (!util::hex2bin(secret_p.c_str(), secret)) {
		response_string(u8"Secret Keyの形式が正しくありません。");
		return;
	}
	std::array<std::byte, Sesame::PK_SIZE> pk{};
	if (Sesame::get_os_ver(model) == Sesame::os_ver_t::os2) {
		auto pk_p = server.arg("pk");
		if (!util::hex2bin(pk_p.c_str(), pk)) {
			response_string(u8"Public Keyの形式が正しくありません。");
			return;
		}
	}
	auto addr = std::string{sesame.c_str() + 3, 6 * 2 + 5};
	NimBLEAddress address{addr, BLE_ADDR_RANDOM};
	set_ind_color(ind_color_t::setting, blink_pattern_t::connecting);
	SesameClient client{};
	client.begin(address, model);
	if (!client.set_keys(pk, secret)) {
		response_string(u8"鍵情報の設定に失敗しました。");
		return;
	}
	SesameClient::state_t cur_state;
	client.set_state_callback([&cur_state](auto& _client, auto state) { cur_state = state; });
	if (!client.connect(5)) {
		response_string(u8"SESAMEへの接続ができませんでした。");
		return;
	}
	uint32_t started = millis();
	while (millis() - started < 10'000 && cur_state != SesameClient::state_t::active) {
		delay(100);
	}
	if (cur_state != SesameClient::state_t::active) {
		response_string(u8"SESAMEの認証に失敗しました。");
		return;
	}
	client.disconnect();

	set_ind_color(ind_color_t::setting, blink_pattern_t::setting);
	Preferences prefs{};
	if (!prefs.begin(prefs_name)) {
		response_string(u8"設定保存領域を使用できません。");
		return;
	}
	std::array<uint8_t, 6> btaddr;
	std::copy(address.getNative(), address.getNative() + 6, btaddr.begin());
	std::reverse(std::begin(btaddr), std::end(btaddr));
	if (prefs.putChar("model", static_cast<int8_t>(model)) != 1 ||
	    prefs.putBytes("addr", btaddr.data(), std::size(btaddr)) != std::size(btaddr) ||
	    prefs.putBytes("pk", pk.data(), std::size(pk)) != std::size(pk) ||
	    prefs.putBytes("secret", secret.data(), std::size(secret)) != std::size(secret)) {
		response_string(u8"設定の保存に失敗しました。");
		return;
	}
	prefs.end();

	response_string(u8"設定が完了しました。再起動してください。");
}
