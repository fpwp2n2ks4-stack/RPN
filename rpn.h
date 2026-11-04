#pragma once

#include "commands.h"

#include <string>
#include <unordered_map>
#include <memory>
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

class RPN {
    std::vector<Value> st;   // pile, sommet = st.back(); entiers exacts = BigInt
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
    bool is_interactive = false;
    bool use_color = false;
    std::vector<std::string> history;
    std::unordered_map<std::string, RegistryEntry> registry;

    static constexpr const char* DATA_DIR  = ".rpn_data";
    static constexpr const char* HIST_FILE = ".rpn_data/historique.txt";
    static constexpr const char* VAR_FILE  = ".rpn_data/variables.txt";

public:
    RPN();
    void run();

private:
    void process_line(const std::string& line);
    void display_result() const;
    void print_stack() const;
    void prompt() const;
    void colorize(const std::string& line) const;
    std::string cat_color(CmdCat cat) const;
    std::string token_color(const std::string& tok) const;
    std::string color_for_partial(const std::string& tok) const;
    void print_colored_tokens(const std::string& line) const;
    void recolor_last_token(const std::string& buf) const;

    bool process_token(const std::string& s, Context& ctx);
    bool handle_var_assignment(const std::string& s);
    bool handle_precision(const std::string& s);
    bool try_parse_number(const std::string& s);

    bool read_line_interactive(std::string& out);
    void replace_line(std::string& field, const std::string& val);
    static int read_char_raw();

    void reg(const std::string& name, CmdCat cat, const Command* cmd);

    void ensure_data_dir();
    void save_history();
    void load_history();
    void save_variables();
    void load_variables();
};
