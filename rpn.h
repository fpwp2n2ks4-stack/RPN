#pragma once

#include "commands.h"
#include "result.h"

#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <vector>
constexpr int HIST_MAX = 1024;

// Catégorie d'une commande — sert à la colorisation (unique source de vérité)
enum class CmdCat {
    ARITH, CONSTANT, UNARY, TRIG, HYP, STACK, MEM, MODE, CONV, HIST, VAR, QUIT, HELP
};

struct RegistryEntry {
    CmdCat cat;
    std::unique_ptr<const Command> cmd;
};

// Résultat structuré d'une ligne complète, rendu par le front-end.
struct LineResult {
    Outputs outputs;            // sorties explicites des commandes (ordre d'exécution)
    bool silent = false;        // une commande d'affichage a eu lieu ; pas d'écho du sommet
    bool quit = false;          // quit/exit demandé
    std::string shell;          // non vide : invitation d'échappement shell (!cmd) —
                                // l'exécution est la prérogative du front-end interactif
};

// ============================================================
// RPN — le moteur sans affichage (étape A du plan Swift).
// Toute impression (terminal, ANSI, termios) vit dans le front-end ;
// le moteur évalue, maintient l'état et produit des sorties structurées.
// ============================================================

class RPN {
    std::vector<Value> st;   // pile, sommet = st.back(); entiers exacts = BigInt
    std::optional<Value> last_x;  // LAST X (HP-15C) : x capturé avant calcul — volatil
    bool visual_mode = false;
    bool deg_mode = true;
    DisplayMode disp_mode = DisplayMode::DEC;
    int precision = 10;
    double memory = 0.0;
    std::unordered_map<std::string, Value> mem;
    bool expect_var = false;
    std::string var_name;
    bool var_is_store = false;
    bool quit_requested = false;
    bool allow_shell = false;   // positionné par le front-end interactif
    bool save_requested = false;   // commande save : l'action se joue en fin de ligne
    bool load_requested = false;   // commande load : idem
    bool expect_file = false;      // commande run : le token suivant est un chemin
    bool clear_hist_requested = false;   // commande hclear : vide en fin de ligne
    bool debug_mode = false;       // mode debug (todo §8) : trace par token
    int run_depth = 0;             // imbrication des run (garde-fou anti-boucle)
    std::vector<std::string> history;
    std::unordered_map<std::string, RegistryEntry> registry;
    std::string data_dir = ".rpn_data";   // persistance (set_data_dir)

    std::string hist_file() const { return data_dir + "/historique.txt"; }
    std::string var_file() const { return data_dir + "/variables.txt"; }

public:
    RPN();

    // Évaluation d'une ligne : les sorties structurées voyagent dans le
    // LineResult, le moteur n'imprime jamais rien. record_history = false
    // pour les évaluations internes d'un front-end (échantillonnage de
    // courbe) : la pile et les variables bougent, l'historique reste propre.
    LineResult process_line(const std::string& line, bool record_history = true);

    // Retire et rend le sommet de la pile (nullopt si vide) — aide pour les
    // front-ends (lecture de paramètres sans passer par les commandes).
    std::optional<Value> pop_top();

    // —— État consultable par les front-ends (rendu, persistance partagée) ——
    const std::vector<Value>& stack() const { return st; }
    double memory_value() const { return memory; }
    bool is_visual() const { return visual_mode; }
    bool is_deg() const { return deg_mode; }
    DisplayMode display_mode() const { return disp_mode; }
    int precision_value() const { return precision; }
    const std::vector<std::string>& history_entries() const { return history; }
    bool quit_requested_v() const { return quit_requested; }

    // Répertoire de persistance : défaut « .rpn_data » RELATIF au répertoire
    // courant (comportement historique du CLI, utilisé par sa suite de
    // tests). Les front-ends installés (.app) fixent un dossier absolu —
    // l'app lancée depuis Finder n'a pas de répertoire courant stable.
    void set_data_dir(const std::string& dir) { data_dir = dir; }

    // L'échappement shell !cmd n'est exécuté que si le front-end l'autorise
    // (session interactive) ; sinon le moteur rend une erreur.
    void set_allow_shell(bool allow) { allow_shell = allow; }
    // Mode debug (todo §8) : posé par le front-end (`--debug`), basculé
    // par la commande `debug` — chaque token rend sa trace.
    void set_debug(bool on) { debug_mode = on; }

    // Persistance partagée entre front-ends (même .rpn_data/).
    void save_state();
    void load_state();

    // —— Recherche registre (couleur et complétion des front-ends) ——
    const RegistryEntry* lookup(const std::string& name) const;

    // Catégorie d'un token complet, ou d'un préfixe UNIQUE de commande
    // (coloration à la frappe : « si » ambigu → nullopt ; « sq » → UNARY).
    std::optional<CmdCat> category_for_partial(const std::string& tok) const;

private:
    bool process_token(const std::string& s, Context& ctx, Outputs& outs);
    bool handle_var_assignment(const std::string& s, Outputs& outs);
    bool handle_precision(const std::string& s);
    bool try_parse_number(const std::string& s, Outputs& outs);
    /// Notation collée (« 8 2/ », « 12 5- », « 5;3+ » après découpe du `;`)
    /// décomposée en atomes nombre/commande, exécutés un à un. Renvoie le
    /// drapeau d'affichage du dernier atome, `nullopt` quand rien ne se
    /// coupe (l'erreur « commande inconnue » canonique suivra). Règles :
    /// tiret collé = soustraction (le négatif s'écrit espacé ou `chs`) ;
    /// les commandes sto/rcl refusent le collage.
    std::optional<bool> executer_colle(const std::string& s, Context& ctx, Outputs& outs);

    void reg(const std::string& name, CmdCat cat, const Command* cmd);

    /// `run <fichier>` (todo §3) : exécute les lignes d'un script —
    /// chaque ligne vit comme une frappe (écho du sommet, historique,
    /// erreurs affichées, on continue). Renvoie le drapeau d'affichage.
    bool executer_run(const std::string& chemin, Outputs& outs);

    void ensure_data_dir();
    void save_history();
    void load_history();
    void save_variables();
    void load_variables();
    /// Lecture JSON du fichier de variables (notre format, écrit par
    /// save_variables) : faux si le contenu n'est pas notre JSON.
    bool charger_variables_json(const std::string& contenu);
};
