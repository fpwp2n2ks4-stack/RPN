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

echo "=== LAST X (registre HP-15C) ==="
test_stdout "lastx binaire"      "2 3 * lastx"                                "3"
test_stdout "lastx unaire"       "5 neg lastx"                                "5"
test_stdout "lastx divmod (~)"   "17 5 ~ lastx"                               "5"
test_stdout "lastx conversion"   "7 kg2lb lastx"                              "7"
test_stdout "lastx accumulateur" "9 m+ lastx"                                 "9"
test_stdout "alias lstx"         "10 4 / lstx"                                "4"
test_stdout "lastx non consommé" $'8 2 /\nlastx\nlastx'                      "2"
test_stdout "lastx exact (BigInt)" \
    "123456789123456789123456789 888888888888888888888888888888 mod lastx" \
    "888888888888888888888888888888"
test_stderr  "lastx sans opération"  "lastx"            "pas encore de LAST X"
test_stderr  "swap ne nourrit pas LAST X" "2 3 swap lastx" "pas encore de LAST X"

echo "=== Pourcentages HP (pct, d%, %t) ==="
test_stdout "pct : 5 pourcents de 200" "200 5 pct"        "10"
test_stdout "pct garde la référence"   "200 5 pct +"      "210"
test_stdout "d% : variation"           "80 100 d%"        "25"
test_stdout "d% : variation négative"  "100 80 d%"        "-20"
test_stdout "%t : part du total"      "200 50 %t"        "25"
test_stdout "pct nourrit LAST X"      "200 5 pct lastx"  "5"
test_stdout "pct sans division (réf. 0)" "0 5 pct"       "0"
test_stderr "d% référence nulle"      "0 50 d%"          "référence nulle"
test_stderr "%t référence nulle"      "0 50 %t"          "référence nulle"
test_stdout "% reste le modulo (dc)"  "17 5 %"           "2"

echo "=== Modes d'affichage sci/eng (todo §0.3) ==="
test_stdout "saisie scientifique (EEX)"   "1.5e3"                              "1500"
test_stdout "saisie exposant négatif"     "1e-2"                               "0.01"
test_stdout "affichage scientifique"      $'.6 sci\n12345'                     "1.2345e+4"
test_stdout "sci exposant négatif"        $'.6 sci\n-0.00042'                  "-4.2e-4"
test_stdout "sci zéro"                    $'.6 sci\n0'                        "0"
test_stdout "affichage ingénieur"         $'.6 eng\n0.0123'                   "12.3e-3"
test_stdout "eng exposant multiple de 3"  $'.6 eng\n12345'                    "12.345e+3"
test_stdout "eng petite valeur"           $'.6 eng\n0.0005'                   "500e-6"
test_stdout "eng arrondi renormalisé"     $'.2 eng\n999999'                   "1e+6"
test_stdout "retour au décimal"           $'.6 sci\n3.5\ndec\n2.5'            "2.5"
test_stdout "sci promeut le BigInt"       $'.6 sci\n123456789123456789123456789' "1.23457e+26"

echo "=== Alias de frappe HP-15C (todo §0.4) ==="
test_stdout "alias 1/x"        "4 1/x"        "0.25"
test_stdout "alias y^x"        "2 10 y^x"     "1024"
test_stdout "alias x<>y"       "1 2 x<>y"     "1"
test_stdout "alias x↔y"        "1 2 x↔y"      "1"
test_stdout "alias chs"        "5 chs"        "-5"
test_stdout "alias e^x"        "1 e^x"        "2.718281828"
test_stdout "alias nourrit LAST X" "5 chs lastx" "5"
test_stderr "1/x par zéro"     "0 1/x"        "division par zéro"

echo "=== Séparateur « ; » — retour chariot virtuel (todo §12) ==="
test_stdout "segments enchaînés"        "3 4 + ; 2 *"        "14"
test_stdout "écho de chaque segment"    "5 ; 7"              "7"
test_stdout "segment final soldé"       "8 2 / ;"            "4"
test_stdout "«;;» sans double écho"     $'4 ; ;\n6'          "6"
test_stdout "«;» seul ne fait rien"      $';\n3'              "3"
test_stdout "quit arrête la ligne"      "5 quit ; 3"         "5"
test_stdout "sto traverse le séparateur" $'42 sto ; x\nrcl x' "42"
test_history "historique par segment"   "8 2 / ;"            "8 2 /"

echo "=== Notation collée — frappes sans Entrée (todo §12) ==="
test_stdout "collé : 5;3+"             "5;3+"                 "8"
test_stdout "collé : addition"          "8 3+"                 "11"
test_stdout "collé : soustraction"       "12 5-"               "7"
test_stdout "collé : négatif"            "-5 3-"                "-8"
test_stdout "collé : tiret = soustraction" "8 3-2"             "2"
test_stdout "collé : puissance"           "2 10^"               "1024"
test_stdout "collé : modulo"              "10 3%"               "1"
test_stdout "collé : division"            "8 2/"                "4"
test_stdout "collé nourrit LAST X"        "5 3+ lastx"          "3"
test_stdout "collé : accumulateur"        $'9 5m+\nmr'           "5"
test_stdout "collé : BigInt exact"        "999999999999999999999999 1+" "1000000000000000000000000"
test_stderr "sto refuse le collage"       "42 sto5"             "commande inconnue"

