#pragma once

#include "bigint.h"
#include "i18n.h"
#include "result.h"

#include <string>
#include <functional>
#include <vector>

// ============================================================
// Mode d'affichage des valeurs — état du moteur, pas du terminal.
// ============================================================

enum class DisplayMode { DEC, HEX, BIN, SCI, ENG };

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
    // LAST X (cf. HP-15C) : copie du sommet capturée par chaque opération
    // qui consomme x pour calculer (binaire, unaire, fact, ~, conversion,
    // accumulateur) — la matière première du « récupère-moi » : une erreur
    // restaurée, et `lastx` rend l'opérande perdu. Volatil : ni sauvegardé
    // ni restauré, comme le registre M.
    std::optional<Value>& last_x;
    // Persistance explicite (todo §1) : la commande lève le drapeau, le
    // moteur agit en fin de ligne — l'action ne se rejoue pas à chaque
    // token du segment.
    bool& save_asked;
    bool& load_asked;
    // Scripts (todo §3) : `run` arme l'attente d'un nom de fichier — le
    // token suivant est consommé comme chemin, pas comme commande.
    bool& expect_file;
    // Mode debug (todo §8) : chaque token rend sa trace au front-end.
    bool& debug_mode;
    // hclear (todo §7) : le moteur vide l'historique en fin de ligne.
    bool& clear_hist_asked;
};

// ============================================================
// Interface Command — les commandes retournent leurs sorties
// structurées ; rien ne s'imprime ici (étape A du plan Swift).
// ============================================================

class Command {
public:
    virtual ~Command() = default;
    virtual Outputs exec(Context& ctx) const = 0;
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
    // nan_key : clé i18n (anglais) rendue en erreur (et opérandes restaurées) si NaN.
    BinaryOp(std::function<double(double, double)> f,
             std::string nan_key = "Result undefined (nan)",
             std::function<std::optional<Value>(const BigInt&, const BigInt&)> bigop = nullptr);
    Outputs exec(Context& ctx) const override;
};

class UnaryOp : public Command {
    std::function<double(double)> op;
    std::function<std::optional<Value>(const BigInt&)> bigop;
    std::string nan_key;
public:
    UnaryOp(std::function<double(double)> f,
            std::string nan_key = "Result undefined (nan)",
            std::function<std::optional<Value>(const BigInt&)> bigop = nullptr);
    Outputs exec(Context& ctx) const override;
};

// ============================================================
// Pourcentages façon HP (pct, d%, %t) — la pile [y, x] devient
// [y, résultat] : x est consommé, la référence y reste en place.
// ============================================================

class PercentOp : public Command {
public:
    enum class Kind { OF, CHANGE, TOTAL };   // pct : x% de y ; d% : Δ% ; %t : part de x dans y
private:
    Kind kind;
public:
    explicit PercentOp(Kind k) : kind(k) {}
    Outputs exec(Context& ctx) const override;
};

// ============================================================
// Pile
// ============================================================

class Dup : public Command {
public:
    Outputs exec(Context& ctx) const override;
};

class Drop : public Command {
public:
    Outputs exec(Context& ctx) const override;
};

class Swap : public Command {
public:
    Outputs exec(Context& ctx) const override;
};

class Clear : public Command {
public:
    Outputs exec(Context& ctx) const override;
};

// lastx / lstx : pousse la copie du registre LAST X sans le vider
// (cf. HP-15C). Toute opération qui dépile x pour calculer l'alimente.
class LastX : public Command {
public:
    Outputs exec(Context& ctx) const override;
};

// —— alias dc —————————————————————————————————
// p : affiche le sommet SANS le dépiler (comme dc)
class PrintTop : public Command {
public:
    Outputs exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// n : affiche le sommet ET le dépile (comme dc)
class PrintPop : public Command {
public:
    Outputs exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// ~ : quotient puis reste (exact en BigInt, comme dc)
class DivmodOp : public Command {
public:
    Outputs exec(Context& ctx) const override;
};

// z : empile la profondeur de la pile (exacte)
class StackDepth : public Command {
public:
    Outputs exec(Context& ctx) const override;
};

// ============================================================
// Affichage & modes
// ============================================================

class StackCmd : public Command {
public:
    Outputs exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

class VisualToggle : public Command {
public:
    Outputs exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

class AngleSetter : public Command {
    bool target;
public:
    explicit AngleSetter(bool t) : target(t) {}
    Outputs exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

class PushConst : public Command {
    double val;
public:
    explicit PushConst(double v) : val(v) {}
    Outputs exec(Context& ctx) const override;
};

class DisplaySetter : public Command {
    DisplayMode target;
public:
    explicit DisplaySetter(DisplayMode t) : target(t) {}
    Outputs exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// ============================================================
// Variables
// ============================================================

class StoRcl : public Command {
    bool store_op;
public:
    explicit StoRcl(bool s) : store_op(s) {}
    Outputs exec(Context& ctx) const override;
    CommandKind kind() const override { return CommandKind::STO_RCL; }
    bool is_store() const { return store_op; }
};

// ============================================================
// Factorielle
// ============================================================

class FactOp : public Command {
public:
    Outputs exec(Context& ctx) const override;
};

// ============================================================
// Persistance & scripts (todo §1-§5)
// ============================================================

// save / savevar : écrit les variables dans le fichier, à la demande —
// l'auto-sauvegarde de fin de session reste en complément.
class SaveVars : public Command {
public:
    Outputs exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// load / loadvar : relit le fichier des variables (JSON, avec repli
// sur l'ancien format texte « nom b|d valeur »).
class LoadVars : public Command {
public:
    Outputs exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// run <fichier> : exécute les lignes d'un script — chaque ligne vit
// comme une frappe (écho, historique, erreurs affichées, on continue).
class RunCmd : public Command {
public:
    Outputs exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// debug (todo §8) : bascule le mode debug — chaque token rend sa trace
// (token lu, pile complète), lisible aussi dans les scripts et le batch.
class DebugToggle : public Command {
public:
    Outputs exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// hclear / histclear (todo §7) : vide l'historique — la commande lève
// le drapeau, le moteur vide (mémoire + fichier) en fin de ligne.
class HistClear : public Command {
public:
    Outputs exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// ============================================================
// Mémoire accumulateur
// ============================================================

enum class MemOp { ADD, SUB, MUL, DIV, RECALL, CLEAR };

class MemoryOp : public Command {
    MemOp op;
public:
    explicit MemoryOp(MemOp o) : op(o) {}
    Outputs exec(Context& ctx) const override;
    bool is_display() const override { return op != MemOp::RECALL; }
};

class MemShow : public Command {
public:
    Outputs exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// ============================================================
// Conversions d'unités
// ============================================================

class UnitConv : public Command {
    double factor;
public:
    explicit UnitConv(double f) : factor(f) {}
    Outputs exec(Context& ctx) const override;
};

// ============================================================
// Historique
// ============================================================

class HistoryCmd : public Command {
public:
    Outputs exec(Context& ctx) const override;
    bool is_display() const override { return true; }
};

// ============================================================
// Help & Quit & Lang
// ============================================================

class HelpCmd : public Command {
public:
    Outputs exec(Context&) const override;
    bool is_display() const override { return true; }
};

class QuitCmd : public Command {
public:
    Outputs exec(Context& ctx) const override;
    CommandKind kind() const override { return CommandKind::QUIT; }
};

// lang : affiche la langue active (détection LC_ALL/LANG ou --lang)
class LangCmd : public Command {
public:
    Outputs exec(Context&) const override;
    bool is_display() const override { return true; }
};
