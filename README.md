# KANTAN GB PLAY

ゲームボーイカラー用の和音（コード）演奏アプリROM。
[KANTAN Play](https://github.com/InstaChord/KANTAN_Play_core) の「degree + key → 和音」の考え方を踏襲し、十字キー8方向でコードを選んでAボタンで弾く楽器です。キーは C major 固定。

```text
             C
       Bdim     Dm
    Am     [+]     Em
       C7       F
             G
```

| 操作 | 機能 |
|---|---|
| 十字キー（8方向） | コード選択（斜めは同時押し） |
| A | 和音を発音（押している間鳴る） |

## サウンド

- **GB APU**: CH1=3rd / CH2=5th（C7は♭7th）/ CH3(波形メモリ・三角波)=ベース（ルートの2オクターブ下）
- **YM2151 (MODRETRO Chromatic FPGA拡張)**: 起動時に `FF2E == 0x51` で拡張を検出すると、`FF28/FF29` 経由でYM2151 CH0-2に和音、CH4にベースを割り当てます。未検出時はAPUへ自動フォールバック（通常のGBC/エミュレータはこちら）

## ビルド

[GBDK-2020](https://github.com/gbdk-2020/gbdk-2020) が必要です（`~/gbdk-install/gbdk` 想定、`make GBDK=...` で変更可）。

```sh
make        # build/kantan-gb-play.gbc を生成
make run    # mGBA で起動
```

## 構成

- `src/main.c` — メインループ（入力→選択→発音）
- `src/chord.c` — 8方向→構成音テーブル（Cメジャーダイアトニック）
- `src/sound.c` — GB APU ドライバ（周波数テーブル・3ch発音）
- `src/ym2151.c` — Chromatic YM2151 拡張ドライバ（検出・パッチ・KeyON/OFF）
- `src/ui.c` — テキストUI・GBCパレットによる選択ハイライト
