#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// ============================================================
// BigInt — entiers signés de taille arbitraire, sans dépendance.
// Stockage : magnitude en base 10^9 par chunks (décimal direct),
// convertie en mots base 2^32 pour l'affichage hex/bin.
// ============================================================

class BigInt {
public:
    BigInt() = default;
    BigInt(const BigInt&) = default;
    BigInt& operator=(const BigInt&) = default;
    BigInt(BigInt&&) = default;
    BigInt& operator=(BigInt&&) = default;

    // Reconnaît uniquement [+-]?[0-9]+ (pas de point, pas d'exposant)
    static bool looks_like_integer(const std::string& s);
    static BigInt from_str(const std::string& s);
    static BigInt from_u64(uint64_t v);

    bool  is_zero()  const { return mag_.empty(); }
    bool  negative() const { return negative_; }
    int   sign() const;

    std::string to_string() const;
    double      to_double() const;
    std::optional<uint64_t> to_u64() const; // nullopt si négatif ou hors plage

    std::string to_hex() const;            // magnitude, sans préfixe ni signe
    std::string to_bin() const;            // magnitude, sans préfixe ni signe

    // Amis — opérations sur les magnitudes (b != 0 requis pour divmod)
    friend BigInt add(const BigInt& a, const BigInt& b);
    friend BigInt sub(const BigInt& a, const BigInt& b);
    friend BigInt mul(const BigInt& a, const BigInt& b);
    friend void divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r);
    friend int  cmp(const BigInt& a, const BigInt& b);

private:
    // chunks base 10^9, petit-boutiste (mag_[0] = poids faible) ; vide = zéro
    std::vector<uint32_t> mag_;
    bool negative_ = false;

    void normalize();  // supprime les chunks de tête nuls, normalise le signe
};

BigInt add(const BigInt& a, const BigInt& b);
BigInt sub(const BigInt& a, const BigInt& b);   // a - b
BigInt mul(const BigInt& a, const BigInt& b);
// Division euclidienne tronquée (quotient vers zéro, reste du signe de a)
void divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r);
int  cmp(const BigInt& a, const BigInt& b);    // -1, 0, +1

// ============================================================
// Value — ce que contient la pile : soit un double, soit un
// entier exact BigInt. La frappe décide : 2 → BigInt, 2.0 → double.
// ============================================================

struct Value {
    std::optional<BigInt> bi;   // présent → entier exact
    double d = 0.0;

    static Value num(double x) { Value v; v.d = x; return v; }
    static Value big(BigInt x) {
        Value v;
        v.bi = std::move(x);
        return v;
    }
    bool   is_big() const { return bi.has_value(); }
    double to_double() const { return bi ? bi->to_double() : d; }
};

// ============================================================
// Opérations exactes BigInt → Value.
// nullopt = « pas d'op exacte ici » : le moteur retombe en double
// (division par zéro → NaN → message ; exposant négatif → pow double…).
// ============================================================

namespace big {
    std::optional<Value> add(const BigInt& a, const BigInt& b);
    std::optional<Value> sub(const BigInt& a, const BigInt& b);
    std::optional<Value> mul(const BigInt& a, const BigInt& b);
    std::optional<Value> div_quot(const BigInt& a, const BigInt& b);
    std::optional<Value> mod(const BigInt& a, const BigInt& b);
    std::optional<Value> pow_big(const BigInt& a, const BigInt& e);
    std::optional<Value> neg(const BigInt& a);
    std::optional<Value> abs(const BigInt& a);
    std::optional<Value> square(const BigInt& a);
    std::optional<Value> same(const BigInt& a);   // identité (floor/ceil/round/trunc)
}
