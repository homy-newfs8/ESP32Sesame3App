# ESP32Sesame3App
ESP32のBluetoothでSESAME 5 / SESAME 5 PRO / SESAME 4 / SESAME 3 / SESAME サイクル / SESAME bot を操作するアプリ

## 概要
これはESP32用ライブラリ[libsesame3bt](http://github.com/homy-newfs8/libsesame3bt)を使ったサンプルアプリです。

[CANDY HOUSE](https://jp.candyhouse.co/)社製のスマートロックおよびbotをESP32搭載マイコンで施錠、開錠することができます。

## 対応機種
以下の機種に対応しています。
- [SESAME 5](https://jp.candyhouse.co/products/sesame5)
- [SESAME 5 PRO](https://jp.candyhouse.co/products/sesame5-pro)
- [SESAME bot](https://jp.candyhouse.co/products/sesame3-bot)
- [SESAME 4](https://jp.candyhouse.co/products/sesame4)
- [SESAME 3](https://jp.candyhouse.co/products/sesame3)
- [SESAME サイクル](https://jp.candyhouse.co/products/sesame3-bike)

## 開発環境
以下のデバイスで開発しました。他のデバイスでも条件が合えば少しの修正で利用可能です。
- [M5StickC](https://www.switch-science.com/catalog/6350/)

## 使用方法
- 本アプリの構築には開発環境[PlatformIO](https://platformio.org/)が必要です。PlatformIOのプロジェクトとしてビルド、アップロードが可能です。

### デバイスに合わせてたアプリの修正
本アプリは以下の入出力を使用しています。
- ボタン x1 (GPIOに接続され押すとLOWレベルになる)
- LED x1 (GPIOに接続されLOWレベルにすると点灯する)

(M5StickCはいずれも内蔵しています)

[main.cpp](src/main.cpp)の先頭付近にある以下2行を接続しているGPIO番号に合わせて変更します。

```C++
constexpr int button_pin = 37;
constexpr int led_pin = 10;
```

### 初期設定モード
アプリをマイコンにアップロードして起動すると、LEDがゆっくり点滅(0.5秒点灯、0.5秒消灯の繰り返し)し、初期設定モードに入ります。

初期設定モードではESP32はWi-Fiアクセスポイント+Webサーバーとして動作しています。アクセスポイント名と接続パスワードは[ソースコード](src/SettingServer.cpp)の先頭付近を参照してください(心配な方は変更してからビルドしてください)。

接続が完了したらWebブラウザで以下にアクセスします。

```
http://10.3.3.3/
```

[BTスキャン実行]ボタンをクリックすると周囲にある対応セサミを20秒間スキャンします。
見つかった対応デバイスが一覧に表示されますので、一覧から選択してから以下の情報を入力して[接続テスト]をクリックしてください。

- Secret Key
- Public Key

これらの情報は、SESAMEスマホアプリの鍵シェア機能で使用するQRコードを[QR Code Reader for SESAME](https://sesame-qr-reader.vercel.app/)で処理すると取得できます(オーナーまたはマネジャー用のQRコードが必要です)。SESAME 5 / SESAME 5 PRO では Secret Key のみ入力します(Public Keyは空欄で)。

接続テストが成功すると、入力された情報をESP32内に保存します。この時点でブラウザ上に再起動を促すメッセージが表示されますので、ESP32マイコンを再起動します。失敗した場合はメッセージを見て何らか対応してください。

初期設定モードでのLEDは以下の表示を行います。

|状態|LEDの点滅|
|----|---------|
|設定モード|遅い点滅(0.5秒点灯、0.5秒消灯)|
|スキャン中|速い点滅(0.25秒点灯、0.25秒消灯)|
|接続中|速い点滅(0.25秒点灯、0.25秒消灯)|

### 通常モード
初期設定が完了した状態で電源投入すると、通常モードとして動作します。通常モードではSESAMEから鍵の状態を取得しLEDの点滅に反映します。

|状態|LEDの点滅|
|----|---------|
|接続中|速い点滅(0.25秒点灯、0.25秒消灯)|
|施錠状態|短い点灯(0.25秒点灯、0.75秒消灯)|
|開錠状態|長い点灯(0.75秒点灯、0.25秒消灯)|
|エラー|速い点滅が継続する|

施錠状態または開錠状態の場合、ボタン操作により施錠、開錠が可能です。

|ボタン操作|動作|
|-|-|
|クリック|施錠|
|ダブルクリック|開錠|
|ロングクリック|クリック(botのみ)|

### 再設定(データ消去)
初期設定をやり直したい場合は以下の操作を行います。

1. 電源を切る
1. ボタンを押しながら電源を入れる(ボタンは押しっぱなしで)
1. そのままボタンを3秒押しっぱなしにすると初期設定モードに入る

初期設定モードに入った時点でデータは消去されていますので、もう使用しないのであればそのまま電源を切ります。

|状態|LEDの状態|
|-|-|
|電源投入|一瞬点灯|
|ボタン押しっぱなし|消灯|
|設定モード|遅い点滅(0.5秒点灯、0.5秒消灯)|

初期設定で保存したSESAMEの情報はESP32に別のアプリを上書きしても消えません。消去したい場合は本アプリがインストールされている状態で上記操作を実行してください。

なお、本操作にデータを消去した場合、通常の方法でアプリからデータを読み出すことはできませんが、本当にESP32チップからデータが消えているかどうかはわかりません。

## 関連リポジトリ
- [libsesame3bt](https://github.com/homy-newfs8/libsesame3bt)
本アプリが使用しているライブラリ。
- [libsesame3bt-dev](https://github.com/homy-newfs8/libsesame3bt-dev)
上記ライブラリの開発用リポジトリ。サンプルファイルの実行等が可能です。

## 制限事項
- 本アプリはSESAMEデバイスの初期設定を行うことができません。公式アプリで初期設定済みのSESAMEのみ操作可能です。

## 謝辞
困難な状況の中で開発を続けている[PlatformIO](https://platformio.org/)プロジェクトのメンバーに感謝します。
