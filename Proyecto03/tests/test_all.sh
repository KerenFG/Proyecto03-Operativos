#!/usr/bin/env bash
# tests/test_all.sh — Plan de pruebas automatizado (Fase 9)
#
# Uso:
#   make check          (desde el directorio raíz del proyecto)
#   bash tests/test_all.sh
#
# Prerrequisito: haber compilado con "make all" previamente.
# El script arranca su propia instancia del servidor en el puerto TEST_PORT
# y la detiene al finalizar. No interfiere con instancias en producción.

PORT=18080
STORAGE=/tmp/awss3_test_storage
TMP=/tmp/awss3_test_local
SERVER_PID=
PASS=0
FAIL=0

GRN='\033[0;32m'
RED='\033[0;31m'
RST='\033[0m'

# ── helpers ──────────────────────────────────────────────────────────

# Invoca al cliente fijando host y puerto por env vars
s3() { AWS_S3_HOST=127.0.0.1 AWS_S3_PORT=$PORT ./aws-s3 "$@"; }

pass() { echo -e "${GRN}[PASS]${RST} $1"; PASS=$((PASS+1)); }
fail() { echo -e "${RED}[FAIL]${RST} $1"; FAIL=$((FAIL+1)); }

# run_ok DESC CMD...  — pasa si el comando retorna 0
run_ok() {
    local desc="$1"; shift
    if "$@" >/dev/null 2>&1; then pass "$desc"
    else fail "$desc"; fi
}

# run_fail DESC CMD...  — pasa si el comando retorna != 0
run_fail() {
    local desc="$1"; shift
    if ! "$@" >/dev/null 2>&1; then pass "$desc"
    else fail "$desc"; fi
}

# contains DESC NEEDLE HAYSTACK  — pasa si HAYSTACK contiene NEEDLE
contains() {
    if printf '%s' "$3" | grep -qF "$2"; then pass "$1"
    else fail "$1 (esperado '$2' en: [$3])"; fi
}

# not_contains DESC NEEDLE HAYSTACK  — pasa si HAYSTACK NO contiene NEEDLE
not_contains() {
    if ! printf '%s' "$3" | grep -qF "$2"; then pass "$1"
    else fail "$1 (no esperado '$2' en: [$3])"; fi
}

# ── setup / teardown ─────────────────────────────────────────────────

teardown() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
    rm -rf "$STORAGE" "$TMP"
}
trap teardown EXIT

rm -rf "$STORAGE" "$TMP"
mkdir -p "$STORAGE" "$TMP/sub"
printf 'contenido uno\n'    > "$TMP/f1.txt"
printf 'contenido dos\n'    > "$TMP/f2.txt"
printf 'en subdirectorio\n' > "$TMP/sub/f3.txt"

./aws-s3_server --port $PORT --storage-dir "$STORAGE" >/dev/null 2>&1 &
SERVER_PID=$!
sleep 0.5   # esperar a que el servidor levante

echo "========================================"
echo "  Plan de pruebas aws-s3 (Fase 9)"
echo "========================================"

# ── T01-T03: mb ──────────────────────────────────────────────────────
echo ""
echo "=== mb (crear bucket) ==="
run_ok   "T01 mb: crea bucket 'test'"   s3 mb s3://test
run_ok   "T02 mb: crea bucket 'bak'"    s3 mb s3://bak
run_fail "T03 mb: rechaza duplicado"    s3 mb s3://test

# ── T04-T06: ls (buckets) ────────────────────────────────────────────
echo ""
echo "=== ls (listar buckets) ==="
LS=$(s3 ls 2>/dev/null || true)
contains     "T04 ls: muestra 'test'"       "test" "$LS"
contains     "T05 ls: muestra 'bak'"        "bak"  "$LS"
run_ok       "T06 ls: lista bucket vacío"   s3 ls s3://test

# ── T07-T10: cp upload ───────────────────────────────────────────────
echo ""
echo "=== cp (subir archivos) ==="
run_ok "T07 cp: sube f1.txt" s3 cp "$TMP/f1.txt" s3://test/f1.txt
run_ok "T08 cp: sube f2.txt" s3 cp "$TMP/f2.txt" s3://test/f2.txt

