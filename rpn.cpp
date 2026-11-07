#include "rpn.h"

#include <sstream>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <charconv>
#include <cstdio>
#include <array>
#include <limits>

// kPi/kE sont des macros POSIX, pas du C++23 strict — constantes propres.
constexpr double kPi = 3.141592653589793;
constexpr double kE  = 2.718281828459045;

// Retire le commentaire d'une ligne (todo §5) : tout ce qui suit « # »
// ou « // » s'ignore — les deux marqueurs au plus tôt gagnent, et « // »
// se coupe avant un simple « / » collé (« 8 2// note » garde la division).
std::string sans_commentaires(const std::string& s) {
    size_t pos = s.find('#');
    const size_t pos2 = s.find("//");
    if (pos2 != std::string::npos && (pos == std::string::npos || pos2 < pos))
        pos = pos2;
    if (pos == std::string::npos) return s;
    return s.substr(0, pos);
}

// —— JSON du fichier de variables (todo §2) : l'écriture est la nôtre,
// la lecture aussi — stricte mais minimale ; tout écart fait retomber
// l'appelant sur l'ancien format texte (rétrocompatibilité).
struct LecteurJson {
    const std::string& s;
    size_t i = 0;
    void blancs() {
        while (i < s.size()
               && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
            ++i;
    }
    bool prend(char c) {
        blancs();   // nos propres écritures espaçent librement autour des signes
        if (i < s.size() && s[i] == c) {
            ++i;
            return true;
        }
        return false;
    }
};

std::string json_chaine(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '\\' || c == '"') out += '\\';
        out += c;                        // l'UTF-8 passe octet par octet
    }
    return out + "\"";
}

