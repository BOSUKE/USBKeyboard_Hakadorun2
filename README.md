# USBKeyboard_Hakadorun2

同人誌「キーボード配列変換ハードウエアの作成」 (初出イベント 技術書典14) と、同人ハードウエア 「キーボード配列変換ハードウエア」のソースコードなど。

https://booth.pm/ja/items/4824592


## キーボード配列変換ハードウエア (Hakadorun2) 使い方

### 接続
1. Hakadroun2の基板上のDIPSWを ON 側 に設定してください。
2. USBキーボードを直接Hakadorun2に接続してください (間にUSBハブを入れないでください)
3. Hakadorun2をPCなどに接続してください (PCとHakadorun2の間にUSBハブを入れることはできます)

認識しない場合、または認識するがキーの反応がおかしい場合は、Hakadorn2をいったん抜いて再度PCなどに接続してみてください。
（不安定な場合、指して → 速攻で抜いて → 速攻で指すというのをやると認識するかもです）

### 設定
Hakadorun2の設定は、　Hakadorun2_Client.exe　を用いて行います。
Hakadorun2_Client.exe は リリースパッケージ (zip) ファイル内に含まれています。
Hakadorun2をPCに接続した状態で、Hakadorun2_Client.exeを以下いずれかの引数をつけて実行してください。

ツールの実行には「Visual C++ 再頒布可能パッケージ」が必要です。ツールを実行して、～.DLLが見つからないといったエラーが出た場合は、 https://learn.microsoft.com/ja-JP/cpp/windows/latest-supported-vc-redist よりVisual Studio 2015、2017、2019、および 2022のC++ 再頒布可能パッケージをダウンロードし、インストールしてください。

#### version
Hakadorun2のファームウエアバージョンを表示します。

#### update
キー配列の変更を行います。

1. "Enter the src key" と表示されたら変換対象のキーを入力してください
2. "Enter the dst key" と表示された変換先のキーを入力してください

以上により、1.で入力したキーを押下すると、代わりに2.のキーがPCなどに送られるようになります。

必要な変換の数だけ、この作業を繰り返してください。

#### reset
キー配列の変換設定をリセットします。

#### lang_ja_en / lang_en_ja / lang_none
英語キーボード / 日本語キーボードの変換設定を行います。

* lang_ja_en : USBキーボードが日本語配列 / PC(OS)の設定が英語 の場合に選択すると、日本語キーボードの刻印通りのキー入力でOSへの入力ができます
* lang_en_ja : USBキーボードが英語配列 / PC(OS)の設定が日本語 の場合に選択すると、英語キーボードの刻印通りのキー入力でOSへの入力ができます
* lang_none : 英語/日本語の切り替えを行いません。

#### macroX (X=0～3)
キーボードマクロの設定を行います。

1. "Enter keys to run macro..." と表示されたらマクロを発動させるためのキーを入力してください。複数のキーの同時押しに対応しています。複数のキーを同時におして、すべてのキーを押すのをやめると、キーが確定します。
2. "Enter key sequence for macro" と表示されたら、記録したキー操作を入力してください。5秒間入力が止まった場合、または記録上限に達した場合に記録は終了します。

以上により、1.で入力したキーを押すと、2.で行ったキー操作が実行されるようになります。

## Hakadorun2のファーム書き換え方法
### USBメモリを利用する方法

★注★
ファームバージョン 0.0.1(技術書典14で頒布した基板に適用されているバージョン) をご利用で https://github.com/BOSUKE/USBKeyboard_Hakadorun2/issues/4 の現象の発生頻度が高いHakadorun2基板の場合、USBメモリを使う方法は更新に失敗する可能性があるため非推奨です。技術書典14でHakadorun2を購入され、ファームの更新を希望の方は郵送での更新を承ります（BOSUKE宅までの送料はご負担ください）。詳細は bosuke@d-rissoku.net までお問い合わせください。

必要なもの: アクセスランプ付きのUSBメモリ

1. USBメモリ内のルートディレクトリにファームを fw.rom という名前で保存する。
2. PCなどに接続していない状態のHakadorun2にUSBメモリを差し込む。
3. Hakadorun2の基板上の DIPSW を '1' と書かれた側に設定する。
4. Hakadorun2をPCなどに接続する。
5. 数十秒間連続してUSBメモリのアクセスランプがアクセス中を示す状態になる。（この状態の期間でファームの更新が行われている)
6. 更新が終わるとUSBメモリのアクセスランプがアクセス中を示すのが約3秒おきになる。（この状態になると更新完了）
7. Hakadorun2をPCなどから取り外す
8. Hakadorun2の基板上の DIPSW を ON 側に設定する。

自分でビルドしたファームをこの方法で書き込む場合は、以下の注意点 & 手順に従ってください。

* 自分でビルドしたファームを書き込み後もUSBメモリでの書き込みを実施したい場合は、そのファームにも [FWUpdate.c](https://github.com/BOSUKE/USBKeyboard_Hakadorun2/blob/main/firmware/FWUpdate.c)相当の処理を組み込んでおく必要があります。

* Hakadorun2にはFlash ROMのアドレス 0x1F740 以降の領域にファーム更新プログラムが書き込まれています。この領域を壊した場合、ファームの書き換えにはデバッガが必要になります。カスタマイズしたファームをビルドした場合は、マップファイルを確認し、.textエリアが 0x1F740 以降の領域に被っていないことを確認してください。ここでいう更新プログラムとは、[AN_159 Vinculum-II Firmware Flash Programming](https://ftdichip.com/document/application-notes/)のReflashFATFile.romのことです。

* [FWUpdate.c](https://github.com/BOSUKE/USBKeyboard_Hakadorun2/blob/main/firmware/FWUpdate.c) は、アップデートの必要性をファームイメージ と Flash ROM それぞれの userDataArea という領域の値を比較することで判断しています。新しいファームをUSBメモリで適用する場合は、古いファームと異なる userDataArea をファームイメージに設定してください。[rename_rom.bat](https://github.com/BOSUKE/USBKeyboard_Hakadorun2/blob/main/firmware/Release/rename_rom.bat)のVinUserがuserDataAreaを設定するツールです。引数 -x 00000002C1C2C3C4C6C7C8C9CACBCDCF の 先頭部 00000002 の箇所を任意の16進数に変更してください。

### デバッガで書き込む方法

同人誌本文中でも紹介している「VNC2 DEBUG MODULE」を使う方法です。

VNC2 DEBUG MODULEのCN2とHakadorun2の未実装ピン（スルーホール）を次のように接続してください。

|DEBUG MODULEのCN2のピン|信号名|接続先Hakadorun2のピン|
|-------------------------------|-----|----------------------|
|1|Debug_IF|1 (□で囲まれていて水晶振動子に一番近いピン)|
|2|Key|接続しない|
|3|GND|4（DIPSWに一番近いピン)|
|4|RESET#|2|
|5|PROG#|3|
|6|VCC|接続しない|

デバッガとHakadorun2との接続は、秋月電子で販売している [スルーホール用テストワイヤ　ＴＰ－２００](https://akizukidenshi.com/catalog/g/gC-09830/)が便利です。
