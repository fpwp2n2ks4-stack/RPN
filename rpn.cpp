#include "rpn.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <charconv>
#include <iomanip>
#include <limits>
#include <array>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>

// kPi/kE sont des macros POSIX, pas du C++23 strict — constantes propres.
constexpr double kPi = 3.141592653589793;
constexpr double kE  = 2.718281828459045;

// ============================================================
// Constructeur — enregistrement des commandes
// ============================================================

RPN::RPN() {
    // Binaires (versions exactes BigInt : + - * / mod pow ; log/atan2 → double)
    reg("+",     CmdCat::ARITH, new BinaryOp(std::plus<double>(),
        "Result undefined (nan)", &big::add));
    reg("-",     CmdCat::ARITH, new BinaryOp(std::minus<double>(),
        "Result undefined (nan)", &big::sub));
    reg("*",     CmdCat::ARITH, new BinaryOp(std::multiplies<double>(),
        "Result undefined (nan)", &big::mul));
    reg("/",     CmdCat::ARITH, new BinaryOp([](double a, double b) {
        return b == 0.0 ? std::numeric_limits<double>::quiet_NaN() : a / b;
    }, "Division by zero", &big::div_quot));
    reg("pow",   CmdCat::ARITH, new BinaryOp([](double a, double b) { return std::pow(a, b); },
        "pow: undefined operation (negative base, non-integer exponent)", &big::pow_big));
    reg("log",   CmdCat::ARITH, new BinaryOp([](double a, double b) {
        return (a <= 0.0 || b <= 0.0 || b == 1.0)
            ? std::numeric_limits<double>::quiet_NaN() : std::log(a) / std::log(b);
    }, "log: invalid arguments"));
    reg("mod",   CmdCat::ARITH, new BinaryOp([](double a, double b) {
        return b == 0.0 ? std::numeric_limits<double>::quiet_NaN() : std::fmod(a, b);
    }, "Modulo by zero", &big::mod));
    reg("atan2", CmdCat::ARITH, new BinaryOp([this](double a, double b) {
        double r = std::atan2(a, b);
        return deg_mode ? r * 180.0 / kPi : r;
    }));

    // Constantes
    reg("pi", CmdCat::CONSTANT, new PushConst(kPi));
    reg("e",  CmdCat::CONSTANT, new PushConst(kE));

    // Unaires classiques (neg/abs/square/floor/ceil/round/trunc exacts en BigInt)
    reg("neg",    CmdCat::UNARY, new UnaryOp([](double x) { return -x; },
        "Result undefined (nan)", &big::neg));
    reg("sqrt",   CmdCat::UNARY, new UnaryOp([](double x) {
        return x < 0.0 ? std::numeric_limits<double>::quiet_NaN() : std::sqrt(x);
    }, "sqrt: negative argument"));
    reg("square", CmdCat::UNARY, new UnaryOp([](double x) { return x * x; },
        "Result undefined (nan)", &big::square));
    reg("inv",    CmdCat::UNARY, new UnaryOp([](double x) {
        return x == 0.0 ? std::numeric_limits<double>::quiet_NaN() : 1.0 / x;
    }, "inv: division by zero"));
    reg("exp",    CmdCat::UNARY, new UnaryOp([](double x) { return std::exp(x); }));
    reg("ln",     CmdCat::UNARY, new UnaryOp([](double x) {
        return x <= 0.0 ? std::numeric_limits<double>::quiet_NaN() : std::log(x);
    }, "ln: argument must be strictly positive"));
    reg("log10",  CmdCat::UNARY, new UnaryOp([](double x) {
        return x <= 0.0 ? std::numeric_limits<double>::quiet_NaN() : std::log10(x);
    }, "log10: argument must be strictly positive"));
    reg("abs",    CmdCat::UNARY, new UnaryOp([](double x) { return std::fabs(x); },
        "Result undefined (nan)", &big::abs));
    reg("floor",  CmdCat::UNARY, new UnaryOp([](double x) { return std::floor(x); },
        "Result undefined (nan)", &big::same));
    reg("ceil",   CmdCat::UNARY, new UnaryOp([](double x) { return std::ceil(x); },
        "Result undefined (nan)", &big::same));
    reg("round",  CmdCat::UNARY, new UnaryOp([](double x) { return std::round(x); },
        "Result undefined (nan)", &big::same));
    reg("trunc",  CmdCat::UNARY, new UnaryOp([](double x) { return std::trunc(x); },
        "Result undefined (nan)", &big::same));

    // Factorielle
    reg("fact", CmdCat::UNARY, new FactOp());

    // Trigonométrie
    reg("sin",  CmdCat::TRIG, new UnaryOp([this](double x) {
        return std::sin(deg_mode ? x * kPi / 180.0 : x); }));
    reg("cos",  CmdCat::TRIG, new UnaryOp([this](double x) {
        return std::cos(deg_mode ? x * kPi / 180.0 : x); }));
    reg("tan",  CmdCat::TRIG, new UnaryOp([this](double x) {
        return std::tan(deg_mode ? x * kPi / 180.0 : x); }));
    reg("asin", CmdCat::TRIG, new UnaryOp([this](double x) {
        double r = std::asin(x); return deg_mode ? r * 180.0 / kPi : r; }));
    reg("acos", CmdCat::TRIG, new UnaryOp([this](double x) {
        double r = std::acos(x); return deg_mode ? r * 180.0 / kPi : r; }));
    reg("atan", CmdCat::TRIG, new UnaryOp([this](double x) {
        double r = std::atan(x); return deg_mode ? r * 180.0 / kPi : r; }));

    // Hyperboliques
    reg("sinh", CmdCat::HYP, new UnaryOp([](double x) { return std::sinh(x); }));
    reg("cosh", CmdCat::HYP, new UnaryOp([](double x) { return std::cosh(x); }));
    reg("tanh", CmdCat::HYP, new UnaryOp([](double x) { return std::tanh(x); }));

    // Pile
    reg("dup",   CmdCat::STACK, new Dup());
    reg("drop",  CmdCat::STACK, new Drop());
    reg("swap",  CmdCat::STACK, new Swap());
    reg("clear", CmdCat::STACK, new Clear());
    reg("stack", CmdCat::STACK, new StackCmd());
    reg("ls",    CmdCat::STACK, new StackCmd());

    // Alias dc (Lorinda Cherry & Robert Morris, Bell Labs, 1971) :
    // d c f r p n z pour la pile, v sqrt, ^ pow, % mod, ~ quotient+reste
    reg("d", CmdCat::STACK, new Dup());
    reg("c", CmdCat::STACK, new Clear());
    reg("f", CmdCat::STACK, new StackCmd());
    reg("r", CmdCat::STACK, new Swap());
    reg("p", CmdCat::STACK, new PrintTop());
    reg("n", CmdCat::STACK, new PrintPop());
    reg("z", CmdCat::STACK, new StackDepth());
    reg("v", CmdCat::UNARY, new UnaryOp([](double x) {
        return x < 0.0 ? std::numeric_limits<double>::quiet_NaN() : std::sqrt(x);
    }, "sqrt: negative argument"));
    reg("^", CmdCat::ARITH, new BinaryOp([](double a, double b) { return std::pow(a, b); },
        "pow: opération non définie (base négative, exposant non entier)", &big::pow_big));
    reg("%", CmdCat::ARITH, new BinaryOp([](double a, double b) {
        return b == 0.0 ? std::numeric_limits<double>::quiet_NaN() : std::fmod(a, b);
    }, "modulo par zéro", &big::mod));
    reg("~", CmdCat::ARITH, new DivmodOp());

    // Modes
    reg("visual", CmdCat::MODE, new VisualToggle());
    reg("deg",    CmdCat::MODE, new AngleSetter(true));
    reg("rad",    CmdCat::MODE, new AngleSetter(false));
    reg("dec",    CmdCat::MODE, new DisplaySetter(DisplayMode::DEC));
    reg("hex",    CmdCat::MODE, new DisplaySetter(DisplayMode::HEX));
    reg("bin",    CmdCat::MODE, new DisplaySetter(DisplayMode::BIN));

    // Variables nommées
    reg("sto", CmdCat::VAR, new StoRcl(true));
    reg("rcl", CmdCat::VAR, new StoRcl(false));

    // Mémoire accumulateur
    reg("m+", CmdCat::MEM, new MemoryOp(MemOp::ADD));
    reg("m-", CmdCat::MEM, new MemoryOp(MemOp::SUB));
    reg("m*", CmdCat::MEM, new MemoryOp(MemOp::MUL));
    reg("m/", CmdCat::MEM, new MemoryOp(MemOp::DIV));
    reg("mr", CmdCat::MEM, new MemoryOp(MemOp::RECALL));
    reg("mc", CmdCat::MEM, new MemoryOp(MemOp::CLEAR));
    reg("ms", CmdCat::MEM, new MemShow());

    // Historique
    reg("history", CmdCat::HIST, new HistoryCmd());
    reg("hist",    CmdCat::HIST, new HistoryCmd());
    reg("h",       CmdCat::HIST, new HistoryCmd());

    // Conversions d'unités
    reg("m2cm",  CmdCat::CONV, new UnitConv(100.0));
    reg("cm2m",  CmdCat::CONV, new UnitConv(0.01));
    reg("m2ft",  CmdCat::CONV, new UnitConv(3.28084));
    reg("ft2m",  CmdCat::CONV, new UnitConv(0.3048));
    reg("kg2lb", CmdCat::CONV, new UnitConv(2.20462));
    reg("lb2kg", CmdCat::CONV, new UnitConv(0.453592));
    reg("c2f", CmdCat::CONV, new UnaryOp([](double c){ return c * 9.0 / 5.0 + 32.0; }));
    reg("f2c", CmdCat::CONV, new UnaryOp([](double f){ return (f - 32.0) * 5.0 / 9.0; }));
    reg("c2k", CmdCat::CONV, new UnaryOp([](double c){ return c + 273.15; }));
    reg("k2c", CmdCat::CONV, new UnaryOp([](double k){ return k - 273.15; }));

    // Contrôle
    reg("help", CmdCat::HELP, new HelpCmd());
    reg("lang", CmdCat::HELP, new LangCmd());
    reg("quit", CmdCat::QUIT, new QuitCmd());
    reg("exit", CmdCat::QUIT, new QuitCmd());
}

