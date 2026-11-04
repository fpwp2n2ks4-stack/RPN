#include "bigint.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace {

constexpr uint32_t DEC_BASE = 1000000000u; // 10^9

// ---------- magnitudes décimales (base 10^9) ----------

std::vector<uint32_t> mag_add(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<uint32_t> r;
    r.reserve(std::max(a.size(), b.size()) + 1);
    uint64_t carry = 0;
    for (size_t i = 0; i < std::max(a.size(), b.size()); ++i) {
        uint64_t cur = carry + (i < a.size() ? a[i] : 0u) + (i < b.size() ? b[i] : 0u);
        r.push_back(static_cast<uint32_t>(cur % DEC_BASE));
        carry = cur / DEC_BASE;
    }
    if (carry) r.push_back(static_cast<uint32_t>(carry));
    return r;
}

// |a| >= |b| requis
std::vector<uint32_t> mag_sub(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<uint32_t> r;
    r.reserve(a.size());
    int64_t borrow = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        int64_t cur = static_cast<int64_t>(a[i]) - (i < b.size() ? static_cast<int64_t>(b[i]) : 0) - borrow;
        if (cur < 0) { cur += DEC_BASE; borrow = 1; } else borrow = 0;
        r.push_back(static_cast<uint32_t>(cur));
    }
    return r;
}

std::vector<uint32_t> mag_mul(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    if (a.empty() || b.empty()) return {};
    std::vector<uint32_t> r(a.size() + b.size(), 0u);
    for (size_t i = 0; i < a.size(); ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < b.size(); ++j) {
            uint64_t cur = static_cast<uint64_t>(r[i + j]) +
                           static_cast<uint64_t>(a[i]) * b[j] + carry;
            r[i + j] = static_cast<uint32_t>(cur % DEC_BASE);
            carry = cur / DEC_BASE;
        }
        size_t k = i + b.size();
        while (carry) {
            uint64_t cur = static_cast<uint64_t>(r[k]) + carry;
            r[k] = static_cast<uint32_t>(cur % DEC_BASE);
            carry = cur / DEC_BASE;
            ++k;
        }
    }
    while (!r.empty() && r.back() == 0u) r.pop_back();
    return r;
}

int mag_cmp(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (size_t i = a.size(); i-- > 0; )
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

// ---------- conversions chunk décimal <-> mot binaire (base 2^32) ----------

std::vector<uint32_t> dec_to_words(const std::vector<uint32_t>& dec) {
    std::vector<uint32_t> w;
    for (size_t i = dec.size(); i-- > 0; ) {   // chunk le plus significatif d'abord
        uint64_t carry = dec[i];
        for (auto& x : w) {
            uint64_t cur = static_cast<uint64_t>(x) * DEC_BASE + carry;
            x = static_cast<uint32_t>(cur);
            carry = cur >> 32;
        }
        while (carry) { w.push_back(static_cast<uint32_t>(carry)); carry >>= 32; }
    }
    while (!w.empty() && w.back() == 0u) w.pop_back();
    return w;
}

std::vector<uint32_t> words_to_dec(const std::vector<uint32_t>& w) {
    std::vector<uint32_t> d;
    for (size_t i = w.size(); i-- > 0; ) {
        uint64_t carry = w[i];
        for (auto& c : d) {
            uint64_t cur = static_cast<uint64_t>(c) * 4294967296ULL + carry;
            c = static_cast<uint32_t>(cur % DEC_BASE);
            carry = cur / DEC_BASE;
        }
        while (carry) { d.push_back(static_cast<uint32_t>(carry)); carry /= DEC_BASE; }
    }
    while (!d.empty() && d.back() == 0u) d.pop_back();
    return d;
}

// ---------- mots binaires : comparaison, décalage, soustraction ----------

int w_cmp(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (size_t i = a.size(); i-- > 0; )
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

void w_shl1(std::vector<uint32_t>& w) {
    uint32_t carry = 0;
    for (auto& x : w) {
        uint32_t top = x >> 31;
        x = (x << 1) | carry;
        carry = top;
    }
    if (carry) w.push_back(carry);
}

// a -= b, |a| >= |b| requis
void w_sub(std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    int64_t borrow = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        int64_t cur = static_cast<int64_t>(a[i]) - (i < b.size() ? static_cast<int64_t>(b[i]) : 0) - borrow;
        if (cur < 0) { cur += 4294967296LL; borrow = 1; } else borrow = 0;
        a[i] = static_cast<uint32_t>(cur);
    }
}

size_t w_bitlen(const std::vector<uint32_t>& w) {
    if (w.empty()) return 0;
    size_t bits = 0;
    for (uint32_t top = w.back(); top != 0; top >>= 1) ++bits;
    return (w.size() - 1) * 32 + bits;
}

bool w_get_bit(const std::vector<uint32_t>& w, size_t i) {
    if ((i >> 5) >= w.size()) return false;
    return ((w[i >> 5] >> (i & 31)) & 1u) != 0;
}

} // namespace

