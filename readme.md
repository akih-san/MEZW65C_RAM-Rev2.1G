
# MEZW65C_RAM-Rev2.1G

工事中( - Under construction - )

<br>
MEZW65C_RAM-Rev2.1（PCB Rev1.3)からの改良点<br>
<br>
１．CPUの高速化<br>
２．CPLD(GAL)導入によるPCBの部品点数を削減<br>
３．ファームウェアによる6502、65816の自動判定<br>

# １．CPUの高速化
<br>
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
（余談）<br>
14MHzで動かすには、もっと高速なSRAMが必要です。<br>
X(旧Twitter)の@DragonBallEZさんが、MH678127UHJ-12の高速SRAM<br>
（以前は秋月電子通商さんで入手出来ましたが、販売終了で入手は困難です）<br>
をAS6C1008のピン配置に変換するアダプタ
[Little Demon1 Rev0.3](https://github.com/akih-san/MEZW65C_RAM-Rev2.1G/blob/main/photo/LittleDemon1.jpg)
を作成しているので、それを使って実験したところ、14MHzで動作することが<br>
確認出来ました。<br>
<br>

# ２．CPLD(GAL)導入によるPCBの部品点数を削減
<br>
Glue logicは、CPLDを使えば複数の標準ロジックICを、規模にもよりますが<br>
１つにまとめることが可能です。<br>
PCB Rev1.3には３つの標準ロジックICがあります。追加のGlue logicと<br>
３つの標準ロジックICを、１つの22V10タイプのCPLDにまとめることが出来ました。<br>
CPLDは、私自信にとって初めての挑戦でしたが、うまくいきました。<br>

# ３．ファームウェアによる6502、65816の自動判定<br>
<br>
Rev2.1(PCB Rev1.3)は、切替スイッチを使って手動でCPUの種類を切り替えていました。<br>
Rev1.2G(PCB Rev1.6)では、ファームウェアによってCPUを自動判定します。<br>
その為、切り替えスイッチが無くなっています。<br>
<br>
CPLDの導入と、ファームウェアによるCPUの自動判定により、PCBの部品点数が削減されて<br>
スッキリとなりました。<br>
CPUの抜き差しをしやすいように、LEDの配置を変更しています。<br>
部品点数が減ったため、SRAMやPLDの抜き差しも楽になりました。<br>

