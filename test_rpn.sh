#!/usr/bin/env bash
# Tests unitaires pour calculate-rpn.cpp
set -uo pipefail

RPN="$(cd "$(dirname "$0")" && pwd)/rpn"
# Messages de la calculatrice : les tests s'appuient sur le français.
export LC_ALL=fr_FR.UTF-8
PASS=0
FAIL=0

# Lance un test : compare la dernière ligne de stdout
test_stdout() {
    local name="$1" input="$2" expected="$3"
    local found
    found=$(printf "%s\n" "$input" | "$RPN" 2>/dev/null | tail -1)
    if [[ "$found" == "$expected" ]]; then
        echo "  ✓ $name"
        ((PASS++))
    else
        echo "  ✗ $name : attendu '$expected', obtenu '$found'"
        ((FAIL++))
    fi
}

# Lance un test d'erreur
test_stderr() {
    local name="$1" input="$2" keyword="$3"
    local err
    err=$(printf "%s\n" "$input" | "$RPN" 2>&1 1>/dev/null)
    if echo "$err" | grep -qi "$keyword"; then
        echo "  ✓ $name"
        ((PASS++))
    else
        echo "  ✗ $name : aucune erreur contenant '$keyword'"
        echo "    stderr: $err"
        ((FAIL++))
    fi
}

# Lance un test d'historique : vérifie qu'une ligne contient le texte attendu
test_history() {
    local name="$1" cmd="$2" expected="$3"
    local out
    out=$(printf "%s\nh" "$cmd" | "$RPN" 2>/dev/null)
    if echo "$out" | grep -qF "$expected"; then
        echo "  ✓ $name"
        ((PASS++))
    else
        echo "  ✗ $name : '$expected' non trouvé dans l'historique"
        echo "    sortie: $out"
        ((FAIL++))
    fi
}

echo "=== Tests de base ==="
test_stdout "addition"       "3 4 +"            "7"
test_stdout "soustraction"   "10 3 -"           "7"
test_stdout "multiplication" "6 7 *"            "42"
test_stdout "division"       "15 3 /"           "5"
test_stdout "multiple ops"   "3 4 + 5 *"        "35"
test_stdout "neg"            "5 neg"            "-5"
test_stdout "sqrt"           "9 sqrt"           "3"
test_stdout "square"         "4 square"         "16"
test_stdout "inv"            "8 inv"            "0.125"
test_stdout "abs négatif"    "-10 abs"          "10"
test_stdout "floor"          "3.7 floor"        "3"
test_stdout "ceil"           "3.7 ceil"         "4"

echo "=== Trigonométrie ==="
test_stdout "sin 45° (deg)"   "45 sin"          "0.7071067812"
test_stdout "cos 60° (deg)"   "60 cos"          "0.5"
test_stdout "tan 45° (deg)"   "45 tan"          "1"
test_stdout "asin 1 (deg)"    "1 asin"          "90"
test_stdout "acos 0 (deg)"    "0 acos"          "90"
test_stdout "sin π/2 (rad)"   "rad pi 2 / sin"  "1"
test_stdout "deg après rad"   "deg 30 sin"      "0.5"

echo "=== Constantes ==="
test_stdout "pi"    "pi"    "3.141592654"
test_stdout "e"     "e"     "2.718281828"

echo "=== Pile ==="
test_stdout "dup"     "5 dup"           "5"
test_stdout "drop"    "1 drop 42"       "42"
test_stdout "swap"    "1 2 swap"        "1"
test_stdout "clear"   "1 2 3 clear 99"  "99"

echo "=== Mémoire ==="
test_stdout "sto puis rcl"        $'42 sto x\nrcl x'          "42"
test_stdout "variables nommées"   $'3.14 sto pi2\nrcl pi2'    "3.14"

