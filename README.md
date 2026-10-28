# BonDriver_Friio_x64

White Friio（白Friio）用の64bit BonDriverです。

白Friioによる地上デジタル放送の受信を目的として、BonSDKおよび
BonDriver_Friio 3.1 / 3.2の解析と実機検証をもとに、64bit WinUSB実装
として再構成しました。

## 対応

- White Friio（白Friio）
- 地上デジタル放送
- 64bit BonDriver対応アプリケーション
- チャンネル切り替え
- TS受信・録画
- チャンネルスキャン
- GetSignalLevel

## 対応していないもの

- Black Friio（黒Friio）
- BS / CS
- LNB制御
- FriioSky
- B-CAS復号
- Network CAS

## 使い方

BonDriver対応アプリケーション(TVTEST.EXEなど)の所定の場所(通常は同フォルダ)へ
`BonDriver_Friio_x64.dll` と `BonDriver_Friio_x64.ini` を配置してください。

通常使用では、iniの初期値を変更する必要はありません。

## ログ

ログ出力は `BonDriver_Friio_x64.ini` の `LogLevel` で変更できます。

- `0` : OFF
- `1` : NORMAL
- `2` : DEBUG

通常使用では `1` を推奨します。

## 動作確認

白Friio実機で以下を確認しています。

- 起動・初期化
- チャンネル切り替え
- 地上デジタル放送の視聴
- TS録画
- チャンネルスキャン
- GetSignalLevel
- 正常終了
- LogLevel 0 / 1 / 2

詳細な開発経緯、既知の制限、謝辞については、GitHub Release の
Release Notesを参照してください。

---

**Developed by AIC feat. AKT**

**白Friioは、まだ動きます。**
