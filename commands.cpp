#include "commands.h"

#include <cmath>
#include <charconv>
#include <array>
#include <limits>
#include <cstdio>

// ============================================================
// Helpers
// ============================================================

namespace {

// Sortie à un seul message d'erreur.
Outputs just_err(std::string msg) {
    Outputs o;
    o.push_back(out_err(std::move(msg)));
    return o;
}

}  // namespace

// ============================================================
// Opérations binaires / unaires
// ============================================================

BinaryOp::BinaryOp(std::function<double(double, double)> f, std::string key,
                   std::function<std::optional<Value>(const BigInt&, const BigInt&)> bf)
    : op(std::move(f)), bigop(std::move(bf)), nan_key(std::move(key)) {}

Outputs BinaryOp::exec(Context& ctx) const {
    auto& st = ctx.stack;
    if (st.size() < 2) {
        char buf[128];
        std::snprintf(buf, sizeof buf, T("Not enough operands (need 2, have %d)"),
                      static_cast<int>(st.size()));
        return just_err(buf);
    }
    Value b = st.back(); st.pop_back();
    Value a = st.back(); st.pop_back();
    ctx.last_x = b;   // LAST X : le contenu de x (sommet) avant l'opération
    if (a.is_big() && b.is_big() && bigop) {
        if (auto exact = bigop(*a.bi, *b.bi)) {   // nullopt → repli double
            st.push_back(std::move(*exact));
            return {};
        }
    }
    double r = op(a.to_double(), b.to_double());
    if (std::isnan(r)) {
        st.push_back(std::move(a)); st.push_back(std::move(b));
        return just_err(T(nan_key.c_str()));
    }
    st.push_back(Value::num(r));
    return {};
}

UnaryOp::UnaryOp(std::function<double(double)> f, std::string key,
                 std::function<std::optional<Value>(const BigInt&)> bf)
    : op(std::move(f)), bigop(std::move(bf)), nan_key(std::move(key)) {}

Outputs UnaryOp::exec(Context& ctx) const {
    auto& st = ctx.stack;
    if (st.empty()) {
        return just_err(T("Not enough operands (need 1, have 0)"));
    }
    Value a = st.back(); st.pop_back();
    ctx.last_x = a;   // LAST X : le contenu de x (sommet) avant l'opération
    if (a.is_big() && bigop) {
        if (auto exact = bigop(*a.bi)) {           // nullopt → repli double
            st.push_back(std::move(*exact));
            return {};
        }
    }
    double r = op(a.to_double());
    if (std::isnan(r)) {
        st.push_back(std::move(a));
        return just_err(T(nan_key.c_str()));
    }
    st.push_back(Value::num(r));
    return {};
}

// ============================================================
// Pourcentages façon HP
// ============================================================

Outputs PercentOp::exec(Context& ctx) const {
    auto& st = ctx.stack;
    if (st.size() < 2) {
        char buf[128];
        std::snprintf(buf, sizeof buf, T("Not enough operands (need 2, have %d)"),
                      static_cast<int>(st.size()));
        return just_err(buf);
    }
    Value b = st.back(); st.pop_back();   // x : le pourcentage (ou la valeur testée)
    Value a = st.back();                  // y : la référence — elle reste en place
    ctx.last_x = b;                       // la famille consomme x → LAST X
    double x = b.to_double(), y = a.to_double();
    // d% et %t divisent par la référence : nulle, le pourcentage n'existe pas.
    if (kind != Kind::OF && y == 0.0) {
        st.push_back(std::move(b));
        return just_err(T("Percent: reference is zero"));
    }
    double r;
    switch (kind) {
        case Kind::OF:     r = y * x / 100.0; break;          // pct : x % de y
        case Kind::CHANGE: r = (x - y) / y * 100.0; break;    // d% : Δ% entre y et x
        case Kind::TOTAL:  r = x / y * 100.0; break;         // %t : part de x dans le total y
    }
    st.push_back(Value::num(r));
    return {};
}


Outputs Dup::exec(Context& ctx) const {
    if (ctx.stack.empty()) {
        return just_err(T("Cannot dup: stack is empty"));
    }
    ctx.stack.push_back(ctx.stack.back());
    return {};
}

Outputs Drop::exec(Context& ctx) const {
    if (ctx.stack.empty()) {
        return just_err(T("Cannot drop: stack is empty"));
    }
    ctx.stack.pop_back();
    return {};
}

Outputs Swap::exec(Context& ctx) const {
    if (ctx.stack.size() < 2) {
        char buf[160];
        std::snprintf(buf, sizeof buf, T("Cannot swap: need 2 elements (have %d)"),
                      static_cast<int>(ctx.stack.size()));
        return just_err(buf);
    }
    Value a = ctx.stack.back(); ctx.stack.pop_back();
    Value b = ctx.stack.back(); ctx.stack.pop_back();
    ctx.stack.push_back(std::move(a));
    ctx.stack.push_back(std::move(b));
    return {};
}

