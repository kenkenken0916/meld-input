# Changelog

## 0.5.1

- 內部模式切換時顯示游標浮動提示：純英文「英」、智慧中文「融」、
  純注音「中」。
- 智慧／純注音與中文／純英文改為兩個獨立狀態。
- 右 Shift 只切換中文與純英文。
- 支援隱含一聲切段，例如 `rucjo4` →「機會」、
  `qu/6rul2l4` →「平交道」。
- 修正 `ru` 一聲預覽錯誤使用未完成音節候選。
- 修正 composition 清空後 Backspace 被輸入法攔截。

## 0.4.0

- 新增 segmented composition module。
- 中文已定段不再因後續英文輸入退回 raw keys。
- 候選選取只鎖定所選範圍，不提交整句。
- Backspace、Enter、Escape 與候選導覽改由一致的 composition 狀態處理。

## 0.3.0

- 智慧與純注音使用不同 Rime schema。
- 純注音保留寬鬆、不完整注音匹配。
- 新增多音節最佳詞組預覽。

## Earlier versions

0.1.x–0.2.x 建立原生 Fcitx5 addon、獨立 Rime 資料目錄、臺灣正體
候選、英文 fallback 與專用圖示。完整歷史說明仍保留於 README。
