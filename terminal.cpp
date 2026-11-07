#include "terminal.h"

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <filesystem>

// ============================================================
// Boucle principale — lecture stdin (interactif ou lot), rendu
// des sorties structurées du moteur.
// ============================================================

void TerminalFrontend::run() {
    eng.load_state();

    interactive = isatty(STDIN_FILENO);
    use_color = isatty(STDOUT_FILENO);
    eng.set_allow_shell(interactive);
    if (interactive) {
        // Ctrl-C : handler vide sans SA_RESTART — le read() de la frappe
        // revient en EINTR, et le flux normal sauvegarde historique +
        // variables avant de quitter (même chemin que Ctrl-D).
        struct sigaction sa{};
        sa.sa_handler = [](int) {};
        sigaction(SIGINT, &sa, nullptr);
        prompt();
    }

    std::string line;
    while (true) {
        LineResult res;
        bool have = interactive ? read_line_interactive(line)
                                 : static_cast<bool>(std::getline(std::cin, line));
        if (!have) break;

        if (!line.empty()) {
            if (interactive) std::cout << "\n";
            // Hors mode interactif, on n'a pas d'écho clavier : on imprime la
            // ligne colorée. En interactif, la coloration live l'a déjà affichée.
            if (use_color && !interactive) colorize(line);
            res = eng.process_line(line);
            render(res);
        }
        if (res.quit) break;
        if (interactive) prompt();
    }
    if (interactive) std::cout << "\n";
    eng.save_state();
}

// ============================================================
// Mode fichier (todo §4) : les scripts passés en argument
// s'exécutent en séquence — la pile traverse les fichiers (décision :
// c'est une calculatrice, l'état coule), sans prompt. Fichier absent :
// erreur, on continue avec le suivant. L'état se sauvegarde à la fin,
// comme à la sortie du stdin.
// ============================================================

void TerminalFrontend::run_files(const std::vector<std::string>& chemins) {
    eng.load_state();
    interactive = false;
    use_color = isatty(STDOUT_FILENO) != 0;
    eng.set_allow_shell(false);   // un fichier ne pipe pas du shell
    for (const std::string& path : chemins) {
        std::ifstream f(path);
        if (!f) {
            char buf[512];
            std::snprintf(buf, sizeof buf, T("Cannot open file: %s"), path.c_str());
            std::cerr << T("Error") << ": " << buf << "\n";
            continue;
        }
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            if (use_color) colorize(line);
            LineResult res = eng.process_line(line);
            render(res);
            if (res.quit) {
                eng.save_state();
                return;
            }
        }
    }
    eng.save_state();
}

// ============================================================
// Rendu des sorties structurées
// ============================================================

void TerminalFrontend::render(const LineResult& res) {
    for (const Output& o : res.outputs) render_output(o);

    // Échappement shell : le moteur l'a remonté, l'exécution reste au
    // front-end interactif (gate !cmd hors session interactive côté moteur).
    if (!res.shell.empty()) {
        std::cout << "shell> " << res.shell << "\n";
        // MISRA 21.3.3 : std::system() interdit en code applicatif.
        // L'échappement est réservé au mode interactif (gate dans le moteur) ;
        // l'appel est conservé ici avec annotation de dérogation documentée —
        // c'est la prérogative du terminal, pas du moteur.
        [[maybe_unused]] int r = std::system(res.shell.c_str());  // NOLINT
    }

    if (!res.silent) display_result();
}

