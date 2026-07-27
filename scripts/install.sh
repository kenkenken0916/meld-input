#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
rime_dir="${XDG_DATA_HOME:-${HOME}/.local/share}/fcitx5/rime"

mkdir -p "${rime_dir}/lua"
install -m 0644 "${project_dir}/rime/smart_bopomofo.schema.yaml" "${rime_dir}/smart_bopomofo.schema.yaml"
install -m 0644 "${project_dir}/rime/smart_english.tsv" "${rime_dir}/smart_english.tsv"
install -m 0644 "${project_dir}/rime/lua/smart_english.lua" "${rime_dir}/lua/smart_english.lua"
install -m 0644 "${project_dir}/rime/lua/smart_english_commit.lua" "${rime_dir}/lua/smart_english_commit.lua"
install -m 0644 "${project_dir}/rime/lua/smart_english_core.lua" "${rime_dir}/lua/smart_english_core.lua"

default_custom="${rime_dir}/default.custom.yaml"
if [[ ! -e "${default_custom}" ]]; then
  install -m 0644 "${project_dir}/rime/default.custom.yaml" "${default_custom}"
  printf '已建立 %s 並啟用 Moji 智慧注音。\n' "${default_custom}"
else
  printf '保留既有設定：%s\n' "${default_custom}"
  if ! grep -q 'schema: smart_bopomofo' "${default_custom}"; then
    printf '請手動把「- schema: smart_bopomofo」加入 schema_list。\n'
  fi
fi

printf '已安裝到 %s\n' "${rime_dir}"
if command -v fcitx5-remote >/dev/null 2>&1; then
  fcitx5-remote -r || true
  printf '已要求 Fcitx5 重新載入；請從 Rime 的 F4 選單選擇 Moji 智慧注音。\n'
else
  printf '未找到 fcitx5-remote；安裝 Fcitx5 後請重新部署 Rime。\n'
fi
