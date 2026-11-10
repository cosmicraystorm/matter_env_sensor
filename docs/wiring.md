# ハードウェア配線

ファームウェアのピン定義: [PinAssign.h](../main/src/frameworksanddrivers/PinAssign.h)

```mermaid
flowchart LR
    subgraph POWER["電源"]
        USB["USB-C<br/>(書き込み・ログ用)"]
        BAT["Li-Po 電池<br/>(運用時)"]
    end

    subgraph XIAO["Seeed XIAO ESP32C6"]
        direction TB
        P3V3["3V3"]
        PGND["GND"]
        PD0["D0 / GPIO0"]
        PBAT["BAT+ / BAT- パッド<br/>(裏面)"]

        subgraph ONBOARD["基板上 (配線不要)"]
            LED["ユーザーLED 黄<br/>GPIO15 / LOW で点灯"]
            RFEN["RFスイッチ電源<br/>GPIO3 = LOW"]
            ANT["アンテナ選択<br/>GPIO14 = LOW → 内蔵アンテナ"]
            BOOT["BOOT ボタン<br/>GPIO9 (未使用)"]
        end
    end

    subgraph SENSOR["AM2302 (DHT22)"]
        direction TB
        S1["1: VDD"]
        S2["2: DATA"]
        S3["3: NC"]
        S4["4: GND"]
    end

    RPU["プルアップ抵抗<br/>4.7kΩ〜10kΩ"]

    USB --- XIAO
    BAT --- PBAT

    P3V3 -->|"3.3V"| S1
    PD0 <-->|"1-wire データ"| S2
    PGND --- S4
    S2 --- RPU
    RPU --- P3V3

    S3 -.-|"未接続"| S3
```

## 接続表

| XIAO ESP32C6 | AM2302 | 備考 |
|---|---|---|
| 3V3 | 1: VDD | AM2302 は 3.3V〜5.5V で動く |
| D0（GPIO0） | 2: DATA | 3V3 との間に 4.7kΩ〜10kΩ のプルアップ抵抗を入れる |
| — | 3: NC | 何もつながない |
| GND | 4: GND | |

## 補足

- **プルアップ:** 内蔵プルアップも有効にしているが弱いので、外付け抵抗を推奨。3ピンの基板付きモジュールはプルアップ抵抗が載っていることが多いので、その場合は不要。
- **D0 を選んだ理由:** GPIO0〜7 は Light Sleep 中も扱える LP IO。将来ボタンで起こすようにするなら、ボタンもこの範囲（D0〜D2 / GPIO0〜2）のピンにする。
- **電池:** Li-Po は裏面の BAT+ / BAT- パッドにつなぐ。USB を挿している間は Light Sleep しない設定（`CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION=y`）なので、消費電流は電池だけで動かして測る。
- **基板上のピン:** LED（GPIO15）と RF スイッチ（GPIO3 / GPIO14）は基板上で配線済みなので外部配線は不要。外部アンテナを使うときは [PinAssign.h](../main/src/frameworksanddrivers/PinAssign.h) と [XiaoBoard.hpp](../main/src/frameworksanddrivers/XiaoBoard.hpp) で GPIO14 を HIGH にする。