void TerminalFrontend::render_output(const Output& o) {
    switch (o.kind) {
        case OutKind::ERR:
            // Les erreurs voyagent en stderr, sans couleurs — comme print_err.
            std::cerr << T("Error") << ": " << o.text << "\n";
            break;
        case OutKind::MSG:
            std::cout << o.text << "\n";
            break;
        case OutKind::MODE: {
            const char* dim = use_color ? C::DIM : "";
            const char* rst = use_color ? C::RESET : "";
            switch (o.tag) {
                case InfoTag::VISUAL: {
                    // ON en vert/gras, OFF en rouge/gras (plat sinon)
                    std::string val = use_color
                        ? std::string(o.value == "ON" ? C::GREEN : C::RED)
                          + C::BOLD + o.value + C::RESET
                        : o.value;
                    std::cout << dim << o.text << " " << rst << val << "\n";
                    break;
                }
                case InfoTag::ANGLE: {
                    std::string val = use_color
                        ? std::string(C::YELLOW) + C::BOLD + o.value + C::RESET
                        : o.value;
                    std::cout << dim << o.text << ": " << rst << val << "\n";
                    break;
                }
                case InfoTag::DISPLAY: {
                    // ACCENT par mode : DEC cyan, HEX vert, BIN magenta
                    const char* accent = C::CYAN;
                    if (o.value == "HEX") accent = C::GREEN;
                    else if (o.value == "BIN") accent = C::MAGENTA;
                    std::string val = use_color
                        ? std::string(accent) + C::BOLD + o.value + C::RESET
                        : o.value;
                    std::cout << dim << o.text << ": " << rst << val << "\n";
                    break;
                }
                case InfoTag::MEMO: {
                    // « Mémoire = valeur » — pas de couleurs historically
                    std::cout << o.text << " = ";
                    print_num(o.v);
                    std::cout << "\n";
                    break;
                }
                case InfoTag::DEBUG: {
                    // debug ON vert / OFF rouge, comme le mode visuel
                    std::string val = use_color
                        ? std::string(o.value == "ON" ? C::GREEN : C::RED)
                          + C::BOLD + o.value + C::RESET
                        : o.value;
                    std::cout << dim << o.text << " " << rst << val << "\n";
                    break;
                }
            }
            break;
        }
        case OutKind::VALUE:
            print_num(o.v);
            std::cout << "\n";
            break;
        case OutKind::STACK:
            print_stack_lines(o.stack,
                              use_color ? C::GRAY : "", use_color ? C::RESET : "");
            break;
        case OutKind::HISTORY:
            for (size_t i = 0; i < o.lines.size(); ++i)
                std::cout << (i + 1) << ": " << o.lines[i] << "\n";
            break;
        case OutKind::HELP:
            render_help();
            break;
        case OutKind::DEBUG:
            // Trace du mode debug (todo §8) : grisée, préfixée —
            // jamais confondue avec un résultat.
            std::cout << (use_color ? C::GRAY : "") << "[DEBUG] " << o.text
                      << (use_color ? C::RESET : "") << "\n";
            break;
    }
}

// Écho du sommet (ou de la pile en mode visuel) quand la ligne n'a
// produit aucun affichage explicite.
void TerminalFrontend::display_result() const {
    const auto& st = eng.stack();
    if (st.empty()) { std::cout << "(?)\n"; return; }
    if (eng.is_visual()) {
        print_stack_lines(st,
                          use_color ? C::GRAY : "", use_color ? C::RESET : "");
        return;
    }
    print_num(st.back());
    std::cout << "\n";
}

// ============================================================
// Valeurs — formatage (présentation) : un BigInt s'affiche
// exactement (décimal, hex, bin) ; la précision `.N` ne s'applique
// qu'aux doubles.
// ============================================================

