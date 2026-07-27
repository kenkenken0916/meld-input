#!/usr/bin/env bash
set -euo pipefail

rime_dir="${XDG_DATA_HOME:-${HOME}/.local/share}/fcitx5/rime"
targets=(
  "${rime_dir}/smart_bopomofo.schema.yaml"
  "${rime_dir}/smart_english.tsv"
  "${rime_dir}/lua/smart_english.lua"
  "${rime_dir}/lua/smart_english_commit.lua"
  "${rime_dir}/lua/smart_english_core.lua"
)

for target in "${targets[@]}"; do
  if [[ -f "${target}" ]]; then
    rm -- "${target}"
    printf '已移除 %s\n' "${target}"
  fi
done

printf '其他 Rime 設定均已保留。請從 default.custom.yaml 移除 smart_bopomofo（若有）。\n'
if command -v fcitx5-remote >/dev/null 2>&1; then
  fcitx5-remote -r || true
fi
