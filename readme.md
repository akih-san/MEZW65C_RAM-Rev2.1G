# MEZW65C_RAM-Rev2.1G

![](photo/R1.6.JPG)<br>
（MEZW65C_RAM Rev1.6メザニンボード）
<br>

## MEZW65C_RAM-Rev2.1（PCB Rev1.3)からの改良点<br>
<br>
１．CPUの高速化<br>
２．CPLD(GAL)導入によるPCBの部品点数を削減<br>
３．ファームウェアによる6502、65816の自動判定<br>

#### １．CPUの高速化(10MHzでの動作）
Rev2.1(PCB Rev1.3)は、6502(8MHz)、65816(6MHz)で動作しています。<br>
しかし、W65C02S6TPG-14とW65C816S6TPG-14を定格のは14MHzです。<br>
使用しているSRAM(AS6C4008-55PCN)のアクセスタイムは55nsですから、<br>
もう少し早く動かすことが出来るはずです。<br>
色々調べた結果、少し回路を修正すれば出来そうです。<br>
その為には、標準ロジックICを追加する必要がありますが、<br>
PCB Rev1.3には面積的にICを追加することは不可能な状況でした。<br>
<br>
今回、CPLDを導入し、部品点数を減らしたPCBを作成することで、<br>
6502、65816を共に10MHzで動作することが出来るようになります。<br>
  
<br>
＜余談＞<br>
14MHzで動かすには、もっと高速なSRAMが必要です。<br>
X(旧Twitter)の@DragonBallEZさんが、MH678127UHJ-12の高速SRAMを<br>
AS6C1008のピン配置に変換するアダプタ「Little Demon1 Rev0.3」を<br>

を作成しているので、 それを使って実験したところ、[14MHzで動作](photo/opening14MHz.png)することが<br>
確認出来ました。[ASCIIARTの結果](photo/asciiart.png)<br>
（MH678127UHJ-12は秋月電子通商での販売が終了しており、入手困難です）<br>

![](photo/LittleDemon1.jpg)<br>
（Little Demon1 Rev0.3）
<br>

#### ２．CPLD(GAL)導入によるPCBの部品点数を削減
Glue logicは、CPLDを使えば複数の標準ロジックICを、規模にもよりますが<br>
１つにまとめることが可能です。<br>
PCB Rev1.3には３つの標準ロジックICがあります。追加のGlue logicと<br>
３つの標準ロジックICを、１つの22V10タイプのCPLDにまとめることが出来ました。<br>
CPLDは、私自信にとって初めての挑戦でしたが、うまくいきました。<br>

![](photo/CPLD_GAL.JPG)<br>
（今回使用したCPLDとGAL）
<br>

#### ３．ファームウェアによる6502、65816の自動判定<br>
Rev2.1(PCB Rev1.3)は、切替スイッチを使って手動でCPUの種類を切り替えていました。<br>
Rev1.2G(PCB Rev1.6)では、ファームウェアによってCPUを自動判定します。<br>
その為、切り替えスイッチが無くなっています。<br>
<br>
CPLDの導入と、ファームウェアによるCPUの自動判定により、PCBの部品点数が削減されて<br>
スッキリとなりました。<br>
CPUの抜き差しをしやすいように、LEDの配置を変更しています。<br>
部品点数が減ったため、SRAMやPLDの抜き差しも楽になりました。<br>

