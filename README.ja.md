# PicoClock

[English](README.md) | 日本語

Raspberry Pi Pico W と Waveshare 2.13 インチ e-Paper ディスプレイを組み合わせた、Wi-Fi NTP デジタル時計です。

起動時に Wi-Fi 経由で NTP サーバーから現在時刻を取得し、以降は1分ごとに画面を更新します。

## 主な機能

- **Wi-Fi + NTP 時刻同期**: 起動時および一定間隔（デフォルト 6時間ごと）で NTP サーバーと時刻を同期します。同期完了後は Wi-Fi を切断します。
- **見やすい横向きレイアウト（250×122）**:
  - 日付: 左上に西暦で日付を表示(`yyyy/mm/dd` 形式)
  - 時刻: 7セグ風フォントで時刻を表示(`HH:MM` 形式)
- **低チラつき・焼き付き対策**:
  - 毎分の更新には e-Paper ディスプレイの **部分更新** を使用し、画面反転のチラつきを抑えます。
  - 焼き付き防止のため、毎時0分（デフォルト 60分間隔、30分に設定すると毎時0分・30分）に **全画面更新** を行います。
  - 更新後はディスプレイをディープスリープ状態にします。
- **省電力動作**:
  - 未使用の USB / ADC クロックおよび `PLL_USB` を停止しています（USB / UART stdio 無効）。
  - 通常時は 12 MHz XOSC（`PLL_SYS` 停止）で動作し、Wi-Fi / NTP 通信時のみ 125 MHz に切り替えます。
  - 毎分の更新待ちや e-Paper の処理待ち中は RP2040 をディープスリープさせて待機します。

## 必要なハードウェア

- **Raspberry Pi Pico W**
- **Waveshare Pico-ePaper-2.13**（V4 / SSD1680、250×122、白黒、HAT 型）

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

`src/app_config.h` を開き、Wi-Fi の SSID とパスワードを設定してください。

```c
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

> **注意**: ソースコードを GitHub 等に公開する場合は、実際の SSID やパスワードを記載した `src/app_config.h` をコミットしないようご注意ください。

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

### 設定変更（`src/app_config.h`）

| マクロ名 | デフォルト値 | 説明 |
| :--- | :--- | :--- |
| `WIFI_AUTH` | `CYW43_AUTH_WPA2_AES_PSK` | Wi-Fi の認証方式 |
| `NTP_SERVER` | `"ntp.nict.jp"` | 参照する NTP サーバー |
| `NTP_PORT` | `123` | NTP サーバーのポート番号 |
| `TIMEZONE_OFFSET_S` | `9 * 3600` | タイムゾーンのオフセット（秒）。初期値は JST（UTC+9） |
| `NTP_RESYNC_MINUTES` | `360` | NTP 再同期の間隔（分） |
| `NTP_RESYNC_RETRY_MINUTES` | `15` | NTP 再同期失敗時のリトライ間隔（分） |
| `FULL_REFRESH_INTERVAL_MINUTES` | `60` | 毎時0分を基準とした全画面更新の間隔（分、60の約数を指定。例: `60`=毎時0分、`30`=毎時0分・30分） |
| `EPD_ROTATE_180` | `0` | ディスプレイを上下逆向きに取り付ける場合は `1` に設定 |

タイムアウトやリトライ設定（`WIFI_CONNECT_TIMEOUT_MS`、`NTP_TIMEOUT_MS`、`NTP_INITIAL_RETRY_DELAY_MS`、`EPD_BUSY_TIMEOUT_MS`、`EPD_SPI_BAUDRATE` 等）も `src/app_config.h` に集約されており、コンパイラの `-D` オプションで上書き可能です。

### デバッグ用に UART / USB stdio を有効にしてビルドする

省電力のため、デフォルトでは UART / USB の stdio 出力は無効になっています。`printf` によるデバッグログを確認したい場合は、`CMakeLists.txt` で UART または USB stdio を有効にしてビルドしてください。

#### 方法 A: UART stdio（省電力動作を含めて検証したい場合におすすめ）

1. `CMakeLists.txt` を開き、`pico_enable_stdio_uart` を `1` に変更します。

   ```cmake
   pico_enable_stdio_uart(picoclock 1)
   pico_enable_stdio_usb(picoclock 0)
   ```

2. 再ビルドして生成された `build/picoclock.uf2` を Pico W に書き込みます。

   ```powershell
   cmake -B build -G Ninja
   cmake --build build
   ```

3. USB-UART 変換アダプタ（3.3 V ロジック）をデフォルトの UART0 ピン（`GP0` = TX、`GP1` = RX、`GND`）に接続し、シリアルターミナルを **115200 bps（8N1）** で開きます。

#### 方法 B: USB stdio（追加配線なしで手軽に確認したい場合）

1. `CMakeLists.txt` を開き、`pico_enable_stdio_usb` を `1` に変更します。

   ```cmake
   pico_enable_stdio_uart(picoclock 0)
   pico_enable_stdio_usb(picoclock 1)
   ```

2. 再ビルドして生成された `build/picoclock.uf2` を Pico W に書き込みます。

   ```powershell
   cmake -B build -G Ninja
   cmake --build build
   ```

3. Pico W を PC に USB 接続し、認識された仮想シリアルポート（Windows では `COMx`、Linux では `/dev/ttyACM0`、macOS では `/dev/cu.usbmodem*` 等）をシリアルターミナルで開きます。

> **注意**:
> - `GP0` / `GP1` は e-Paper HAT の使用ピン（`GP8`〜`GP13`）と競合しません。
> - **UART stdio** 有効時は、ディープスリープを維持したまま 12 MHz / 125 MHz のクロック切替時およびスリープ前に `power_mgmt` が自動で送信完了待ちとボーレート再設定を行います。
> - **USB stdio** 有効時は、USB CDC 接続を維持するため `power_mgmt` が自動で `PLL_USB` / `clk_usb` の停止とディープスリープ時のクロックゲーティングを無効化し、通常の WFI 待機になります。
> - デバッグ完了後は省電力動作に戻すため、`pico_enable_stdio_uart` / `pico_enable_stdio_usb` ともに `0` に戻してください。

## ファイル構成

```text
.
├── CMakeLists.txt          # CMake ビルド設定
├── pico_sdk_import.cmake   # Pico SDK インポートスクリプト
├── lwipopts.h              # lwIP ネットワークスタック設定
├── LICENSE                 # Apache License 2.0
└── src/                    # アプリケーションのソース
    ├── main.c              # メイン処理（RTC 管理・画面更新ループ）
    ├── app_config.h        # Wi-Fi / NTP / 更新間隔の設定
    ├── epd_ssd1680.h/.c    # Waveshare Pico-ePaper-2.13 (SSD1680) ドライバ
    ├── graphics.h/.c       # 250×122 フレームバッファ描画・フォント
    ├── ntp_client.h/.c     # Wi-Fi 接続・NTP 時刻取得クライアント
    └── power_mgmt.h/.c     # クロック制御・ディープスリープ省電力管理
```

## ライセンス

このプロジェクトは **Apache License 2.0** のもとで公開されています。詳細は [LICENSE](LICENSE) をご覧ください。

なお、ビルドスクリプトに含まれる `pico_sdk_import.cmake` は Raspberry Pi (Trading) Ltd. による **BSD 3-Clause License** で提供されています。
