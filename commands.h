#pragma once

#include "bigint.h"
#include "i18n.h"

#include <string>
#include <functional>
#include <vector>

// ============================================================
// ANSI colors
// ============================================================

namespace C {
    inline constexpr const char* RESET   = "\033[0m";
    inline constexpr const char* BOLD    = "\033[1m";
    inline constexpr const char* DIM     = "\033[2m";
    inline constexpr const char* RED     = "\033[31m";
    inline constexpr const char* GREEN   = "\033[32m";
    inline constexpr const char* YELLOW  = "\033[33m";
    inline constexpr const char* BLUE     = "\033[34m";
    inline constexpr const char* MAGENTA = "\033[35m";
    inline constexpr const char* CYAN    = "\033[36m";
    inline constexpr const char* WHITE   = "\033[37m";
    inline constexpr const char* GRAY    = "\033[90m";
}

// ============================================================
// Helpers
// ============================================================

void print_err(const std::string& msg);

enum class DisplayMode { DEC, HEX, BIN };

void print_num(double v, DisplayMode mode, int precision);
// Surcharge Value : un BigInt s'affiche exactement (décimal, hex, bin) ;
// la précision `.N` ne s'applique qu'aux doubles.
void print_num(const Value& v, DisplayMode mode, int precision);

// Affiche une pile (le sommet est v.back() — ordre du bas vers le haut)
void print_stack_lines(const std::vector<Value>& v, DisplayMode mode, int prec,
                       const char* idx_col = "", const char* reset_col = "");

// ============================================================
// Command kind — remplace dynamic_cast (MISRA 12.3.1)
// ============================================================

enum class CommandKind {
    GENERIC,
    STO_RCL,
    QUIT
};

// ============================================================
// Contexte partagé moteur ↔ commandes — fin des pointeurs bruts :
// les commandes reçoivent l'état du moteur à l'exécution.
// ============================================================

struct Context {
    std::vector<Value>& stack;          // pile, sommet = stack.back()
    double& memory;                      // accumulateur M
    std::vector<std::string>& history;
    bool& visual_mode;
    bool& deg_mode;
    DisplayMode& disp_mode;
    int& precision;
    bool& quit_flag;
    bool& expect_var;
    std::string& var_name;
    bool& color_out;                     // ANSI actif (sortie terminal seulement)
};

// ============================================================
// Interface Command
// ============================================================

class Command {
public:
    virtual ~Command() = default;
    virtual void exec(Context& ctx) const = 0;
    virtual bool is_display() const { return false; }
    virtual CommandKind kind() const { return CommandKind::GENERIC; }
};

// ============================================================
// Opérations
// ============================================================

class BinaryOp : public Command {
    std::function<double(double, double)> op;
    std::function<std::optional<Value>(const BigInt&, const BigInt&)> bigop;
    std::string nan_key;
public:
    // bigop : version exacte BigInt→BigInt. Absente ou nullopt → calcul double.
    // nan_key : clé i18n (anglais) affichée (et opérandes restaurées) si NaN.
    BinaryOp(std::function<double(double, double)> f,
             std::string nan_key = "Result undefined (nan)",
             std::function<std::optional<Value>(const BigInt&, const BigInt&)> bigop = nullptr);
    void exec(Context& ctx) const override;
};

class UnaryOp : public Command {
    std::function<double(double)> op;
    std::function<std::optional<Value>(const BigInt&)> bigop;
    std::string nan_key;
public:
    UnaryOp(std::function<double(double)> f,
            std::string nan_key = "Result undefined (nan)",
            std::function<std::optional<Value>(const BigInt&)> bigop = nullptr);
    void exec(Context& ctx) const override;
};

// ============================================================
// Pile
// ============================================================

class Dup : public Command {
public:
    void exec(Context& ctx) const override;
};

class Drop : public Command {
public:
    void exec(Context& ctx) const override;
};

class Swap : public Command {
public:
    void exec(Context& ctx) const override;
};

class Clear : public Command {
public:
    void exec(Context& ctx) const override;
};

// —— alias dc —————————————————————————————————
// p : affiche le sommet SANS le dépiler (comme dc)
class PrintTop : public Command {
public:
    void exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// n : affiche le sommet ET le dépile (comme dc)
class PrintPop : public Command {
public:
    void exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// ~ : quotient puis reste (exact en BigInt, comme dc)
class DivmodOp : public Command {
public:
    void exec(Context& ctx) const override;
};

// z : empile la profondeur de la pile (exacte)
class StackDepth : public Command {
public:
    void exec(Context& ctx) const override;
};

// ============================================================
// Affichage & modes
// ============================================================

class StackCmd : public Command {
public:
    void exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

class VisualToggle : public Command {
public:
    void exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

class AngleSetter : public Command {
    bool target;
public:
    explicit AngleSetter(bool t) : target(t) {}
    void exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

class PushConst : public Command {
    double val;
public:
    explicit PushConst(double v) : val(v) {}
    void exec(Context& ctx) const override;
};

class DisplaySetter : public Command {
    DisplayMode target;
public:
    explicit DisplaySetter(DisplayMode t) : target(t) {}
    void exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// ============================================================
// Variables
// ============================================================

class StoRcl : public Command {
    bool store_op;
public:
    explicit StoRcl(bool s) : store_op(s) {}
    void exec(Context& ctx) const override;
    CommandKind kind() const override { return CommandKind::STO_RCL; }
    bool is_store() const { return store_op; }
};

// ============================================================
// Factorielle
// ============================================================

class FactOp : public Command {
public:
    void exec(Context& ctx) const override;
};

// ============================================================
// Mémoire accumulateur
// ============================================================

enum class MemOp { ADD, SUB, MUL, DIV, RECALL, CLEAR };

class MemoryOp : public Command {
    MemOp op;
public:
    explicit MemoryOp(MemOp o) : op(o) {}
    void exec(Context& ctx) const override;
    bool is_display() const override { return op != MemOp::RECALL; }
};

class MemShow : public Command {
public:
    void exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// ============================================================
// Conversions d'unités
// ============================================================

class UnitConv : public Command {
    double factor;
public:
    explicit UnitConv(double f) : factor(f) {}
    void exec(Context& ctx) const override;
};

// ============================================================
// Historique
// ============================================================

class HistoryCmd : public Command {
public:
    void exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// ============================================================
// Échappement shell
// ============================================================

class ShellExec : public Command {
    std::string cmd;
public:
    explicit ShellExec(const std::string& c) : cmd(c) {}
    void exec(Context&) const override;
    bool is_display() const override { return true; }
};

// ============================================================
// Help & Quit & Lang
// ============================================================

class HelpCmd : public Command {
public:
    void exec(Context&) const override;
    bool is_display() const override { return true; }
};

class QuitCmd : public Command {
public:
    void exec(Context& ctx) const override;
    CommandKind kind() const override { return CommandKind::QUIT; }
};

// lang : affiche la langue active (détection LC_ALL/LANG ou --lang)
class LangCmd : public Command {
public:
    void exec(Context&) const override;
    bool is_display() const override { return true; }
};