// ============================================================
// Construction / conversion
// ============================================================

void BigInt::normalize() {
    while (!mag_.empty() && mag_.back() == 0u) mag_.pop_back();
    if (mag_.empty()) negative_ = false;
}

bool BigInt::looks_like_integer(const std::string& s) {
    if (s.empty()) return false;
    size_t pos = 0;
    if (s[0] == '-' || s[0] == '+') pos = 1;
    if (pos >= s.size()) return false;
    for (size_t i = pos; i < s.size(); ++i)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

BigInt BigInt::from_str(const std::string& s) {
    BigInt r;
    size_t pos = 0;
    if (!s.empty() && (s[0] == '-' || s[0] == '+')) {
        if (s[0] == '-') r.negative_ = true;
        pos = 1;
    }
    // groupes de 9 chiffres depuis la FIN : mag_ doit rester petit-boutiste
    // (les nombres construits par from_u64 le sont déjà).
    size_t end = s.size();
    while (end > pos) {
        size_t start = (end - pos > 9) ? end - 9 : pos;
        uint32_t chunk = 0;
        for (size_t k = start; k < end; ++k) chunk = chunk * 10u + static_cast<uint32_t>(s[k] - '0');
        r.mag_.push_back(chunk);
        end = start;
    }
    r.normalize();
    return r;
}

BigInt BigInt::from_u64(uint64_t v) {
    BigInt r;
    if (v == 0) return r;
    r.negative_ = false;
    while (v != 0) { r.mag_.push_back(static_cast<uint32_t>(v % DEC_BASE)); v /= DEC_BASE; }
    return r;
}

int BigInt::sign() const {
    if (mag_.empty()) return 0;
    return negative_ ? -1 : 1;
}

std::string BigInt::to_string() const {
    if (mag_.empty()) return "0";
    std::string out = negative_ ? "-" : "";
    out += std::to_string(mag_.back());
    for (size_t i = mag_.size() - 1; i-- > 0; ) {
        char buf[16];
        std::snprintf(buf, sizeof buf, "%09u", static_cast<unsigned>(mag_[i]));
        out += buf;
    }
    return out;
}

std::string BigInt::to_hex() const {
    auto w = dec_to_words(mag_);
    if (w.empty()) return "0";
    std::string out;
    char buf[16];
    std::snprintf(buf, sizeof buf, "%x", static_cast<unsigned>(w.back()));
    out += buf;
    for (size_t i = w.size() - 1; i-- > 0; ) {
        std::snprintf(buf, sizeof buf, "%08x", static_cast<unsigned>(w[i]));
        out += buf;
    }
    return out;
}

std::string BigInt::to_bin() const {
    auto w = dec_to_words(mag_);
    if (w.empty()) return "0";
    std::string out;
    for (uint32_t top = w.back(); top != 0; top >>= 1)
        out += static_cast<char>('0' + (top & 1u));
    std::reverse(out.begin(), out.end());
    for (size_t i = w.size() - 1; i-- > 0; ) {
        std::string b;
        for (uint32_t x = w[i], k = 0; k < 32; ++k, x >>= 1)
            b += static_cast<char>('0' + (x & 1u));
        std::reverse(b.begin(), b.end());
        out += b;
    }
    return out;
}

double BigInt::to_double() const {
    std::string s = to_string();
    return std::strtod(s.c_str(), nullptr);   // arrondi double correct quelle que soit la longueur
}

std::optional<uint64_t> BigInt::to_u64() const {
    if (negative_) return std::nullopt;
    uint64_t v = 0;
    for (size_t i = mag_.size(); i-- > 0; ) {
        uint64_t chunk = mag_[i];
        if (v > (std::numeric_limits<uint64_t>::max() - chunk) / DEC_BASE)
            return std::nullopt;
        v = v * DEC_BASE + chunk;
    }
    return v;
}

// ============================================================
// Opérations signées
// ============================================================

BigInt add(const BigInt& a, const BigInt& b) {
    BigInt r;
    if (a.negative_ == b.negative_) {
        r.mag_ = mag_add(a.mag_, b.mag_);
        r.negative_ = a.negative_;
    } else {
        int c = mag_cmp(a.mag_, b.mag_);
        if (c == 0) return r;   // zéro
        if (c > 0) { r.mag_ = mag_sub(a.mag_, b.mag_); r.negative_ = a.negative_; }
        else       { r.mag_ = mag_sub(b.mag_, a.mag_); r.negative_ = b.negative_; }
    }
    r.normalize();
    return r;
}

BigInt sub(const BigInt& a, const BigInt& b) {
    BigInt mb = b;
    mb.negative_ = !mb.negative_ && !mb.mag_.empty();
    return add(a, mb);
}

BigInt mul(const BigInt& a, const BigInt& b) {
    BigInt r;
    r.mag_ = mag_mul(a.mag_, b.mag_);
    r.negative_ = !r.mag_.empty() && (a.negative_ != b.negative_);
    return r;
}

void divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r) {
    q = BigInt();
    r = BigInt();
    if (a.mag_.empty()) return;                       // 0 / b = 0, reste 0
    auto wa = dec_to_words(a.mag_);
    auto wb = dec_to_words(b.mag_);
    size_t nb = w_bitlen(wa);
    std::vector<uint32_t> wr;
    std::vector<uint32_t> wq((nb + 31) / 32, 0u);
    for (size_t i = nb; i-- > 0; ) {                   // du bit le plus fort au plus faible
        w_shl1(wr);
        if (w_get_bit(wa, i)) {
            if (wr.empty()) wr.push_back(1u);
            else wr[0] |= 1u;
        }
        if (w_cmp(wr, wb) >= 0) {
            w_sub(wr, wb);
            wq[i >> 5] |= (1u << (i & 31));
        }
    }
    q.mag_ = words_to_dec(wq);
    q.negative_ = !q.mag_.empty() && (a.negative_ != b.negative_);
    r.mag_ = words_to_dec(wr);
    r.negative_ = !r.mag_.empty() && a.negative_;
}

