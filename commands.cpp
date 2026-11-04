#include "commands.h"

#include <iostream>
#include <cmath>
#include <charconv>
#include <array>
#include <limits>

// ============================================================
// Helpers
// ============================================================

void print_err(const std::string& msg) {
    std::cerr << T("Error") << ": " << msg << "\n";
}

// borne = 2^63 : les doubles au-delà ne tiennent pas dans un long long
static bool fit_ll(double v) {
    constexpr double LL_BOUND = 9223372036854775808.0; // 2^63 exact en double
    return !std::isnan(v) && v >= -LL_BOUND && v < LL_BOUND;
}

void print_num(double v, DisplayMode mode, int precision) {
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

void print_stack_lines(const std::vector<Value>& v, DisplayMode mode, int prec,
                       const char* idx_col, const char* reset_col) {
    for (size_t i = 0; i < v.size(); ++i) {
        std::cout << idx_col << (i + 1) << ": " << reset_col;
        print_num(v[i], mode, prec);
        std::cout << "\n";
    }
}

void print_num(const Value& v, DisplayMode mode, int precision) {
    if (v.is_big()) {
        const BigInt& b = *v.bi;
        switch (mode) {
            case DisplayMode::HEX: std::cout << (b.negative() ? "-0x" : "0x") << b.to_hex(); break;
            case DisplayMode::BIN: std::cout << (b.negative() ? "-0b" : "0b") << b.to_bin(); break;
            default:               std::cout << b.to_string(); break;   // exact, précision ignorée
        }
        return;
    }
    print_num(v.d, mode, precision);
}

// ============================================================
// Opérations binaires / unaires
// ============================================================

BinaryOp::BinaryOp(std::function<double(double, double)> f, std::string key,
                   std::function<std::optional<Value>(const BigInt&, const BigInt&)> bf)
    : op(std::move(f)), bigop(std::move(bf)), nan_key(std::move(key)) {}

void BinaryOp::exec(Context& ctx) const {
    auto& st = ctx.stack;
    if (st.size() < 2) {
        char buf[128];
        std::snprintf(buf, sizeof buf, T("Not enough operands (need 2, have %d)"),
                      static_cast<int>(st.size()));
        print_err(buf);
        return;
    }
    Value b = st.back(); st.pop_back();
    Value a = st.back(); st.pop_back();
    if (a.is_big() && b.is_big() && bigop) {
        if (auto exact = bigop(*a.bi, *b.bi)) {   // nullopt → repli double
            st.push_back(std::move(*exact));
            return;
        }
    }
    double r = op(a.to_double(), b.to_double());
    if (std::isnan(r)) {
        print_err(T(nan_key.c_str()));
        st.push_back(std::move(a)); st.push_back(std::move(b));
        return;
    }
    st.push_back(Value::num(r));
}

UnaryOp::UnaryOp(std::function<double(double)> f, std::string key,
                 std::function<std::optional<Value>(const BigInt&)> bf)
    : op(std::move(f)), bigop(std::move(bf)), nan_key(std::move(key)) {}

void UnaryOp::exec(Context& ctx) const {
    auto& st = ctx.stack;
    if (st.empty()) {
        print_err(T("Not enough operands (need 1, have 0)"));
        return;
    }
    Value a = st.back(); st.pop_back();
    if (a.is_big() && bigop) {
        if (auto exact = bigop(*a.bi)) {           // nullopt → repli double
            st.push_back(std::move(*exact));
            return;
        }
    }
    double r = op(a.to_double());
    if (std::isnan(r)) {
        print_err(T(nan_key.c_str()));
        st.push_back(std::move(a));
        return;
    }
    st.push_back(Value::num(r));
}

// ============================================================
// Pile
// ============================================================

void Dup::exec(Context& ctx) const {
    if (ctx.stack.empty()) {
        std::cerr << T("Error") << ": " << T("Cannot dup: stack is empty") << "\n";
        return;
    }
    ctx.stack.push_back(ctx.stack.back());
}

void Drop::exec(Context& ctx) const {
    if (ctx.stack.empty()) {
        std::cerr << T("Error") << ": " << T("Cannot drop: stack is empty") << "\n";
        return;
    }
    ctx.stack.pop_back();
}

void Swap::exec(Context& ctx) const {
    if (ctx.stack.size() < 2) {
        char buf[160];
        std::snprintf(buf, sizeof buf, T("Cannot swap: need 2 elements (have %d)"),
                      static_cast<int>(ctx.stack.size()));
        std::cerr << T("Error") << ": " << buf << "\n";
        return;
    }
    Value a = ctx.stack.back(); ctx.stack.pop_back();
    Value b = ctx.stack.back(); ctx.stack.pop_back();
    ctx.stack.push_back(std::move(a));
    ctx.stack.push_back(std::move(b));
}

void Clear::exec(Context& ctx) const {
    ctx.stack.clear();
}

// ============================================================
// Alias dc
// ============================================================

void PrintTop::exec(Context& ctx) const {
    if (ctx.stack.empty()) { std::cout << "(?)\n"; return; }
    print_num(ctx.stack.back(), ctx.disp_mode, ctx.precision);
    std::cout << "\n";
}

void PrintPop::exec(Context& ctx) const {
    if (ctx.stack.empty()) { print_err("pile vide (n)"); return; }
    print_num(ctx.stack.back(), ctx.disp_mode, ctx.precision);
    std::cout << "\n";
    ctx.stack.pop_back();
}

void DivmodOp::exec(Context& ctx) const {
    auto& st = ctx.stack;
    if (st.size() < 2) {
        char buf[128];
        std::snprintf(buf, sizeof buf, T("Not enough operands (need 2, have %d)"),
                      static_cast<int>(st.size()));
        print_err(buf);
        return;
    }
    Value b = st.back(); st.pop_back();
    Value a = st.back(); st.pop_back();
    if (a.is_big() && b.is_big()) {
        if (b.bi->is_zero()) {
            print_err(T("Division by zero"));
            st.push_back(std::move(a)); st.push_back(std::move(b));
            return;
        }
        BigInt q, r;
        divmod(*a.bi, *b.bi, q, r);
        st.push_back(Value::big(std::move(q)));
        st.push_back(Value::big(std::move(r)));
        return;
    }
    double da = a.to_double(), db = b.to_double();
    if (db == 0.0) {
        print_err(T("Division by zero"));
        st.push_back(std::move(a)); st.push_back(std::move(b));
        return;
    }
    st.push_back(Value::num(std::trunc(da / db)));
    st.push_back(Value::num(std::fmod(da, db)));
}

void StackDepth::exec(Context& ctx) const {
    ctx.stack.push_back(Value::big(BigInt::from_u64(ctx.stack.size())));
}

// ============================================================
// Affichage & modes
// ============================================================

void StackCmd::exec(Context& ctx) const {
    if (ctx.stack.empty()) { std::cout << T("(empty)") << "\n"; return; }
    print_stack_lines(ctx.stack, ctx.disp_mode, ctx.precision);
}

void VisualToggle::exec(Context& ctx) const {
    ctx.visual_mode = !ctx.visual_mode;
    const char* dim = ctx.color_out ? C::DIM : "";
    const char* rst = ctx.color_out ? C::RESET : "";
    std::string val = ctx.visual_mode
        ? (ctx.color_out ? std::string(C::GREEN) + C::BOLD + "ON" + C::RESET : "ON")
        : (ctx.color_out ? std::string(C::RED) + C::BOLD + "OFF" + C::RESET : "OFF");
    std::cout << dim << T("Visual mode") << " " << rst << val << "\n";
}

void AngleSetter::exec(Context& ctx) const {
    ctx.deg_mode = target;
    const char* dim = ctx.color_out ? C::DIM : "";
    const char* rst = ctx.color_out ? C::RESET : "";
    std::string val = ctx.color_out
        ? std::string(C::YELLOW) + C::BOLD + (ctx.deg_mode ? "DEG" : "RAD") + C::RESET
        : std::string(ctx.deg_mode ? "DEG" : "RAD");
    std::cout << dim << T("Angle mode") << ": " << rst << val << "\n";
}

void PushConst::exec(Context& ctx) const {
    ctx.stack.push_back(Value::num(val));
}

void DisplaySetter::exec(Context& ctx) const {
    ctx.disp_mode = target;
    static const char* names[] = { "DEC", "HEX", "BIN" };
    static const char* colors[] = { C::CYAN, C::GREEN, C::MAGENTA };
    const char* dim = ctx.color_out ? C::DIM : "";
    const char* rst = ctx.color_out ? C::RESET : "";
    std::string val = ctx.color_out
        ? std::string(colors[static_cast<int>(target)]) + C::BOLD + names[static_cast<int>(target)] + C::RESET
        : std::string(names[static_cast<int>(target)]);
    std::cout << dim << T("Display mode") << ": " << rst << val << "\n";
}

// ============================================================
// Variables
// ============================================================

void StoRcl::exec(Context& ctx) const {
    ctx.expect_var = true;
    ctx.var_name.clear();
}

// ============================================================
// Factorielle
// ============================================================

void FactOp::exec(Context& ctx) const {
    auto& st = ctx.stack;
    if (st.empty()) { print_err(T("Not enough operands (fact)")); return; }
    Value a = st.back(); st.pop_back();

    if (a.is_big()) {
        if (a.bi->negative()) { print_err(T("Invalid argument for fact")); st.push_back(std::move(a)); return; }
        auto u = a.bi->to_u64();
        if (!u || *u > 1000) { print_err(T("Fact argument too large (max 1000)")); st.push_back(std::move(a)); return; }
        BigInt r = BigInt::from_u64(1);
        for (uint64_t i = 2; i <= *u; ++i) r = mul(r, BigInt::from_u64(i));
        st.push_back(Value::big(std::move(r)));
        return;
    }

    double x = a.d;
    if (x < 0.0 || x != std::floor(x) || x > 170.0) {
        print_err(T("Invalid argument for fact"));
        st.push_back(std::move(a));
        return;
    }
    double r = 1.0;
    for (int i = 2; i <= static_cast<int>(x); ++i) r *= static_cast<double>(i);
    st.push_back(Value::num(r));
}

// ============================================================
// Mémoire accumulateur
// ============================================================

void MemoryOp::exec(Context& ctx) const {
    auto& st = ctx.stack;
    auto pop_top = [&]() -> double {
        double v = st.back().to_double();   // promote les BigInt
        st.pop_back();
        return v;
    };
    switch (op) {
        case MemOp::ADD:
            if (st.empty()) { print_err(std::string(T("Stack is empty")) + " (m+)"); return; }
            ctx.memory += pop_top(); break;
        case MemOp::SUB:
            if (st.empty()) { print_err(std::string(T("Stack is empty")) + " (m-)"); return; }
            ctx.memory -= pop_top(); break;
        case MemOp::MUL:
            if (st.empty()) { print_err(std::string(T("Stack is empty")) + " (m*)"); return; }
            ctx.memory *= pop_top(); break;
        case MemOp::DIV:
            if (st.empty()) { print_err(std::string(T("Stack is empty")) + " (m/)"); return; }
            if (st.back().to_double() == 0.0) { print_err(std::string(T("Division by zero")) + " (m/)"); return; }
            ctx.memory /= pop_top(); break;
        case MemOp::RECALL:
            st.push_back(Value::num(ctx.memory)); break;
        case MemOp::CLEAR:
            ctx.memory = 0.0; break;
    }
}

void MemShow::exec(Context& ctx) const {
    std::cout << T("Memory") << " = ";
    print_num(ctx.memory, ctx.disp_mode, ctx.precision);
    std::cout << "\n";
}

// ============================================================
// Conversions d'unités
// ============================================================

void UnitConv::exec(Context& ctx) const {
    if (ctx.stack.empty()) { print_err(std::string(T("Stack is empty")) + " (conversion)"); return; }
    Value a = ctx.stack.back(); ctx.stack.pop_back();
    // conversion physique → double (promote les BigInt)
    ctx.stack.push_back(Value::num(a.to_double() * factor));
}

// ============================================================
// Historique
// ============================================================

void HistoryCmd::exec(Context& ctx) const {
    if (ctx.history.empty()) { std::cout << T("(no history yet)") << "\n"; return; }
    for (size_t i = 0; i < ctx.history.size(); ++i)
        std::cout << (i + 1) << ": " << ctx.history[i] << "\n";
}

// ============================================================
// Échappement shell (MISRA 21.3.3 — encapsulé)
// ============================================================

void ShellExec::exec(Context&) const {
    std::cout << "shell> " << cmd << "\n";
    // MISRA 21.3.3 : std::system() interdit en code applicatif.
    // Cette ligne doit être remplacée par un wrapper fourni par
    // l'architecture système (ex: SecurityPolicy::execute(cmd)).
    // Pour les besoins de ce programme, l'appel est conservé ici
    // avec une annotation de dérogation documentée.
    // NB : l'échappement est réservé au mode interactif (gate dans process_line).
    [[maybe_unused]] int r = std::system(cmd.c_str()); // NOLINT
}

// ============================================================
// Help — boîte générée (largeurs calculées : alignement garanti EN/FR/ES)
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

} // namespace

