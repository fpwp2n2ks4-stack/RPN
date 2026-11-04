# RPN — The Calculator That Respects Its Elders

**100% terminal. 0% HP keyboard. 100% chance you'll type your math backwards
and find it perfectly normal within 10 minutes.**

Written in C++23 with zero external dependencies. No framework, no third-party
library, no middleman. Just code, a stack, and the ghost memory of a green LCD
glowing on a black background.

---

## Wait, what is this?

It's an RPN calculator that lives in your terminal. You type numbers first,
operators after. It's the opposite of the arithmetic school shoved down your
throat, and it's exactly like life: you give first, and the results come later.

The **HP-35** started it all in 1972 — the first pocket scientific calculator
in the world, designed by people who believed an engineer deserved better
than a slide rule and a pencil. The **HP-15C** of 1982 then elevated the
concept to art, with its 44 registers, its matrix math, and 200+ functions
in the palm of your hand. Cults formed. Marriages ended. Engineers quit their
jobs to draw stack diagrams on graph paper.

You, in 2026, compile this with `clang++` on your Mac. Humanity has peaked.

---

## Building

Got `make`? You've got everything:

```sh
make          # builds ./rpn
make test     # runs the test suite
make clean    # erases everything, like bad memories
```

No `make`? Type this while staring at your screen:

```sh
clang++ -std=c++23 -Wall -Wextra -Wpedantic -O3 -o rpn main.cpp rpn.cpp commands.cpp bigint.cpp i18n.cpp
```

No `clang++` or `g++`? You have bigger problems than this README can solve.

> **Warning:** building this software may cause sudden urges to dig a physical
> calculator out of your drawer. Side effects include: typing `3 4 +` in your
> browser, and answering your spouse in postfix notation.

---

## Usage

```sh
./rpn                         # interactive mode (it talks to you!)
echo "3 4 + 5 *" | ./rpn      # batch mode (it shuts up and calculates)
```

In interactive mode you get a prompt worthy of a 1982 terminal:

```
rpn> _
```

It's small, it's discreet, it's here to guide you through the postfix
darkness. It won't judge you. (The HP-15C didn't judge you either.)

As you type, commands **light up in their category's color the moment
they are unambiguous** — type `p` and it's already yellow (that's `pow`
talking), type `si` and it stays modest until it knows whether you mean
`sin` or `sinh`. Numbers stay plain and neutral. It's 1972 hardware
etiquette with 2026 paint. The HP-35 engineers would have killed for
a keyboard that did that.

The calculator speaks your language: it detects `LC_ALL`/`LANG` at
startup (Spanish on a Spanish system, French on a French one, English
otherwise) and translates its messages and help — errors included.
Impatient? `./rpn --lang=es` overrules the system. Curious? The `lang`
command tells you what's in charge. The HP-15C only ever spoke one
language. So did its engineers, probably louder.

---

## How does RPN work?

In RPN (Reverse Polish Notation), the order is: **operands first,
operator after**. The HP-35 popularized this in 1972, and the HP-15C
perfected it. You type a number, it drops into the stack. You type
another, it stacks on top. You type `+`, the two top values pop out,
the result goes in. No parentheses, no operator precedence, no confusion.
Just the stack.

```
rpn> 3 4 +
7

rpn> 10 3.0 /
3.333333333

rpn> 2 10 pow
1024
```

---

## Available Commands

### Arithmetic
*The bare minimum, but in postfix*

| Command | Description | Example | HP-15C |
|---------|-------------|---------|--------|
| `+` | Addition | `3 4 +` → `7` | `[+]` |
| `-` | Subtraction | `10 3 -` → `7` | `[-]` |
| `*` | Multiplication | `6 7 *` → `42` | `[×]` |
| `/` | Division | `15 3 /` → `5` | `[÷]` |
| `pow` | Power | `2 10 pow` → `1024` | `[yˣ]` |
| `log` | Logarithm base b | `8 2 log` → `3` | — |
| `mod` | Modulo | `10 3 mod` → `1` | `[R]` |
| `atan2` | Arc tangent 2 args | `1 1 atan2` → `45` | — |

### Constants
*Because typing 14 decimal places by hand is for people who enjoy suffering*

| Command | Value |
|---------|-------|
| `pi` | 3.14159265... |
| `e` | 2.71828182... |

### Unary Functions
*One operator, one argument, zero ambiguity*

| Command | Description | HP-15C |
|---------|-------------|--------|
| `neg` | Negation | `[CHS]` |
| `abs` | Absolute value | `[ABS]` |
| `sqrt` | Square root | `[√x]` |
| `square` | Square | `[x²]` |
| `inv` | Inverse (1/x) | `[1/x]` |
| `exp` | Exponential | `[eˣ]` |
| `ln` | Natural logarithm | `[LN]` |
| `log10` | Decimal logarithm | `[LOG]` |
| `floor` | Floor | — |
| `ceil` | Ceil | — |
| `round` | Round to nearest | — |
| `trunc` | Truncate | — |
| `fact` | Factorial | `[x!]` |

### Trigonometry
*For those who passed their exams in DEG mode, like the HP-15C by default*

| Command | Description |
|---------|-------------|
| `sin` | Sine |
| `cos` | Cosine |
| `tan` | Tangent |
| `asin` | Arc sine |
| `acos` | Arc cosine |
| `atan` | Arc tangent |

### Hyperbolic Functions
*The HP-15C didn't have them. We do. We're not telling.*

| Command | Description |
|---------|-------------|
| `sinh` | Hyperbolic sine |
| `cosh` | Hyperbolic cosine |
| `tanh` | Hyperbolic tangent |

### Big Numbers
*The HP-15C overflowed past 10^100. We don't overflow.*

Every integer typed without a decimal point or an exponent is an
**exact arbitrary-precision integer**. As long as both operands are
integers, `+`, `-`, `*`, `/` (exact quotient), `mod`, `pow` and `fact`
are **exact** — no rounding, ever:

```
rpn> 2 1000 pow
10715086071862673209484250490600018105614048117055336074437503883703510511249361224931983788156958581275946729175531468251871452856923140435984577574698574803934567774824230985421074605062371141877954182153046474983581941267398767559165543946077062914571196477686542167660429831652624386837205668069376
```

Want a floating-point result? Type a floating-point operand:
`10 3 /` → `3` (exact quotient), `10 3.0 /` → `3.333333333`.
Trig, hyperbolics, sqrt and unit conversions promote BigInts to
double seamlessly. `fact` tops out at 1000. `hex`/`bin` display
every last bit of a bigint — exactly.

### Stack Manipulation
*The heart of the system. The HP-15C had `x↔y`, `R↓`, `CLx`. We go further.*

| Command | Description | HP-15C |
|---------|-------------|--------|
| `dup` | Duplicate top | `[ENTER]` |
| `drop` | Drop top | `[CLx]` |
| `swap` | Swap top two | `[x↔y]` |
| `clear` | Clear entire stack | — |
| `stack` | Display numbered stack | — |
| `ls` | Alias for `stack` | — |

### dc Aliases
*In 1971, Lorinda Cherry and Robert Morris wrote `dc` — the first RPN
calculator, at Bell Labs, with exact integer arithmetic. The HP-35 hadn't
been born. We owe these keys a living.* Use them or don't — both notations
speak the same stack.

| Command | Description | dc since 1971 |
|---------|-------------|---------------|
| `d` | Duplicate top (`dup`) | `d` |
| `c` | Clear stack (`clear`) | `c` |
| `f` | Display full stack (`stack`) | `f` |
| `r` | Swap top two (`swap`) | `r` |
| `p` | Display top, WITHOUT popping | `p` |
| `n` | Display top AND pop | `n` |
| `z` | Push stack depth (exact, of course) | `z` |
| `v` | Square root (`sqrt`) | `v` |
| `^` | Power (`pow`) | `^` |
| `%` | Modulo (`mod`) | `%` |
| `~` | Quotient THEN remainder — one keystroke, both results, exact in BigInt | `~` |