// ============================================================
// Boucle principale
// ============================================================

void RPN::run() {
    load_history();
    load_variables();

    bool interactive = isatty(STDIN_FILENO);
    is_interactive = interactive;
    use_color = isatty(STDOUT_FILENO);
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
    while (interactive ? read_line_interactive(line) : static_cast<bool>(std::getline(std::cin, line))) {
        if (!line.empty()) {
            if (interactive) std::cout << "\n";
            process_line(line);
        }
        if (quit_requested) break;
        if (interactive) prompt();
    }
    if (interactive) std::cout << "\n";
    save_history();
    save_variables();
}

// ============================================================
// Traitement d'une ligne
// ============================================================

void RPN::process_line(const std::string& line) {
    if (line.empty()) return;

    if (line[0] == '!' && line.size() > 1 && line[1] != '!') {
        // Échappement shell réservé au session interactive : un `!cmd` venu
        // d'un pipe ou d'un fichier exécuterait du code sans accord utilisateur.
        if (!is_interactive) { print_err(T("Shell escape (!) is only allowed in interactive sessions")); return; }
        Context ctx{st, memory, history, visual_mode, deg_mode, disp_mode,
                    precision, quit_requested, expect_var, var_name, use_color};
        ShellExec(line.substr(1)).exec(ctx);
        return;
    }

    // Hors mode interactif, on n'a pas d'écho clavier : on imprime la ligne
    // colorée. En interactif, la coloration live l'a déjà affichée.
    if (use_color && !is_interactive) colorize(line);

    history.push_back(line);
    if (static_cast<int>(history.size()) > HIST_MAX)
        history.erase(history.begin());

    std::istringstream iss(line);
    std::string tok;
    bool silent = false;
    Context ctx{st, memory, history, visual_mode, deg_mode, disp_mode,
                precision, quit_requested, expect_var, var_name, use_color};
    while (iss >> tok) silent = process_token(tok, ctx);

    if (expect_var) {
        print_err(T("sto/rcl requires a variable name"));
        expect_var = false;
    }

    if (!silent) display_result();
}

