# Meld Input

Meld Input is a Taiwanese Zhuyin and English input method that decides the
language of each segment while preserving already resolved text.

## Language

**Smart mode（智慧模式）**:
The mode in which Meld decides whether each pending segment is Taiwanese
Zhuyin or English.
_Avoid_: Mixed mode, fuzzy mode

**Settled segment（已定段）**:
A segment whose language has been resolved; a Chinese settled segment also has
a fixed Zhuyin reading. Its displayed Hanzi may still change through phrase
ranking, but it cannot turn back into English or raw input.
_Avoid_: Committed segment, finished word

**Pending segment（判定中段）**:
The current run of raw keystrokes whose language and interpretation have not
yet been resolved.
_Avoid_: Incomplete segment, raw word

**Adjacent-language bias（相鄰語言偏向）**:
When there is no space after a settled segment, its language is preferred only
while the pending segment remains ambiguous. Explicit complete Zhuyin or
confident English evidence may override the preference.
_Avoid_: Previous-character rule, language lock

**Phrase preview（詞組預覽）**:
The best currently ranked Hanzi for contiguous Chinese settled segments. It
may be replaced by a better phrase candidate until the text is committed.
_Avoid_: Final text, locked candidate

**First-tone Space（一聲 Space）**:
A Space that confirms the first-tone preview, settling the pending segment as
Chinese without producing whitespace.
_Avoid_: Selection Space, omitted tone

**First-tone preview（一聲預覽）**:
A provisional Chinese preview for a complete untoned Zhuyin pending segment.
It remains pending until Space or Enter confirms it, is replaced by another
tone, or is withdrawn when later keys establish an English interpretation.
_Avoid_: Settled Chinese, implicit commit

**Implicit first-tone boundary（隱含一聲邊界）**:
When an untoned complete Zhuyin syllable is immediately followed by another
complete explicitly toned syllable, Smart mode may settle the first syllable
as first tone without requiring Space, provided both parses have Chinese
candidates. For example, `ru｜cjo4` forms `機會` and
`qu/6｜rul｜2l4` forms `平交道`.
_Avoid_: Universal omitted tone, partial-Zhuyin split

**Commit（提交）**:
The moment Meld sends resolved text to the application, after which Meld may
no longer rerank or reinterpret it.
_Avoid_: Candidate selection, segment settlement

**Composition Backspace（組字 Backspace）**:
Backspace removes one whole Zhuyin syllable whenever that syllable is already
displayed as Hanzi, including a first-tone preview. If the display is still
raw, it removes one raw character instead.
_Avoid_: Always delete one key, always delete one word

Backspace always performs exactly one visible deletion per keypress. After
Left or Right editing-focus movement, it deletes the Chinese syllable or raw
character immediately before the caret. Deleting a pinned character also
removes its associated pin and Zhuyin data. If the candidate list is open,
Backspace closes it and performs the deletion in the same keypress; Escape is
used when the user wants to close the list without deleting anything.

**Tone replacement（聲調覆蓋）**:
A newly entered tone replaces the tone of the most recent editable Chinese
syllable and reranks its preview, rather than requiring the old tone to be
deleted key by key.
_Avoid_: Tone append, tone backtracking

**Candidate navigation（候選導覽）**:
The candidate list is opened with Down and navigated with Up and Down. Plain
number keys remain available to Dachen Zhuyin; Shift+1 through Shift+0 and a
mouse click provide direct candidate selection. Low-frequency controls such
as full-width and half-width settings live in the Down menu.
_Avoid_: Left-right candidate navigation, plain-number candidate selection

When the candidate list is open, Enter selects and pins the highlighted
candidate, while Escape closes the list without changing the preview. When
the list is closed, Enter keeps its normal confirmation or commit meaning.
Space is not used for candidate selection because it is reserved for
first-tone confirmation and English whitespace.

With the candidate list closed, Left and Right move the editing focus among
Chinese characters or word spans in the composition. Down opens candidates
for the focused span; once open, Up and Down move the highlighted candidate.
Enter pins only the span covered by that candidate. Escape exits the list with
no change to text, focus, preview, or pin state.

With a closed candidate list, Escape never deletes or commits composition
content; it only clears sentence-editing focus and returns the caret to the end.
With no active composition, Escape is forwarded to the application.

**Pinned candidate（鎖定候選）**:
A Hanzi span explicitly selected by the user. Phrase ranking may continue for
the remaining unpinned spans, but it may not replace a pinned candidate.
Selecting or pinning one span never settles, freezes, or commits the entire
composition. Only that selected word or character becomes fixed.
_Avoid_: Committed candidate, final composition

**Complete-Zhuyin evidence（完整注音證據）**:
Smart mode may interpret a pending segment as Chinese only when every raw key
is consumed by a legal Dachen Zhuyin parse and that parse has a Chinese
candidate. A complete parse with an explicit tone is strong Chinese evidence;
a complete untoned parse may create only a provisional first-tone preview.
Any leftover key, illegal syllable, or parse without a Chinese candidate keeps
the original keys as English.
_Avoid_: Best-effort Zhuyin, partial-match Chinese

