# air_monitor_matter ハンドオフ

最終更新: 2026-11-10 / 状態: **V1 完成**

XIAO ESP32C6 + AM2302 の温湿度センサを Matter over Thread の SED(ICD) として Aqara Home に登録し、約30秒ごとに値を送るデバイス。
元プロジェクト `D:\projects\gerbera-management-system\software\AutomaticWateringSystemCore`(Arduino)を最小構成で ESP-IDF + esp-matter に作り直したもの。

> git リポジトリではない。変更前にバックアップを取るか、必要なら `git init` を提案すること。

---

## 1. V1 の仕様

| 項目 | 内容 |
|---|---|
| 動作 | ICD(SIT) / Sleepy End Device。Idle 30秒 → Active 復帰ごとに測定・送信。間は自動 Light Sleep |
| Matter | エンドポイント1: Temperature Sensor、2: Humidity Sensor(ルートノードは0) |
| センサ異常 | リトライ1回でも読めなければ温度・湿度とも **null** を1回だけ送信。正常に読めるまで再送しない |
| LED (GPIO15, Low で点灯) | 優先順: ①通信未確立(起動直後・全ファブリック削除後)=常時点灯 ②センサ異常=1秒周期で 50ms点灯→50ms消灯→50ms点灯 ③通常=消灯、SED 復帰ごとに 50ms 点灯 |
| エコシステム | Aqara Home(ハブ M100 / M200 で登録確認済み) |
| 電池見積り | 1000mAh 常時給電で約195日(**推定のみ、実測していない**)。センサ異常中は1秒ごとに起きるので短くなる(数日持てばよい、とユーザー了承済み) |

## 2. ハードウェア

- 配線図: [docs/wiring.md](docs/wiring.md)
- AM2302 DATA → D0(GPIO0)。DATA–3V3 間に 4.7〜10kΩ プルアップ。センサの VCC は 3V3
- AM2302 単体のピン: 格子側から見て左から 1:VCC 2:DATA 3:NC 4:GND
- XIAO C6 固有: RF スイッチ GPIO3=Low(有効)、アンテナ選択 GPIO14=Low(内蔵)。BOOT は GPIO9(Light Sleep からの起床には使えない)

## 3. ソース構成 (クリーンアーキテクチャ + C++20 コルーチン)

```
main/app_main.cpp                 組み立て(コンポジションルート)、Matter イベント処理
main/src/entities/                CoroTask(ホットスタートの Task<T>)、MainThreadDispatcher(FreeRTOS キュー)、
                                  DelayAwaiter(FreeRTOS タイマー)、AsyncSignal(自動リセットのラッチ)、AirCondition
main/src/usecases/                AirMonitorCore(起床→測定→送信ループ)、StatusIndicator(LED 表示仕様)、
                                  Indicator / AirConditionSensor / Linkage(抽象)
main/src/interfaceadapters/       Am2302Convert(生データ→値。データシートの例で static_assert)
main/src/frameworksanddrivers/    Am2302Sensor(ビットバング+失敗原因の分類ログ)、StatusLed(esp_timer でパルス/パターン)、
                                  MatterAirLinkage(エンドポイント作成・属性更新)、MatterIcdObserver(Active/Idle 遷移)、
                                  HeldOutputPin、XiaoBoard、PinAssign
```

- メインタスクは `MainThreadDispatcher::WaitAndExecute()` でコルーチンの再開を待つだけ(待機中に Light Sleep)
- CHIP スレッドからの属性更新は `ScheduleLambda` で CHIP スレッドに投げ、完了後にメインに戻る(MatterAirLinkage)
- StatusIndicator は CHIP スレッドとメインスレッドの両方から呼ばれるので mutex で排他

## 4. ビルド・書き込み

環境: WSL(Ubuntu) に ESP-IDF **v5.4.4**(`~/esp/esp-idf-v5.4.4`)+ esp-matter **release/v1.5**(`~/esp/esp-matter`)。
**IDF 5.5 系は不可**(connectedhomeip の gn 引数 `@toolchain/cflags` で "Invalid token" になる)。

### ビルド(WSL)
```bash
. ~/esp/esp-idf-v5.4.4/export.sh && . ~/esp/esp-matter/export.sh
cd /mnt/d/projects/air_monitor_matter

# 通常版(電池運用。Light Sleep・ICD 有効)
idf.py build

# USB デバッグ版(PM 無効。USB が切れずログが見られる。ICD/SED の動作は通常版と同じ)
idf.py -B build.usb_debug -D SDKCONFIG=sdkconfig.usb_debug \
       -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.usb_debug" build

# 調査用 ICD 無効版(運用不可。sdkconfig.defaults.no_icd 冒頭のコメント参照)
idf.py -B build.no_icd -D SDKCONFIG=sdkconfig.no_icd \
       -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.usb_debug;sdkconfig.defaults.no_icd" build
```
Windows 側から実行する場合: `MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu -- bash -l <スクリプト>`(上の内容をスクリプト化して渡す)。