// Trace du mode debug (todo §8) : le token vient d'être traité, la
// pile se dit entière — BigInt exact, double à 17 chiffres (la trace
// est un outil, pas un affichage : pas de mode, pas de i18n).
std::string trace_debug(const std::string& tok, const std::vector<Value>& pile) {
    std::string out = "\u00ab " + tok + " \u00bb \u2192 pile(";
    out += std::to_string(pile.size()) + "):";
    for (size_t i = 0; i < pile.size(); ++i) {
        out += ' ';
        if (pile[i].is_big()) out += pile[i].bi->to_string();
        else {
            std::array<char, 32> buf{};
            std::snprintf(buf.data(), buf.size(), "%.17g", pile[i].d);
            out += buf.data();
        }
    }
    return out;
}

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
    reg("lastx", CmdCat::STACK, new LastX());
    reg("lstx",  CmdCat::STACK, new LastX());
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
    // Pourcentages HP — la référence y reste sur la pile ; le `%` modulo
    // (alias dc) ne bouge pas (todo.md §0.2, option B).
    reg("pct", CmdCat::ARITH, new PercentOp(PercentOp::Kind::OF));       // x % de y
    reg("d%",  CmdCat::ARITH, new PercentOp(PercentOp::Kind::CHANGE));    // Δ% : (x−y)/y×100
    reg("%t",  CmdCat::ARITH, new PercentOp(PercentOp::Kind::TOTAL));     // %T : part de x dans y
    reg("~", CmdCat::ARITH, new DivmodOp());

    // Modes
    reg("visual", CmdCat::MODE, new VisualToggle());
    reg("deg",    CmdCat::MODE, new AngleSetter(true));
    reg("rad",    CmdCat::MODE, new AngleSetter(false));
    reg("dec",    CmdCat::MODE, new DisplaySetter(DisplayMode::DEC));
    reg("hex",    CmdCat::MODE, new DisplaySetter(DisplayMode::HEX));
    reg("bin",    CmdCat::MODE, new DisplaySetter(DisplayMode::BIN));
    reg("sci",    CmdCat::MODE, new DisplaySetter(DisplayMode::SCI));
    reg("eng",    CmdCat::MODE, new DisplaySetter(DisplayMode::ENG));
    reg("debug",  CmdCat::MODE, new DebugToggle());     // mode debug (todo §8)

    // Alias de frappe HP-15C (todo.md §0.4) : même comportement que les
    // commandes longues, autres instances — le registre possède ses
    // commandes par entrée (unique_ptr).
    reg("1/x",  CmdCat::UNARY, new UnaryOp([](double x) {
        return x == 0.0 ? std::numeric_limits<double>::quiet_NaN() : 1.0 / x;
    }, "inv: division by zero"));
    reg("chs",  CmdCat::UNARY, new UnaryOp([](double x) { return -x; },
        "Result undefined (nan)", &big::neg));
    reg("e^x",  CmdCat::UNARY, new UnaryOp([](double x) { return std::exp(x); }));
    reg("y^x",  CmdCat::ARITH, new BinaryOp([](double a, double b) { return std::pow(a, b); },
        "pow: undefined operation (negative base, non-integer exponent)", &big::pow_big));
    reg("x<>y", CmdCat::STACK, new Swap());
    reg("x↔y",  CmdCat::STACK, new Swap());

    // Variables nommées
    reg("sto", CmdCat::VAR, new StoRcl(true));
    reg("rcl", CmdCat::VAR, new StoRcl(false));

    // Persistance explicite & scripts (todo §1-§5)
    reg("save",    CmdCat::VAR, new SaveVars());
    reg("savevar", CmdCat::VAR, new SaveVars());
    reg("load",    CmdCat::VAR, new LoadVars());
    reg("loadvar", CmdCat::VAR, new LoadVars());
    reg("run",     CmdCat::VAR, new RunCmd());

    // Mémoire accumulateur
    reg("m+", CmdCat::MEM, new MemoryOp(MemOp::ADD));
    reg("m-", CmdCat::MEM, new MemoryOp(MemOp::SUB));
    reg("m*", CmdCat::MEM, new MemoryOp(MemOp::MUL));
    reg("m/", CmdCat::MEM, new MemoryOp(MemOp::DIV));
    reg("mr", CmdCat::MEM, new MemoryOp(MemOp::RECALL));
    reg("mc", CmdCat::MEM, new MemoryOp(MemOp::CLEAR));
    reg("ms", CmdCat::MEM, new MemShow());

    // Historique
    reg("history",  CmdCat::HIST, new HistoryCmd());
    reg("hist",     CmdCat::HIST, new HistoryCmd());
    reg("h",        CmdCat::HIST, new HistoryCmd());
    reg("hclear",   CmdCat::HIST, new HistClear());     // vide l'historique (todo §7)
    reg("histclear", CmdCat::HIST, new HistClear());

    // Conversions d'unités
    reg("m2cm",  CmdCat::CONV, new UnitConv(100.0));
    reg("cm2m",  CmdCat::CONV, new UnitConv(0.01));
    reg("m2ft",  CmdCat::CONV, new UnitConv(3.28084));
    reg("ft2m",  CmdCat::CONV, new UnitConv(0.3048));
    reg("kg2lb",  CmdCat::CONV, new UnitConv(2.20462));
    reg("lb2kg",  CmdCat::CONV, new UnitConv(0.453592));
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
// Traitement d'une ligne — sorties structurées uniquement
// ============================================================