echo "=== Erreurs ==="
test_stderr "sous-op binaire"    "+"       "pas assez d'opérandes"
test_stderr "sous-op unaire"     "sqrt"    "pas assez d'opérandes"
test_stderr "dup vide"           "dup"     "impossible de dupliquer"
test_stderr "drop vide"          "drop"    "impossible de supprimer"
test_stderr "swap insuffisant"   "1 swap"  "impossible d'échanger"
test_stderr "division par zéro"  "5 0 /"   "division par zéro"
test_stderr "sto sans valeur"    "sto"     "nécessite un nom de variable"

echo "=== Factorielle ==="
test_stdout "fact 0"     "0 fact"    "1"
test_stdout "fact 1"     "1 fact"    "1"
test_stdout "fact 5"     "5 fact"    "120"
test_stdout "fact 10"    "10 fact"   "3628800"

echo "=== Fonctions hyperboliques ==="
test_stdout "sinh 0"     "0 sinh"    "0"
test_stdout "cosh 0"     "0 cosh"    "1"
test_stdout "tanh 0"     "0 tanh"    "0"

echo "=== Mémoire accumulateur ==="
test_stdout "m+ puis mr"     "5 m+ mr"              "5"
test_stdout "mc"             $'10 m+\nmc mr'        "0"

echo "=== Conversions d'unités ==="
test_stdout "m2cm"           "1 m2cm"       "100"
test_stdout "cm2m"           "100 cm2m"     "1"
test_stdout "kg2lb"          "1 kg2lb"      "2.20462"
test_stdout "c2f"            "0 c2f"        "32"
test_stdout "f2c"            "32 f2c"       "0"
test_stdout "c2k"            "0 c2k"        "273.15"
test_stdout "k2c"            "273.15 k2c"   "0"

echo "=== Précision configurable ==="
test_stdout "precision .3"    $'.3\n3.14159'  "3.14"
test_stdout "precision .1"    $'.1\n3.14159'  "3"

echo "=== Historique ==="
test_history "h affiche"       "1 2 +"    "1 2 +"

echo "=== Erreurs nouvelles ==="
test_stderr "fact négatif"    "-1 fact"  "argument invalide"
test_stderr "m+ vide"         "m+"       "pile vide"

echo "=== Régressions critiques ==="
test_stderr "token inconnu signalé"  "3 4 pluss"  "commande inconnue"

# Une ligne valide ne doit RIEN écrire sur stderr (garde-fou anti-erreur parasite)
test_no_stderr() {
    local name="$1" input="$2"
    local err
    err=$(printf "%s\n" "$input" | "$RPN" 2>&1 1>/dev/null)
    if [[ -z "$err" ]]; then
        echo "  ✓ $name"
        ((PASS++))
    else
        echo "  ✗ $name : stderr inattendu : $err"
        ((FAIL++))
    fi
}
test_no_stderr "addition silencieuse"  "1 2 +"
test_no_stderr "quit silencieux"       "quit"

# Persistance des variables : précision non tronquée (aller-retour exact)
test_var_precision() {
    local name="$1" expected="$2"
    local tmp found
    tmp=$(mktemp -d)
    ( cd "$tmp" && printf '3.141592653589793 sto x\nquit\n' | "$RPN" >/dev/null 2>&1 )
    found=$( ( cd "$tmp" && printf 'rcl x\nquit\n' | "$RPN" 2>/dev/null | tail -1) )
    rm -rf "$tmp"
    if [[ "$found" == "$expected" ]]; then
        echo "  ✓ $name"
        ((PASS++))
    else
        echo "  ✗ $name : attendu '$expected', obtenu '$found'"
        ((FAIL++))
    fi
}
test_var_precision "précision variable persistée" "3.141592654"