### Accumulator Memory
*A register, like the famous M register on the HP-15C*

| Command | Description | HP-15C |
|---------|-------------|--------|
| `m+` | Add top to memory | `[M+]` |
| `m-` | Subtract top from memory | — |
| `m*` | Multiply memory by top | — |
| `m/` | Divide memory by top | — |
| `mr` | Recall memory value | `[RCL] [M]` |
| `mc` | Reset memory | — |
| `ms` | Show memory value | — |

### Named Variables
*The HP-15C had 20 registers (R0-R9, I0-I9). We have names. It's classier.*

| Command | Description |
|---------|-------------|
| `sto <name>` | Store top into a variable |
| `rcl <name>` | Push variable value onto stack |

### Unit Conversions
*The HP-35 didn't have this. The HP-15C didn't either. We do. We're fancy.*

| Command | Description |
|---------|-------------|
| `m2cm` | Meters → Centimeters |
| `cm2m` | Centimeters → Meters |
| `m2ft` | Meters → Feet |
| `ft2m` | Feet → Meters |
| `kg2lb` | Kilograms → Pounds |
| `lb2kg` | Pounds → Kilograms |
| `c2f` | Celsius → Fahrenheit |
| `f2c` | Fahrenheit → Celsius |
| `c2k` | Celsius → Kelvin |
| `k2c` | Kelvin → Celsius |

### Display Modes
*The HP-15C only did DEC. We're more ambitious.*

| Command | Description |
|---------|-------------|
| `dec` | Decimal mode (default) |
| `hex` | Hexadecimal mode |
| `bin` | Binary mode |
| `visual` | Show full stack after each calculation |

### Configurable Precision
*Want 3 digits? 15? 0? It's your world. The HP-15C gave you 10.
We don't limit you.*

```
rpn> .6 pi
3.14159

rpn> .15 pi
3.14159265358979

rpn> .0 12345
1e+04
```

### Angles

| Command | Description |
|---------|-------------|
| `deg` | Degree mode (default, like the HP-15C) |
| `rad` | Radian mode |

### History
*The HP-15C didn't have this. You're lucky.*

| Command | Description |
|---------|-------------|
| `h` | Show history |
| `hist` | Alias for `h` |
| `history` | Alias for `h` |

In interactive mode, **Up/Down arrows** navigate history.
It's like a tape drive, but better.

### Shell Escape
*When you need a little Unix in your RPN.
The HP-15C couldn't do this. You had a notebook for that.*

```
rpn> !ls -la
rpn> !date
rpn> !echo "I am a genius"
```

Live interactive sessions only — a `!` arriving from a pipe or a
file gets respectfully declined. Even escape hatches have bouncers.

### Control

| Command | Description |
|---------|-------------|
| `help` | Show help (if you're reading this, you don't need it) |
| `quit` | Exit the program |
| `exit` | Alias for `quit` |
| `lang` | Shows the active language (auto: `LC_ALL`/`LANG`, or `--lang=xx`) |

---

## Architecture

```
├── main.cpp      Entry point. 9 lines — two of them are i18n.
├── rpn.h         RPN class — stack, history, variables, modes.
├── rpn.cpp       Main loop, parsing, persistence.
├── commands.h    Command interface + all subclasses.
├── commands.cpp  Implementation of each operation.
├── bigint.h      Exact arbitrary-precision integers (BigInt + Value).
├── bigint.cpp    The arithmetic your HP-15C could only dream of.
├── i18n.h        Runtime messages in EN/FR/ES — the system language decides.
└── i18n.cpp      And if you prefer, --lang=en|fr|es decides instead.
```

The model is a strict **Command pattern**: every operation is a class
derived from `Command`, registered in a `registry` by name. The `RPN`
class doesn't know what it's executing — it delegates everything.
It's the same separation the HP-15C had between its keyboard and its
Saturn processor: you press a button, the processor does its job,
and you only see the result.

Everything compiles in **C++23** with `-Wall -Wextra -Wpedantic -O3`.
Zero warnings. The Makefile demands it, and it deserves it.

---

## Persistence

Data is saved automatically in `.rpn_data/`:

```
.rpn_data/
├── historique.txt    Your commands (MAX 1024 — more than the HP-15C had)
└── variables.txt     Your named variables
```

It's like the HP-15C's non-volatile memory, except it lives on a hard
disk instead of a coin cell battery. And it survives everything —
quit, Ctrl-D, even Ctrl-C mid-formula. The HP-15C saved your registers
through power cuts; we save your history through your worst keyboard
habits. Same philosophy, better insurance.

To wipe everything:

```sh
rm -rf .rpn_data
```

Your privacy. The HP-15C didn't have GDPR either.

---

## Testing

```sh
make test
```

114 tests checking every feature. One by one. Like an HP engineer
inspecting each calculator before sealing it in its orange-and-black
case. If one fails, the world is unfair. Or you broke something.
Probably both.

---

## FAQ

**Q: Why "RPN"?**
A: Reverse Polish Notation. Yes, it's Polish. No, it's not a joke.
Jan Łukasiewicz invented it in 1920. Friedrich L. Bauer implemented
it in 1968. HP made it famous in 1972. You compile it in 2026.
The circle is complete.

**Q: Is this like the HP-15C?**
A: Better, worse, and different. The HP-15C had 200+ functions, an LCD
screen, 44 registers, and fit in a shirt pocket. We have 83 commands,
a color ANSI terminal that colors as you type, named variables,
**exact arbitrary-precision integers** (the HP-15C overflowed past
10^100; we ship 100,000 digits), a set of **dc keys from 1971** still
warm from Bell Labs, and the ability to launch shell commands. The
HP-15C couldn't run `!ls -la`. You needed a
notebook for that.

**Q: Can it speak my language?**
A: It's already listening. The calculator detects your system language
(`LC_ALL`/`LANG`) and speaks English, French, or Spanish — errors, mode
messages, and the whole help included. `--lang=xx` forces it, and the
`lang` command tells you what it decided. The HP-15C spoke DISPLAY,
a dialect made of forty-four register grunts.

**Q: Does it run on Windows?**
A: Technically yes. Practically, if you're on Windows, you already have
more pressing problems. The HP-15C ran everywhere, though.

**Q: Why not Python?**
A: Because we have class. And because in Python, there's no
`std::stack<double>` with a nicely structured Command pattern.
We do things properly here.