LineResult RPN::process_line(const std::string& line, bool record_history) {
    LineResult res;
    if (line.empty()) return res;

    if (line[0] == '!' && line.size() > 1 && line[1] != '!') {
        // Échappement shell réservé à la session interactive : un `!cmd` venu
        // d'un pipe ou d'un fichier exécuterait du code sans accord utilisateur.
        // Le moteur ne l'exécute pas : il le remonte au front-end habilité.
        if (!allow_shell) {
            res.outputs.push_back(
                out_err(T("Shell escape (!) is only allowed in interactive sessions")));
            return res;
        }
        res.shell = line.substr(1);
        return res;
    }

    // Commentaires en ligne (todo §5) : tout ce qui suit « # » ou « // »
    // s'ignore, en frappe comme en script. Les « !cmd » restent entiers —
    // le shell fait ses propres commentaires. Effet de bord assumé : un
    // nom de variable contenant « # » ne s'écrit plus (la frappe espace).
    std::string texte = sans_commentaires(line);
    if (texte.empty()) { res.silent = true; return res; }

    Context ctx{st, memory, history, visual_mode, deg_mode, disp_mode,
                precision, quit_requested, expect_var, var_name, last_x,
                save_requested, load_requested, expect_file,
                debug_mode, clear_hist_requested};

    // Pré-découpe : le « ; » collé (« 5;3+ », « 8 2/;p ») devient des
    // tokens séparés avant l'analyse — le reste de la ligne ne saura
    // jamais qu'ils étaient collés. Un octet « ; » (0x3B) n'existe dans
    // aucune séquence UTF-8 : la découpe est sûre pour « x↔y » etc.
    std::vector<std::string> toks;
    {
        std::istringstream iss(texte);
        std::string mot;
        while (iss >> mot) {
            if (mot.find(';') == std::string::npos) {
                toks.push_back(mot);
                continue;
            }
            std::string piece;
            for (char c : mot) {
                if (c != ';') { piece += c; continue; }
                if (!piece.empty()) { toks.push_back(piece); piece.clear(); }
                toks.push_back(";");
            }
            if (!piece.empty()) toks.push_back(piece);
        }
    }

    // Le séparateur « ; » solde un segment comme un retour chariot le
    // ferait : écho du sommet, entrée d'historique propre, drapeau silent.
    // Décisions (todo §12) : chaque segment a SA ligne d'historique ;
    // « quit » clôt la ligne en cours de route (le reste de la ligne ne
    // s'exécute pas) ; le duo « sto »/nom de variable traverse le
    // séparateur sans se couper (machine à états inter-tokens).
    const bool segmente = texte.find(';') != std::string::npos;
    if (record_history && !segmente) {
        // Chemin immuable d'origine : la ligne utile entière.
        history.push_back(texte);
        if (static_cast<int>(history.size()) > HIST_MAX)
            history.erase(history.begin());
    }
    if (!segmente) {
        for (const std::string& tok : toks) {
            res.silent = process_token(tok, ctx, res.outputs);
            if (debug_mode)
                res.outputs.push_back(out_debug(trace_debug(tok, st)));
            if (quit_requested) break;
        }
    } else {
        std::string segment;             // tokens du segment courant, recollés
        bool segment_display = false;     // le segment vient d'afficher lui-même
        bool solde = false;              // le dernier segment est déjà soldé
        auto enregistrer = [&](const std::string& contenu) {
            if (!record_history || contenu.empty()) return;
            history.push_back(contenu);
            if (static_cast<int>(history.size()) > HIST_MAX)
                history.erase(history.begin());
        };
        auto solder = [&]() {
            solde = true;
            if (segment.empty()) return;   // « ;; » : rien à solder de plus
            enregistrer(segment);
            // Écho du sommet, comme en fin de ligne — sauf si le segment
            // vient d'en afficher un lui-même (stack, p, help…).
            if (!segment_display && !st.empty())
                res.outputs.push_back(out_value(st.back()));
            segment.clear();
            segment_display = false;
        };
        for (const std::string& tok : toks) {
            if (tok == ";") {
                if (!expect_var) solder();   // sto/rcl : « ; » traversant
                continue;
            }
            solde = false;                   // nouveau token → segment ouvert
            if (!segment.empty()) segment += ' ';
            segment += tok;
            segment_display = process_token(tok, ctx, res.outputs);
            if (debug_mode)
                res.outputs.push_back(out_debug(trace_debug(tok, st)));
            if (quit_requested) break;  // quit clôt la ligne, le reste s'arrête
        }
        if (!segment.empty()) {
            // Le segment final non soldé vit comme une ligne ordinaire.
            enregistrer(segment);
            res.silent = segment_display;
        } else if (solde) {
            // La ligne finit par « ; » : chaque écho a déjà eu lieu au
            // séparateur — pas de double écho en fin de ligne.
            res.silent = true;
        }
    }

    if (expect_var) {
        res.outputs.push_back(out_err(T("sto/rcl requires a variable name")));
        expect_var = false;
    }
    if (expect_file) {
        res.outputs.push_back(out_err(T("run requires a file name")));
        expect_file = false;
    }
    // Persistance explicite (todo §1) : le drapeau levé par la commande
    // se joue ici, une fois par ligne — les fichiers et les messages
    // appartiennent au moteur.
    if (save_requested) {
        save_requested = false;
        save_variables();
        res.outputs.push_back(out_msg(T("Variables saved in file")));
    }
    if (load_requested) {
        load_requested = false;
        load_variables();
        res.outputs.push_back(out_msg(T("Variables reloaded")));
    }
    // hclear (todo §7) : l'historique vide, mémoire et fichier — la
    // ligne courante reste dans l'historique nul, la suivante commence
    // un historique neuf.
    if (clear_hist_requested) {
        clear_hist_requested = false;
        history.clear();
        save_history();
        res.outputs.push_back(out_msg(T("history cleared")));
    }

    res.quit = quit_requested;
    return res;
}