echo "=== Sauvegarde / chargement explicites (todo §1) ==="
test_stdout "save puis load restaure"  $'5 sto sv1\nsave\n9 sto sv1\nload\nrcl sv1' "5"
test_stdout "alias savevar/loadvar"    $'5 sto sv1\nsavevar\nloadvar\nrcl sv1'      "5"

echo "=== Persistance JSON (todo §2) ==="
printf '7777 sto svj\nquit\n' | "$RPN" >/dev/null 2>&1
if grep -q '"svj": {"type": "b"' .rpn_data/variables.txt; then
    echo "  ✓ format JSON écrit"; ((PASS++))
else
    echo "  ✗ format JSON : entrée svj absente de variables.txt"; ((FAIL++))
fi
test_stdout "JSON : retour de session (BigInt)" "rcl svj" "7777"
printf '0.1 0.2 + sto svd\nquit\n' | "$RPN" >/dev/null 2>&1
test_stdout "JSON : aller-retour double" $'.18\nrcl svd' "0.300000000000000044"

echo "=== run <fichier> (todo §3) ==="
SCRIT=/tmp/rpn_script_$$.rpn
printf '3 4 +\n5 *\n' > "$SCRIT"
test_stdout "run exécute le script"    "run $SCRIT"       "35"
printf '# entête\n1 0 /\n7 7* # collé-commenté\n' > "$SCRIT"
test_stdout "run continue après erreur" "run $SCRIT"      "49"
test_stderr "run : l'erreur remonte"     "run $SCRIT"      "division par zéro"
printf '10 sto rjj\n' > "$SCRIT"
test_stdout "run : variables persistées" "run $SCRIT
rcl rjj" "10"
test_stderr "run : fichier absent"       "run /tmp/nexiste_pas_$$.rpn" "impossible d'ouvrir"

echo "=== Fichiers en argument (todo §4) ==="
F1=/tmp/rpn_b1_$$.rpn; F2=/tmp/rpn_b2_$$.rpn
printf '10\n' > "$F1"; printf '4 +\n' > "$F2"
trouve=$("$RPN" "$F1" "$F2" 2>/dev/null | tail -1)
if [[ "$trouve" == "14" ]]; then echo "  ✓ séquence, pile persistante"; ((PASS++)); else echo "  ✗ attendu 14, obtenu '$trouve'"; ((FAIL++)); fi
err=$("$RPN" /tmp/manquant_$$.rpn 2>&1 >/dev/null)
if echo "$err" | grep -qi "impossible d'ouvrir"; then echo "  ✓ fichier absent signalé"; ((PASS++)); else echo "  ✗ aucune erreur d'ouverture"; ((FAIL++)); fi
rm -f "$SCRIT" "$F1" "$F2"

echo "=== Commentaires (todo §5) ==="
test_stdout "commentaire #"             "3 4 + # somme"       "7"
test_stdout "commentaire //"            "6 7* // produit"      "42"
test_stdout "collé puis commentaire"    "8 2/ # division"    "4"
test_stdout "// coupe avant / collé"    "8 2// seuil"         "2"

echo "=== Mode debug (todo §8) ==="
test_stdout "debug : le résultat reste"   $'debug\n5 3+'       "8"
if printf 'debug\n5 3+\nquit\n' | "$RPN" 2>/dev/null | grep -c '\[DEBUG\]' > /dev/null; then
    echo "  ✓ trace [DEBUG] par token"; ((PASS++))
else
    echo "  ✗ aucune trace [DEBUG]"; ((FAIL++))
fi
if printf 'debug\n2 0 /\nquit\n' | "$RPN" 2>&1 | grep -c '\[DEBUG\] « / »' > /dev/null; then
    echo "  ✓ contexte d'erreur tracé (pile au moment du token)"; ((PASS++))
else
    echo "  ✗ le token fautif n'est pas tracé"; ((FAIL++))
fi
printf '5 3+\n' > /tmp/rpn_dbg_$$.rpn
if "$RPN" --debug /tmp/rpn_dbg_$$.rpn 2>/dev/null | grep -c '\[DEBUG\]' > /dev/null; then
    echo "  ✓ option --debug (fichier en argument)"; ((PASS++))
else
    echo "  ✗ --debug silencieux"; ((FAIL++))
fi
rm -f /tmp/rpn_dbg_$$.rpn
der=$(printf 'debug\ndebug\n5 3+\nquit\n' | "$RPN" 2>/dev/null | tail -1)
if [[ "$der" == "8" ]]; then
    echo "  ✓ debug s'éteint (bascule)"; ((PASS++))
else
    echo "  ✗ la bascule debug ne coupe pas les traces (dernier: '$der')"; ((FAIL++))
fi

echo "=== Historique : effacement (todo §7) ==="
if ! printf '41 1 +\nhclear\nh\nquit\n' | "$RPN" 2>/dev/null | grep -q '41 1 +'; then
    echo "  ✓ hclear vide l'historique"; ((PASS++))
else
    echo "  ✗ l'entrée 41 1 + survit"; ((FAIL++))
fi

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
