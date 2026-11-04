#include "rpn.h"
#include "i18n.h"

int main(int argc, char* argv[]) {
    i18n_init(argc, argv);   // --lang=<xx> ou LC_ALL/LANG, sinon anglais
    RPN calc;
    calc.run();
    return 0;
}
