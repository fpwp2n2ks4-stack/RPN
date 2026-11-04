#include "i18n.h"

#include <array>
#include <cstdlib>
#include <unordered_map>

namespace {

Lang g_lang = Lang::EN;

// Préfixe de locale : "fr_FR.UTF-8" → "fr"
std::string locale_prefix(const char* s) {
    if (!s) return {};
    std::string loc(s);
    size_t n = 0;
    while (n < loc.size() && loc[n] != '_' && loc[n] != '.' && loc[n] != '@') ++n;
    return loc.substr(0, n);
}

Lang lang_from_string(const std::string& s) {
    if (s == "fr") return Lang::FR;
    if (s == "es") return Lang::ES;
    return Lang::EN;   // défaut : anglais
}

Lang detect() {
    const char* env = std::getenv("LC_ALL");
    if (!env || !*env) env = std::getenv("LC_MESSAGES");
    if (!env || !*env) env = std::getenv("LANG");
    return lang_from_string(locale_prefix(env));
}

// Catalogue — clé = anglais. Ordre : EN, FR, ES.
const std::unordered_map<std::string, std::array<const char*, 3>>& catalog() {
    static const std::unordered_map<std::string, std::array<const char*, 3>> cat = {
        {"Error",                      {"Error",   "Erreur",   "Error"}},
        {"Result undefined (nan)",     {"Result undefined (nan)", "Résultat non défini (nan)", "Resultado no definido (nan)"}},
        {"Not enough operands (need 2, have %d)",
         {"Not enough operands (need 2, have %d)", "Pas assez d'opérandes (besoin 2, dispo %d)", "No hay suficientes operandos (necesito 2, hay %d)"}},
        {"Not enough operands (need 1, have 0)",
         {"Not enough operands (need 1, have 0)", "Pas assez d'opérandes (besoin 1, dispo 0)", "No hay suficientes operandos (necesito 1, hay 0)"}},
        {"Not enough operands (fact)",
         {"Not enough operands (fact)", "Pas assez d'opérandes (fact)", "No hay suficientes operandos (fact)"}},
        {"Cannot dup: stack is empty",
         {"Cannot dup: stack is empty", "impossible de dupliquer, la pile est vide", "no se puede duplicar, la pila está vacía"}},
        {"Cannot drop: stack is empty",
         {"Cannot drop: stack is empty", "impossible de supprimer, la pile est vide", "no se puede eliminar, la pila está vacía"}},
        {"Cannot swap: need 2 elements (have %d)",
         {"Cannot swap: need 2 elements (have %d)", "impossible d'échanger, besoin de 2 éléments (disponible %d)", "no se puede intercambiar, se necesitan 2 elementos (hay %d)"}},
        {"Stack is empty",           {"Stack is empty", "pile vide", "pila vacía"}},
        {"(empty)",                   {"(empty)", "(vide)", "(vacía)"}},
        {"Memory",                    {"memory", "mémoire", "memoria"}},
        {"Visual mode",               {"visual mode", "mode visuel", "modo visual"}},
        {"Angle mode",                {"Angle mode", "mode angle", "modo de ángulo"}},
        {"Display mode",              {"Display mode", "mode d'affichage", "modo de visualización"}},
        {"Unknown command",           {"unknown command", "commande inconnue", "comando desconocido"}},
        {"sto/rcl requires a variable name",
         {"sto/rcl requires a variable name", "sto/rcl nécessite un nom de variable", "sto/rcl necesita un nombre de variable"}},
        {"Shell escape (!) is only allowed in interactive sessions",
         {"Shell escape (!) is only allowed in interactive sessions",
          "commande shell (!) interdite hors du mode interactif",
          "el comando shell (!) solo está permitido en sesiones interactivas"}},
        {"Division by zero",          {"Division by zero", "division par zéro", "división por cero"}},
        {"Modulo by zero",            {"Modulo by zero", "modulo par zéro", "módulo por cero"}},
        {"pow: undefined operation (negative base, non-integer exponent)",
         {"pow: undefined operation (negative base, non-integer exponent)",
          "pow: opération non définie (base négative, exposant non entier)",
          "pow: operación no definida (base negativa, exponente no entero)"}},
        {"log: invalid arguments",    {"log: invalid arguments", "log: arguments invalides", "log: argumentos inválidos"}},
        {"sqrt: negative argument",   {"sqrt: negative argument", "sqrt: argument négatif", "sqrt: argumento negativo"}},
        {"inv: division by zero",     {"inv: division by zero", "inv: division par zéro", "inv: división por cero"}},
        {"ln: argument must be strictly positive",
         {"ln: argument must be strictly positive", "ln: argument non strictement positif", "ln: el argumento debe ser estrictamente positivo"}},
        {"log10: argument must be strictly positive",
         {"log10: argument must be strictly positive", "log10: argument non strictement positif", "log10: el argumento debe ser estrictamente positivo"}},
        {"Invalid argument for fact",
         {"Invalid argument for fact", "fact argument invalide", "argumento inválido para fact"}},
        {"Fact argument too large (max 1000)",
         {"Fact argument too large (max 1000)", "fact argument trop grand (max 1000)", "argumento de fact demasiado grande (máx 1000)"}},
        {"<out of range>",            {"<out of range>", "<hors plage>", "<fuera de rango>"}},
        {"<not an integer>",          {"<not an integer>", "<non entier>", "<no entero>"}},
        {"(no history yet)",          {"(no history yet)", "(pas d'historique)", "(sin historial)"}},
        {"Language",                  {"language", "langue", "idioma"}},
        // — aide —
        {"HELP — RPN Calculator",     {"HELP — RPN Calculator", "AIDE — Calculatrice RPN", "AYUDA — Calculadora RPN"}},
        {"Arithmetic",                {"Arithmetic", "Arithmétique", "Aritmética"}},
        {"Constants",                 {"Constants", "Constantes", "Constantes"}},
        {"Unary",                     {"Unary", "Unaires", "Unarias"}},
        {"Hyperbolic",                {"Hyperbolic", "Hyperboliques", "Hiperbólicas"}},
        {"Trig",                      {"Trig", "Trigo", "Trigo"}},
        {"StackC",                    {"Stack", "Pile", "Pila"}},
        {"MemoryC",                   {"Memory", "Mémoire", "Memoria"}},
        {"VariablesC",                {"Variables", "Variables", "Variables"}},
        {"Conversions",               {"Conversions", "Conversions", "Conversiones"}},
        {"Modes",                     {"Modes", "Modes", "Modos"}},
        {"Angles",                    {"Angles", "Angles", "Ángulos"}},
        {"Control",                    {"Control", "Contrôle", "Control"}},
        {"HistoryC",                  {"History", "Historique", "Historial"}},
        {"Precision",                 {"Precision", "Précision", "Precisión"}},
        {"(interactive only)",        {"(interactive only)", "(interactif uniquement)", "(solo interactivo)"}},
        {"(e.g. .6)",                 {"(e.g. .6)", "(ex : .6)", "(ej: .6)"}},
        {"LanguageC",                 {"Language", "Langue", "Idioma"}},
    };
    return cat;
}

} // namespace

void i18n_init(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg.rfind("--lang=", 0) == 0) {
            g_lang = lang_from_string(arg.substr(7));
            return;
        }
        if (arg == "--lang" && i + 1 < argc) {
            g_lang = lang_from_string(argv[++i]);
            return;
        }
    }
    g_lang = detect();
}

Lang lang_current() { return g_lang; }

const char* lang_name(Lang l) {
    switch (l) {
        case Lang::FR: return "français";
        case Lang::ES: return "español";
        default:       return "english";
    }
}

const char* T(const char* key) {
    const auto& cat = catalog();
    auto it = cat.find(key);
    if (it == cat.end()) return key;   // clé absente → l'anglais (la clé elle-même)
    return it->second[static_cast<int>(g_lang)];
}
