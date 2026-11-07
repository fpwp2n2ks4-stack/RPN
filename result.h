#pragma once

#include "bigint.h"

#include <string>
#include <vector>

// ============================================================
// Sorties structurées moteur → front-end (étape A du plan Swift).
// Le cœur ne connaît ni std::cout, ni std::cerr, ni séquence ANSI :
// chaque commande produit des Output que le front-end (terminal,
// SwiftUI, lot) rend à sa manière.
// ============================================================

// Information d'état dont la mise en forme dépend du front-end.
// MEMO : value vide, la valeur est dans v (double de l'accumulateur M).
enum class InfoTag { VISUAL, ANGLE, DISPLAY, MEMO, DEBUG };

enum class OutKind {
    ERR,       // erreur (payload : text) — le front-end choisit le canal (stderr)
    MSG,       // ligne de texte simple (payload : text, sans mise en forme)
    MODE,      // information d'état (payload : tag, text = libellé, value)
    VALUE,     // valeur isolée à afficher (payload : v)
    STACK,     // instantané de pile (payload : stack, sommet = back)
    HISTORY,   // contenu brut de l'historique (payload : lines)
    HELP,      // demande d'aide — le rendu (boîte, liste, fenêtre) est au front-end
    DEBUG,     // trace du mode debug (payload : text) — préfixe [DEBUG] au rendu
};

struct Output {
    OutKind kind;
    InfoTag tag{};                     // MODE
    std::string text;                  // ERR / MSG / MODE (libellé)
    std::string value;                 // MODE
    Value v{};                         // VALUE / MODE(MEMO)
    std::vector<Value> stack;          // STACK
    std::vector<std::string> lines;    // HISTORY
};

using Outputs = std::vector<Output>;

// ——— constructeurs ——— chaque front rend un type donné de la même façon.

inline Output out_err(std::string msg) {
    Output o;
    o.kind = OutKind::ERR;
    o.text = std::move(msg);
    return o;
}

inline Output out_msg(std::string msg) {
    Output o;
    o.kind = OutKind::MSG;
    o.text = std::move(msg);
    return o;
}

inline Output out_mode(InfoTag tag, std::string label, std::string value) {
    Output o;
    o.kind = OutKind::MODE;
    o.tag = tag;
    o.text = std::string(label);
    o.value = std::move(value);
    return o;
}

inline Output out_value(Value v) {
    Output o;
    o.kind = OutKind::VALUE;
    o.v = std::move(v);
    return o;
}

inline Output out_stack(const std::vector<Value>& snapshot) {
    Output o;
    o.kind = OutKind::STACK;
    o.stack = snapshot;
    return o;
}

inline Output out_history(const std::vector<std::string>& entries) {
    Output o;
    o.kind = OutKind::HISTORY;
    o.lines = entries;
    return o;
}

inline Output out_help() {
    Output o;
    o.kind = OutKind::HELP;
    return o;
}

inline Output out_debug(std::string trace) {
    Output o;
    o.kind = OutKind::DEBUG;
    o.text = std::move(trace);
    return o;
}