Outputs Clear::exec(Context& ctx) const {
    ctx.stack.clear();
    return {};
}

Outputs LastX::exec(Context& ctx) const {
    if (!ctx.last_x) {
        return just_err(T("No LAST X yet"));
    }
    // Le registre n'est pas vidé : plusieurs lastx ramènent la même valeur.
    ctx.stack.push_back(*ctx.last_x);
    return {};
}

// ============================================================
// Alias dc
// ============================================================

Outputs PrintTop::exec(Context& ctx) const {
    if (ctx.stack.empty()) {
        Outputs o;
        o.push_back(out_msg("(?)"));
        return o;
    }
    Outputs o;
    o.push_back(out_value(ctx.stack.back()));
    return o;
}

Outputs PrintPop::exec(Context& ctx) const {
    if (ctx.stack.empty()) {
        return just_err("pile vide (n)");
    }
    Outputs o;
    o.push_back(out_value(ctx.stack.back()));
    ctx.stack.pop_back();
    return o;
}

Outputs DivmodOp::exec(Context& ctx) const {
    auto& st = ctx.stack;
    if (st.size() < 2) {
        char buf[128];
        std::snprintf(buf, sizeof buf, T("Not enough operands (need 2, have %d)"),
                      static_cast<int>(st.size()));
        return just_err(buf);
    }
    Value b = st.back(); st.pop_back();
    Value a = st.back(); st.pop_back();
    ctx.last_x = b;   // LAST X : le contenu de x (sommet) avant l'opération
    if (a.is_big() && b.is_big()) {
        if (b.bi->is_zero()) {
            st.push_back(std::move(a)); st.push_back(std::move(b));
            return just_err(T("Division by zero"));
        }
        BigInt q, r;
        divmod(*a.bi, *b.bi, q, r);
        st.push_back(Value::big(std::move(q)));
        st.push_back(Value::big(std::move(r)));
        return {};
    }
    double da = a.to_double(), db = b.to_double();
    if (db == 0.0) {
        st.push_back(std::move(a)); st.push_back(std::move(b));
        return just_err(T("Division by zero"));
    }
    st.push_back(Value::num(std::trunc(da / db)));
    st.push_back(Value::num(std::fmod(da, db)));
    return {};
}

Outputs StackDepth::exec(Context& ctx) const {
    ctx.stack.push_back(Value::big(BigInt::from_u64(ctx.stack.size())));
    return {};
}

// ============================================================
// Affichage & modes
// ============================================================

Outputs StackCmd::exec(Context& ctx) const {
    if (ctx.stack.empty()) {
        Outputs o;
        o.push_back(out_msg(T("(empty)")));
        return o;
    }
    Outputs o;
    o.push_back(out_stack(ctx.stack));
    return o;
}

Outputs VisualToggle::exec(Context& ctx) const {
    ctx.visual_mode = !ctx.visual_mode;
    Outputs o;
    o.push_back(out_mode(InfoTag::VISUAL, T("Visual mode"),
                         ctx.visual_mode ? "ON" : "OFF"));
    return o;
}

Outputs AngleSetter::exec(Context& ctx) const {
    ctx.deg_mode = target;
    Outputs o;
    o.push_back(out_mode(InfoTag::ANGLE, T("Angle mode"),
                         ctx.deg_mode ? "DEG" : "RAD"));
    return o;
}

Outputs PushConst::exec(Context& ctx) const {
    ctx.stack.push_back(Value::num(val));
    return {};
}

Outputs DisplaySetter::exec(Context& ctx) const {
    ctx.disp_mode = target;
    static const char* names[] = { "DEC", "HEX", "BIN", "SCI", "ENG" };
    Outputs o;
    o.push_back(out_mode(InfoTag::DISPLAY, T("Display mode"),
                         names[static_cast<int>(target)]));
    return o;
}

// ============================================================
// Variables
// ============================================================

Outputs StoRcl::exec(Context& ctx) const {
    ctx.expect_var = true;
    ctx.var_name.clear();
    return {};
}

// ============================================================
// Factorielle
// ============================================================

Outputs FactOp::exec(Context& ctx) const {
    auto& st = ctx.stack;
    if (st.empty()) {
        return just_err(T("Not enough operands (fact)"));
    }
    Value a = st.back(); st.pop_back();
    ctx.last_x = a;   // LAST X : le contenu de x (sommet) avant l'opération

    if (a.is_big()) {
        if (a.bi->negative()) {
            st.push_back(std::move(a));
            return just_err(T("Invalid argument for fact"));
        }
        auto u = a.bi->to_u64();
        if (!u || *u > 1000) {
            st.push_back(std::move(a));
            return just_err(T("Fact argument too large (max 1000)"));
        }
        BigInt r = BigInt::from_u64(1);
        for (uint64_t i = 2; i <= *u; ++i) r = mul(r, BigInt::from_u64(i));
        st.push_back(Value::big(std::move(r)));
        return {};
    }

    double x = a.d;
    if (x < 0.0 || x != std::floor(x) || x > 170.0) {
        st.push_back(std::move(a));
        return just_err(T("Invalid argument for fact"));
    }
    double r = 1.0;
    for (int i = 2; i <= static_cast<int>(x); ++i) r *= static_cast<double>(i);
    st.push_back(Value::num(r));
    return {};
}

