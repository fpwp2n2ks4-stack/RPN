#include "rpn.h"
#include "terminal.h"
#include "i18n.h"

#include <cstring>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    i18n_init(argc, argv);   // --lang=<xx> ou LC_ALL/LANG, sinon anglais
    RPN engine;              // le moteur sans affichage (étape A du plan Swift)
    TerminalFrontend cli(engine);

    // Fichiers en argument (todo §4) : mode script/batch — l'entrée
    // standard reste le mode par défaut. `--debug` (todo §8) trace
    // chaque token ; les autres options (« --lang=… », déjà consommées
    // par i18n_init) commencent par « - ».
    std::vector<std::string> fichiers;
    for (int i = 1; i < argc; ++i) {
        if (!argv[i]) continue;
        if (std::strcmp(argv[i], "--debug") == 0) { engine.set_debug(true); continue; }
        if (argv[i][0] != '-') fichiers.emplace_back(argv[i]);
    }

    if (fichiers.empty()) cli.run();
    else cli.run_files(fichiers);
    return 0;
}
