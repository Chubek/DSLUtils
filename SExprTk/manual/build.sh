#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="${ROOT_DIR}/build"

mkdir -p "${OUT_DIR}"
python3 "${ROOT_DIR}/generate_manual.py"

cat "${ROOT_DIR}"/chapter*.md > "${OUT_DIR}/SExprTk-Manual.md"

pandoc "${OUT_DIR}/SExprTk-Manual.md" --metadata title="SExprTk Manual" -s -o "${OUT_DIR}/SExprTk-Manual.html"
pandoc "${OUT_DIR}/SExprTk-Manual.md" --metadata title="SExprTk Manual" -s -o "${OUT_DIR}/SExprTk-Manual.tex"
pandoc "${OUT_DIR}/SExprTk-Manual.md" --metadata title="SExprTk Manual" -t docbook -s -o "${OUT_DIR}/SExprTk-Manual.xml"
python3 "${ROOT_DIR}/markdown_to_pdf.py" "${OUT_DIR}/SExprTk-Manual.md" "${OUT_DIR}/SExprTk-Manual.pdf"