# CPLDへのプログラミングと書き込み
CPLDの開発環境は、マイクロチップ社がPLD Design Resourcesとして[WinCUPL](https://www.microchip.com/en-us/products/fpgas-and-plds/spld-cplds/pld-design-resources)を提供しています。<br>
プログラミング自体は、慣れたテキストエディタを使えます。コンパイルとコード生成（jedファイル）<br>
をWinCUPLで行います。<br>
WinCUPLは少し古いソフトですが、Windows11でも動作しています。ただし、インストールすると<br>
既存の環境変数を書き換えてしまうので、既存の環境変数をバックアップしておく必要があります。<br>
そうしないと、既存のソフトウェアが立ち上がらなくなったりする不具合が発生するので、<br>
注意が必要です。<br>
CPLDのプログラミングについては[ここ](https://satoru8765.hatenablog.com/entry/2024/09/16/174243)が参考になると思います。<br>
<br>
 - CPLDへの書き込み<br>

   CPLDには、ROMライタを使用してMEZW65C_RAM.jedファイルを書込みます。<br>
   使用したのは、XGecu Programmer Model TL866Ⅱ PLUSです。<br>
   少し古いですが、問題なく書き込みが出来ました。XGecu Official Siteは[こちら](https://xgecu.myshopify.com/)<br>

   ![](photo/ROM_WRITER.JPG)<br>
（今回使用したROMライタ）

# PIC18F47Q43/Q84/Q83への書き込み
PICへの書き込みツールを用いて、ヘキサファイルを書き込みます。<br>
<br>
- PIC18F47Q43 - R2.1GQ43.hex
- PIC18F47Q84 - R2.1GQ84.hex
- PIC18F47Q83 - R2.1GQ83.hex
<br>
＜注意点＞<br>
EMUZ80ボードから、メザニンボード（MEZW65C_RAM Rev2.1G）を外した状態で<br>
PICへの書き込みを行います。メザニンボードを装着しての書き込みは、失敗が<br>
多いです。<br>
もしくは、PICに書き込めるライターを使用します。<br>
<br>
- snap(マイクロチップ社の書き込みツール)<br>

  - [snap](https://www.microchip.com/en-us/development-tool/PG164100)

<br>
- PICkit3（または互換ツール）<br><br>
  PICkitminus書き込みソフトを用いて、書き込むことが出来ます。以下で入手できます。<br>

  - [PICkitminus](http://kair.us/projects/pickitminus/)
<br>

# μSDカードの作成
[MEZW65C_RAM Rev2.1のDISKSディレクトリの中身全部](https://github.com/akih-san/MEZW65C_RAM-Rev2.1/tree/main/DISKS)のファイルをμSDカードにコピーします。<br>
6502/65816を10MHzで動かすため、[コンフィグファイル](DISKS)を上書きします。<br>
<br>

# その他
詳細については、<br>
  - [MEZW65C_RAM Rev2.0](https://github.com/akih-san/MEZW65C_RAM-Rev2.0)<br>
  - [MEZW65C_RAM Rev2.1](https://github.com/akih-san/MEZW65C_RAM-Rev2.1)<br>
<br>
を参照してください。<br>

# 参考

＜EMUZ80＞<br>
EUMZ80はZ80CPUとPIC18F47Q43のDIP40ピンIC2つで構成されるシンプルなコンピュータです。<br>
（電脳伝説 - EMUZ80が完成）  <br>
https://vintagechips.wordpress.com/2022/03/05/emuz80_reference  <br>
<br>
＜SuperMEZ80＞<br>
SuperMEZ80は、EMUZ80にSRAMを追加しZ80をノーウェイトで動かすことができます。<br>
https://github.com/satoshiokue/SuperMEZ80<br>
<br>
＜＠hanyazouさんのソース＞<br>
https://github.com/hanyazou/SuperMEZ80/tree/mez80ram-cpm<br>
<br>
＜@electrelicさんのユニバーサルモニタ＞<br>
https://electrelic.com/electrelic/node/1317<br>

＜オレンジピコショップ＞  <br>
オレンジピコさんでEMUZ80、その他メザニンボードの購入できます。<br>
<br>
https://store.shopping.yahoo.co.jp/orangepicoshop/pico-a-051.html<br>
https://store.shopping.yahoo.co.jp/orangepicoshop/pico-a-061.html<br>
https://store.shopping.yahoo.co.jp/orangepicoshop/pico-a-062.html<br>
https://store.shopping.yahoo.co.jp/orangepicoshop/pico-a-079.html<br>
https://store.shopping.yahoo.co.jp/orangepicoshop/pico-a-087.html<br>
https://store.shopping.yahoo.co.jp/orangepicoshop/pico-a-089.html<br>