**Open-vocabulary English（開放詞彙英文）**:
English validity does not depend on an English dictionary. Unknown words,
names, code, and misspellings remain valid English input and must not be
forced into a partial Chinese match merely because the dictionary lacks them.
_Avoid_: Dictionary-approved English, spellcheck gate

**Language override candidate（語言覆寫候選）**:
In Smart mode, the candidate list always offers the raw keys as an English
candidate and offers every legal complete-Zhuyin Chinese candidate. Selecting
one pins only that segment to the chosen language, preventing later automatic
reinterpretation without committing the surrounding composition. Pure Zhuyin
omits raw-English candidates, and Pure English omits Chinese candidates.
_Avoid_: Global language override, whole-sentence language lock

**Evidence-driven split（證據切段）**:
Smart mode may create a new language segment without whitespace only when the
new language has strong evidence. After Chinese, keys that cannot form a
complete Zhuyin parse begin an English pending segment. After English, only a
complete Zhuyin parse with an explicit tone may split off a Chinese segment;
a provisional first-tone preview alone must not split the English segment.
The split is previewed immediately but does not invent whitespace.
_Avoid_: Character-by-character switching, implicit whitespace

For example, `hellosu3` remains English until the `3` supplies strong Chinese
evidence, at which point it is segmented as `hello｜你`. English is committed
at an explicit boundary such as Space, Enter, or punctuation.

**Contextual punctuation（語境標點）**:
In the default Auto setting, punctuation follows the preceding settled
segment: Chinese produces Taiwanese full-width punctuation and English
produces ASCII punctuation. With no preceding segment, the current pending
language is used when resolved; if there is no context or it remains
ambiguous, half-width ASCII punctuation is the fixed default.
_Avoid_: Last-used punctuation, always-full-width punctuation

The Down menu offers persistent `Auto`, `Full-width`, and `Half-width`
choices. This setting controls punctuation and symbols only; it does not
convert Latin letters or digits to full-width forms. Entering punctuation
first resolves the pending segment under the normal language rules, emits the
selected punctuation style, and commits the result.

**Input mode（輸入模式）**:
Meld keeps two independent persistent controls. Smart selection toggles
between Smart and Pure Zhuyin and displays `● 智慧切換` or `○ 智慧切換`.
Language selection displays `中` or `EN` and toggles Chinese input versus Pure
English. Changing Smart selection while EN is active does not leave English;
it chooses which Chinese mode will be restored later.
_Avoid_: Mode-transition arrows, left-Shift mode toggle

Tapping Right Shift by itself switches to Pure English; tapping it again while
in Pure English returns to the independently selected Chinese mode. Holding
Right Shift with another key behaves only as a normal Shift modifier and does
not switch modes. Left Shift never switches modes. Both GUI indicators update
immediately and their selections survive login or restart.

**Mode switch indicator（模式切換提示）**:
Every internal mode change shows Fcitx's temporary cursor popup. Pure English
shows `英` regardless of the hidden Smart preference, Smart Chinese shows
`融`, and Pure Zhuyin shows `中`.
_Avoid_: Persistent candidate banner, Smart-dependent English label

**Modifier pass-through（修飾鍵直通）**:
Ctrl, Alt, and Super key combinations are forwarded unchanged while the
composition remains intact. Either Shift key combined with a letter produces
uppercase English and is strong English evidence. With an open candidate list,
Shift+1 through Shift+0 directly select candidates; with the list closed they
retain their normal symbol behavior. Only a Right-Shift press-and-release with
no intervening key is a mode action.
_Avoid_: Shortcut-triggered commit, modifier-triggered reinterpretation

**Mode-local reinterpretation（模式局部重判）**:
Changing input mode during composition reinterprets only the pending segment.
Settled segments and pinned candidates are preserved and nothing is committed
merely because the mode changed. Pure English keeps pending keys literal; Pure
Zhuyin reparses them with its relaxed parser; Smart applies complete-Zhuyin
evidence. If the new mode cannot parse the pending keys, their raw form is
preserved without data loss.
_Avoid_: Whole-composition reset, mode-switch commit

**Composition Enter（組字 Enter）**:
With a closed candidate list and non-empty composition, Enter resolves and
commits the entire composition without forwarding an Enter keystroke to the
application. This includes confirming a first-tone preview or literal English.
After the commit, a second Enter is handled normally by the application. With
an open candidate list, Enter only pins the highlighted candidate and leaves
the rest of the composition editable.
_Avoid_: Commit-and-newline, candidate-select commit

**Focus-loss commit（失焦提交）**:
When the input field loses focus, Meld commits exactly the currently visible
composition: current Chinese phrase previews and first-tone previews as Hanzi,
unparseable raw keys as literal English, and pinned candidates unchanged. An
open candidate list is closed without applying its merely highlighted item.
No whitespace or Enter keystroke is invented.
_Avoid_: Focus-loss cancel, highlighted-candidate auto-selection