### 書き込み(Windows の esptool。ポートは COM3)
```powershell
cd D:\projects\air_monitor_matter\build      # デバッグ版なら build.usb_debug
& "C:\Users\cosmi\esptool\esptool-windows-amd64\esptool.exe" --chip esp32c6 -p COM3 -b 460800 `
  --before default-reset --after hard-reset write-flash "@flash_args"
```
- `write-flash @flash_args` は NVS を消さないので **Aqara の登録は残る**
- 通常版は Light Sleep で USB が切れ COM3 が消える → BOOT を押しながら RESET で書き込みモードにしてから書く。COM3 が出るまでポーリングして出た瞬間に esptool を実行すると確実
- COM3 が全く出ない(Espressif VID 303A が PnP に現れない)ときは、まずケーブル(充電専用ケーブル)を疑う
- VS Code タスクもある(.vscode/tasks.json: WSL ビルド / Windows esptool 書き込み)

### シリアルログ
USB デバッグ版を書いて COM3 を 115200 で開く。リセットは RTS パルス(USB 再列挙されるので開き直す)。
AM2302 読み取り(クリティカルセクション約5ms)直後のログが USB Serial/JTAG で欠けることがあるが、動作は正常。

### ペアリング情報(テスト用 DAC)
- 手動コード `3497-011-2332` / QR `MT:Y.K9042C00KA0648G00`(画像: [docs/matter_pairing_qr.png](docs/matter_pairing_qr.png))
- VID 0xFFF1 / PID 0x8000 / discriminator 3840。コミッショニングウィンドウは起動後15分で閉じる
- 登録をやり直すときは、コントローラー側で削除してから NVS を消去: `esptool ... erase-region 0x10000 0xC000`(古いファブリックが残っていると登録できない)

## 5. ハマりどころ(再発防止)

1. **Aqara M100/M200 は ICD Management クラスタ(0x0046)があるとコミッショニングを中断する**(クラスタを読んだ直後に ArmFailSafe(0s))。
   → ICD の動作は残し、`app_main.cpp` でエンドポイント0から当該クラスタだけ `cluster::destroy` している。消すと Aqara 側は普通の SED として扱う
2. **自分の送信で SED が Idle→Active に遷移し、起床通知が来る**。そのまま測定すると「送信→起床→測定→送信」の2秒ループになる。
   → `AirMonitorCore` の `kMinMeasurePeriodMs`(20秒)より短い間隔の起床は無視。ICD Idle 期間を変えるときはこの値も見直す
3. ICD 無効にすると esp-matter が Thread を Minimal End Device(無線常時受信)にする → 電池では数日しか持たない
4. SIT ICD の Slow Poll は15秒が上限(static_assert)。Idle 30秒の間も親ルータへ15秒ごとにポーリングする
5. `ICDStateObserver::OnEnterActiveMode` は Idle→Active の遷移時にしか呼ばれない
6. Light Sleep 中も LED のレベルを保つため、`gpio_hold_en` を毎回掛け直す(HeldOutputPin)
7. `CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION` があっても、Windows が USB をサスペンドすると切断と判定されて Light Sleep に入り、COM3 が消える
8. XIAO のユーザー LED は Low で点灯(元の Arduino コードは極性が逆だった)

## 6. 既知の課題・今後の候補

- **起動直後の最初の AM2302 読み取りがほぼ毎回失敗する**(`data timeout at bit 27` など)。リトライで読めるので実害はない。原因は未調査
- 消費電流の実測(通常版を電池につないで電流計で測る)は未実施
- 通常版でのセンサ異常点滅(DATA 線を外す→点滅、戻す→停止)は未確認(USB デバッグ版のロジックは同じ)
- 1回の失敗(リトライ後)で異常点滅を始める。ノイズで点滅が出るようなら「2回連続で失敗したら異常」に変える案あり
- 本番用の DAC / VID / PID ではなくテスト用(0xFFF1)のまま
- センサ電源を GPIO で切って省電力にする案は議論のみ(未実装)
- `Failed to advertise operational node: 3` が起動直後に出るが、その後の動作に影響なし