// ============================================================
// Traitement d'un token
// ============================================================

bool RPN::process_token(const std::string& s, Context& ctx, Outputs& outs) {
    if (handle_var_assignment(s, outs)) return false;
    // `run` armé : ce token est un chemin de fichier, pas une commande.
    if (expect_file) {
        expect_file = false;
        return executer_run(s, outs);
    }
    if (handle_precision(s)) return true;
    auto it = registry.find(s);
    if (it != registry.end()) {
        const Command* c = it->second.cmd.get();
        if (c->kind() == CommandKind::STO_RCL)
            var_is_store = static_cast<const StoRcl*>(c)->is_store();
        Outputs produced = c->exec(ctx);
        outs.insert(outs.end(), produced.begin(), produced.end());
        return c->is_display();
    }
    // Notation collée : « 5;3+ » (le ; découpé en amont), « 8 2/ »,
    // « 12 5- » — des frappes sans Entrée, l'ordre postfixe est préservé.
    if (s.size() >= 2) {
        if (auto affiche = executer_colle(s, ctx, outs)) return *affiche;
    }
    return try_parse_number(s, outs);
}

// ============================================================
// Notation collée — atomes nombre/commande, frappes sans Entrée
// ============================================================

std::optional<bool> RPN::executer_colle(const std::string& s, Context& ctx, Outputs& outs) {
    // Vrai quand le token se lit entièrement comme un nombre — le tiret
    // collé qui suit un tel atome est alors le « - » arithmétique.
    auto est_nombre = [](const std::string& t) {
        double v = 0.0;
        auto [p, ec] = std::from_chars(t.data(), t.data() + t.size(), v);
        return ec == std::errc{} && p == t.data() + t.size();
    };

    // Phase d'analyse pure : rien ne s'exécute si la découpe échoue —
    // l'erreur « commande inconnue » canonique reste à l'appelant.
    std::vector<std::string> atomes;
    std::string reste = s;
    bool apres_nombre = false;
    while (!reste.empty()) {
        size_t coupe = 0;
        if (apres_nombre && reste[0] == '-') {
            coupe = 1;   // le tiret collé après un nombre soustrait (règle §12)
        } else {
            // la commande enregistrée qui préfixe le reste — la plus
            // longue gagne ; jamais sto/rcl : leur nom de variable
            // doit rester un token à part.
            size_t nom = 0;
            for (const auto& [cle, entree] : registry) {
                if (entree.cmd->kind() == CommandKind::STO_RCL) continue;
                if (cle.size() > nom && reste.compare(0, cle.size(), cle) == 0) {
                    nom = cle.size();
                }
            }
            // le nombre le plus long : from_chars dit où il s'arrête
            size_t num = 0;
            double v = 0.0;
            auto [p, ec] = std::from_chars(reste.data(), reste.data() + reste.size(), v);
            if (p != reste.data()) num = static_cast<size_t>(p - reste.data());
            coupe = std::max(nom, num);
        }
        if (coupe == 0) return std::nullopt;
        std::string atome = reste.substr(0, coupe);
        apres_nombre = est_nombre(atome);
        atomes.push_back(atome);
        reste = coupe < reste.size() ? reste.substr(coupe) : "";
    }
    // Un seul atome = le token entier (nombre pur collé de rien) : ce
    // n'est pas du collage — le traitement normal du token reprend, sinon
    // la récursion atomes↔process_token ne s'arrêterait jamais.
    if (atomes.size() < 2) return std::nullopt;

    bool affichage = false;
    for (const std::string& atome : atomes) {
        affichage = process_token(atome, ctx, outs);
        if (quit_requested) break;   // quit clôt la ligne, même collé
    }
    return affichage;
}

// ============================================================
// run <fichier> — le script vit comme des frappes (todo §3)
// ============================================================

