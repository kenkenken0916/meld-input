#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

for required in \
  native/CMakeLists.txt \
  native/PKGBUILD \
  native/addon/CMakeLists.txt \
  native/addon/meld.cpp \
  native/addon/meld.h \
  native/addon/meld.conf \
  native/addon/meld-addon.conf.in \
  native/addon/composition_model.cpp \
  native/addon/composition_model.h \
  native/addon/key_routing.h \
  native/addon/smart_engine.cpp \
  native/addon/smart_engine.h \
  native/tests/test_composition_model.cpp \
  native/tests/test_key_routing.cpp \
  native/tests/test_smart_engine.cpp \
  native/data/meld_bopomofo.schema.yaml \
  native/data/meld_bopomofo_relaxed.schema.yaml; do
  test -s "${project_dir}/${required}"
done

grep -q 'Name=Meld Input' "${project_dir}/native/addon/meld.conf"
grep -q 'Name\[zh_TW\]=融輸入法' "${project_dir}/native/addon/meld.conf"
grep -q 'Icon=fcitx-meld' "${project_dir}/native/addon/meld.conf"
test -s "${project_dir}/native/data/fcitx-meld.svg"
grep -q 'schema: meld_bopomofo' "${project_dir}/native/addon/meld.cpp"
grep -q 'start_maintenance(true)' "${project_dir}/native/addon/meld.cpp"
if grep -q 'zhuyin:/abbreviation' \
  "${project_dir}/native/data/meld_bopomofo.schema.yaml"; then
  printf '智慧 schema 不應接受不完整注音。\n' >&2
  exit 1
fi
grep -q 'zhuyin:/abbreviation' \
  "${project_dir}/native/data/meld_bopomofo_relaxed.schema.yaml"
grep -q 'meld_bopomofo_relaxed' "${project_dir}/native/addon/meld.cpp"
grep -q 'return state && state->smart() ? "● 智慧切換" : "○ 智慧切換";' \
  "${project_dir}/native/addon/meld.cpp"
grep -q '"meld-language-mode"' "${project_dir}/native/addon/meld.cpp"
grep -q 'state && state->english() ? "EN" : "中"' \
  "${project_dir}/native/addon/meld.cpp"
grep -q 'showCustomInputMethodInformation' \
  "${project_dir}/native/addon/meld.cpp"
if grep -q '智慧切換.*→\\|智慧切換.*->' "${project_dir}/native/addon/meld.cpp"; then
  printf '原生狀態 Action 不應顯示切換箭頭。\n' >&2
  exit 1
fi
if grep -q 'name: smart_english' "${project_dir}/native/data/meld_bopomofo.schema.yaml"; then
  printf '原生版不應把智慧模式放回 Rime switcher。\n' >&2
  exit 1
fi

if command -v pkg-config >/dev/null 2>&1 \
  && pkg-config --exists Fcitx5Core rime \
  && command -v c++ >/dev/null 2>&1; then
  # Arch Fcitx 5.1.21 headers use C++20 (std::span/string_view starts_with).
  # This catches API drift without linking or installing the addon.
  # shellcheck disable=SC2046
  c++ -std=c++20 -fsyntax-only \
    $(pkg-config --cflags Fcitx5Core rime) \
    -I"${project_dir}/native/addon" \
    -DMELD_VERSION=\"0.5.1\" \
    -DRIME_DATA_DIR=\"/usr/share/rime-data\" \
    "${project_dir}/native/addon/meld.cpp"
fi

if command -v c++ >/dev/null 2>&1; then
  composition_test="$(mktemp "${TMPDIR:-/tmp}/meld-composition-test.XXXXXX")"
  routing_test="$(mktemp "${TMPDIR:-/tmp}/meld-routing-test.XXXXXX")"
  native_test="$(mktemp "${TMPDIR:-/tmp}/meld-smart-test.XXXXXX")"
  trap 'rm -f "${composition_test}" "${routing_test}" "${native_test}"' EXIT
  c++ -std=c++20 \
    "${project_dir}/native/addon/composition_model.cpp" \
    "${project_dir}/native/tests/test_composition_model.cpp" \
    -o "${composition_test}"
  "${composition_test}"
  c++ -std=c++20 \
    "${project_dir}/native/addon/composition_model.cpp" \
    "${project_dir}/native/tests/test_key_routing.cpp" \
    -o "${routing_test}"
  "${routing_test}"
  c++ -std=c++20 \
    "${project_dir}/native/addon/smart_engine.cpp" \
    "${project_dir}/native/tests/test_smart_engine.cpp" \
    -o "${native_test}"
  "${native_test}" "${project_dir}/rime/smart_english.tsv"
fi

for required in \
  rime/smart_bopomofo.schema.yaml \
  rime/smart_english.tsv \
  rime/lua/smart_english.lua \
  rime/lua/smart_english_commit.lua \
  rime/lua/smart_english_core.lua; do
  test -s "${project_dir}/${required}"
done

grep -q 'schema_id: smart_bopomofo' "${project_dir}/rime/smart_bopomofo.schema.yaml"
grep -q 'lua_translator@\*smart_english' "${project_dir}/rime/smart_bopomofo.schema.yaml"
grep -q 'lua_processor@\*smart_english_commit' "${project_dir}/rime/smart_bopomofo.schema.yaml"
grep -q 'simplifier@taiwan_fixed' "${project_dir}/rime/smart_bopomofo.schema.yaml"
if grep -q 'name: zh_hans' "${project_dir}/rime/smart_bopomofo.schema.yaml"; then
  printf '不應在 F4 switches 中出現簡體中文選項。\n' >&2
  exit 1
fi
if grep -q 'Control+Shift+E' "${project_dir}/rime/smart_bopomofo.schema.yaml"; then
  printf '智慧模式應只從 GUI/F4 選單切換。\n' >&2
  exit 1
fi
grep -q 'punct_auto' "${project_dir}/rime/smart_bopomofo.schema.yaml"
grep -q 'name: smart_english' "${project_dir}/rime/smart_bopomofo.schema.yaml"
grep -q 'states: \[ ○ 智慧切換, ● 智慧切換 \]' "${project_dir}/rime/smart_bopomofo.schema.yaml"
if grep -q 'name: full_shape' "${project_dir}/rime/smart_bopomofo.schema.yaml"; then
  printf '全半形不應出現在 F4 switches。\n' >&2
  exit 1
fi
grep -q $'^linux\t' "${project_dir}/rime/smart_english.tsv"

if ! command -v lua >/dev/null 2>&1; then
  printf '靜態檢查通過；未找到 lua，略過 Lua 單元測試。\n'
  exit 0
fi

LUA_PATH="${project_dir}/rime/lua/?.lua;;" \
  lua "${project_dir}/tests/test_smart_english.lua" "${project_dir}/rime/smart_english.tsv"
LUA_PATH="${project_dir}/rime/lua/?.lua;;" \
  lua "${project_dir}/tests/test_smart_english_commit.lua" "${project_dir}/rime"
