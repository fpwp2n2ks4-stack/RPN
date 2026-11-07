#pragma once

#include "rpn.h"

#include <string>
#include <vector>

// ============================================================
// ANSI colors — service du front-end terminal (le moteur n'en
// connaît aucune, étape A du plan Swift).
// ============================================================

namespace C {
    inline constexpr const char* RESET   = "\033[0m";
    inline constexpr const char* BOLD    = "\033[1m";
    inline constexpr const char* DIM     = "\033[2m";
    inline constexpr const char* RED     = "\033[31m";
    inline constexpr const char* GREEN   = "\033[32m";
    inline constexpr const char* YELLOW  = "\033[33m";
    inline constexpr const char* BLUE    = "\033[34m";
    inline constexpr const char* MAGENTA = "\033[35m";
    inline constexpr const char* CYAN    = "\033[36m";
    inline constexpr const char* WHITE    = "\033[37m";
    inline constexpr const char* GRAY     = "\033[90m";
}

// ============================================================
// TerminalFrontend — le front-end CLI du moteur RPN : boucle de
// lecture (brut termios, historique par flèches), coloration live,
// rendu des Output (messages, valeurs, pile, boîte d'aide) et
// échappement shell. Le moteur, lui, n'imprime rien.
// ============================================================

class TerminalFrontend {
public:
    explicit TerminalFrontend(RPN& engine) : eng(engine) {}

    void run();
    // Mode fichier (todo §4) : les scripts passés en argument
    // s'exécutent en séquence — la pile traverse les fichiers.
    void run_files(const std::vector<std::string>& chemins);

private:
    RPN& eng;
    bool interactive = false;
    bool use_color = false;

    void render(const LineResult& res);
    void render_output(const Output& o);
    void render_help() const;
    void display_result() const;

    void print_num(const Value& v) const;
    void print_stack_lines(const std::vector<Value>& v,
                           const char* idx_col = "", const char* reset_col = "") const;

    void prompt() const;
    void colorize(const std::string& line) const;
    std::string cat_color(CmdCat cat) const;
    std::string token_color(const std::string& tok) const;
    std::string color_for_partial(const std::string& tok) const;
    void print_colored_tokens(const std::string& line) const;
    void recolor_last_token(const std::string& buf) const;

    bool read_line_interactive(std::string& out);
    // Ctrl-R (todo §7) : recherche arrière dans l'historique — vrai
    // quand une entrée est retenue ET soumise (rappel + Entrée).
    bool recherche_interactive(std::string& out);
    void replace_line(std::string& field, const std::string& val);
    static int read_char_raw();
};