namespace {

// borne = 2^63 : les doubles au-delà ne tiennent pas dans un long long
bool fit_ll(double v) {
    constexpr double LL_BOUND = 9223372036854775808.0;  // 2^63 exact en double
    return !std::isnan(v) && v >= -LL_BOUND && v < LL_BOUND;
}

// Format scientifique (`base` = 10, mantisse ∈ [1,10)) ou ingénieur
// (`base` = 1000, mantisse ∈ [1,1000), exposant multiple de 3) :
// mantisse décimale arrondie aux `precision` chiffres significatifs —
// les zéros de queue disparaissent — puis exposant signé, `12.345e+3`.
// Le mantisse-exposant du printf n'est jamais utilisé : l'arrondi
// significatif est fait à la main, un débordement (9.9999 → 10, 999.999
// → 1000) se renormalise d'un pas d'exposant.
std::string format_mantisse_exp(double v, double base, int precision) {
    if (v == 0.0) return "0";
    bool negatif = std::signbit(v);
    double u = std::fabs(v);
    int pas = static_cast<int>(std::round(std::log10(base)));   // 1 (SCI) ou 3 (ENG)
    int exp10 = static_cast<int>(std::floor(std::log10(u)));
    int expo = static_cast<int>(std::floor(static_cast<double>(exp10) / pas)) * pas;
    double mant = u / std::pow(10.0, expo);

    // Arrondi aux chiffres significatifs renormalisé (garde-fou d'arrondi).
    int chiffres = std::max(1, precision);
    double echelle = std::pow(10.0, chiffres - 1 - std::floor(std::log10(mant)));
    double arrondi = std::round(mant * echelle) / echelle;
    if (arrondi >= base) {
        arrondi /= base;
        expo += pas;
    }

    // Mantisse en décimal pur : %g repasserait en `e` quand la mantisse
    // dépasse le nombre de chiffres — ici elle ne le peut jamais.
    int entiers = static_cast<int>(std::floor(std::log10(arrondi))) + 1;
    int decimales = std::max(0, chiffres - entiers);
    std::array<char, 64> buf{};
    std::snprintf(buf.data(), buf.size(), "%.*f", decimales, arrondi);
    std::string s(buf.data());
    if (s.find('.') != std::string::npos) {
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    return (negatif ? "-" : "") + s + "e"
           + (expo < 0 ? "-" : "+") + std::to_string(std::abs(expo));
}

void print_num_d(double v, DisplayMode mode, int precision) {
    if (mode == DisplayMode::SCI || mode == DisplayMode::ENG) {
        if (std::isnan(v))      std::cout << "nan";
        else if (std::isinf(v)) std::cout << (v > 0.0 ? "inf" : "-inf");
        else std::cout << format_mantisse_exp(
            v, mode == DisplayMode::SCI ? 10.0 : 1000.0, precision);
        return;
    }
    switch (mode) {
        case DisplayMode::HEX: {
            if (!fit_ll(v)) { std::cout << T("<out of range>"); break; }
            if (v != std::trunc(v)) { std::cout << T("<not an integer>"); break; }
            auto n = static_cast<long long>(v);
            bool neg = n < 0;
            auto m = neg ? -static_cast<unsigned long long>(n)
                         :  static_cast<unsigned long long>(n);
            std::cout << (neg ? "-0x" : "0x") << std::hex << m << std::dec;
            break;
        }
        case DisplayMode::BIN: {
            if (!fit_ll(v)) { std::cout << T("<out of range>"); break; }
            if (v != std::trunc(v)) { std::cout << T("<not an integer>"); break; }
            auto n = static_cast<long long>(v);
            bool neg = n < 0;
            auto m = neg ? -static_cast<unsigned long long>(n)
                         :  static_cast<unsigned long long>(n);
            if (m == 0u) { std::cout << "0b0"; break; }
            std::string s;
            while (m != 0u) { s = (m & 1u ? '1' : '0') + s; m >>= 1; }
            std::cout << (neg ? "-0b" : "0b") << s;
            break;
        }
        default: {
            if (std::isnan(v))      std::cout << "nan";
            else if (std::isinf(v)) std::cout << (v > 0.0 ? "inf" : "-inf");
            else {
                std::array<char, 64> buf{};
                std::snprintf(buf.data(), buf.size(), "%.*g", precision, v);
                std::cout << buf.data();
            }
        }
    }
}

}  // namespace

void TerminalFrontend::print_num(const Value& v) const {
    if (v.is_big()) {
        const BigInt& b = *v.bi;
        switch (eng.display_mode()) {
            case DisplayMode::HEX: std::cout << (b.negative() ? "-0x" : "0x") << b.to_hex(); break;
            case DisplayMode::BIN: std::cout << (b.negative() ? "-0b" : "0b") << b.to_bin(); break;
            // SCI/ENG : le mode parle en doubles, le BigInt est promu —
            // seule la mantisse affichée est arrondie (todo.md §0.3).
            case DisplayMode::SCI:
            case DisplayMode::ENG:
                print_num_d(v.to_double(), eng.display_mode(), eng.precision_value());
                break;
            default:               std::cout << b.to_string(); break;   // exact, précision ignorée
        }
        return;
    }
    print_num_d(v.d, eng.display_mode(), eng.precision_value());
}

// Affiche une pile (le sommet est v.back() — ordre du bas vers le haut)
void TerminalFrontend::print_stack_lines(const std::vector<Value>& v,
                                         const char* idx_col,
                                         const char* reset_col) const {
    for (size_t i = 0; i < v.size(); ++i) {
        std::cout << idx_col << (i + 1) << ": " << reset_col;
        print_num(v[i]);
        std::cout << "\n";
    }
}

// ============================================================
// Coloration — catégories du registre moteur, palette du terminal
// ============================================================

void TerminalFrontend::prompt() const {
    std::cout << C::CYAN << C::BOLD << "rpn" << C::RESET << C::DIM << ">" << C::RESET << " " << std::flush;
}

std::string TerminalFrontend::cat_color(CmdCat cat) const {
    switch (cat) {
        case CmdCat::ARITH:
        case CmdCat::CONSTANT:
        case CmdCat::VAR:    return std::string(C::YELLOW) + C::BOLD;
        case CmdCat::UNARY:  return C::GREEN;
        case CmdCat::TRIG:
        case CmdCat::MODE:   return C::CYAN;
        case CmdCat::HYP:    return C::MAGENTA;
        case CmdCat::STACK:  return std::string(C::BLUE) + C::BOLD;
        case CmdCat::MEM:
        case CmdCat::QUIT:   return std::string(C::RED) + C::BOLD;
        case CmdCat::CONV:
        case CmdCat::HIST:   return C::GRAY;
        case CmdCat::HELP:   return std::string(C::WHITE) + C::BOLD;
    }
    return "";
}

std::string TerminalFrontend::token_color(const std::string& tok) const {
    const RegistryEntry* e = eng.lookup(tok);
    return e ? cat_color(e->cat) : std::string();
}

// Couleur d'un token incomplet : match exact, ou préfixe UNIQUE d'une
// commande — permet de colorer dès la frappe, avant le délimiteur.
// Préfixe ambigu ("si" → sin/sinh) → en clair jusqu'à désambiguïsation.
std::string TerminalFrontend::color_for_partial(const std::string& tok) const {
    if (std::optional<CmdCat> cat = eng.category_for_partial(tok))
        return cat_color(*cat);
    return "";
}

void TerminalFrontend::print_colored_tokens(const std::string& line) const {
    std::istringstream iss(line);
    std::string tok;
    bool first = true;
    while (iss >> tok) {
        if (!first) std::cout << ' ';
        first = false;
        std::string col = token_color(tok);
        if (!col.empty()) std::cout << col << tok << C::RESET;
        else std::cout << tok;
    }
}

void TerminalFrontend::colorize(const std::string& line) const {
    if (line[0] == '!') { std::cout << line << "\n"; return; }
    print_colored_tokens(line);
    std::cout << "\n";
}

// ============================================================
// Lecture interactive (flèches, historique)
// ============================================================

bool TerminalFrontend::read_line_interactive(std::string& out) {
    out.clear();
    int hist_idx = -1;
    std::string saved;

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    // Le complément reste dans le type du champ (tcflag_t) : pas de
    // passage signé/non-signé (-Wsign-conversion, esprit MISRA).
    newt.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    bool ok = true;
    while (true) {
        int c = read_char_raw();
        if (c == -1) { eng.save_state(); ok = false; break; }
        if (c == '\n') { recolor_last_token(out); break; }
        if (c == 127 || c == 8) {
            if (!out.empty()) { out.pop_back(); std::cout << "\b \b" << std::flush; recolor_last_token(out); }
        } else if (c == 18) {
            // Ctrl-R (todo §7) : recherche dans l'historique — une entrée
            // retenue est soumise (l'équivalent du rappel + Entrée).
            if (recherche_interactive(out)) break;
        } else if (c == 27) {
            int c2 = read_char_raw(); if (c2 == -1) { ok = false; break; }
            if (c2 == '[') {
                int c3 = read_char_raw(); if (c3 == -1) { ok = false; break; }
                const auto& hist = eng.history_entries();
                if (c3 == 'A' && !hist.empty()) {
                    if (hist_idx == -1) { saved = out; hist_idx = static_cast<int>(hist.size()) - 1; }
                    else if (hist_idx > 0) --hist_idx;
                    replace_line(out, hist[static_cast<size_t>(hist_idx)]);
                } else if (c3 == 'B' && hist_idx >= 0) {
                    ++hist_idx;
                    if (hist_idx >= static_cast<int>(hist.size())) { hist_idx = -1; replace_line(out, saved); }
                    else replace_line(out, hist[static_cast<size_t>(hist_idx)]);
                }
            }
        } else if (c == ' ') {
            // Fin de token : on le recolore si c'est une commande connue
            recolor_last_token(out);
            out += ' ';
            std::cout << ' ' << std::flush;
        } else if (c >= 32) {
            out += static_cast<char>(c);
            std::cout << static_cast<char>(c) << std::flush;
            recolor_last_token(out);
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ok;
}

// ============================================================
// Ctrl-R — recherche arrière dans l'historique (todo §7)
// ============================================================

// Chaque frappe affine la requête et saute au match le plus récent ;
// un Ctrl-R de plus passe au match plus ancien ; Entrée retient le
// match ET le soumet (l'équivalent du rappel suivi d'Entrée) ; Échap
// (ou Entrée sans match) annule — la frappe reprend où elle était,
// la ligne de recherche s'efface d'un retour chariot + effacement.
bool TerminalFrontend::recherche_interactive(std::string& out) {
    std::string requete;
    int position = -1;   // dernier match montré (négatif : pas de match)
    std::cout << "\n(recherche): " << std::flush;
    while (true) {
        int c = read_char_raw();
        if (c == -1) return false;
        if (c == '\n') {
            if (position < 0) {          // rien à retenir : on repart frapper
                std::cout << "\n";
                print_colored_tokens(out);
                std::cout << std::flush;
                return false;
            }
            out = eng.history_entries()[static_cast<size_t>(position)];
            return true;
        }
        if (c == 27) {                  // Échap : annulé
            std::cout << "\n";
            print_colored_tokens(out);
            std::cout << std::flush;
            return false;
        }
        int depart;
        if (c == 18) {                  // Ctrl-R encore : plus ancien
            depart = position - 1;
        } else if ((c == 127 || c == 8) && !requete.empty()) {
            requete.pop_back();
            depart = static_cast<int>(eng.history_entries().size()) - 1;
        } else if (c >= 32) {
            requete += static_cast<char>(c);
            depart = static_cast<int>(eng.history_entries().size()) - 1;
        } else {
            continue;
        }
        position = -1;
        const auto& hist = eng.history_entries();
        for (int i = depart; i >= 0 && i < static_cast<int>(hist.size()); --i) {
            if (hist[static_cast<size_t>(i)].find(requete) != std::string::npos) {
                position = i;
                break;
            }
        }
        std::string apercu = position >= 0
            ? hist[static_cast<size_t>(position)] : std::string("");
        std::cout << "\r\033[K(recherche: " << requete << "): " << apercu << std::flush;
    }
}

void TerminalFrontend::replace_line(std::string& field, const std::string& val) {
    while (!field.empty()) { field.pop_back(); std::cout << "\b \b"; }
    field = val;
    print_colored_tokens(field);
    std::cout << std::flush;
}

// Réécrit à l'écran le token en cours de frappe avec sa couleur du moment :
// couleur de sa catégorie (match exact ou préfixe unique), sinon en clair.
void TerminalFrontend::recolor_last_token(const std::string& buf) const {
    if (!use_color) return;
    size_t start = buf.find_last_of(' ');
    start = (start == std::string::npos) ? 0 : start + 1;
    std::string tok = buf.substr(start);
    if (tok.empty()) return;
    std::string col = color_for_partial(tok);
    for (size_t i = 0; i < tok.size(); ++i) std::cout << '\b';
    std::cout << col << tok << C::RESET << std::flush;
}

int TerminalFrontend::read_char_raw() {
    unsigned char c{};
    return read(STDIN_FILENO, &c, 1) == 1 ? static_cast<int>(c) : -1;
}

// ============================================================
// Help — boîte générée (largeurs calculées : alignement garanti
// EN/FR/ES). Rendu du terminal : le moteur signale HELP, la boîte
// est notre façon à nous de la dessiner.
// ============================================================

namespace {

// longueur visible : saute les séquences ANSI, compte 1 par caractère UTF-8
size_t vislen(const std::string& s) {
    size_t n = 0, i = 0;
    while (i < s.size()) {
        if (s.compare(i, 2, "\033[") == 0) {
            size_t j = s.find('m', i);
            if (j == std::string::npos) break;
            i = j + 1;
        } else {
            if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) ++n;
            ++i;
        }
    }
    return n;
}

// L'octet où commence la colonne VISIBLE `col` : les accents UTF-8 et les
// séquences ANSI pèsent plus d'un octet, mais l'alignement des colonnes
// du bandeau se compte en caractères affichés.
size_t byte_of_col(const std::string& s, size_t col) {
    size_t n = 0, i = 0;
    while (i < s.size() && n < col) {
        if (s.compare(i, 2, "\033[") == 0) {
            size_t j = s.find('m', i);
            if (j == std::string::npos) break;
            i = j + 1;
        } else {
            if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) ++n;
            ++i;
        }
    }
    return i;
}

}  // namespace

void TerminalFrontend::render_help() const {
    bool c = use_color;
    auto cY = [c](const char* s) { return c ? std::string(C::YELLOW) + C::BOLD + s + C::RESET : std::string(s); };
    auto cG = [c](const char* s) { return c ? std::string(C::GREEN) + s + C::RESET : std::string(s); };
    auto cC = [c](const char* s) { return c ? std::string(C::CYAN) + s + C::RESET : std::string(s); };
    auto cM = [c](const char* s) { return c ? std::string(C::MAGENTA) + s + C::RESET : std::string(s); };
    auto cB = [c](const char* s) { return c ? std::string(C::BLUE) + C::BOLD + s + C::RESET : std::string(s); };
    auto cR = [c](const char* s) { return c ? std::string(C::RED) + C::BOLD + s + C::RESET : std::string(s); };
    auto cW = [c](const char* s) { return c ? std::string(C::WHITE) + C::BOLD + s + C::RESET : std::string(s); };
    auto cD = [c](const char* s) { return c ? std::string(C::GRAY) + s + C::RESET : std::string(s); };

    constexpr size_t W = 65;   // largeur utile entre les deux bordures
    auto rep = [](const char* s) {
        std::string r;
        for (size_t i = 0; i < W; ++i) r += s;
        return r;
    };
    std::string top("╔" + rep("═") + "╗\n");
    std::string sep("╠" + rep("═") + "╣\n");
    std::string bot("╚" + rep("═") + "╝\n");

    // place un fragment coloré à la colonne visible demandée, sans laisser
    // les octets des accents ni des séquences ANSI décaler la suite.
    auto place = [](std::string& buf, size_t col, const std::string& frag) {
        buf = buf.substr(0, byte_of_col(buf, col))
            + frag
            + buf.substr(byte_of_col(buf, col + vislen(frag)));
    };
    auto render1 = [&](const std::string& s, size_t col) {
        std::string buf(W, ' ');
        if (col + vislen(s) <= W)
            place(buf, col, s);
        return std::string("║") + buf + "║\n";
    };
    auto row2 = [&](const std::string& label, const std::string& toks,
                    const std::string& right) {
        std::string buf(W, ' ');
        if (!label.empty()) {
            std::string l = label + ":";
            if (2 + vislen(l) <= W)
                place(buf, 2, l);
        }
        if (!toks.empty() && 17 + vislen(toks) <= W)
            place(buf, 17, toks);
        if (!right.empty() && 40 + vislen(right) <= W)
            place(buf, 40, right);
        return std::string("║") + buf + "║\n";
    };

    std::string hdr = T("HELP — RPN Calculator");
    std::string out("\n" + top);
    out += render1(hdr, (W - vislen(hdr)) / 2);
    out += sep;
    out += row2(T("Arithmetic"), cY("+") + "  " + cY("-") + "  " + cY("*") + "  " + cY("/") + "  "
                + cY("pow") + "  " + cY("log") + "  " + cY("mod"), "");
    out += row2("", cY("atan2") + "  " + cY("pct") + "  " + cY("d%") + "  " + cY("%t"), "");
    out += sep;
    out += row2(T("Constants"), cW("pi") + "  " + cW("e"), "");
    out += row2(T("Unary"), cG("neg") + "  " + cG("sqrt") + "  " + cG("square") + "  " + cG("inv")
                + "  " + cG("abs") + "  " + cG("exp") + "  " + cG("ln") + "  " + cG("log10"),
                "");
    out += row2("", cG("floor") + "  " + cG("ceil") + "  " + cG("round") + "  " + cG("trunc")
                + "  " + cG("fact"), "");
    out += row2(T("Trig"), cC("sin") + "  " + cC("cos") + "  " + cC("tan") + "  " + cC("asin")
                + "  " + cC("acos") + "  " + cC("atan"), "");
    out += row2(T("Hyperbolic"), cM("sinh") + "  " + cM("cosh") + "  " + cM("tanh"), "");
    out += row2(T("StackC"), cB("dup") + "  " + cB("drop") + "  " + cB("swap") + "  " + cB("lastx")
                + "  " + cB("clear") + "  " + cB("stack") + " (" + cB("ls") + ")", "");
    out += row2("dc 1971", cB("d") + "  " + cB("c") + "  " + cB("f") + "  " + cB("r") + "  "
                + cB("p") + "  " + cB("n") + "  " + cB("z") + "  " + cG("v") + "  "
                + cY("^") + "  " + cY("%") + "  " + cY("~"), "");
    // Les touches HP-15C frappeables : mêmes opérations que les commandes
    // longues (alias du registre, todo.md §0.4) — touche colorée par sa
    // catégorie, équivalent grisé pour la découverte.
    out += row2("HP-15C", cG("1/x") + cD("=") + cD("inv") + "  "
                + cY("y^x") + cD("=") + cD("pow") + "  "
                + cB("x↔y") + cD("=") + cD("swap") + "  "
                + cG("chs") + cD("=") + cD("neg") + "  "
                + cG("e^x") + cD("=") + cD("exp"), "");
    out += sep;
    out += row2(T("MemoryC"), cR("m+") + "  " + cR("m-") + "  " + cR("m*") + "  " + cR("m/")
                + "  " + cR("mr") + "  " + cR("mc") + "  " + cR("ms"), "");
    out += row2(T("VariablesC"), cY("sto") + " x  /  " + cY("rcl") + " x", "");
    out += row2(T("ScriptC"), cY("save") + "  " + cY("load") + "  " + cY("run") + " f", "#  //");
    out += row2(T("Conversions"), cD("m2cm") + "  " + cD("cm2m") + "  " + cD("m2ft") + "  "
                + cD("ft2m") + "  " + cD("kg2lb") + "  " + cD("lb2kg"), "");
    out += row2("", cD("c2f") + "  " + cD("f2c") + "  " + cD("c2k") + "  " + cD("k2c"), "");
    out += sep;
    out += row2(T("Modes"), cC("dec") + "  " + cC("hex") + "  " + cC("bin") + "  " + cC("sci") + "  "
                + cC("eng") + "  " + cC("visual") + "  " + cC("debug"), "");
    out += row2("", cD(".N") + " " + T("(e.g. .6)"), "");
    out += row2(T("Angles"), cY("deg") + "  " + cY("rad"), "");
    // La notation scientifique à la frappe — la touche EEX des HP.
    out += row2("EEX", cW("1.5e3") + "  " + cW("6.02e23") + "  " + cW("1e-2"), "");
    out += row2(T("SeparatorC"), cD(";"), T("(virtual Enter)"));
    out += row2("Shell", cY("!cmd") + " " + T("(interactive only)"), "");
    out += row2(T("HistoryC"), cD("h") + "  " + cD("hist") + "  " + cD("history") + "  "
                + cD("hclear") + "   ↑↓  ^R", "");
    out += row2(T("LanguageC"), cW("lang"), "LC_ALL / LANG");
    out += sep;
    out += row2(T("Control"), cR("quit") + " / " + cR("exit") + "   " + cW("help"), "");
    out += bot;
    out += "\n";
    std::cout << out;
}