echo "=== Erreurs de domaine (ex-nan silencieux) ==="
test_stderr "mod par zéro"        "5 0 mod"   "modulo par zéro"
test_stderr "sqrt négatif"        "-1 sqrt"   "sqrt: argument négatif"
test_stderr "ln non positif"      "0 ln"      "ln:"
test_stderr "inv de zéro"         "0 inv"     "inv: division par zéro"
test_stderr "pow non défini"      "-8 0.5 pow" "pow:"
# Après une erreur, les opérandes sont RESTAURÉES sur la pile (pas de nan)
test_stdout "opérandes restaurées" $'5 0 /\n1 +' "1"

echo "=== Sortie batch sans ANSI ==="
test_no_ansi() {
    local name="$1" input="$2"
    if printf "%s\n" "$input" | "$RPN" 2>/dev/null | grep -q $'\033'; then
        echo "  ✗ $name : codes ANSI présents en sortie"
        ((FAIL++))
    else
        echo "  ✓ $name"
        ((PASS++))
    fi
}
test_no_ansi "pipe sans couleurs"    "3 4 +"
test_no_ansi "stack sans couleurs"   "3 4 + stack"
test_no_ansi "modes sans couleurs"   $'deg\nvisual\nhex\n3 4 +'
test_no_ansi "help sans couleurs"    "help"

echo "=== Langues de la calculatrice (i18n) ==="
test_stdout "lang : langue active"   "lang"  "langue: français  (LC_ALL / LANG, --lang=<xx>)"

# erreurs traduites : --lang prioritaire, sinon LC_ALL, sinon anglais
test_lang_err() {
    local name="$1" input="$2" expected="$3" locale="$4"; shift 4
    local err
    err=$(printf "%s\n" "$input" | env LC_ALL="$locale" "$RPN" "$@" 2>&1 1>/dev/null | tail -1)
    if [[ "$err" == *"$expected"* ]]; then
        echo "  ✓ $name"
        ((PASS++))
    else
        echo "  ✗ $name : attendu '$expected', obtenu '$err'"
        ((FAIL++))
    fi
}
test_lang_err "anglais forcé via --lang=en"  "1 +"  "Not enough operands" fr_FR.UTF-8 --lang=en
test_lang_err "español forcé via --lang=es"  "1 +"  "No hay suficientes"  fr_FR.UTF-8 --lang=es
test_lang_err "détection auto : es"          "5 0 /"  "división por cero"  es_ES.UTF-8
test_lang_err "détection auto : en"           "unknown" "unknown command"  en_US.UTF-8
test_lang_err "français (défaut des tests)"   "5 0 /"  "division par zéro"  fr_FR.UTF-8

test_stdout "ms formaté" $'5 m+ ms' "mémoire = 5"

echo "=== HEX/BIN robustes ==="
test_stdout "hex entier"       $'hex\n255'     "0xff"
test_stdout "hex négatif"        $'hex\n-5'      "-0x5"
test_stdout "hex non entier"     $'hex\n3.7'    "<non entier>"
test_stdout "hex hors plage"     $'hex\n1e20'   "<hors plage>"
test_stdout "bin entier"         $'bin\n5'      "0b101"
test_stdout "bin négatif"        $'bin\n-5'     "-0b101"
test_stdout "bin zéro"           $'bin\n0'      "0b0"
test_stdout "bin non entier"     $'bin\n1.5'    "<non entier>"
test_stdout "retour en dec"      $'hex\ndec\n255' "255"

echo "=== Sécurité ==="
test_stderr "!cmd interdit en batch"  "!ls"  "interdite"