**Q: `fact` gave me an error for -1.**
A: The factorial is only defined for non-negative integers. The HP-15C
did the same. It's not my call, it's math. (The HP-15C displayed `E`
on its LCD. We print a colorful error message. That's progress.)

**Q: How do I compute a cube root?**
A: `3 1/x pow` — or `0.3333333 pow` if you're feeling lazy. The HP-15C
had a `yˣ` button for that. We have a keyboard. We adapt.

**Q: Is this open source?**
A: You have the source code, you have the freedom. Do what you want
with it. The HP-15C, on the other hand, was closed. You couldn't even
open it without voiding the warranty.

**Q: Why the ANSI colors?**
A: Nostalgia. Each command category gets its color: yellow for arithmetic,
green for unary functions, cyan for trigonometry, magenta for hyperbolics,
blue for stack ops, red for memory. And in interactive mode the coloring
is **live**: a command lights up as soon as its prefix is unambiguous
(`p` is yellow before you even finish `pow`), an ambiguous prefix stays
plain until it makes up its mind. It's like the colored buttons on the
HP-15C, except nothing blinks and the labels anticipate you.

**Q: The `!command` thing, isn't that dangerous?**
A: You're launching shell commands from within a program. Yes, it's
dangerous — which is why it only works in a live, interactive session:
a `!` arriving from a pipe or a file is refused with an error. Your
keyboard, your responsibility; a script's keyboard is nobody's.
The HP-15C couldn't launch commands either, and nobody complained.

**Q: Why does `10 3 /` give me `3` and not `3.333333333`?**
A: Because `10` and `3` are exact integers — and exact integers divide
exactly, like `dc` has been doing since Bell Labs in 1971. Lorinda Cherry
and Robert Morris wrote the first RPN calculator with exact integer
arithmetic before the HP-35 existed. We're just carrying the torch —
in fact, we hired its keyboard: see the dc Aliases table. Want the
floating result? Say so: `10 3.0 /` → `3.333333333`.

**Q: I typed a wrong operation and got an error. Is my stack dead?**
A: No. Errors in arithmetic (`5 0 /`, `-1 sqrt`, `0 ln`…) print a
message and **put your operands back, untouched**. The stack is exactly
what it was before your mistake. The HP-15C flashed an `E` and left you
guessing; we restore the crime scene.

---

## Credits

- **Jan Łukasiewicz** — for Polish notation in 1920. Without him,
  none of this would exist.
- **Friedrich L. Bauer** — for the first RPN implementation in 1968.
  A forgotten pioneer.
- **Lorinda Cherry & Robert Morris** — for **`dc`** at Bell Labs in
  1971: the first RPN calculator, with exact integer arithmetic in
  postfix, before the HP-35 existed. Their precision lives on in ours.
- **Al-Khwārizmī** — the algorithms, the decimal place-value arithmetic,
  the name "algorithm". He never saw a stack, but every digit of our
  100,000-digit `2 332192 pow` stands on his shoulders.
- **Hewlett-Packard** — for the **HP-35** (1972) and the **HP-15C** (1982).
  The HP-35 proved a calculator could fit in a pocket. The HP-15C proved
  it could fit in a heart.
- **You** — for reading this README to the end. The HP-15C wouldn't
  have had that patience.

---

## License

MIT License — Copyright (c) 2026 fpwp2n2ks4-stack

See [LICENSE](LICENSE) for the full text.

---

*Built with love, C++23, and a dash of digital nostalgia.*
*No warranty. No support. No LCD glow. No regrets.*

*Stack responsibly. Always divide by something non-zero.
Back-to-back those `dup`s and `drop`s, champ.*

---
---

# ─── FR ───────────────────────────────────────────────────────

# RPN — La Calculatrice Polonaise Inverse qui Respecte ses Aînées

**100% terminal. 0% clavier HP. 100% de chances que vous tapiez vos calculs
à l'envers et que ça vous semble normal en 10 minutes.**

Écrite en C++23 sans aucune dépendance externe. Pas de framework, pas de
bibliothèque tierce, pas de tiers. Juste du code, une pile, et le souvenir
fantôme d'un voyant LCD vert sur fond noir.

---

## Mais au fait, c'est quoi ?

C'est une calculatrice RPN qui vit dans votre terminal. Vous entrez les
nombres d'abord, l'opérateur après. C'est le contraire de l'arithmétique
que l'école vous a imposé, et c'est exactement comme la vie : on donne
d'abord, et les résultats viennent après.

La **HP-35** de 1972 a commencé tout ça — la première calculatrice
scientifique de poche au monde, conçue par des gens qui pensaient qu'un
ingénieur méritait mieux qu'une règle à calcul et un crayon. La **HP-15C** de 1982
a ensuite élevé le concept au rang d'art, avec sa matrice de 44 registres
et ses 200+ fonctions dans le creux de la main. Des cultes se sont formés.
Des marriages ont éclaté. Des ingénieurs ont quitté leur travail pour
dessiner des pions de pile sur du papier quadrillé.

Vous, en 2026, vous compilez ça avec `clang++` sur votre Mac. L'humanité
a atteint son apogée.

---

## Compilation

Vous avez un `make` ? Vous avez tout :

```sh
make          # construit ./rpn
make test     # lance la batterie de tests
make clean    # efface tout, comme les mauvais souvenirs
```

Vous n'avez pas de `make` ? Tapez ceci tout en fixant votre écran :

```sh
clang++ -std=c++23 -Wall -Wextra -Wpedantic -O3 -o rpn main.cpp rpn.cpp commands.cpp bigint.cpp i18n.cpp
```

Vous n'avez pas `clang++` ni `g++` ? Vous avez de plus gros problèmes
que ce README ne peut résoudre.

> **Avertissement :** compiler ce programme peut provoquer des envies
> soudaines de sortir une calculette physique de votre tiroir. Les effets
> secondaires incluent : taper `3 4 +` dans votre navigateur, et
> répondre à votre conjoint avec des expressions postfixées.

---

## Utilisation

```sh
./rpn                         # mode interactif (il vous parle !)
echo "3 4 + 5 *" | ./rpn      # mode batch (il se tait et calcule)
```

En mode interactif, vous obtenez un prompt digne d'un terminal de 1982 :

```
rpn> _
```

Il est petit, il est discret, il est là pour vous guider dans les ténèbres
du postfixé. Il ne vous jugera pas. (Le HP-15C ne vous jugeait pas non plus.)

Et pendant la frappe, les commandes **s'illuminent de la couleur de leur
catégorie dès qu'elles ne sont plus ambiguës** — tapez `p`, il est déjà
jaune (c'est `pow` qui parle) ; tapez `si`, il reste modeste jusqu'à
savoir si vous visez `sin` ou `sinh`. Les nombres restent en clair,
neutres. C'est l'étiquette matérielle de 1972 avec la peinture de 2026.
Les ingénieurs du HP-35 auraient tué pour un clavier qui fait ça.

La calculatrice parle votre langue : elle détecte `LC_ALL`/`LANG` au
démarrage (espagnol sur un système espagnol, français sur un système
français, anglais sinon) et traduit ses messages et son aide — erreurs
comprises. Impatient ? `./rpn --lang=fr` passe outre le système.
Curieux ? La commande `lang` vous dit qui commande. La HP-15C n'a
toujours parlé qu'une seule langue. Ses ingénieurs aussi, sans doute
plus fort.

---

## Comment ça marche, la RPN ?

En RPN (Reverse Polish Notation), l'ordre est : **operandes d'abord,
opérateur après**. La HP-35 a popularisé ça en 1972, et la HP-15C l'a
porté à la perfection. Vous tapez un nombre, il tombe dans la pile.
Vous tapez un autre nombre, il empile par dessus. Vous tapez `+`,
les deux sommets sortent, le résultat rentre. Pas de parenthèses,
pas de priorité opératoire, pas de confusion. Juste la pile.

```
rpn> 3 4 +
7

rpn> 10 3.0 /
3.333333333

rpn> 2 10 pow
1024
```

Vous trouvez ça bizarre ? Attendez de voir les hyperboliques.

---

## Commandes disponibles

### Arithmétique
*Le minimum syndical, mais en postfixé*

| Commande | Description | Exemple | HP-15C |
|----------|-------------|---------|--------|
| `+` | Addition | `3 4 +` → `7` | `[+]` |
| `-` | Soustraction | `10 3 -` → `7` | `[-]` |
| `*` | Multiplication | `6 7 *` → `42` | `[×]` |
| `/` | Division | `15 3 /` → `5` | `[÷]` |
| `pow` | Puissance | `2 10 pow` → `1024` | `[yˣ]` |
| `log` | Logarithme base b | `8 2 log` → `3` | — |
| `mod` | Modulo | `10 3 mod` → `1` | `[R]` |
| `atan2` | Arc tangente 2 args | `1 1 atan2` → `45` | — |

### Constantes
*Parce que taper 14 décimales à la main, c'est pour les amateurs de labeur*

| Commande | Valeur |
|----------|--------|
| `pi` | 3.14159265... |
| `e` | 2.71828182... |

### Fonctions unaires
*Un opérateur, un argument, zéro ambiguïté*

| Commande | Description | HP-15C |
|----------|-------------|--------|
| `neg` | Négation | `[CHS]` |
| `abs` | Valeur absolue | `[ABS]` |
| `sqrt` | Racine carrée | `[√x]` |
| `square` | Carré | `[x²]` |
| `inv` | Inverse (1/x) | `[1/x]` |
| `exp` | Exponentielle | `[eˣ]` |
| `ln` | Logarithme naturel | `[LN]` |
| `log10` | Logarithme décimal | `[LOG]` |
| `floor` | Arrondi inférieur | — |
| `ceil` | Arrondi supérieur | — |
| `round` | Arrondi au plus proche | — |
| `trunc` | Troncature | — |
| `fact` | Factorielle | `[x!]` |

### Trigonométrie
*Pour ceux qui ont passé leur bac en mode DEG, comme la HP-15C par défaut*

| Commande | Description |
|----------|-------------|
| `sin` | Sinus |
| `cos` | Cosinus |
| `tan` | Tangente |
| `asin` | Arc sinus |
| `acos` | Arc cosinus |
| `atan` | Arc tangente |

### Fonctions hyperboliques
*La HP-15C n'en avait pas. Nous, si. On avoue pas.*

| Commande | Description |
|----------|-------------|
| `sinh` | Sinus hyperbolique |
| `cosh` | Cosinus hyperbolique |
| `tanh` | Tangente hyperbolique |

### Grands nombres
*La HP-15C débordait au-delà de 10^100. Nous, on ne déborde pas.*

Tout entier tapé sans point décimal ni exposant est un **entier exact
de taille arbitraire**. Tant que les deux opérandes sont des entiers,
`+`, `-`, `*`, `/` (quotient exact), `mod`, `pow` et `fact` sont
**exacts** — aucun arrondi, jamais :

```
rpn> 2 1000 pow
10715086071862673209484250490600018105614048117055336074437503883703510511249361224931983788156958581275946729175531468251871452856923140435984577574698574803934567774824230985421074605062371141877954182153046474983581941267398767559165543946077062914571196477686542167660429831652624386837205668069376
```

Vous voulez un résultat flottant ? Tapez un opérande flottant :
`10 3 /` → `3` (quotient exact), `10 3.0 /` → `3.333333333`.
Trigonométrie, hyperboliques, sqrt et conversions convertissent les
entiers en double de façon transparente. `fact` plafonne à 1000.
`hex`/`bin` affichent chaque bit d'un grand entier — exactement.

### Manipulation de la pile
*Le cœur du système. La HP-15C avait `x↔y`, `R↓`, `CLx`. Nous, on va plus loin.*

| Commande | Description | HP-15C |
|----------|-------------|--------|
| `dup` | Duplique le sommet | `[ENTER]` |
| `drop` | Jette le sommet | `[CLx]` |
| `swap` | Échange les deux premiers | `[x↔y]` |
| `clear` | Vide toute la pile | — |
| `stack` | Affiche la pile numérotée | — |
| `ls` | Alias de `stack` | — |

### Alias dc
*En 1971, Lorinda Cherry et Robert Morris écrivaient `dc` — la première
calculatrice RPN, aux Bell Labs, avec arithmétique entière exacte. Le HP-35
n'était pas né. On doit à ces touches d'être vivantes.* Servez-vous en ou
pas — les deux notations parlent la même pile.

| Commande | Description | dc depuis 1971 |
|---------|-------------|-----------------|
| `d` | Duplique le sommet (`dup`) | `d` |
| `c` | Vide la pile (`clear`) | `c` |
| `f` | Affiche la pile entière (`stack`) | `f` |
| `r` | Échange les deux premiers (`swap`) | `r` |
| `p` | Affiche le sommet SANS le dépiler | `p` |
| `n` | Affiche le sommet ET le dépile | `n` |
| `z` | Empile la profondeur de la pile (exacte, évidemment) | `z` |
| `v` | Racine carrée (`sqrt`) | `v` |
| `^` | Puissance (`pow`) | `^` |
| `%` | Modulo (`mod`) | `%` |
| `~` | Quotient PUIS reste — une frappe, deux résultats, exacts en BigInt | `~` |

### Mémoire accumulateur
*Un registre, comme la fameuse registre M de la HP-15C*

| Commande | Description | HP-15C |
|----------|-------------|--------|
| `m+` | Ajoute le sommet à la mémoire | `[M+]` |
| `m-` | Soustrait le sommet de la mémoire | — |
| `m*` | Multiplie la mémoire par le sommet | — |
| `m/` | Divise la mémoire par le sommet | — |
| `mr` | Rappelle la valeur mémoire | `[RCL] [M]` |
| `mc` | Réinitialise la mémoire | — |
| `ms` | Affiche la valeur mémoire | — |

### Variables nommées
*La HP-15C avait 20 registres (R0-R9, I0-I9). Nous, on a des noms.
C'est plus classe.*

| Commande | Description |
|----------|-------------|
| `sto <nom>` | Stocke le sommet dans une variable |
| `rcl <nom>` | Empile la valeur d'une variable |

### Conversions d'unités
*La HP-35 n'avait pas ça. La HP-15C non plus. Nous, on a de la tenue.*

| Commande | Description |
|----------|-------------|
| `m2cm` | Mètres → Centimètres |
| `cm2m` | Centimètres → Mètres |
| `m2ft` | Mètres → Pieds |
| `ft2m` | Pieds → Mètres |
| `kg2lb` | Kilogrammes → Livres |
| `lb2kg` | Livres → Kilogrammes |
| `c2f` | Celsius → Fahrenheit |
| `f2c` | Fahrenheit → Celsius |
| `c2k` | Celsius → Kelvin |
| `k2c` | Kelvin → Celsius |

### Modes d'affichage
*La HP-15C affichait en DEC. Nous, on est plus ambitieux.*

| Commande | Description |
|----------|-------------|
| `dec` | Mode décimal (défaut) |
| `hex` | Mode hexadécimal |
| `bin` | Mode binaire |
| `visual` | Affiche la pile complète après chaque calcul |

### Précision configurable
*Vous voulez 3 chiffres ? 15 ? 0 ? C'est votre monde. La HP-15C
donnait 10 chiffres. On ne vous limite pas.*

```
rpn> .6 pi
3.14159

rpn> .15 pi
3.14159265358979

rpn> .0 12345
1e+04
```

### Angles

| Commande | Description |
|----------|-------------|
| `deg` | Mode degrés (défaut, comme la HP-15C) |
| `rad` | Mode radians |

### Historique
*La HP-15C n'en avait pas. Vous, vous avez de la chance.*

| Commande | Description |
|----------|-------------|
| `h` | Affiche l'historique |
| `hist` | Alias de `h` |
| `history` | Alias de `h` |

En mode interactif, les **flèches Haut/Bas** naviguent dans l'historique.
C'est comme un défileur magnétique, mais en mieux.

### Échappement shell
*Quand vous avez besoin d'un peu de Unix dans votre RPN.
La HP-15C ne pouvait pas faire ça. Vous aviez un cahier pour ça.*

```
rpn> !ls -la
rpn> !date
rpn> !echo "Je suis un génie"
```

Réservé aux sessions interactives en direct — un `!` arrivant d'un
pipe ou d'un fichier est poliment refusé. Même les portes de sortie
ont leurs videurs.

### Contrôle

| Commande | Description |
|----------|-------------|
| `help` | Affiche l'aide (si vous lisez ce fichier, vous n'en avez pas besoin) |
| `quit` | Quitte le programme |
| `exit` | Alias de `quit` |
| `lang` | Affiche la langue active (auto : `LC_ALL`/`LANG`, ou `--lang=xx`) |

---

## Architecture

```
├── main.cpp      Point d'entrée. 9 lignes — dont deux d'i18n.
├── rpn.h         Classe RPN — pile, historique, variables, modes.
├── rpn.cpp       Boucle principale, parsing, persistance.
├── commands.h    Interface Command + toutes les sous-classes.
├── commands.cpp  L'implémentation de chaque opération.
├── bigint.h      Entiers exacts de taille arbitraire (BigInt + Value).
├── bigint.cpp    L'arithmétique dont la HP-15C ne pouvait que rêver.
├── i18n.h        Messages d'exécution EN/FR/ES — la langue du système décide.
└── i18n.cpp      Et si vous préférez, --lang=en|fr|es décide à sa place.
```

Le modèle est strict **pattern Command** : chaque opération est une
classe dérivée de `Command`, enregistrée dans un `registry` par nom.
La classe `RPN` ne sait pas ce qu'elle exécute — elle délègue tout.
C'est la même séparation que la HP-15C entre son clavier et son
processeur Saturn : vous appuyez sur un bouton, le processeur fait
son travail, et vous ne voyez que le résultat.

Le tout est compilé en **C++23** avec `-Wall -Wextra -Wpedantic -O3`.
Zéro avertissement. Le Makefile le veut ainsi, et il le mérite.

---

## Persistance

Les données sont sauvegardées automatiquement dans `.rpn_data/` :

```
.rpn_data/
├── historique.txt    Vos commandes (MAX 1024 — c'est plus que la HP-15C)
└── variables.txt     Vos variables nommées
```

C'est comme la mémoire non-volatile de la HP-15C, sauf que ça tient
sur un disque dur et pas sur une pile bouton. Et ça survit à tout —
quit, Ctrl-D, même Ctrl-C en pleine formule. La HP-15C sauvegardait
vos registres à travers les coupures de courant ; nous, on sauvegarde
votre historique à travers vos pires habitudes de clavier. Même
philosophie, meilleure assurance.

Pour tout effacer :

```sh
rm -rf .rpn_data
```

C'est votre vie privée. La HP-15C n'avait pas de RGPD.

---

## Tests

```sh
make test
```

114 tests qui vérifient chaque fonctionnalité. Un par un. Comme un
ingénieur HP vérifiait chaque calculatrice avant de la sceller dans
son boîtier orange et noir. Si un échoue, c'est que le monde est
injuste. Ou que vous avez cassé quelque chose. Probablement les deux.

---

## FAQ

**Q : Pourquoi « RPN » ?**
R : Reverse Polish Notation. La notation polonaise inversée. Oui, c'est
polonais. Non, ce n'est pas une blague. Jan Łukasiewicz l'a inventée
en 1920. Friedrich L. Bauer l'a implémentée en 1968. HP l'a rendue
célèbre en 1972. Vous, vous la compilez en 2026. La boucle est bouclée.

**Q : C'est comme la HP-15C ?**
R : En mieux, en pire, et différemment. La HP-15C avait 200+ fonctions,
un écran LCD, 44 registres, et rentrait dans une chemise. Nous, on a
83 commandes, un terminal ANSI qui se colore à la frappe, des variables
nommées, des **entiers exacts de taille arbitraire** (la HP-15C
débordait au-delà de 10^100 ; nous, on expédie 100 000 chiffres), des
**touches dc de 1971** encore chaudes des Bell Labs, et une habileté à
lancer des commandes shell. La HP-15C ne pouvait pas faire `!ls -la`.
Vous aviez un carnet pour ça.

**Q : Est-ce qu'elle parle ma langue ?**
R : Elle écoute déjà. La calculatrice détecte la langue du système
(`LC_ALL`/`LANG`) et parle anglais, français ou espagnol — erreurs,
messages de mode et aide complète compris. `--lang=xx` la force, et la
commande `lang` vous dit ce qu'elle a décidé. La HP-15C parlait DISPLAY,
un dialecte fait de grognements de 44 registres.

**Q : Est-ce que ça tourne sur Windows ?**
R : Techniquement oui. Pratiquement, si vous êtes sur Windows, vous avez
déjà des problèmes plus urgents. La HP-15C tournait partout, elle.

**Q : Pourquoi pas Python ?**
R : Parce qu'on a de la classe. Et parce qu'en Python, il n'y a pas
de `std::stack<double>` avec un pattern Command bien structuré.
On fait les choses sérieusement ici.

**Q : Le fact m'a renvoyé une erreur pour -1.**
R : La factorielle n'est définie que pour les entiers positifs.
La HP-15C faisait pareil. C'est pas moi qui décide, c'est les maths.
(La HP-15C affichait `E` sur son écran LCD. Nous, on affiche un
message d'erreur en couleur. C'est le progrès.)

**Q : Comment je fais une racine cubique ?**
R : `3 1/x pow` — ou `0.3333333 pow` si vous êtes paresseux.
La HP-15C avait un bouton `yˣ` pour ça. Nous, on a un clavier.
On s'adapte.

**Q : C'est open source ?**
R : Vous avez le code source, vous avez la liberté.
Faites-en ce que vous voulez. La HP-15C, elle, était fermée.
Vous ne pouviez même pas la ouvrir sans perdre la garantie.

**Q : Pourquoi les couleurs ANSI ?**
R : Pour la nostalgie. Chaque catégorie de commande a sa couleur :
jaune pour l'arithmétique, vert pour les unaires, cyan pour la
trigonométrie, magenta pour les hyperboliques, bleu pour la pile,
rouge pour la mémoire. Et en mode interactif, la coloration est
**vivante** : une commande s'allume dès que son préfixe n'est plus
ambigu (`p` devient jaune avant même que vous finissiez `pow`), un
préfixe ambigu reste en clair jusqu'à se décider. C'est comme les
boutons colorés de la HP-15C, sauf que ça clignote pas et que les
touches vous anticipent.

**Q : Le `!commande`, c'est dangereux ?**
R : Vous lancez du shell dans un programme. Oui, c'est dangereux —
c'est pour ça que ça ne marche qu'en session interactive en direct :
un `!` arrivant d'un pipe ou d'un fichier est refusé avec une erreur.
Votre clavier, votre responsabilité ; le clavier d'un script
n'appartient à personne. La HP-15C ne pouvait pas lancer de commandes
non plus, et personne ne s'en plaignait.

**Q : Pourquoi `10 3 /` me donne `3` et pas `3.333333333` ?**
R : Parce que `10` et `3` sont des entiers exacts — et les entiers
exacts se divisent exactement, comme `dc` le fait depuis les Bell Labs
en 1971. Lorinda Cherry et Robert Morris ont écrit la première
calculatrice RPN à arithmétique entière exacte avant même que le HP-35
existe. Nous ne faisons que porter la torche — nous avons même embauché
son clavier : cf. le tableau des Alias dc. Vous voulez le résultat
flottant ? Dites-le : `10 3.0 /` → `3.333333333`.

**Q : J'ai tapé une opération invalide et j'ai une erreur. Ma pile est morte ?**
R : Non. Les erreurs d'arithmétique (`5 0 /`, `-1 sqrt`, `0 ln`…)
affichent un message et **remettent vos opérandes en place, intactes**.
La pile est exactement celle d'avant votre erreur. La HP-15C affichait
`E` et vous laissait deviner ; nous, on remet la scène de crime en
état.

---

## Crédits

- **Jan Łukasiewicz** — pour la notation polonaise en 1920. Sans lui,
  on ne serait pas là.
- **Friedrich L. Bauer** — pour la première implémentation RPN en 1968.
  Un pionnier oublié.
- **Lorinda Cherry et Robert Morris** — pour **`dc`** aux Bell Labs en
  1971 : la première calculatrice RPN, à arithmétique entière exacte en
  postfixé, avant même l'existence du HP-35. Leur précision vit dans
  la nôtre.
- **Al-Khwārizmī** — les algorithmes, l'arithmétique décimale par
  positions, le mot « algorithme ». Il n'a jamais vu une pile, mais
  chacun des chiffres de notre `2 332192 pow` à 100 000 chiffres
  repose sur ses épaules.
- **Hewlett-Packard** — pour la **HP-35** (1972) et la **HP-15C** (1982).
  La HP-35 a prouvé qu'une calculatrice pouvait tenir dans une poche.
  La HP-15C a prouvé qu'elle pouvait tenir dans un cœur.
- **Vous** — pour avoir lu ce README jusqu'au bout. La HP-15C
  n'aurait pas eu cette patience.

---

## Licence

MIT License — Copyright (c) 2026 fpwp2n2ks4-stack

Voir le fichier [LICENSE](LICENSE) pour le texte complet.

---

*Fabriqué avec du C++23, et un soupçon de nostalgie numérique.*
*Pas de warranty. Pas de support. Pas de voyant LCD. Pas de regrets.*

*Empilez responsablement. Divisez toujours par quelque chose de non nul.
Back-to-back vos `dup` et vos `drop`, champion.*

---
---

# ─── ES ───────────────────────────────────────────────────────

# RPN — La Calculadora Polaca Inversa que Respeta a sus Mayores

**100% terminal. 0% teclado HP. 100% de probabilidades de que escribas tus
cálculos al revés y te parezca lo más normal del mundo en 10 minutos.**

Escrita en C++23 sin ninguna dependencia externa. Sin framework, sin
biblioteca de terceros, sin intermediarios. Solo código, una pila, y el
recuerdo fantasma de un LCD verde brillando sobre fondo negro.

---

## ¿Pero qué es esto?

Es una calculadora RPN que vive en tu terminal. Escribes primero los
números, los operadores después. Es lo contrario de la aritmética que te
metió la escuela a la fuerza, y es exactamente como la vida: uno da
primero, y los resultados llegan después.

La **HP-35** empezó todo en 1972 — la primera calculadora científica de
bolsillo del mundo, diseñada por gente que creía que un ingeniero merecía
algo mejor que una regla de cálculo y un lápiz. La **HP-15C** de 1982
elevó el concepto a la categoría de arte, con sus 44 registros, su
álgebra matricial y más de 200 funciones en la palma de la mano. Se
formaron sectas. Hubo divorcios. Ingenieros renunciaron a su trabajo
para dibujar diagramas de pila en papel cuadriculado.

Y tú, en 2026, compilas esto con `clang++` en tu Mac. La humanidad ha
tocado techo.

---

## Compilación

¿Tienes `make`? Lo tienes todo:

```sh
make          # construye ./rpn
make test     # lanza la batería de pruebas
make clean    # borra todo, como los malos recuerdos
```

¿No tienes `make`? Escribe esto mientras miras fijamente la pantalla:

```sh
clang++ -std=c++23 -Wall -Wextra -Wpedantic -O3 -o rpn main.cpp rpn.cpp commands.cpp bigint.cpp i18n.cpp
```

¿Ni `clang++` ni `g++`? Tienes problemas más grandes de los que este
README puede resolver.

> **Aviso:** compilar este software puede provocar ganas repentinas de
> desenterrar una calculadora física del cajón. Efectos secundarios:
> escribir `3 4 +` en tu navegador y contestar a tu pareja en notación
> postfija.

---

## Uso

```sh
./rpn                         # modo interactivo (¡te habla!)
echo "3 4 + 5 *" | ./rpn      # modo por lotes (se calla y calcula)
```

En modo interactivo recibes un prompt digno de un terminal de 1982:

```
rpn> _
```

Es pequeño, discreto, y está ahí para guiarte por la oscuridad postfija.

La calculadora habla tu idioma: detecta `LC_ALL`/`LANG` al arrancar
(español en un sistema español, francés en uno francés, inglés en
cualquier otro caso) y traduce sus mensajes y su ayuda — errores
incluidos. ¿Impaciente? `./rpn --lang=es` se impone al sistema.
¿Curioso? El comando `lang` te dice quién manda. La HP-15C solo hablaba
un idioma. Sus ingenieros también, probablemente más alto.
No te va a juzgar. (La HP-15C tampoco juzgaba.)

Y mientras escribes, los comandos **se iluminan con el color de su
categoría en cuanto dejan de ser ambiguos** — escribes `p` y ya está en
amarillo (`pow` está hablando); escribes `si` y se queda modesto hasta
decir si vas a por `sin` o `sinh`. Los números permanecen en claro,
neutros. Es la etiqueta del hardware de 1972 con la pintura de 2026.
Los ingenieros de la HP-35 habrían matado por un teclado así.

---

## ¿Cómo funciona la RPN?

En notación polaca inversa (RPN), el orden es: **primero los operandos,
después el operador**. La HP-35 la popularizó en 1972 y la HP-15C la
llevó a la perfección. Escribes un número, cae en la pila. Escribes
otro, se apila encima. Escribes `+`, los dos valores de arriba salen,
y el resultado entra. Sin paréntesis, sin precedencia, sin confusión.
Solo la pila.

```
rpn> 3 4 +
7

rpn> 10 3.0 /
3.333333333

rpn> 2 10 pow
1024
```

¿Te parece raro? Espera a ver las hiperbólicas.

---

## Comandos disponibles

### Aritmética
*El mínimo sindical, pero en postfijo*

| Comando | Descripción | Ejemplo | HP-15C |
|---------|-------------|---------|--------|
| `+` | Suma | `3 4 +` → `7` | `[+]` |
| `-` | Resta | `10 3 -` → `7` | `[-]` |
| `*` | Multiplicación | `6 7 *` → `42` | `[×]` |
| `/` | División | `15 3 /` → `5` | `[÷]` |
| `pow` | Potencia | `2 10 pow` → `1024` | `[yˣ]` |
| `log` | Logaritmo base b | `8 2 log` → `3` | — |
| `mod` | Módulo | `10 3 mod` → `1` | `[R]` |
| `atan2` | Arco tangente de 2 args | `1 1 atan2` → `45` | — |

### Constantes
*Porque escribir 14 decimales a mano es cosa de amantes del trabajo duro*

| Comando | Valor |
|---------|-------|
| `pi` | 3.14159265... |
| `e` | 2.71828182... |

### Funciones unarias
*Un operador, un argumento, cero ambigüedad*

| Comando | Descripción | HP-15C |
|---------|-------------|--------|
| `neg` | Negación | `[CHS]` |
| `abs` | Valor absoluto | `[ABS]` |
| `sqrt` | Raíz cuadrada | `[√x]` |
| `square` | Cuadrado | `[x²]` |
| `inv` | Inverso (1/x) | `[1/x]` |
| `exp` | Exponencial | `[eˣ]` |
| `ln` | Logaritmo natural | `[LN]` |
| `log10` | Logaritmo decimal | `[LOG]` |
| `floor` | Redondeo inferior | — |
| `ceil` | Redondeo superior | — |
| `round` | Redondeo al más próximo | — |
| `trunc` | Truncamiento | — |
| `fact` | Factorial | `[x!]` |

### Trigonometría
*Para los que aprobaron el examen en modo DEG, como la HP-15C por defecto*

| Comando | Descripción |
|---------|-------------|
| `sin` | Seno |
| `cos` | Coseno |
| `tan` | Tangente |
| `asin` | Arcoseno |
| `acos` | Arcocoseno |
| `atan` | Arcotangente |

### Funciones hiperbólicas
*La HP-15C no las tenía. Nosotros sí. No lo contamos.*

| Comando | Descripción |
|---------|-------------|
| `sinh` | Seno hiperbólico |
| `cosh` | Coseno hiperbólico |
| `tanh` | Tangente hiperbólica |

### Grandes números
*La HP-15C desbordaba más allá de 10^100. Nosotros no desbordamos.*

Todo entero escrito sin punto decimal ni exponente es un **entero exacto
de tamaño arbitrario**. Mientras ambos operandos sean enteros,
`+`, `-`, `*`, `/` (cociente exacto), `mod`, `pow` y `fact` son
**exactos** — sin redondeos, nunca:

```
rpn> 2 1000 pow
10715086071862673209484250490600018105614048117055336074437503883703510511249361224931983788156958581275946729175531468251871452856923140435984577574698574803934567774824230985421074605062371141877954182153046474983581941267398767559165543946077062914571196477686542167660429831652624386837205668069376
```

¿Quieres un resultado de coma flotante? Escribe un operando flotante:
`10 3 /` → `3` (cociente exacto), `10 3.0 /` → `3.333333333`.
Trigonometría, hiperbólicas, sqrt y conversiones convierten los enteros
a double sin fricción. `fact` se limita a 1000. `hex`/`bin` muestran
cada bit de un entero grande — exactamente.

### Alias de dc
*En 1971, Lorinda Cherry y Robert Morris escribieron `dc` — la primera
calculadora RPN, en los Bell Labs, con aritmética entera exacta. La HP-35
ni siquiera había nacido. Estas teclas nos deben estar vivas.* Úsalas o
no — ambas notaciones hablan la misma pila.

| Comando | Descripción | dc desde 1971 |
|---------|-------------|---------------|
| `d` | Duplica la cima (`dup`) | `d` |
| `c` | Vacía la pila (`clear`) | `c` |
| `f` | Muestra la pila entera (`stack`) | `f` |
| `r` | Intercambia los dos primeros (`swap`) | `r` |
| `p` | Muestra la cima SIN desapilar | `p` |
| `n` | Muestra la cima Y la desapila | `n` |
| `z` | Apila la profundidad de la pila (exacta, claro) | `z` |
| `v` | Raíz cuadrada (`sqrt`) | `v` |
| `^` | Potencia (`pow`) | `^` |
| `%` | Módulo (`mod`) | `%` |
| `~` | Cociente Y LUEGO resto — una pulsación, dos resultados, exactos en BigInt | `~` |

### Manipulación de la pila
*El corazón del sistema. La HP-15C tenía `x↔y`, `R↓`, `CLx`. Nosotros vamos más lejos.*

| Comando | Descripción | HP-15C |
|---------|-------------|--------|
| `dup` | Duplica la cima | `[ENTER]` |
| `drop` | Elimina la cima | `[CLx]` |
| `swap` | Intercambia los dos primeros | `[x↔y]` |
| `clear` | Vacía toda la pila | — |
| `stack` | Muestra la pila numerada | — |
| `ls` | Alias de `stack` | — |

### Memoria acumulador
*Un registro, como el famoso registro M de las calculadoras clásicas*

| Comando | Descripción | HP-15C |
|---------|-------------|--------|
| `m+` | Suma la cima a la memoria | `[M+]` |
| `m-` | Resta la cima de la memoria | — |
| `m*` | Multiplica la memoria por la cima | — |
| `m/` | Divide la memoria por la cima | — |
| `mr` | Recupera el valor de la memoria | `[RCL] [M]` |
| `mc` | Reinicia la memoria | — |
| `ms` | Muestra el valor de la memoria | — |

### Variables con nombre
*La HP-15C tenía 20 registros (R0-R9, I0-I9). Nosotros tenemos nombres.
Más elegante.*

| Comando | Descripción |
|---------|-------------|
| `sto <nombre>` | Guarda la cima en una variable |
| `rcl <nombre>` | Apila el valor de una variable |

### Conversiones de unidades
*La HP-35 no tenía esto. La HP-15C tampoco. Nosotros tenemos clase.*

| Comando | Descripción |
|---------|-------------|
| `m2cm` | Metros → Centímetros |
| `cm2m` | Centímetros → Metros |
| `m2ft` | Metros → Pies |
| `ft2m` | Pies → Metros |
| `kg2lb` | Kilogramos → Libras |
| `lb2kg` | Libras → Kilogramos |
| `c2f` | Celsius → Fahrenheit |
| `f2c` | Fahrenheit → Celsius |
| `c2k` | Celsius → Kelvin |
| `k2c` | Kelvin → Celsius |

### Modos de visualización
*La HP-15C solo hacía DEC. Nosotros somos más ambiciosos.*

| Comando | Descripción |
|---------|-------------|
| `dec` | Modo decimal (por defecto) |
| `hex` | Modo hexadecimal |
| `bin` | Modo binario |
| `visual` | Muestra la pila completa tras cada cálculo |

### Precisión configurable
*¿Quieres 3 dígitos? ¿15? ¿0? Es tu mundo. La HP-15C te daba 10.
Nosotros no te ponemos límites.*

```
rpn> .6 pi
3.14159

rpn> .15 pi
3.14159265358979

rpn> .0 12345
1e+04
```

### Ángulos

| Comando | Descripción |
|---------|-------------|
| `deg` | Modo grados (por defecto, como la HP-15C) |
| `rad` | Modo radianes |

### Historial
*La HP-15C no lo tenía. Tú tienes suerte.*

| Comando | Descripción |
|---------|-------------|
| `h` | Muestra el historial |
| `hist` | Alias de `h` |
| `history` | Alias de `h` |

En modo interactivo, las **flechas de arriba/abajo** navegan por el
historial. Como una cinta magnética, pero mejor.

### Escape al shell
*Cuando necesitas un poco de Unix en tu RPN.
La HP-15C no podía hacer esto. Para eso tenías una libreta.*

```
rpn> !ls -la
rpn> !date
rpn> !echo "Soy un genio"
```

Reservado a sesiones interactivas en directo — un `!` que llegue de una
tubería o de un archivo es rechazado con cortesía. Hasta las salidas de
emergencia tienen porteros.

### Control

| Comando | Descripción |
|---------|-------------|
| `help` | Muestra la ayuda (si lees este archivo, no la necesitas) |
| `quit` | Sale del programa |
| `exit` | Alias de `quit` |
| `lang` | Muestra el idioma activo (auto: `LC_ALL`/`LANG`, o `--lang=xx`) |

---

## Arquitectura

```
├── main.cpp      Punto de entrada. 9 líneas — dos de ellas son i18n.
├── rpn.h         Clase RPN — pila, historial, variables, modos.
├── rpn.cpp       Bucle principal, análisis sintáctico, persistencia.
├── commands.h    Interfaz Command + todas las subclases.
├── commands.cpp  Implementación de cada operación.
├── bigint.h      Enteros exactos de tamaño arbitrario (BigInt + Value).
├── bigint.cpp    La aritmética con la que la HP-15C solo podía soñar.
├── i18n.h        Mensajes en EN/FR/ES — manda el idioma del sistema.
└── i18n.cpp      Y si prefieres, --lang=en|fr|es manda en su lugar.
```

El modelo es un **patrón Command** estricto: cada operación es una clase
derivada de `Command`, registrada en un registro por nombre y categoría.
La clase `RPN` no sabe qué está ejecutando — delega todo. Es la misma
separación que había entre el teclado de la HP-15C y su procesador:
aprietas un botón, el procesador hace su trabajo, y tú solo ves el
resultado.

Todo compila en **C++23** con `-Wall -Wextra -Wpedantic -O3`.
Cero avisos. El Makefile lo exige, y se lo ha ganado.

---

## Persistencia

Los datos se guardan automáticamente en `.rpn_data/`:

```
.rpn_data/
├── historique.txt    Tus comandos (MÁX 1024 — más de los que tuvo la HP-15C)
└── variables.txt     Tus variables con nombre
```

Es como la memoria no volátil de la HP-15C, salvo que vive en un disco
duro y no en una pila de botón. Y sobrevive a todo — quit, Ctrl-D,
incluso Ctrl-C en plena fórmula. La HP-15C salvaba tus registros a
través de los cortes de luz; nosotros salvamos tu historial a través
de tus peores vicios de teclado. Misma filosofía, mejor seguro.

Para borrarlo todo:

```sh
rm -rf .rpn_data
```

Es tu privacidad. La HP-15C tampoco tenía RGPD.

---

## Pruebas

```sh
make test
```

114 pruebas que verifican cada funcionalidad. Una por una. Como un
ingeniero de HP revisando cada calculadora antes de sellarla en su
caja naranja y negra. Si una falla, el mundo es injusto. O tú rompiste
algo. Probablemente ambas cosas.

---

## FAQ

**P: ¿Por qué «RPN»?**
R: Notación Polaca Inversa (Reverse Polish Notation). Sí, es polaca.
No, no es un chiste. Jan Łukasiewicz la inventó en 1920. Friedrich L.
Bauer la implementó en 1968. HP la hizo famosa en 1972. Y tú la
compilas en 2026. El círculo está completo.

**P: ¿Es como la HP-15C?**
R: Mejor, peor, y diferente. La HP-15C tenía más de 200 funciones,
una pantalla LCD, 44 registros y cabía en un bolsillo de camisa.
Nosotros tenemos 83 comandos, un terminal ANSI que se colorea mientras
escribes, variables con nombre, **enteros exactos de tamaño
arbitrario** (la HP-15C desbordaba más allá de 10^100; nosotros
enviamos 100.000 dígitos), **teclas de dc de 1971** aun calientes de
los Bell Labs, y la capacidad de lanzar comandos del shell. La HP-15C
no podía ejecutar `!ls -la`. Para eso tenías una libreta.

**P: ¿Puede hablar mi idioma?**
R: Ya te está escuchando. La calculadora detecta el idioma del sistema
(`LC_ALL`/`LANG`) y habla inglés, francés o español — errores, mensajes
de modo y la ayuda completa incluidos. `--lang=xx` lo impone, y el
comando `lang` te dice qué decidió. La HP-15C hablaba DISPLAY, un
dialecto hecho de gruñidos de 44 registros.

**P: ¿Funciona en Windows?**
R: Técnicamente sí. En la práctica, si estás en Windows ya tienes
problemas más urgentes. La HP-15C, en cambio, funcionaba en todas
partes.

**P: ¿Y por qué no Python?**
R: Porque tenemos clase. Y porque en Python no existe un
`std::vector<Value>` con un patrón Command bien estructurado. Aquí
las cosas se hacen en serio.

**P: `fact` me devuelve un error con -1.**
R: El factorial solo está definido para enteros no negativos. La HP-15C
hacía lo mismo. No lo decido yo, lo dicen las mates. (La HP-15C
mostraba una `E` en su LCD. Nosotros imprimimos un mensaje de error en
color. Eso es progreso.)

**P: ¿Cómo calculo una raíz cúbica?**
R: `3 1/x pow` — o `3 0.3333333 pow` si te da pereza. La HP-15C tenía
un botón `yˣ` para eso. Nosotros tenemos un teclado. Nos adaptamos.

**P: ¿Es open source?**
R: Tienes el código fuente, tienes la libertad. Haz lo que quieras
con él. La HP-15C, en cambio, era cerrada. Ni siquiera podías abrirla
sin perder la garantía.

**P: ¿Por qué los colores ANSI?**
R: Nostalgia. Cada categoría de comando tiene su color: amarillo para
la aritmética, verde para las unarias, cian para la trigonometría,
magenta para las hiperbólicas, azul para la pila, rojo para la memoria.
Y en modo interactivo la coloración es **viva**: un comando se enciende
en cuanto su prefijo deja de ser ambiguo (`p` se vuelve amarillo antes
de que termines de escribir `pow`); un prefijo ambiguo permanece en
claro hasta decidirse. Es como los botones de colores de la HP-15C,
salvo que no parpadea nada y las teclas se te adelantan.

**P: Lo de `!comando`, ¿no es peligroso?**
R: Estás lanzando comandos del shell desde dentro de un programa. Sí,
es peligroso — por eso solo funciona en una sesión interactiva en
directo: un `!` llegado de una tubería o de un archivo se rechaza con
un error. Tu teclado, tu responsabilidad; el teclado de un script no
es de nadie. La HP-15C tampoco podía lanzar comandos, y nadie se
quejó.

**P: ¿Por qué `10 3 /` me da `3` y no `3.333333333`?**
R: Porque `10` y `3` son enteros exactos — y los enteros exactos se
dividen exactamente, como `dc` hace desde los Bell Labs en 1971.
Lorinda Cherry y Robert Morris escribieron la primera calculadora RPN
con aritmética entera exacta antes incluso de que existiera la HP-35.
Nosotros solo llevamos la antorcha — de hecho, contratamos a su teclado:
mira la tabla de Alias de dc. ¿Quieres el resultado flotante?
Pídelo: `10 3.0 /` → `3.333333333`.

**P: Escribí una operación inválida y me saltó un error. ¿Mi pila está muerta?**
R: No. Los errores aritméticos (`5 0 /`, `-1 sqrt`, `0 ln`…) muestran
un mensaje y **devuelven tus operandos a su sitio, intactos**. La pila
queda exactamente como estaba antes de tu error. La HP-15C parpadeaba
con una `E` y te dejaba adivinando; nosotros reconstruimos la escena
del crimen.

---

## Créditos

- **Jan Łukasiewicz** — por la notación polaca en 1920. Sin él, nada
  de esto existiría.
- **Friedrich L. Bauer** — por la primera implementación RPN en 1968.
  Un pionero olvidado.
- **Lorinda Cherry y Robert Morris** — por **`dc`** en los Bell Labs en
  1971: la primera calculadora RPN, con aritmética entera exacta en
  postfijo, antes incluso de que existiera la HP-35. Su precisión vive
  en la nuestra.
- **Al-Juarismi (Al-Khwārizmī)** — los algoritmos, la aritmética decimal
  posicional, la propia palabra «algoritmo». Nunca vio una pila, pero
  cada uno de los dígitos de nuestro `2 332192 pow` de 100.000 cifras
  se apoya en sus hombros.
- **Hewlett-Packard** — por la **HP-35** (1972) y la **HP-15C** (1982).
  La HP-35 demostró que una calculadora podía caber en un bolsillo.
  La HP-15C demostró que podía caber en un corazón.
- **Tú** — por leer este README hasta el final. La HP-15C no habría
  tenido esa paciencia.

---

## Licencia

MIT License — Copyright (c) 2026 fpwp2n2ks4-stack

Consulta el archivo [LICENSE](LICENSE) para el texto completo.

---

*Fabricado con C++23 y una pizca de nostalgia digital.*
*Sin garantía. Sin soporte. Sin brillo de LCD. Sin arrepentimientos.*

*Apila con responsabilidad. Divide siempre entre algo distinto de cero.
Encadena esos `dup` y esos `drop`, campeón.*
