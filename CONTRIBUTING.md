# Contributing to Meld Input

感謝協助改善融輸入法。這個專案主要針對 Arch Linux、Fcitx5、
KDE Plasma 與臺灣大千注音。

## 開發環境

```bash
sudo pacman -S --needed base-devel cmake ninja pkgconf \
  fcitx5 librime rime-bopomofo rime-terra-pinyin
```

先執行不需安裝 addon 的測試：

```bash
./scripts/test.sh
```

在 Arch 建置並安裝：

```bash
./scripts/build-native-arch.sh
fcitx5 -rd
```

## 回報問題

請附上：

- Meld Input 版本。
- Fcitx5 與 librime 版本。
- KDE Plasma 使用 Wayland 或 X11。
- 可重現問題的完整按鍵序列，例如 `rucjo4`。
- 實際顯示與預期顯示。
- 編譯問題請附完整 `makepkg` 輸出。

請勿貼出含私人同步資料的完整 Rime user directory。

## Composition 規則

修改輸入行為前請先閱讀 [CONTEXT.md](CONTEXT.md) 與
[docs/adr/0001-segmented-composition-state.md](docs/adr/0001-segmented-composition-state.md)。
已定段、判定中段、候選鎖定與提交必須保持為不同狀態。

新增或修正行為時，請在 `native/tests/` 增加能重現該按鍵規則的測試。