# Ctrl-C doit sauvegarder l'historique avant de quitter (nécessite un pty → expect)
test_ctrl_c_save() {
    local name="$1" cmd="$2" expected="$3"
    if ! command -v expect >/dev/null 2>&1; then
        echo "  ? $name : expect indisponible, test ignoré"
        return
    fi
    local tmp
    tmp=$(mktemp -d) || return
    ( cd "$tmp" && expect -c "
        set timeout 5
        spawn $RPN
        send \"$cmd\r\"
        sleep 0.3
        send \"\x03\"
        expect eof
    " >/dev/null 2>&1 )
    if [[ ! -f "$tmp/.rpn_data/historique.txt" ]]; then
        echo "  ? $name : pty indisponible, test ignoré"
    elif grep -qF "$expected" "$tmp/.rpn_data/historique.txt"; then
        echo "  ✓ $name"
        ((PASS++))
    else
        echo "  ✗ $name : '$expected' absent de l'historique"
        ((FAIL++))
    fi
    rm -rf "$tmp"
}
test_ctrl_c_save "Ctrl-C sauve l'historique" "1 2 +" "1 2 +"

echo "=== BigInt — entiers exacts de taille arbitraire ==="
test_stdout "pow exact 2^128"  "2 128 pow" "340282366920938463463374607431768211456"
test_stdout "fact 50 exact"    "50 fact"   "30414093201713378043612608166064768844377641568960512000000000000"
test_stdout "division exacte"  "10 3 /"    "3"
test_stdout "reste exact"      "10 3 mod"  "1"
test_stdout "div tronquée"     "-7 2 /"    "-3"
test_stdout "reste signé"      "-7 2 mod"  "-1"
test_stdout "produit multi-chunks" "12345678901234567890 98765432109876543210 *" "1219326311370217952237463801111263526900"
test_stdout "au-delà de u64"   "18446744073709551616 1 +" "18446744073709551617"
test_stdout "hex d'un tapé grand" $'hex\n12345678901234567890' "0xab54a98ceb1f0ad2"
test_stdout "hex exact 2^64"   $'hex\n2 64 pow' "0x10000000000000000"
test_stdout "hex fact 20"     $'hex\n20 fact'  "0x21c3677c82b40000"
test_stdout "bin exact 2^8"    $'bin\n2 8 pow' "0b100000000"
test_stderr "fact trop grand"  "2000 fact" "trop grand"
test_stdout "pow neg expo → double" "2 -2 pow" "0.25"
# La pile est préservée après une erreur fact trop grand
test_stdout "fact trop grand restaure" $'2000 fact\ndrop 42' "42"

test_var_big() {
    # aller-retour exact d'un BigInt en persistance (marqueur « b »)
    local name="$1" input="$2" expected="$3"
    local tmp found
    tmp=$(mktemp -d) || return
    ( cd "$tmp" && printf '%s\nquit\n' "$input" | "$RPN" >/dev/null 2>&1 )
    found=$( ( cd "$tmp" && printf 'rcl x\nquit\n' | "$RPN" 2>/dev/null | tail -1) )
    rm -rf "$tmp"
    if [[ "$found" == "$expected" ]]; then
        echo "  ✓ $name"
        ((PASS++))
    else
        echo "  ✗ $name : attendu '$expected', obtenu '$found'"
        ((FAIL++))
    fi
}
test_var_big "BigInt persisté intact" "2 64 pow sto x" "18446744073709551616"

echo "=== Alias dc (Cherry & Morris, Bell Labs, 1971) ==="
test_stdout "dc d (dup)"        "5 d -"          "0"
test_stdout "dc ~ quotient+reste" "7 3 ~"        "1"
test_stdout "dc ~ signé"        "-7 3 ~"         "-1"
test_stdout "dc % (mod)"        "7 3 %"          "1"
test_stdout "dc ^ (pow)"        "2 10 ^"         "1024"
test_stdout "dc v (sqrt)"       "25 v"           "5"
test_stdout "dc p sans dépiler" $'42 p 1 +'      "43"
test_stdout "dc n dépile"       $'5 n 7'         "7"
test_stdout "dc z (profondeur)" "1 2 z"          "2"
test_stdout "dc c (clear)"      "1 2 c z"        "0"
test_stderr "~ par zéro restauré" "5 0 ~"        "division par zéro"

echo ""
echo "=== Résultat : $PASS réussi(s), $FAIL échoué(s) ==="
exit $FAIL