bool RPN::executer_run(const std::string& chemin, Outputs& outs) {
    // Un script qui se run lui-même bouclerait : la profondeur est
    // plafonnée (un run dans un run dans un run… reste humain).
    if (run_depth >= 16) {
        outs.push_back(out_err(T("run: nesting too deep")));
        return true;
    }
    std::ifstream f(chemin);
    if (!f) {
        char buf[512];
        std::snprintf(buf, sizeof buf, T("Cannot open file: %s"), chemin.c_str());
        outs.push_back(out_err(buf));
        return true;
    }

    // L'annonce précède l'exécution : les échos des lignes suivent, le
    // dernier résultat reste la dernière ligne visible.
    std::vector<std::string> lignes;
    int compte = 0;
    {
        std::string ligne;
        while (std::getline(f, ligne)) {
            if (sans_commentaires(ligne).find_first_not_of(" \t") == std::string::npos)
                continue;                       // ligne vide ou tout-commentaire
            ++compte;
            lignes.push_back(ligne);
        }
    }
    char buf[512];
    std::snprintf(buf, sizeof buf, T("run: %s — %d lines"),
                  chemin.c_str(), compte);
    outs.push_back(out_msg(buf));

    // Politique d'erreur (todo §3, décision) : on CONTINUE — chaque ligne
    // rapporte ses erreurs comme une frappe, la suivante s'exécute. `quit`
    // dans le script arrête tout, comme une frappe.
    ++run_depth;
    for (const std::string& ligne : lignes) {
        LineResult r = process_line(ligne, true);
        outs.insert(outs.end(), r.outputs.begin(), r.outputs.end());
        // L'écho de fin de ligne des frappes : le script l'a aussi.
        if (!r.silent && !st.empty()) outs.push_back(out_value(st.back()));
        if (r.quit || quit_requested) break;
    }
    --run_depth;
    return true;   // run est une commande d'affichage : pas d'écho final dupliqué
}

