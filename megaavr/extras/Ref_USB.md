# AVR64DU32 Curiosity Nano のネイティブ USB

初期対応は **Windows / Arduino IDE 2.x / AVR64DU32 Curiosity Nano** です。
ボード一覧の **AVR64DU32 Curiosity Nano (native USB CDC + euboot)** を選択します。
内蔵 20 MHz、TCB1 による millis()/micros()、PF2 の LED、PF6 の SW0 を使用します。
この機能は実験的です。ビルドとホスト側のテストに加え、下記の実機確認が必要です。

## できること

- 通常時は OS 標準の CDC ドライバーで仮想 COM ポートとして使用。
- IDE のシリアルモニターから SerialUSB と通信。
- IDE からの書き込み時に 1200 bps のポート開閉で euboot に移行し、更新・検証後にスケッチを起動。
- 書き込み時だけ HID / CMSIS-DAP / EDBG を使用。標準 DFU とは異なります。

Serial と Serial0 は UART0 のままです。Serial1 は Nano のデバッガーに
接続された PD6 / PD7 の UART1 に設定されます。
SERIAL_PORT_MONITOR と SERIAL_PORT_USBVIRTUAL は SerialUSB、
SERIAL_PORT_HARDWARE と SERIAL_PORT_HARDWARE_OPEN は Serial0 を指します。

## 開発版の導入

1. Arduino IDE のボードマネージャーで DxCore を導入します。
   AVR DU 対応の AVR-GCC と、DxCore 1.6.2 以降に付属する AVRDUDE 8.1 が必要です。
   USB 書き込み処理は、この AVRDUDE をバージョン付きパスで選択します。
2. このリポジトリの megaavr フォルダーを、スケッチブックの
   hardware/DxCoreUSB/megaavr にコピーします。
   DxCoreUSB という別名にすることで、導入済み DxCore と開発版を区別できます。
3. 開発版フォルダーに Python が付属しない場合は、同フォルダー内の platform.local.txt に
   次を記述します。Python 3 の実際の場所を指定してください。

   ```ini
   tools.euboot.python=C:/path/to/python.exe
   ```

4. IDE を再起動して USB 対応ボードを選択します。

ボードパッケージとして配布する場合は、既存 SerialUPDI と同じ Python ランタイムを使用します。
アップローダーに pip パッケージの追加は不要です。

## 初回のブートローダー導入

Nano にはデバッガー側とターゲット側の2つの USB コネクタがあります。
初回導入には **デバッガー側**を使います。

1. デバッガー側 USB を PC に接続します。
2. USB 対応ボードを選び、書込装置を
   **Microchip Curiosity Nano (nEDBG/ATSAMD21 debug chip)** にします。
3. 「ブートローダを書き込む」を実行します。

開発版の CLI では次と同等です。

```powershell
arduino-cli burn-bootloader --fqbn DxCoreUSB:megaavr:avrduusb --programmer nedbg
```

ブート領域は先頭の 2,560 バイト、アプリ開始は 0x0A00 です。
スケッチ上限は 62,976 バイトになります。
BOOTSIZE=5、CODESIZE=0、SYSCFG0=0xD1 とし、UPDI を維持して PF6 を入力にします。
euboot 自体の再導入・fuse の変更・UPDI からの復旧にはデバッガー側 USB を使います。

## 最初のスケッチと手動復旧

まだ CDC スケッチが入っていない場合、ターゲット側の COM ポートはありません。
次の手順で euboot を手動起動します。

1. USB ケーブルを含め、ターゲットへのすべての給電を外します。
2. SW0 を押しながら **ターゲット側 USB** を PC に接続します。
3. LED がブートローダー待機状態になったら SW0 を離します。
4. ビルド済みスケッチを、COM ポート指定なしでアップロードします。

```powershell
arduino-cli compile --fqbn DxCoreUSB:megaavr:avrduusb --output-dir ./usb-build ./SerialUSBEcho
arduino-cli upload --fqbn DxCoreUSB:megaavr:avrduusb --input-dir ./usb-build
```

IDE がポート未選択でアップロードを許可しない場合も、この CLI 経路を使用できます。
SerialUSB.end() を呼んだ場合、割り込みを停止した場合、スケッチが停止した場合も
同じ手順で復旧できます。PF6 はこの設定ではハードウェア RESET ではありません。

通常はターゲット側 COM ポートを選択し、IDE の書き込みボタンを押します。
書き込み中は CDC が消えて HID に切り替わるため、新しい COM ポートは待ちません。
更新後に CDC が再認識されたらシリアルモニターを開きます。
COM 番号が変わった場合は選び直してください。

## スケッチ API