void RPN::display_result() const {
    if (st.empty()) { std::cout << "(?)\n"; return; }
    if (visual_mode) { print_stack(); return; }
    print_num(st.back(), disp_mode, precision);
    std::cout << "\n";
}

void RPN::print_stack() const {
    print_stack_lines(st, disp_mode, precision,
                      use_color ? C::GRAY : "", use_color ? C::RESET : "");
}

void RPN::prompt() const { std::cout << C::CYAN << C::BOLD << "rpn" << C::RESET << C::DIM << ">" << C::RESET << " " << std::flush; }

std::string RPN::cat_color(CmdCat cat) const {
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

std::string RPN::token_color(const std::string& tok) const {
    auto it = registry.find(tok);
    if (it == registry.end()) return "";
    return cat_color(it->second.cat);
}

// Couleur d'un token incomplet : match exact, ou préfixe UNIQUE d'une
// commande — permet de colorer dès la frappe, avant le délimiteur.
// Préfixe ambigu ("si" → sin/sinh) → en clair jusqu'à désambiguïsation.
std::string RPN::color_for_partial(const std::string& tok) const {
    if (std::string col = token_color(tok); !col.empty()) return col;
    const RegistryEntry* uniq = nullptr;
    for (const auto& [name, e] : registry) {
        if (name.rfind(tok, 0) == 0) {
            if (uniq) return "";
            uniq = &e;
        }
    }
    return uniq ? cat_color(uniq->cat) : "";
}

void RPN::print_colored_tokens(const std::string& line) const {
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

void RPN::colorize(const std::string& line) const {
    if (line[0] == '!') { std::cout << line << "\n"; return; }
    print_colored_tokens(line);
    std::cout << "\n";
}

// ============================================================
// Traitement d'un token
// ============================================================

bool RPN::process_token(const std::string& s, Context& ctx) {
    if (handle_var_assignment(s)) return false;
    if (handle_precision(s)) return true;
    auto it = registry.find(s);
    if (it != registry.end()) {
        const Command* c = it->second.cmd.get();
        if (c->kind() == CommandKind::STO_RCL)
            var_is_store = static_cast<const StoRcl*>(c)->is_store();
        c->exec(ctx);
        return c->is_display();
    }
    return try_parse_number(s);
}

bool RPN::handle_var_assignment(const std::string& s) {
    if (!expect_var) return false;
    var_name = s;
    if (var_is_store) {
        if (st.empty()) print_err(std::string(T("Stack is empty")) + " (sto)");
        else { mem[var_name] = st.back(); st.pop_back(); }
    } else {
        auto it = mem.find(var_name);
        if (it != mem.end()) st.push_back(it->second);
    }
    expect_var = false;
    return true;
}

bool RPN::handle_precision(const std::string& s) {
    if (s.size() <= 1 || s[0] != '.') return false;
    for (size_t i = 1; i < s.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;

    std::array<char, 4> buf{};
    std::size_t len = s.size() - 1;
    if (len > buf.size() - 1) len = buf.size() - 1;
    std::copy_n(s.data() + 1, len, buf.data());
    buf[len] = '\0';

    int p = 0;
    auto [ptr, ec] = std::from_chars(buf.data(), buf.data() + len, p);
    if (ec != std::errc{} || p < 0 || p > 20) return false;
    precision = p;
    return true;
}

bool RPN::try_parse_number(const std::string& s) {
    // Entier plein → exact (BigInt) ; sinon double (point, exposant…)
    if (BigInt::looks_like_integer(s)) {
        st.push_back(Value::big(BigInt::from_str(s)));
        return false;
    }
    double v = 0.0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size()) {
        print_err(std::string(T("Unknown command")) + ": " + s);
        return false;
    }
    st.push_back(Value::num(v));
    return false;
}

// ============================================================
// Lecture interactive (flèches, historique)
// ============================================================

bool RPN::read_line_interactive(std::string& out) {
    out.clear();
    int hist_idx = -1;
    std::string saved;

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    bool ok = true;
    while (true) {
        int c = read_char_raw();
        if (c == -1) { save_history(); save_variables(); ok = false; break; }
        if (c == '\n') { recolor_last_token(out); break; }
        if (c == 127 || c == 8) {
            if (!out.empty()) { out.pop_back(); std::cout << "\b \b" << std::flush; recolor_last_token(out); }
        } else if (c == 27) {
            int c2 = read_char_raw(); if (c2 == -1) { ok = false; break; }
            if (c2 == '[') {
                int c3 = read_char_raw(); if (c3 == -1) { ok = false; break; }
                if (c3 == 'A' && !history.empty()) {
                    if (hist_idx == -1) { saved = out; hist_idx = static_cast<int>(history.size()) - 1; }
                    else if (hist_idx > 0) --hist_idx;
                    replace_line(out, history[static_cast<size_t>(hist_idx)]);
                } else if (c3 == 'B' && hist_idx >= 0) {
                    ++hist_idx;
                    if (hist_idx >= static_cast<int>(history.size())) { hist_idx = -1; replace_line(out, saved); }
                    else replace_line(out, history[static_cast<size_t>(hist_idx)]);
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

void RPN::replace_line(std::string& field, const std::string& val) {
    while (!field.empty()) { field.pop_back(); std::cout << "\b \b"; }
    field = val;
    print_colored_tokens(field);
    std::cout << std::flush;
}

// Réécrit à l'écran le token en cours de frappe avec sa couleur du moment :
// couleur de sa catégorie (match exact ou préfixe unique), sinon en clair.
void RPN::recolor_last_token(const std::string& buf) const {
    if (!use_color) return;
    size_t start = buf.find_last_of(' ');
    start = (start == std::string::npos) ? 0 : start + 1;
    std::string tok = buf.substr(start);
    if (tok.empty()) return;
    std::string col = color_for_partial(tok);
    for (size_t i = 0; i < tok.size(); ++i) std::cout << '\b';
    std::cout << col << tok << C::RESET << std::flush;
}

int RPN::read_char_raw() {
    unsigned char c{};
    return read(STDIN_FILENO, &c, 1) == 1 ? static_cast<int>(c) : -1;
}

// ============================================================
// Registre
// ============================================================

void RPN::reg(const std::string& name, CmdCat cat, const Command* cmd) {
    auto& e = registry[name];
    e.cat = cat;
    e.cmd.reset(cmd);
}

// ============================================================
// Persistance disque
// ============================================================

void RPN::ensure_data_dir() { std::filesystem::create_directories(DATA_DIR); }

void RPN::save_history() {
    ensure_data_dir();
    std::ofstream f(HIST_FILE);
    for (auto& h : history) f << h << "\n";
}

void RPN::load_history() {
    std::ifstream f(HIST_FILE);
    std::string line;
    while (std::getline(f, line))
        if (!line.empty() && static_cast<int>(history.size()) < HIST_MAX)
            history.push_back(line);
}

void RPN::save_variables() {
    ensure_data_dir();
    std::ofstream f(VAR_FILE);
    // max_digits10 : nombre minimal de chiffres garantissant l'aller-retour
    // exact d'un double (17) — la précision par défaut (6) tronquait les valeurs.
    f << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (auto& [k, v] : mem) {
        // marqueur de type : b = BigInt exact, d = double
        if (v.is_big()) f << k << " b " << v.bi->to_string() << "\n";
        else            f << k << " d " << v.d << "\n";
    }
}

void RPN::load_variables() {
    std::ifstream f(VAR_FILE);
    std::string name, tag;
    while (f >> name >> tag) {
        if (tag == "b") {
            std::string digits;
            if (f >> digits) mem[name] = Value::big(BigInt::from_str(digits));
        } else if (tag == "d") {
            double v;
            if (f >> v) mem[name] = Value::num(v);
        } else {
            // ancien format sans marqueur : « nom valeur »
            double v = 0.0;
            auto [ptr, ec] = std::from_chars(tag.data(), tag.data() + tag.size(), v);
            if (ec == std::errc{} && ptr == tag.data() + tag.size())
                mem[name] = Value::num(v);
        }
    }
}