// ============================================================
// Persistance & scripts (todo §1-§5) — la commande ne fait que
// commuter un drapeau : le moteur agit (fichier, sorties traduites)
// en fin de ligne pour save/load, au token suivant pour run.
// ============================================================

Outputs SaveVars::exec(Context& ctx) const {
    ctx.save_asked = true;
    return {};
}

Outputs LoadVars::exec(Context& ctx) const {
    ctx.load_asked = true;
    return {};
}

Outputs RunCmd::exec(Context& ctx) const {
    ctx.expect_file = true;
    return {};
}

Outputs DebugToggle::exec(Context& ctx) const {
    ctx.debug_mode = !ctx.debug_mode;
    Outputs o;
    o.push_back(out_mode(InfoTag::DEBUG, T("Debug mode"),
                         ctx.debug_mode ? "ON" : "OFF"));
    return o;
}

Outputs HistClear::exec(Context& ctx) const {
    ctx.clear_hist_asked = true;
    return {};
}

// ============================================================
// Mémoire accumulateur
// ============================================================

Outputs MemoryOp::exec(Context& ctx) const {
    auto& st = ctx.stack;
    auto pop_top = [&]() -> double {
        double v = st.back().to_double();   // promote les BigInt
        st.pop_back();
        return v;
    };
    switch (op) {
        case MemOp::ADD:
            if (st.empty()) return just_err(std::string(T("Stack is empty")) + " (m+)");
            ctx.last_x = st.back();   // l'accumulateur consomme x → LAST X
            ctx.memory += pop_top(); return {};
        case MemOp::SUB:
            if (st.empty()) return just_err(std::string(T("Stack is empty")) + " (m-)");
            ctx.last_x = st.back();
            ctx.memory -= pop_top(); return {};
        case MemOp::MUL:
            if (st.empty()) return just_err(std::string(T("Stack is empty")) + " (m*)");
            ctx.last_x = st.back();
            ctx.memory *= pop_top(); return {};
        case MemOp::DIV:
            if (st.empty()) return just_err(std::string(T("Stack is empty")) + " (m/)");
            if (st.back().to_double() == 0.0)
                return just_err(std::string(T("Division by zero")) + " (m/)");
            ctx.last_x = st.back();
            ctx.memory /= pop_top(); return {};
        case MemOp::RECALL:
            st.push_back(Value::num(ctx.memory)); return {};
        case MemOp::CLEAR:
            ctx.memory = 0.0; return {};
    }
    return {};
}

Outputs MemShow::exec(Context& ctx) const {
    Outputs o;
    o.push_back(out_mode(InfoTag::MEMO, T("Memory"), ""));
    o.back().v = Value::num(ctx.memory);
    return o;
}

// ============================================================
// Conversions d'unités
// ============================================================

Outputs UnitConv::exec(Context& ctx) const {
    if (ctx.stack.empty()) {
        return just_err(std::string(T("Stack is empty")) + " (conversion)");
    }
    Value a = ctx.stack.back(); ctx.stack.pop_back();
    ctx.last_x = a;   // LAST X : le contenu de x (sommet) avant la conversion
    // conversion physique → double (promote les BigInt)
    ctx.stack.push_back(Value::num(a.to_double() * factor));
    return {};
}

// ============================================================
// Historique
// ============================================================

Outputs HistoryCmd::exec(Context& ctx) const {
    if (ctx.history.empty()) {
        Outputs o;
        o.push_back(out_msg(T("(no history yet)")));
        return o;
    }
    Outputs o;
    o.push_back(out_history(ctx.history));
    return o;
}

// ============================================================
// Help — le moteur signale l'aide ; la boîte (ou la fenêtre) est
// le rendu du front-end (étape A du plan Swift).
// ============================================================

Outputs HelpCmd::exec(Context&) const {
    Outputs o;
    o.push_back(out_help());
    return o;
}

// ============================================================
// Lang — langue active, assemblée en texte simple
// ============================================================

Outputs LangCmd::exec(Context&) const {
    Outputs o;
    o.push_back(out_msg(std::string(T("Language")) + ": " + lang_name(lang_current())
                        + "  (LC_ALL / LANG, --lang=<xx>)"));
    return o;
}

// ============================================================
// Quit — signalise via flag (MISRA 21.3.5)
// ============================================================

Outputs QuitCmd::exec(Context& ctx) const {
    ctx.quit_flag = true;
    return {};
}
