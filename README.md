# ステッピングモータでMIDIを再生できるやつ

stepping-motor-midi-player

USB MIDIで受け取った音を、ステッピングモーターに実際に鳴かせて音にするMIDIシンセサイザー。

**Languages:** [日本語](#日本語) | [English](#english) | [中文](#中文) | [한국어](#한국어)

---

## 日本語

### これは何?

STEPピンをMIDIノートの音程とぴったり同じ周波数でパカパカ切り替えると、ステッピングモーターがその音程で鳴いてくれる(singing stepperってやつ)。この現象を使って、A4988ドライバ+モーター1組を1音分の音源として、DAWやシーケンサーからMIDIで弾けるようにしたもの。各チャンネルにArduinoを1枚使うので、マスター機とかは無し。全部のArduinoが個別にUSB MIDI機器として認識される。

### 中身

- **`firmware/`** — Arduino(Pro Micro互換ボード)に書き込むスケッチ。
  - `stp_motr_channel_velocity/` → こっちが最終版。音程だけじゃなくベロシティ(強弱)もマイクロステップの粗さで表現できる。
  - `stp_motr_channel/` → ベロシティは見てない、シンプルな方。
  - 全チャンネル、同じスケッチを書き込むだけでOK。
- **`hardware/`** — 本番基板(2ch、100×50mm)関連を全部入れてある。
  - **`stp-motr-2ch-100x50-gerbers.zip`** → 製造用データ一式。KiCadすら開かずに、これをJLCPCBとかにそのままアップロードすれば発注できる。
  - `gerbers/` → 上のZIPを展開したやつ。個別に見たい時用。
  - `stp-motr-2ch-100x50.kicad_pro` / `.kicad_sch` / `.kicad_pcb` → 編集・確認したい人向けのKiCadデータ。自作フットプリント/シンボルも `stp_motr.pretty/` `stp_motr.kicad_sym` に同封してるので、これだけで開ける。
  - **`BOM.csv`** → 部品リスト。
  - **`digikey_order.csv`** → Digikeyにそのままインポートできる注文リスト。

### 用意するもの

- Arduino Pro Micro互換ボード(ATmega32U4, 5V/16MHz)をチャンネル数分
- Pololu A4988(ヒートシンク付きがおすすめ)をチャンネル数分
- NEMA17クラスのステッピングモーターをチャンネル数分
- 細かい部品は `hardware/BOM.csv` 見てください

### 基板の発注

1. `hardware/stp-motr-2ch-100x50-gerbers.zip` をダウンロード
2. JLCPCBとかPCBWayにそのままアップロード
3. 枚数と色選んで注文するだけ

### 部品の発注

- 全部 `hardware/BOM.csv` に書いてある
- Digikeyで買うなら `hardware/digikey_order.csv` をBOM Managerに突っ込むだけ

### ライセンス

MIT License。`LICENSE` 見てください。

---

## English

### What is this?

If you toggle an A4988's STEP pin at exactly the frequency of a MIDI note's pitch, the stepper motor it's driving will "sing" that pitch — people call this the "singing stepper" trick. This project turns one motor + A4988 pair into a single-voice synth channel, playable live from a DAW or sequencer over USB MIDI. One Arduino per channel, no master board — each one just shows up as its own USB MIDI device.

### What's in here

- **`firmware/`** — sketches for Pro Micro-compatible Arduinos.
  - `stp_motr_channel_velocity/` — the final one. Covers velocity too, not just pitch, by varying how coarse the microstepping is.
  - `stp_motr_channel/` — simpler, pitch-only version.
  - Just flash the same sketch to every channel's board, that's it.
- **`hardware/`** — everything for the actual board (2-channel, 100×50mm).
  - **`stp-motr-2ch-100x50-gerbers.zip`** — the manufacturing files. Upload this straight to JLCPCB or wherever, no need to even open KiCad.
  - `gerbers/` — same thing, already unzipped, if you want to poke at individual files.
  - `stp-motr-2ch-100x50.kicad_pro` / `.kicad_sch` / `.kicad_pcb` — the actual KiCad files if you want to look around or make changes. The custom footprints/symbols are bundled in `stp_motr.pretty/` and `stp_motr.kicad_sym` so it just opens.
  - **`BOM.csv`** — the parts list.
  - **`digikey_order.csv`** — drop this straight into Digikey's order tool.

### What you'll need

- One Arduino Pro Micro-compatible board (ATmega32U4, 5V/16MHz) per channel
- One Pololu A4988 (heatsink recommended) per channel
- One NEMA17-class stepper per channel
- Everything else is in `hardware/BOM.csv`

### Ordering the board

1. Grab `hardware/stp-motr-2ch-100x50-gerbers.zip`
2. Upload it to JLCPCB, PCBWay, whatever you use
3. Pick a quantity/color and you're done

### Ordering parts

- It's all in `hardware/BOM.csv`
- Digikey people: just import `hardware/digikey_order.csv` into their BOM Manager

### License

MIT — see `LICENSE`.

---

## 中文

### 这是啥

如果让A4988的STEP引脚正好按某个MIDI音高对应的频率来回切换,它驱动的步进电机就会跟着"唱"出那个音高——大家管这叫"会唱歌的步进电机"。这个项目就是靠这招,把一个电机+A4988凑成一路单音音源,接上USB MIDI就能直接用DAW或音序器弹。每个声部一块Arduino,没有主控这种东西,每块板子自己就是一个独立的USB MIDI设备。

### 里面都有什么

- **`firmware/`** — 刷到Pro Micro兼容板上的程序。
  - `stp_motr_channel_velocity/` — 这是最终版,不只管音高,力度也靠微步细不细来表现。
  - `stp_motr_channel/` — 简化版,没管力度。
  - 每个声部的板子刷同一份程序就行。
- **`hardware/`** — 实际用的那块板子(2通道、100×50mm)的所有东西。
  - **`stp-motr-2ch-100x50-gerbers.zip`** — 生产用的文件,直接传到JLCPCB之类的厂家就能下单,KiCad都不用打开。
  - `gerbers/` — 同样的东西,解压好了,想单独看哪个文件就去这里。
  - `stp-motr-2ch-100x50.kicad_pro` / `.kicad_sch` / `.kicad_pcb` — 想看看电路或者改改的话,这是KiCad源文件。自定义的封装/符号都打包在 `stp_motr.pretty/` 和 `stp_motr.kicad_sym` 里了,打开就能用。
  - **`BOM.csv`** — 物料清单。
  - **`digikey_order.csv`** — 直接丢进Digikey的下单工具就行。

### 得准备点啥

- 每个声部一块Arduino Pro Micro兼容板(ATmega32U4,5V/16MHz)
- 每个声部一块Pololu A4988(建议带散热片)
- 每个声部一台NEMA17级步进电机
- 剩下的零件都列在 `hardware/BOM.csv` 里了

### 怎么下单做板子

1. 下载 `hardware/stp-motr-2ch-100x50-gerbers.zip`
2. 传到JLCPCB、PCBWay随便哪家
3. 选数量选颜色,完事

### 怎么买零件

- 都写在 `hardware/BOM.csv` 里
- 用Digikey的话,把 `hardware/digikey_order.csv` 直接导进它的BOM Manager就行

### 许可证

MIT,详见 `LICENSE`。

---

## 한국어

### 이게 뭐냐면

A4988의 STEP 핀을 MIDI 노트 음높이랑 정확히 같은 주파수로 토글해주면, 그 모터가 그 음으로 "노래"를 부른다. 다들 "노래하는 스테퍼"라고 부르는 그 트릭이다. 이걸로 모터+A4988 한 쌍을 음 하나짜리 채널로 만들어서, USB MIDI로 DAW나 시퀀서에서 바로 연주할 수 있게 한 거다. 채널마다 Arduino 한 장씩, 마스터 보드 같은 건 없고 각자 독립된 USB MIDI 장치로 인식된다.

### 들어있는 것

- **`firmware/`** — Pro Micro 호환 보드에 올리는 스케치.
  - `stp_motr_channel_velocity/` — 이게 최종판. 음높이뿐 아니라 벨로시티도 마이크로스텝 거칠기로 표현한다.
  - `stp_motr_channel/` — 벨로시티는 안 보는 단순 버전.
  - 모든 채널 보드에 같은 스케치 올리면 끝.
- **`hardware/`** — 실제 쓰는 보드(2채널, 100×50mm) 관련 전부.
  - **`stp-motr-2ch-100x50-gerbers.zip`** — 제작용 파일 묶음. KiCad 켤 필요도 없이 이걸 JLCPCB 같은 데 바로 올리면 주문된다.
  - `gerbers/` — 같은 걸 풀어놓은 것. 개별 파일 보고 싶을 때.
  - `stp-motr-2ch-100x50.kicad_pro` / `.kicad_sch` / `.kicad_pcb` — 회로 들여다보거나 고치고 싶으면 이 KiCad 파일들. 커스텀 풋프린트/심볼도 `stp_motr.pretty/`랑 `stp_motr.kicad_sym`에 같이 들어있어서 그냥 열면 된다.
  - **`BOM.csv`** — 부품 목록.
  - **`digikey_order.csv`** — Digikey BOM Manager에 그냥 넣으면 되는 주문 리스트.

### 필요한 것

- 채널당 Arduino Pro Micro 호환 보드(ATmega32U4, 5V/16MHz) 1개
- 채널당 Pololu A4988(방열판 추천) 1개
- 채널당 NEMA17급 스테핑 모터 1개
- 나머지는 `hardware/BOM.csv`에 다 있음

### 보드 주문하기

1. `hardware/stp-motr-2ch-100x50-gerbers.zip` 다운로드
2. JLCPCB든 PCBWay든 그대로 업로드
3. 수량/색상 고르고 끝

### 부품 주문하기

- `hardware/BOM.csv`에 다 적혀있음
- Digikey 쓴다면 `hardware/digikey_order.csv`를 BOM Manager에 그대로 넣으면 됨

### 라이선스

MIT. `LICENSE` 참고.
