# AVR DU のネイティブ USB

対応環境は **Windows / Arduino IDE 2.x** です。
Curiosity Nano と、AVR64DU32 / AVR32DU32 / AVR64DU28 / AVR32DU28 の IC 単体を対象にします。
Nano は **AVR64DU32 Curiosity Nano (native USB CDC + euboot)**、
IC 単体は **AVR DU-series (native USB CDC + euboot, standalone)** を選択します。
初期設定は内蔵 20 MHz、TCB1 による millis()/micros() です。
クロックやタイマーなどはツールメニューで変更できます。PF6 は BOOT 入力を使用します。
状態 LED は Nano が PF2、汎用版が PA7 です。
この機能は実験的です。Nano の基本通信と自動 USB 書き込みは実機確認済みです。
確認した範囲と未確認項目は「検証」を参照してください。IC 単体はビルド確認のみです。

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
   tools.serialupdi.cmd=C:/path/to/python.exe
   tools.euboot.python=C:/path/to/python.exe
   ```

4. IDE を再起動して USB 対応ボードを選択します。

ボードパッケージとして配布する場合は、既存 SerialUPDI と同じ Python ランタイムを使用します。
アップローダーに pip パッケージの追加は不要です。

## ツールメニューの設定

Nano と IC 単体の USB ボードには、次の共通メニューがあります。
既存の開発版を導入済みの場合は megaavr を再コピーし、IDE を再起動してください。
メニューが表示されない場合は、一度別のボードを選択してから USB ボードを選び直してください。
今回の Nano では、この操作で追加メニューが表示されることをユーザーが確認しています。

| メニュー | 選択肢・適用方法 |
| --- | --- |
| Clock Speed | 内蔵 12 / 16 / 20 / 24 MHz、水晶 12 / 16 / 20 / 24 MHz、外部クロック 16 / 20 / 24 MHz。スケッチの再書き込みで適用 |
| millis()/micros() timer | TCB1（初期値）、TCB0、TCA0、無効。スケッチの再書き込みで適用 |
| WDT Timeout / WDT "window" (sketch, before setup) | 無効、62.5 / 125 / 250 / 500 ms、1 / 2 / 4 / 8 秒。スケッチの再書き込みで適用 |
| BOD level / BOD Mode | 元の DU ボードと同じ電圧・動作モード。「ブートローダを書き込む」で適用 |
| Save EEPROM | EEPROM 保持（初期値）/ 非保持。「ブートローダを書き込む」で適用 |
| Startup Time | 0（初期値）/ 1 / 2 / 4 / 8 / 16 / 32 / 64 ms。「ブートローダを書き込む」で適用 |
| printf() / attachInterrupt() Version / Wire Library mode | 元の DU ボードと同じ選択肢。スケッチの再書き込みで適用 |
| How to set FLMAP | 64 KB 品種で section 1（初期値）/ section 0 のロック、またはロック解除。32 KB 品種では効果なし |

USB には 12 MHz 以上の周辺クロックが必要です。外部クロック・水晶を選ぶ場合は、
実際にその周波数の発振源を接続してください。Nano でも外部発振源の追加が必要です。
euboot 自体は、スケッチの選択にかかわらず内蔵 20 MHz で動作します。
PF6 の BOOT 入力、UPDI、ブート領域の予約は維持します。
USB 版ではリセット端子の切り替えと Flash ライブラリーの自己書き込み設定は提供しません。

WDT は USB 初期化後、`setup()` を呼ぶ直前にソフトウェアで有効にします。
Timeout が無効なら Window の選択にかかわらず WDT は無効です。
スケッチ側で `<avr/wdt.h>` を読み込んで `wdt_reset()` を呼び、
ウィンドウ使用時は閉じている期間を避けてください。
コアによる自動クリアは行わず、WDT 設定のロックも行いません。
この設定は `setup()` / `loop()` の監視用であり、それ以前の初期化は監視しません。

元の DxCore のヒューズ方式と異なり、USB 版の WDT ヒューズは無効のままにします。
ヒューズで WDT を有効にするとリセット直後に設定がロックされるため、短いタイムアウトや
閉じたウィンドウが euboot の待機・書き込み処理を中断するのを避けるためです。
ソフトウェアリセット後もまずヒューズの無効設定に戻り、euboot は WDT を自身で管理します。
ヒューズ方式が必要な場合は既存の **AVR DU-series (no bootloader)** を使用してください。
仕様は [AVR64DU28/32 データシートの CLKCTRL / WDT / USB の各章](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/AVR64DU-28-32-Prelim-DataSheet-DS40002548.pdf)を参照してください。

USB での通常のスケッチ更新は EEPROM を消去しません。Save EEPROM は UPDI での
チップ消去時の保持設定です。BOD・EEPROM・起動時間を変更するときは、
Nano のデバッガー側または外部 UPDI 書き込み器からブートローダーを再書き込みしてください。
追加メニューはビルド・設定値の検査対象です。各設定での実機動作は未確認です。

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
BOOTSIZE=5、CODESIZE=0、初期設定の SYSCFG0=0xD1 とし、UPDI を維持して PF6 を入力にします。
EEPROM 非保持を選んだ場合の SYSCFG0 は 0xD0 です。
euboot 自体の再導入・fuse の変更・UPDI からの復旧にはデバッガー側 USB を使います。

Nano 用の配布 HEX は hex/nano 以下のローカル再生成版です。
USB RESUME を切断と判断して再起動する処理を削除し、Windows の HID 休止復帰でも
待機状態を保つよう変更しています。元の HEX は比較用として保持しています。
旧版を導入済みの場合は、この変更を適用するためにブートローダーを再書き込みします。
この版で Windows 実機の USB 書き込み・CDC 通信・自動書き込みを確認しています。
休止復帰そのものの試験と、各変更が初期の切断症状に与えた影響の分離は未実施です。

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

デバッガー側の COM ポートは UART 用です。1200 bps による自動移行には、
ターゲット側 USB の SerialUSB が提供する COM ポートを選択してください。
euboot の手動待機中は HID として認識され、COM ポートは現れません。
アップローダーは一時的な AVRDUDE 追加設定で USB VID/PID を指定します。
AVRDUDE 8.1 は usb:VID:PID の HID オープンに失敗すると、同じ文字列を
シリアル番号として再解釈して既定のデバイス ID を探すため、エラーが紛らわしくなります。
ポートは usb とし、jtag3updi を継承した専用設定を使用して、この再解釈を避けます。

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

2026-11-01、ユーザーの AVR64DU32 Curiosity Nano 1台で、Windows / Arduino IDE 2.x
による次の動作を確認しました。書き込み部分は提示された AVRDUDE ログ、
エコー・LED・自動書き込みはユーザーの実機操作報告に基づきます。

- 内蔵デバッガーから fuse 設定と修正版 euboot の 2,400 バイトを書き込み・検証。
- SW0 を押した電源投入で euboot を起動し、TARGET 側 USB から通常の IDE 書き込みに成功。
- AVRDUDE 8.1 / dxcore_euboot で SerialUSBEcho の 4,066 バイトを
  0x0A00..0x19E1 に書き込み・検証。
- スケッチ起動後に CDC の COM ポートが出現。シリアルモニターで送信文字が返り、
  改行で PF2 の LED が切り替わることを確認。
- USB を接続したまま、SW0 を押さずに通常の IDE 書き込みに成功。
  この操作では TARGET 側 CDC のポートを選択。

ホスト側は Python 3.15.1 を使用。ブートローダー導入時の AVRDUDE は 8.0-arduino.1、
USB スケッチ書き込みは DxCore 付属の 8.1 です。
実機は AVR64DU32 revision 1.3、内蔵デバッガー FW 1.34 (rel. 89)、約5 Vで動作しました。
この結果は、他の品種・電源方式・OS や、下記の耐久・復旧試験まで保証するものではありません。

ホスト側の回帰テスト:

追加メニューは、Nano の全 11 クロック設定、単体 4 品種 × 2 USB 電源方式、
Nano 初期設定、従来の USB なし DU ボードの計 21 ビルドで確認しました。
USB ビルドでは HEX のアプリ領域、ベクタ位置、CDC ディスクリプタ、DMA 配置も検査しています。
WDT・タイマー・printf・割り込み・Wire・FLMAP の代替設定を含みます。
ホスト側の回帰テストは 28 件です。追加設定の実機動作は未確認です。

```powershell
python -m unittest discover -s megaavr/tools/tests -v
```

ビルド後の HEX 範囲、ベクタ位置、ディスクリプタ、DMA 配置の検査:

```powershell
python megaavr/extras/ci/verify_usb_build.py path/to/SerialUSBEcho.ino.elf
```

アップローダー単体の --dry-run は HEX を検査し、AVRDUDE コマンドだけを表示します。
デバイスへの接続・書き込みは行いません。

実機での完了条件（上記の基本動作以外は未確認）:

- 初回 UPDI 導入、SW0 からの初回更新、通常の IDE 書き込みを確認。
- Windows 標準 CDC ドライバーで認識し、シリアルモニターを開閉して送受信できる。
- エコーで 1 / 63 / 64 / 65 / 127 / 128 / 129 / 1024 バイトと連続データを照合。
- USB を抜き差しし、PC のスリープから復帰して再通信できる。
- シリアルモニターを閉じてから更新し、更新後のスケッチで再び通信・更新できる。
- USB 停止、割り込み停止、更新中の給電中断後に SW0 または UPDI で復旧できる。
- 更新前後でブート領域を読み出して一致を確認し、通常の更新で euboot が消えない。

ビルド確認は、USB の列挙・実転送・実機更新の確認を代替しません。
AVR16DU、14/20 ピン品種、他の OS、標準 DFU、汎用 HID、リモートウェイク、
低消費電力動作は後続対応です。

## IC 単体 / 汎用ボード

ボード一覧で **AVR DU-series (native USB CDC + euboot, standalone)** を選びます。
Chip メニューで実際の型番を、USB power メニューで回路に合う電源方式を選択します。
各設定には専用のブートローダー HEX を用意しています。実機での確認は未実施です。

| Chip | スケッチ容量 | SRAM | ピン定義 |
| --- | ---: | ---: | --- |
| AVR64DU32 | 62,976 B | 8,192 B | 32pin-duseries |
| AVR32DU32 | 30,208 B | 4,096 B | 32pin-duseries |
| AVR64DU28 | 62,976 B | 8,192 B | 28pin-duseries |
| AVR32DU28 | 30,208 B | 4,096 B | 28pin-duseries |

初期設定は 20 MHz 内蔵クロック、TCB1 による millis()/micros() です。
64 KB 品種は初期設定で FLMAP section 1 をロック、32 KB 品種は既定のマッピングを使用します。
共通のツールメニューで変更できます。
SerialUSB / Serial / Serial0 / Serial1 の API は Nano 版と共通です。

### USB 電源の選択

| USB power | 回路の前提 | ブートローダーとスケッチの動作 |
| --- | --- | --- |
| 5 V VDD, internal USB regulator | VDD に 5 V、VUSB は内蔵レギュレータの出力 | USBVREG=1、起動時間 0 ms で SYSCFG1=0x08 |
| 3.3 V VUSB externally supplied | VDD と VUSB に外部レギュレータから 3.3 V を供給 | USBVREG=0、起動時間 0 ms で SYSCFG1=0x00 |

初期の汎用構成は **USB VBUS から給電するバスパワー回路**を前提にします。
3.3 V 方式では、USB VBUS を外部レギュレータで 3.3 V に変換して供給してください。
USB を外しても他の電源から MCU が動く自己給電・二重給電回路の VBUS 検出は今回の対象外です。

電源方式を変更した場合は、スケッチだけでなく **ブートローダーを再書き込み**してください。
選択した方式をブート時とスケッチ実行時で一致させる必要があります。
外部 3.3 V を与える構成では内蔵レギュレータを無効にします。
VUSB に USB VBUS の 5 V を直接接続しないでください。
電源の立ち上がり速度、VDD/VUSB のデカップリング、USB コネクタ周辺回路は
対象品種のデータシートに従って設計してください。

参考: [AVR64DU28/32 データシート・電源構成](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/AVR64DU-28-32-Prelim-DataSheet-DS40002548.pdf)、
[AVR32DU28 のデータシート](https://www.microchip.com/en-us/product/AVR32DU28)。
IC のパッケージごとに端子番号が異なるため、以下は信号名で示します。

| 信号 | 用途 |
| --- | --- |
| DP / DM | USB コネクタの D+ / D- |
| VDD / GND / VUSB | 選択した方式に合う電源とデカップリング |
| PF7 / UPDI | 初回導入・復旧用の UPDI 接続を保持 |
| PF6 | BOOT ボタンを GND へ接続。内蔵プルアップで起動時に判定 |
| PA7 | ブート状態 LED。LED は抵抗を介して GND へ接続、外付け LED の省略も可能 |

汎用版 euboot は PA7 を起動時の状態表示に使用します。
他の回路を接続する場合は、この起動時の出力を考慮してください。
PF6 は GPIO 入力として使うため、この設定ではハードウェア RESET 端子ではありません。

### 初回導入と USB 更新

IC 単体には Nano の内蔵デバッガーがないため、外部の UPDI 書き込み器が必要です。
SerialUPDI、PICkit、Atmel-ICE など、実際に使用する書き込み器を IDE の
「ツール → 書込装置」で選択します。書き込み器の電圧は対象の VDD に合わせます。

1. UPDI、GND、および必要な電源を接続します。
2. 汎用 USB ボード、Chip、USB power を選択します。
3. 「ブートローダを書き込む」を実行します。既存のスケッチは消去されます。
4. すべての給電を外し、PF6 の BOOT ボタンを押しながら USB を接続します。
5. 最初の USB スケッチをアップロードします。

たとえば AVR32DU28 / 外部 3.3 V / SerialUPDI の場合:

```powershell
arduino-cli burn-bootloader --fqbn DxCoreUSB:megaavr:avrduusb_generic:chip=avr32du28,usbpower=external --programmer serialupdi --port COM5
arduino-cli compile --fqbn DxCoreUSB:megaavr:avrduusb_generic:chip=avr32du28,usbpower=external --output-dir ./usb-build ./SerialUSBEcho
arduino-cli upload --fqbn DxCoreUSB:megaavr:avrduusb_generic:chip=avr32du28,usbpower=external --input-dir ./usb-build
```

COM5 は SerialUPDI アダプターのポート例です。
初回の USB アップロードには COM ポートが存在しなくても構いません。
USB スケッチが動作して CDC が認識された後は、通常の自動書き込みを使えます。

32 KB 品種では SRAM が 0x7000..0x7FFF、64 KB 品種では 0x6000..0x7FFF にあります。
末尾の起動マーカーは共通でも、ブートローダーの作業領域の開始位置が異なります。
Nano / 64 KB 用 HEX を32 KB 品種に流用せず、Chip と電源選択に対応する HEX を使用します。
アップローダーも型番ごとのフラッシュ上限を検査し、32 KB 品種への範囲外書き込みを拒否します。

### ブートローダーの再生成

通常の利用では配布済み HEX を使います。再生成が必要な場合:

```powershell
python megaavr/bootloaders/euboot/build_generic.py --toolchain C:/path/to/avr-gcc/bin --output ./boot-build
```

4品種×2電源方式を生成し、先頭の起動/SPM コード、フラッシュサイズ、
初期化不要の RAM 作業領域、SRAM 内の配置を検査します。
検査後の HEX をリポジトリ内の hex/generic に保存する場合だけ --write-assets を付けます。
実機への書き込みや外部公開は行いません。
生成条件・ソース・コンパイラ・デバイスヘッダー・イメージのハッシュは manifest.json に記録します。

## 出典

- [MultiX Zinnia SDK](https://github.com/askn37/multix-zinnia-sdk-modernAVR)
- [MacroMicroAPI_lib 1804a52](https://github.com/askn37/MacroMicroAPI_lib/tree/1804a5239181f33b2f07ab7bac4c83bfc74d9c8b)
- [euboot 66cdc00](https://github.com/askn37/euboot/tree/66cdc00dac142bb1cf70f5fcbf5fbca5d45894ad)

移植元の MIT ライセンスは CDC / euboot それぞれのディレクトリに保持しています。