```cpp
void setup() {
  SerialUSB.begin(); // USB ボードでは setup() 前に自動起動済み。再呼び出し可能。
}

void loop() {
  while (SerialUSB.available() && SerialUSB.availableForWrite()) {
    SerialUSB.write((uint8_t)SerialUSB.read());
  }
}
```

- begin() と begin(baud) は接続待ちで停止しません。baud は USB の転送速度を設定しません。
- bool(SerialUSB) は構成済み・非サスペンド・DTR 有効のとき true です。
  ホスト待ちが必要ならスケッチ側で明示的に行います。
- 未受信時の read() / peek() は -1、未接続・DTR 無効時の write() は 0 を返します。
- write() の送信待ちは1バイトあたり最大100 ms、flush() は最大200 msです。
  ホストが読まない場合は短い書き込みになるため、戻り値を確認してください。
- USB API は通常のスケッチ処理から使用してください。
  割り込み無効時の write() / flush() は送信待ちを行いません。
- Stream の readBytes()、readBytesUntil()、parseInt()、setTimeout() 等が使えます。
  Stream の受信タイムアウトと USB 送信待ちの上限は別です。
- USB ボードでは空スケッチでも CDC が起動するため、次回も自動書き込みできます。
- 1200 bps に設定して DTR を解除する操作は、ブートローダー起動に予約されています。
  通常のシリアルモニターは 115200 bps など別の値に設定してください。

実例は DxCore ライブラリーの USB / SerialUSBEcho と USB / SerialUSBAndUART にあります。
UART 併用例では USB と Nano デバッガーの別 COM ポートを使って双方向に通信できます。

## メモリと USB 識別子

CDC は MacroMicroAPI_lib の実装を移植し、Arduino Stream との互換性、
非同期 EP0 制御転送、有限の送信待ちを追加しています。
USB の DMA バッファは偶数アドレスに配置し、制御転送と CDC データは別の領域を使用します。
USB の処理は USB 割り込みと SOF で行い、Arduino のタイマーを追加で占有しません。

評価用の識別子は次のとおりです。

| 動作 | VID | PID |
| --- | --- | --- |
| CDC | 0x04D8 | 0x0B15 |
| euboot HID | 0x04D8 | 0x0B12 |

CDC の値はボード定義の build.vid / build.pid、
ブートローダー検索値は upload.boot_vid / upload.boot_pid にあります。
CDC 文字列は cores/dxcore/usb/USBConfig.h に集約しています。
euboot の識別子を変更する場合は、euboot の configuration.h を変更して再生成し、
検索値も一致させてください。
これらは参考実装と同じ Microchip 評価用設定です。製品配布時の独自識別子は別途用意してください。

現時点で検証対象となるのは接続されたターゲット1台です。
複数の同一 euboot HID を検出した場合、アップローダーは書き込まずに停止します。
DxCore の Flash ライブラリーによる自己書き込みは、このボードでは対応しません。

## 検証

ホスト側の回帰テスト:

```powershell
python -m unittest discover -s megaavr/tools/tests -v
```

ビルド後の HEX 範囲、ベクタ位置、ディスクリプタ、DMA 配置の検査:

```powershell
python megaavr/extras/ci/verify_usb_build.py path/to/SerialUSBEcho.ino.elf
```

アップローダー単体の --dry-run は HEX を検査し、AVRDUDE コマンドだけを表示します。
デバイスへの接続・書き込みは行いません。

実機での完了条件:

- 初回 UPDI 導入、SW0 からの初回更新、通常の IDE 書き込みを確認。
- Windows 標準 CDC ドライバーで認識し、シリアルモニターを開閉して送受信できる。
- エコーで 1 / 63 / 64 / 65 / 127 / 128 / 129 / 1024 バイトと連続データを照合。
- USB を抜き差しし、PC のスリープから復帰して再通信できる。
- シリアルモニターを閉じてから更新し、更新後のスケッチで再び通信・更新できる。
- USB 停止、割り込み停止、更新中の給電中断後に SW0 または UPDI で復旧できる。
- 更新前後でブート領域を読み出して一致を確認し、通常の更新で euboot が消えない。

ビルド確認は、USB の列挙・実転送・実機更新の確認を代替しません。
他の DU 品種、他の OS、標準 DFU、汎用 HID、リモートウェイク、低消費電力動作は後続対応です。

## 出典

- [MultiX Zinnia SDK](https://github.com/askn37/multix-zinnia-sdk-modernAVR)
- [MacroMicroAPI_lib 1804a52](https://github.com/askn37/MacroMicroAPI_lib/tree/1804a5239181f33b2f07ab7bac4c83bfc74d9c8b)
- [euboot 66cdc00](https://github.com/askn37/euboot/tree/66cdc00dac142bb1cf70f5fcbf5fbca5d45894ad)

移植元の MIT ライセンスは CDC / euboot それぞれのディレクトリに保持しています。