OLS=$(s3 ls s3://test 2>/dev/null || true)
contains "T09 ls: f1.txt visible en test" "f1.txt" "$OLS"
contains "T10 ls: f2.txt visible en test" "f2.txt" "$OLS"

# ── T11-T12: cp download ─────────────────────────────────────────────
echo ""
echo "=== cp (descargar archivos) ==="
run_ok "T11 cp: descarga f1.txt" s3 cp s3://test/f1.txt "$TMP/dl.txt"
DLC=$(cat "$TMP/dl.txt" 2>/dev/null || true)
contains "T12 cp: contenido descargado correcto" "contenido uno" "$DLC"

# ── T13-T14: cp sobrescritura ────────────────────────────────────────
echo ""
echo "=== cp (sobrescritura) ==="
printf 'contenido nuevo\n' > "$TMP/f1v2.txt"
run_ok "T13 cp: sobrescribe objeto existente" \
    s3 cp "$TMP/f1v2.txt" s3://test/f1.txt
s3 cp s3://test/f1.txt "$TMP/dl2.txt" >/dev/null 2>&1 || true
DL2=$(cat "$TMP/dl2.txt" 2>/dev/null || true)
contains "T14 cp: contenido actualizado tras sobrescritura" "contenido nuevo" "$DL2"

# ── T15-T16: cp bucket-to-bucket ─────────────────────────────────────
echo ""
echo "=== cp (bucket → bucket) ==="
run_ok "T15 cp: copia f2.txt de test a bak" \
    s3 cp s3://test/f2.txt s3://bak/f2_bak.txt
BAKLS=$(s3 ls s3://bak 2>/dev/null || true)
contains "T16 ls: f2_bak.txt visible en bak" "f2_bak" "$BAKLS"

# ── T17-T20: cp --recursive ──────────────────────────────────────────
echo ""
echo "=== cp --recursive ==="
run_ok "T17 mb: crea bucket 'rec'" s3 mb s3://rec
run_ok "T18 cp --recursive: sube árbol local" \
    s3 cp --recursive "$TMP/" s3://rec/
RECLS=$(s3 ls s3://rec 2>/dev/null || true)
contains "T19 ls: sub/f3.txt subido a rec" "f3.txt" "$RECLS"

mkdir -p "$TMP/dl_rec"
run_ok "T20 cp --recursive: descarga árbol del bucket" \
    s3 cp --recursive s3://rec/ "$TMP/dl_rec/"

# ── T21-T23: mv ──────────────────────────────────────────────────────
echo ""
echo "=== mv ==="
printf 'para mover\n' > "$TMP/mv_src.txt"
run_ok "T21 mv: local→s3 (upload + eliminar fuente)" \
    s3 mv "$TMP/mv_src.txt" s3://test/mv_dst.txt
if [ ! -f "$TMP/mv_src.txt" ]; then
    pass "T22 mv: archivo fuente eliminado localmente"
else
    fail "T22 mv: archivo fuente eliminado localmente"
fi
MVLS=$(s3 ls s3://test 2>/dev/null || true)
contains "T23 mv: mv_dst.txt visible en test" "mv_dst" "$MVLS"

# ── T24-T25: rm ──────────────────────────────────────────────────────
echo ""
echo "=== rm ==="
run_ok "T24 rm: elimina f2.txt del bucket" s3 rm s3://test/f2.txt
RMLS=$(s3 ls s3://test 2>/dev/null || true)
not_contains "T25 rm: f2.txt ya no aparece en ls" "f2.txt" "$RMLS"

# ── T26-T29: rm --recursive ──────────────────────────────────────────
echo ""
echo "=== rm --recursive ==="
run_ok "T26 mb: crea bucket 'rmr'" s3 mb s3://rmr
s3 cp "$TMP/f1.txt" s3://rmr/a/x.txt >/dev/null 2>&1 || true
s3 cp "$TMP/f1.txt" s3://rmr/a/y.txt >/dev/null 2>&1 || true
s3 cp "$TMP/f1.txt" s3://rmr/b/z.txt >/dev/null 2>&1 || true
run_ok "T27 rm --recursive: elimina prefijo a/" \
    s3 rm --recursive s3://rmr/a/
RMRLS=$(s3 ls s3://rmr 2>/dev/null || true)
not_contains "T28 rm --recursive: a/x.txt eliminado"    "x.txt" "$RMRLS"
contains     "T29 rm --recursive: b/z.txt no afectado"  "z.txt" "$RMRLS"

# ── T30-T32: sync ────────────────────────────────────────────────────
echo ""
echo "=== sync ==="
SDIR="$TMP/sync_src"
mkdir -p "$SDIR/sub"
printf 'sync a\n' > "$SDIR/sa.txt"
printf 'sync b\n' > "$SDIR/sb.txt"
printf 'sync c\n' > "$SDIR/sub/sc.txt"

run_ok "T30 mb: crea bucket 'syn'" s3 mb s3://syn
SY1=$(s3 sync "$SDIR/" s3://syn/ 2>&1 || true)
contains "T31 sync: reporte incluye 'Subidos'"    "Subidos:"     "$SY1"

SY2=$(s3 sync "$SDIR/" s3://syn/ 2>&1 || true)
contains "T32 sync: archivos iguales quedan sin cambios" "Sin cambios:" "$SY2"

# ── T33-T34: sync --delete ───────────────────────────────────────────
echo ""
echo "=== sync --delete ==="
s3 cp "$TMP/f1.txt" s3://syn/extra.txt >/dev/null 2>&1 || true
SYDEL=$(s3 sync --delete "$SDIR/" s3://syn/ 2>&1 || true)
contains "T33 sync --delete: reporte incluye 'Eliminados'" "Eliminados:" "$SYDEL"
SYNLS=$(s3 ls s3://syn 2>/dev/null || true)
not_contains "T34 sync --delete: extra.txt eliminado del bucket" "extra.txt" "$SYNLS"

# ── T35-T39: rb ──────────────────────────────────────────────────────
echo ""
echo "=== rb (eliminar bucket) ==="
run_ok   "T35 mb: crea bucket vacío 'emp'"           s3 mb s3://emp
run_ok   "T36 rb: elimina bucket vacío 'emp'"        s3 rb s3://emp
run_fail "T37 rb: rechaza bucket no vacío 'syn'"     s3 rb s3://syn
run_ok   "T38 rb --force: elimina bucket con datos"  s3 rb --force s3://syn

FINALBS=$(s3 ls 2>/dev/null || true)
not_contains "T39 ls: 'syn' ya no aparece tras rb" "syn" "$FINALBS"

# ── resumen ──────────────────────────────────────────────────────────
echo ""
echo "========================================"
printf "  Resultados: %d pasados, %d fallidos\n" "$PASS" "$FAIL"
echo "========================================"
[ "$FAIL" -eq 0 ]
