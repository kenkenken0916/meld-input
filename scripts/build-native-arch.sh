#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

for package in base-devel cmake ninja pkgconf fcitx5 librime rime-bopomofo rime-terra-pinyin; do
  if ! pacman -Q "${package}" >/dev/null 2>&1; then
    printf '缺少套件：%s\n' "${package}" >&2
    printf '請先執行：sudo pacman -S --needed base-devel cmake ninja pkgconf fcitx5 librime rime-bopomofo rime-terra-pinyin\n' >&2
    exit 1
  fi
done

cd "${project_dir}/native"
makepkg --syncdeps --install --cleanbuild --needed

printf '\n已安裝 Meld Input。請先從 Fcitx 輸入法群組移除舊的 Rime，重新啟動 Fcitx：\n'
printf '  fcitx5 -rd\n'
printf '再開啟 fcitx5-configtool，把「融輸入法 / Meld Input」加入群組。\n'