bool RPN::handle_var_assignment(const std::string& s, Outputs& outs) {
    if (!expect_var) return false;
    var_name = s;
    if (var_is_store) {
        if (st.empty()) {
            outs.push_back(out_err(std::string(T("Stack is empty")) + " (sto)"));
        } else {
            mem[var_name] = st.back();
            st.pop_back();
        }
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

bool RPN::try_parse_number(const std::string& s, Outputs& outs) {
    // Entier plein → exact (BigInt) ; sinon double (point, exposant…)
    if (BigInt::looks_like_integer(s)) {
        st.push_back(Value::big(BigInt::from_str(s)));
        return false;
    }
    double v = 0.0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size()) {
        outs.push_back(out_err(std::string(T("Unknown command")) + ": " + s));
        return false;
    }
    st.push_back(Value::num(v));
    return false;
}

// ============================================================
// Pile — aide pour les front-ends
// ============================================================

std::optional<Value> RPN::pop_top() {
    if (st.empty()) return std::nullopt;
    Value v = st.back();
    st.pop_back();
    return v;
}

// ============================================================
// Recherche registre
// ============================================================

const RegistryEntry* RPN::lookup(const std::string& name) const {
    auto it = registry.find(name);
    return it == registry.end() ? nullptr : &it->second;
}

std::optional<CmdCat> RPN::category_for_partial(const std::string& tok) const {
    if (const RegistryEntry* e = lookup(tok)) return e->cat;
    const RegistryEntry* uniq = nullptr;   // préfixe unique d'une seule commande ?
    for (const auto& [name, e] : registry) {
        if (name.rfind(tok, 0) == 0) {
            if (uniq) return std::nullopt;   // ambigu : « si » → sin/sinh
            uniq = &e;
        }
    }
    return uniq ? std::make_optional(uniq->cat) : std::nullopt;
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

void RPN::ensure_data_dir() { std::filesystem::create_directories(data_dir); }

void RPN::save_history() {
    ensure_data_dir();
    std::ofstream f(hist_file());
    for (auto& h : history) f << h << "\n";
}

void RPN::load_history() {
    std::ifstream f(hist_file());
    std::string line;
    while (std::getline(f, line))
        if (!line.empty() && static_cast<int>(history.size()) < HIST_MAX)
            history.push_back(line);
}

void RPN::save_variables() {
    ensure_data_dir();
    std::ofstream f(var_file());
    f << "{\n";
    bool premier = true;
    for (auto& [k, v] : mem) {
        if (!premier) f << ",\n";
        premier = false;
        // JSON (todo §2) : le type voyage avec la valeur — chaîne décimale
        // exacte pour un BigInt ; pour un double, to_chars rend la plus
        // courte écriture qui fait l'aller-retour exact (%.17g ne suffit
        // pas : %g élague les zéros, un digit peut manquer).
        std::string valeur;
        if (v.is_big()) {
            valeur = v.bi->to_string();
        } else {
            std::array<char, 32> buf{};
            auto [p, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), v.d);
            valeur = std::string(buf.data(), p);
        }
        f << "  " << json_chaine(k) << ": {\"type\": \""
          << (v.is_big() ? 'b' : 'd') << "\", \"value\": " << json_chaine(valeur)
          << "}";
    }
    if (!premier) f << "\n";
    f << "}\n";
}

bool RPN::charger_variables_json(const std::string& contenu) {
    // Notre propre JSON, simple et strict : des paires « clé : { "type":
    // "b|d", "value": "…" } ». Tout écart → faux : l'appelant retombe sur
    // l'ancien format texte (rétrocompatibilité).
    LecteurJson j{contenu};
    auto cle = [&]() -> std::optional<std::string> {
        j.blancs();
        if (!j.prend('"')) return std::nullopt;
        std::string out;
        while (j.i < contenu.size() && contenu[j.i] != '"') {
            if (contenu[j.i] == '\\' && j.i + 1 < contenu.size()) ++j.i;
            out += contenu[j.i++];
        }
        if (!j.prend('"')) return std::nullopt;
        return out;
    };
    j.blancs();
    if (!j.prend('{')) return false;
    j.blancs();
    if (j.prend('}')) return true;   // fichier vide {} : rien à charger
    while (true) {
        auto nom = cle();
        if (!nom) return false;
        if (!j.prend(':') || !j.prend('{')) return false;
        std::string type, valeur;
        bool bon_type = false, bon_poids = false;
        while (true) {
            auto champ = cle();
            if (!champ || (*champ != "type" && *champ != "value"))
                return false;
            if (!j.prend(':')) return false;
            auto contenu_champ = cle();
            if (!contenu_champ) return false;
            if (*champ == "type") { type = *contenu_champ; bon_type = true; }
            else                  { valeur = *contenu_champ; bon_poids = true; }
            if (j.prend(',')) continue;
            if (!j.prend('}')) return false;
            break;
        }
        if (!bon_type || !bon_poids) return false;
        if (type == "b") {
            if (!BigInt::looks_like_integer(valeur)) return false;
            mem[*nom] = Value::big(BigInt::from_str(valeur));
        } else if (type == "d") {
            double d = 0.0;
            auto [p, ec] = std::from_chars(valeur.data(),
                                           valeur.data() + valeur.size(), d);
            if (ec != std::errc{} || p != valeur.data() + valeur.size())
                return false;
            mem[*nom] = Value::num(d);
        } else {
            return false;
        }
        if (j.prend(',')) continue;
        if (!j.prend('}')) return false;
        break;
    }
    j.blancs();
    return j.i >= contenu.size();
}

void RPN::load_variables() {
    std::ifstream f(var_file());
    if (!f) return;
    std::stringstream buf;
    buf << f.rdbuf();
    const std::string contenu = buf.str();
    // JSON d'abord (notre format) ; l'ancien texte « nom b|d valeur »
    // reste lu tel quel — migration silencieuse au prochain save.
    if (charger_variables_json(contenu)) return;
    std::istringstream flux(contenu);
    std::string name, tag;
    while (flux >> name >> tag) {
        if (tag == "b") {
            std::string digits;
            if (flux >> digits) mem[name] = Value::big(BigInt::from_str(digits));
        } else if (tag == "d") {
            double v;
            if (flux >> v) mem[name] = Value::num(v);
        } else {
            // ancien format sans marqueur : « nom valeur »
            double v = 0.0;
            auto [ptr, ec] = std::from_chars(tag.data(), tag.data() + tag.size(), v);
            if (ec == std::errc{} && ptr == tag.data() + tag.size())
                mem[name] = Value::num(v);
        }
    }
}

void RPN::save_state() {
    save_history();
    save_variables();
}

void RPN::load_state() {
    load_history();
    load_variables();
}
