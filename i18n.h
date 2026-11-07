#pragma once

#include <string>

// ============================================================
// i18n — messages d'exécution en anglais, français, espagnol.
// Détection : --lang=<xx> sur la ligne de commande, sinon
// LC_ALL, LC_MESSAGES, LANG (préfixe de la locale), sinon anglais.
// Les clés sont les chaînes anglaises ; T() rend la traduction.
// ============================================================

enum class Lang { EN = 0, FR = 1, ES = 2 };

// Détection + éventuel forçage --lang=<xx>. À appeler avant tout affichage.
void i18n_init(int argc, char* argv[]);

// Force la langue ("en"/"fr"/"es", préfixe de locale accepté) — façade C ABI.
void i18n_set(const char* lang);

Lang lang_current();

// Nom lisible ("english", "français", "español")
const char* lang_name(Lang l);

// Traduction : clé absente du catalogue → la clé elle-même (anglais).
const char* T(const char* key);