int cmp(const BigInt& a, const BigInt& b) {
    int sa = a.sign(), sb = b.sign();
    if (sa != sb) return sa < sb ? -1 : 1;
    int c = mag_cmp(a.mag_, b.mag_);
    return sa >= 0 ? c : -c;
}

// ============================================================
// Helpers Value (nullopt → repli double côté moteur)
// ============================================================

namespace big {

std::optional<Value> add(const BigInt& a, const BigInt& b) { return Value::big(::add(a, b)); }
std::optional<Value> sub(const BigInt& a, const BigInt& b) { return Value::big(::sub(a, b)); }
std::optional<Value> mul(const BigInt& a, const BigInt& b) { return Value::big(::mul(a, b)); }

std::optional<Value> div_quot(const BigInt& a, const BigInt& b) {
    if (b.is_zero()) return std::nullopt;   // → double → NaN → « division par zéro »
    BigInt q, r;
    divmod(a, b, q, r);
    return Value::big(std::move(q));
}

std::optional<Value> mod(const BigInt& a, const BigInt& b) {
    if (b.is_zero()) return std::nullopt;   // → double → NaN → « modulo par zéro »
    BigInt q, r;
    divmod(a, b, q, r);
    return Value::big(std::move(r));
}

std::optional<Value> pow_big(const BigInt& a, const BigInt& e) {
    if (e.negative()) return std::nullopt;  // exposant négatif → pow double
    auto exp = e.to_u64();
    if (!exp) return std::nullopt;          // trop grand pour être exact → pow double
    // Garde-fou : borne la taille du résultat (~100 000 chiffres) via
    // digits ≈ expo × log10(base) ; au-delà, repli double (affichera inf).
    if (!a.is_zero() && a.to_double() > 1.0) {
        double d = static_cast<double>(*exp) * std::log10(a.to_double());
        if (d > 100000.0) return std::nullopt;
    }
    BigInt result = BigInt::from_u64(1);
    BigInt base = a;
    uint64_t k = *exp;
    while (k != 0) {
        if (k & 1u) result = ::mul(result, base);
        k >>= 1;
        if (k != 0) base = ::mul(base, base);
    }
    return Value::big(std::move(result));
}

std::optional<Value> neg(const BigInt& a) {
    if (a.is_zero()) return Value::big(a);
    return Value::big(::sub(BigInt(), a));   // 0 - a
}

std::optional<Value> abs(const BigInt& a) {
    if (a.negative()) return Value::big(::sub(BigInt(), a));
    return Value::big(a);
}

std::optional<Value> square(const BigInt& a) { return Value::big(::mul(a, a)); }

std::optional<Value> same(const BigInt& a) { return Value::big(a); }

} // namespace big
