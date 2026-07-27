# Meld Input／融輸入法

給 Arch Linux + KDE Plasma 使用的智慧注音／英文混合輸入法。

原始碼、問題回報與版本下載：
[github.com/kenkenken0916/meld-input](https://github.com/kenkenken0916/meld-input)

專案目前包含兩個版本：

- `native/`：新的原生 Fcitx5 C++ addon，顯示名稱為「Meld Input／融輸入法」。
- `rime/`：舊的 Moji Master Rime + Lua 原型，保留作為演算法參考與 fallback。

新安裝請優先使用原生 addon。原生版的智慧切換是 Fcitx Action，選單只顯示目前狀態：

- `● 智慧切換`
- `○ 智慧切換`

點擊後原地切換，不會再出現 `目前狀態 → 下一狀態`。

## 原生版安裝（Arch Linux）

可從 [GitHub Releases](https://github.com/kenkenken0916/meld-input/releases)
下載最新版 `meld-input-*.tar.gz`，解壓後執行：

解壓縮後，在專案資料夾執行：

```bash
sudo pacman -S --needed base-devel cmake ninja pkgconf \
  fcitx5 librime rime-bopomofo rime-terra-pinyin
./scripts/build-native-arch.sh
```

第一次測試前：

1. 從 `fcitx5-configtool` 的目前輸入法群組移除舊的「Rime」。
2. 執行 `fcitx5 -rd` 完整重啟 Fcitx。
3. 再用 `fcitx5-configtool` 加入「融輸入法／Meld Input」。

目前原生版與官方 `fcitx5-rime` 都會使用同一個行程內的 librime
singleton；測試時不要同時載入兩個 addon。這不需要移除
`fcitx5-rime` 套件，只要暫時從輸入法群組移除 Rime 並重新啟動即可。

### 原生版操作

| 操作 | 結果 |
|---|---|
| 點擊 `●／○ 智慧切換` | 只切換智慧中文與純注音 |
| 點擊 `中／EN` 或右 Shift | 只切換中文與純英文 |
| 完整注音 + 聲調 | 顯示並提交臺灣正體中文候選 |
| 一聲注音 + Space | 先加入一聲，再由中文候選確認 |
| 英文 + Space／Enter | 原樣提交英文 |
| 純注音模式 | 完整交給 Rime 大千注音後端 |

原生 addon 使用 C++20，直接控制 raw keys、Space／Enter、候選與
Fcitx 狀態列；中文轉換仍使用本機 librime，大千鍵位及臺灣正體設定
放在 `native/data/meld_bopomofo.schema.yaml`。

0.2.0 起，中英文核心位於 `native/addon/smart_engine.cpp`，包含：

- 完整含聲調注音優先中文。
- 一聲注音在 Space 後確認中文候選。
- 英文字典、常用字前綴、英文後綴與不合理注音結構判斷。
- 與高權重英文相差一個字元時保留原始輸入，另外顯示修正候選。
- 無法形成中文候選時，Space／Enter 回退成原始英文。

0.2.1 修正原生版首次啟動沒有部署 `meld_bopomofo` 的問題。Meld
會在自己的 `~/.local/share/fcitx5/meld-rime/default.custom.yaml`
加入 schema，完整部署後才建立輸入 session。

0.2.2 修正中文候選只被選取、沒有結束 composition 的問題；完整
中文現在會經過 `commit_composition()` 提交。智慧模式也改為每個
音節都必須有聲調，下一個音節仍不完整時回到 raw/英文顯示。純注音
模式維持 Rime 的不完整注音匹配。另加入藍橘 `A／ㄅ` 專用圖示。

0.3.0 將智慧與純注音拆成兩個真正不同的 Rime prism：
`meld_bopomofo` 不載入 abbreviation，不接受不完整注音；
`meld_bopomofo_relaxed` 只在純注音模式使用。完成兩個以上音節時，
組字區直接預覽語言模型排名第一的中文詞。中→英會先提交完整中文
prefix，再提交英文尾段；英→中則在完整聲調出現時切開 composition。

0.4.0 新增獨立的 segmented composition module。已確認的中文音節、
英文段、目前判定中的 raw keys、詞組預覽及手動鎖定候選不再混放在
同一個 librime composition。完整聲調只建立已定段，不會立即送進
應用程式；候選列開啟時 Enter 只鎖定反白字詞，候選列關閉時 Enter
才提交整段。Backspace 每次只造成一次可見刪除，模式切換也不清除
已定段。

0.4.1 修正 composition 已清空後，Meld 仍攔截 Backspace，導致應用
程式無法刪除已提交文字的問題。現在只有存在組字內容或候選列時才
由 Meld 處理 Backspace；其他情況原樣交回應用程式。同時修正一聲
預覽的候選查詢：例如 `ru`（ㄐㄧ）會先以明確一聲查詢「機」等候選，
不再用未完成音節的預測結果顯示「就」。

0.5.0 將智慧選擇與中英切換拆成兩個獨立狀態列按鈕。右 Shift 只操作
`中／EN`，不再改變智慧偏好；在 EN 狀態切換智慧設定也不會離開英文。
智慧模式亦支援隱含一聲切段，`ru|cjo4` 可直接形成「機會」，
`qu/6|rul|2l4` 可直接形成「平交道」，不必在一聲字後輸入 Space。

0.5.1 在內部模式切換時使用 Fcitx 游標浮動提示：純英文顯示「英」，
智慧中文顯示「融」，純注音顯示「中」。GUI 按鈕與右 Shift 都使用
同一條提示路徑，提示會由 Fcitx 自動在約一秒後隱藏。

`native/addon` 刻意不命名為 `native/src`：`makepkg --cleanbuild`
會把 `native/src` 當成建置工作目錄刪除。

## 舊版 Rime + Lua 原型

它以 Fcitx5 + Rime 為底層，同一串按鍵同時交給：

- 大千注音：由 Rime `bopomofo_tw` 與語言模型產生臺灣正體中文候選。
- 英文判斷器：保留原始 QWERTY 按鍵，依常用詞、前綴、字形與 URL／Email 規則評分。

當英文判斷達到門檻時，原始英文會成為第一候選並顯示 `〔EN〕`；完整含聲調的注音直接交給中文候選。這不是創音或華碩輸入法的移植（兩者的核心並未開源），而是以開源元件重新實作相近的雙路互動。

## 功能

- 純注音模式：保留 Rime 原本缺聲調、缺韻母的寬鬆大千注音。
- 智慧模式：大千注音和英文同時判斷；含聲調的完整注音優先中文。
- 純英文模式：按一下右 Shift 切換，狀態顯示為 `EN`。
- F4／Rime GUI 以 `○／● 智慧切換` 顯示純注音與智慧模式。
- 標點模式可選自動、中文或英文；自動模式依上一段提交文字決定全形中文或半形 ASCII 標點。
- 全半形不再佔用 F4；常用標點的全／半形版本放在候選清單，以 `↓` 選擇。
- URL、Email、版本號、camelCase、snake_case、kebab-case 直接視為英文。
- 以注音聲母／介音／韻母結構檢查純字母；過度破碎的組合優先視為英文。
- 英文與高權重詞相差一個字元時仍保留原始英文，並提供修正候選。
- 英文詞庫為純文字 TSV，可自行增加詞彙與權重。
- 全部在本機執行，不傳送輸入內容。

### 舊版安裝（Arch Linux）

先安裝官方套件：

```bash
sudo pacman -S fcitx5-im fcitx5-rime librime rime-bopomofo
```

安裝本專案到使用者的 Rime 資料夾：

```bash
./scripts/install.sh
```

如果原本沒有 `default.custom.yaml`，腳本會自動啟用「Moji 智慧注音」。如果已經有自訂方案，腳本不會覆寫它；請把下列項目加入既有的 `schema_list`：

```yaml
patch:
  schema_list:
    - schema: smart_bopomofo
```

接著重新部署：

```bash
fcitx5-remote -r
```

在 KDE「系統設定 → 鍵盤 → 虛擬鍵盤」選擇 Fcitx 5，並用 `fcitx5-configtool` 把 Rime 加進目前輸入法。進入 Rime 後可按 `F4` 選擇「Moji 智慧注音」。

Wayland 下建議在「系統設定 → 鍵盤 → 虛擬鍵盤」設定 Fcitx 5，不要只靠全域 `QT_IM_MODULE`。XWayland／GTK 應用若無法輸入，再執行 `fcitx5-diagnose` 檢查環境。

### 舊版使用

| 操作 | 結果 |
|---|---|
| 純注音模式 | 保留不完整注音與手機式寬鬆選字 |
| 智慧模式 | 常用英文、拼字近似或不合理注音優先英文 |
| 注音按鍵 + 聲調 | 完整含聲調的注音優先中文 |
| 模稜兩可輸入 + 空白 | 先把空白當一聲；無中文候選才送出英文 |
| 右 Shift | 智慧混合／純英文切換 |
| `F4` 或 ``Ctrl+` `` | 勾選智慧辨識及選擇標點模式（固定臺灣正體） |

英文完整詞、已知前綴、模糊詞或注音結構不合理時會產生英文候選。輸入含聲調、結構合理的注音時不產生英文候選，避免英文 translator 因合併順序壓過中文。模稜兩可時，空白會先作為一聲重新計算；沒有中文候選才回退英文。

F4 選單中的模式：

- `○ 智慧切換／● 智慧切換`：空心代表關閉（純注音），實心代表開啟（智慧模式）。
- `中文輸入中／Latin 純英文`：EN 時完全繞過注音解析，原樣送出按鍵。右 Shift 也會切換此模式。
- `自動標點／中文標點／英文標點`：自動模式依上一段送出的中文或英文選擇標點。

按常用標點後可用 `↓` 展開候選，例如 `，/,`、`。/.`、`？/?`；英數本身固定半形。

## 調整英文判斷

編輯 `rime/smart_english.tsv`：

```text
word<TAB>weight
```

權重建議：

- `100–199`：一般詞。
- `200–399`：常用詞，較積極判斷為英文。
- `400+`：技術詞或品牌詞，優先判斷為英文。

修改後再次執行 `./scripts/install.sh` 並重新部署。進階參數在 `smart_bopomofo.schema.yaml` 的 `smart_english` 區段。

## 開發與測試

核心判斷刻意寫成不依賴 Rime 的 Lua 模組，方便測試：

```bash
./scripts/test.sh
```

若系統沒有 `lua`，測試腳本會先完成靜態檢查並提示安裝 `lua`；在 Arch 可執行 `sudo pacman -S lua`。

貢獻流程、測試範圍與回報問題所需資料請見
[CONTRIBUTING.md](CONTRIBUTING.md)。版本變更整理於
[CHANGELOG.md](CHANGELOG.md)。

## 移除舊版

```bash
./scripts/uninstall.sh
```

移除腳本只刪除本專案安裝的五個檔案，不會碰使用者的其他 Rime 設定。若曾手動把 `smart_bopomofo` 加到 `default.custom.yaml`，請自行移除該行。

## 目前限制

- Rime 前端無法完全複製閉源輸入法逐鍵顯示的浮動 `EN／中` UI；目前以候選註解與 Fcitx/Rime 狀態呈現。
- 未收錄的全小寫專有名詞可能先被當作注音；加入 TSV 後即可固定識別。
- 第一版採透明、可調的規則評分。後續可加入本機使用者學習與更大的英文詞頻模型。
