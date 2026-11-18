# PicoClock

Raspberry Pi Pico W と Waveshare 2.13インチ e-Paper ディスプレイ（`Pico-ePaper-2.13`）を組み合わせた、C言語製の Wi-Fi NTP デジタル時計です。

起動時に Wi-Fi 経由で NTP サーバーから現在時刻を取得し、以降は1分ごとに画面を更新します。

## 主な機能

- **Wi-Fi + NTP 時刻同期**: 起動時および一定間隔（デフォルト 6時間ごと）で NTP サーバーと時刻を同期します。同期完了後は Wi-Fi を切断します。
- **見やすい横向きレイアウト（250×122）**:
  - 日付: 左上に西暦で日付を表示(`yyyy/mm/dd` 形式)
  - 時刻: 7セグ風フォントで時刻を表示(`HH:MM` 形式)
- **低チラつき・焼き付き対策**:
  - 毎分の更新には e-Paper ディスプレイの **部分更新** を使用し、画面反転のチラつきを抑えます。
  - 焼き付き防止のため、一定回数（デフォルト 60回 = 1時間ごと）に1回 **全画面更新** を行います。
  - 更新後はディスプレイをディープスリープ状態にします。

## 必要なハードウェア

- **Raspberry Pi Pico W**
- **Waveshare Pico-ePaper-2.13**（250×122、白黒、HAT 型）

### ピン配置（Waveshare Pico-ePaper-2.13 HAT 標準）

| Pico GPIO | 信号 | 説明 |
| :--- | :--- | :--- |
| `GP8` | `DC` | Data / Command 切り替え |
| `GP9` | `CS` | SPI チップセレクト |
| `GP10` | `CLK` | SPI1 クロック（SCK） |
| `GP11` | `MOSI` | SPI1 データ出力（DIN） |
| `GP12` | `RST` | リセット |
| `GP13` | `BUSY` | ビジー状態入力 |

## 開発環境・前提条件

- **Raspberry Pi Pico SDK**（2.3.1 で動作確認）
- **CMake**（3.13 以上）
- **ARM GCC ツールチェイン** / **Ninja**
- （推奨）VS Code + Raspberry Pi Pico 拡張機能

## セットアップとビルド

### 1. Wi-Fi 設定の編集

`wifi_config.h` を開き、Wi-Fi の SSID とパスワードを設定してください。

```c
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

> **注意**: ソースコードを GitHub 等に公開する場合は、実際の SSID やパスワードを記載した `wifi_config.h` をコミットしないようご注意ください。

### 2. ビルド

VS Code の Raspberry Pi Pico 拡張機能から **Build** を実行するか、プロジェクトルートで以下を実行します。

```powershell
cmake -B build -G Ninja
cmake --build build
```

ビルドが成功すると `build/picoclock.uf2` が生成されます。

### 3. Pico W への書き込み

1. Raspberry Pi Pico W の **BOOTSEL** ボタンを押しながら PC に USB 接続します。
2. マウントされたドライブ（`RPI-RP2`）に `build/picoclock.uf2` をコピーします。
3. 自動的に再起動し、画面に `Syncing NTP...` が表示されたあと現在日時が表示されます。

## カスタマイズ

### 設定変更（`wifi_config.h`）

| マクロ名 | デフォルト値 | 説明 |
| :--- | :--- | :--- |
| `NTP_SERVER` | `"ntp.nict.jp"` | 参照する NTP サーバー |
| `TIMEZONE_OFFSET_S` | `9 * 3600` | タイムゾーンのオフセット（秒）。初期値は JST（UTC+9） |
| `NTP_RESYNC_MINUTES` | `360` | NTP 再同期の間隔（分） |
| `FULL_REFRESH_INTERVAL_MINUTES` | `60` | 焼き付き防止用の全画面更新の間隔（分） |

### 画面の上下反転（`gfx.h`）

ディスプレイを上下逆向きに取り付けた場合は、`gfx.h` の `EPD_ROTATE_180` を `1` に変更して再ビルドしてください。

```c
#define EPD_ROTATE_180 1
```

## ファイル構成

```text
.
├── CMakeLists.txt   # CMake ビルド設定
├── picoclock.c      # メイン処理（RTC 管理・画面更新ループ）
├── epd_2in13.h/.c   # Waveshare Pico-ePaper-2.13 (SSD1680) ドライバ
├── gfx.h/.c         # 250×122 フレームバッファ描画・フォント
├── ntp_client.h/.c  # Wi-Fi 接続・NTP 時刻取得クライアント
├── wifi_config.h    # Wi-Fi / NTP / 更新間隔の設定
└── lwipopts.h       # lwIP ネットワークスタック設定
```

## ライセンス

このプロジェクトは **Apache License 2.0** のもとで公開されています。詳細は [LICENSE](LICENSE) をご覧ください。

なお、ビルドスクリプトに含まれる `pico_sdk_import.cmake` は Raspberry Pi (Trading) Ltd. による **BSD 3-Clause License** で提供されています。