void HelpCmd::exec(Context& ctx) const {
    bool c = ctx.color_out;
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

    // place un fragment coloré à la colonne donnée dans une ligne vierge
    auto render1 = [&](const std::string& s, size_t col) {
        std::string buf(W, ' ');
        if (col + vislen(s) <= W)
            buf = buf.substr(0, col) + s + buf.substr(col + vislen(s));
        return std::string("║") + buf + "║\n";
    };
    auto row2 = [&](const std::string& label, const std::string& toks,
                    const std::string& right) {
        std::string buf(W, ' ');
        if (!label.empty()) {
            std::string l = label + ":";
            if (2 + vislen(l) <= W)
                buf = buf.substr(0, 2) + l + buf.substr(2 + vislen(l));
        }
        if (!toks.empty() && 17 + vislen(toks) <= W)
            buf = buf.substr(0, 17) + toks + buf.substr(17 + vislen(toks));
        if (!right.empty() && 40 + vislen(right) <= W)
            buf = buf.substr(0, 40) + right + buf.substr(40 + vislen(right));
        return std::string("║") + buf + "║\n";
    };

    std::string hdr = T("HELP — RPN Calculator");
    std::string out("\n" + top);
    out += render1(hdr, (W - vislen(hdr)) / 2);
    out += sep;
    out += row2(T("Arithmetic"), cY("+") + "  " + cY("-") + "  " + cY("*") + "  " + cY("/") + "  "
                + cY("pow") + "  " + cY("log") + "  " + cY("mod"), "");
    out += row2("", cY("atan2"), "");
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
    out += row2(T("StackC"), cB("dup") + "  " + cB("drop") + "  " + cB("swap") + "  " + cB("clear")
                + "  " + cB("stack") + " (" + cB("ls") + ")", "");
    out += row2("dc 1971", cB("d") + "  " + cB("c") + "  " + cB("f") + "  " + cB("r") + "  "
                + cB("p") + "  " + cB("n") + "  " + cB("z") + "  " + cG("v") + "  "
                + cY("^") + "  " + cY("%") + "  " + cY("~"), "");
    out += sep;
    out += row2(T("MemoryC"), cR("m+") + "  " + cR("m-") + "  " + cR("m*") + "  " + cR("m/")
                + "  " + cR("mr") + "  " + cR("mc") + "  " + cR("ms"), "");
    out += row2(T("VariablesC"), cY("sto") + " x  /  " + cY("rcl") + " x", "");
    out += row2(T("Conversions"), cD("m2cm") + "  " + cD("cm2m") + "  " + cD("m2ft") + "  "
                + cD("ft2m") + "  " + cD("kg2lb") + "  " + cD("lb2kg"), "");
    out += row2("", cD("c2f") + "  " + cD("f2c") + "  " + cD("c2k") + "  " + cD("k2c"), "");
    out += sep;
    out += row2(T("Modes"), cC("dec") + "  " + cC("hex") + "  " + cC("bin") + "  " + cC("visual"),
                cD(".N") + " " + T("(e.g. .6)"));
    out += row2(T("Angles"), cY("deg") + "  " + cY("rad"), "");
    out += row2("Shell", cY("!cmd") + " " + T("(interactive only)"), "");
    out += row2(T("HistoryC"), cD("h") + "  " + cD("hist") + "  " + cD("history")
                + "   ↑↓", "");
    out += row2(T("LanguageC"), cW("lang"), "LC_ALL / LANG");
    out += sep;
    out += row2(T("Control"), cR("quit") + " / " + cR("exit") + "   " + cW("help"), "");
    out += bot;
    out += "\n";
    std::cout << out;
}

// ============================================================
// Lang — affiche la langue active
// ============================================================

void LangCmd::exec(Context&) const {
    std::cout << T("Language") << ": " << lang_name(lang_current())
              << "  (LC_ALL / LANG, --lang=<xx>)\n";
}

// ============================================================
// Quit — signalise via flag (MISRA 21.3.5)
// ============================================================

void QuitCmd::exec(Context& ctx) const { ctx.quit_flag = true; }
