//Ivan Kuzanov <251ADB124>
//Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
//Resources: https://craftinginterpreters.com/parsing-expressions.html, https://www.cs.cornell.edu/courses/cs2112/2015fa/lectures/lec_parsing, https://introcs-python.cs.luc.edu/recursion/recursiondparser.html, https://en.wikipedia.org/wiki/Exponentiation_by_squaring
//AI: Gemini - Conversation link: https://share.gemini.google/QhTwS5AkOxjM
#define _POSIX_C_SOURCE 200809L  // needed for mkdir and stat in c17

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FIRSTNAME "ivan"
#define LASTNAME "kuzanov"
#define STUDENTID "251ADB124"

static char *text;      //input file buffer in memory
static size_t pos;      //current char index in buffer (0-based)
static size_t err_pos;  //1-based error position (0 = no error)

//Position tracking:
//The full input file is loaded into memory array `text`. `pos` is the zero-based array index.
//The lab requires 1-based character indexing for error reporting, so error position = `pos + 1`.
//Because whitespace and comments remain in the array buffer rather than being stripped by 
//a separate lexer, `pos + 1` directly matches the 1-based byte index in the source file.
static int fail(size_t index) {
    err_pos = index + 1;
    return 0;  // returns 0 so caller can do "return fail(pos)"
}

static void skip_spaces(void) {
    while (text[pos] == ' ' || text[pos] == '\t') pos++;
}

//Grammar (EBNF):
//expr    -> term (('+' | '-') term)*
//term    -> power (('*' | '/') power)*
//power   -> primary ('**' power)?
//primary -> NUMBER | '(' expr ')'
//
//Precedence: expr calls term, term calls power. So * and ** run before + and -
//(e.g. 2 + 3 * 4 evaluates 3 * 4 first).
//
//** Associativity: power() calls itself recursively for exponent, so 2**3**2
//becomes 2**(3**2) = 512 instead of looping.
//
//Integers: Task specifies integer truncation toward 0. Using long long keeps
//exact math without float rounding issues.
//
//Error positions:
//-Syntax error / missing token: fails at `pos` where parser got stuck.
//-Div by 0 or negative exp: saved index before parsing operand points to
//  start of invalid operand before `pos` moves.
//-Extra stuff on line: caught at end of line in evaluate().

static int parse_expr(long long *out);

static int match(char c) {
    skip_spaces();
    if (text[pos] != c) return 0;
    pos++;
    return 1;
}

static int parse_primary(long long *out) {
    skip_spaces();

    if (isdigit((unsigned char)text[pos])) {
        long long v = 0;
        while (isdigit((unsigned char)text[pos])) {
            v = v * 10 + (text[pos] - '0');
            pos++;
        }
        *out = v;
        return 1;
    }

    if (match('(')) {
        if (!parse_expr(out)) return 0;
        if (!match(')')) return fail(pos);  // expected ')' at current pos
        return 1;
    }

    return fail(pos); //unexpected char or missing number at pos
}

static int parse_power(long long *out) {
    if (!parse_primary(out)) return 0;
    skip_spaces();
    if (text[pos] == '*' && text[pos + 1] == '*') {
        pos += 2;
        skip_spaces();
        size_t exp_index = pos; //saved index for error location before parsing exponent
        long long exponent;
        
        //Recursive call makes ** right-associative
        if (!parse_power(&exponent)) return 0;
        if (exponent < 0) return fail(exp_index); //report at start of exponent

        // Binary Exponentiation (Exponentiation by Squaring): O(log exponent) efficiency
        // - (exponent & 1): checks if current exponent power is odd (multiplies base into result)
        // - exponent >>= 1: divides exponent by 2 using right shift
        // - base *= base: squares the base for the next power of 2
        long long result = 1, base = *out;
        while (exponent > 0) {
            if (exponent & 1) result *= base;
            exponent >>= 1;
            if (exponent > 0) base *= base;
        }
        *out = result;
    }
    return 1;
}

static int parse_term(long long *out) {
    if (!parse_power(out)) return 0;
    while (match('*') || match('/')) {  //loop handles left-associativity
        char op = text[pos - 1];
        skip_spaces();
        size_t right_index = pos;  // Save starting index of operand before parsing
        long long right;
        if (!parse_power(&right)) return 0;
        if (op == '*') {
            *out = *out * right;
        } else {
            // Division by zero: report error at start of divisor (right_index)
            if (right == 0) return fail(right_index);
            *out = *out / right;
        }
    }
    return 1;
}

static int parse_expr(long long *out) {
    if (!parse_term(out)) return 0;
    while (match('+') || match('-')) {  //loop handles left-associativity
        char op = text[pos - 1];
        long long right;
        if (!parse_term(&right)) return 0;
        if (op == '+') *out = *out + right;
        else *out = *out - right;
    }
    return 1;
}

static size_t evaluate(FILE *out) {
    pos = 0;
    err_pos = 0;
    while (text[pos] != '\0') {
        skip_spaces();
        if (text[pos] == '\n') {
            pos++;
            continue;
        }
        if (text[pos] == '\0') break;
        if (text[pos] == '#') {  //skip comment line
            while (text[pos] != '\n' && text[pos] != '\0') pos++;
            continue;
        }

        long long value;
        if (!parse_expr(&value)) return err_pos;
        skip_spaces();
        if (text[pos] != '\n' && text[pos] != '\0') {  //extra tokens on line
            fail(pos);
            return err_pos;
        }
        fprintf(out, "%lld\n", value);
    }
    return 0;
}

//Large files: Entire file read into memory once with fread(). Parser works directly on
//text[pos] without splitting strings or copying buffer.

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

static void get_base(const char *path, char *out, size_t size) {
    const char *name = strrchr(path, '/');
    name = (name != NULL) ? name + 1 : path;
    size_t n = strlen(name);
    if (n > 4 && strcmp(name + n - 4, ".txt") == 0) n -= 4;
    if (n >= size) n = size - 1;
    memcpy(out, name, n);
    out[n] = '\0';
}

static int make_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode) ? 0 : -1;
    return mkdir(path, 0777);
}

static void usage(void) {
    fprintf(stderr, "usage: calc [-o OUTDIR | --output-dir OUTDIR] input.txt\n");
}

int main(int argc, char **argv) {
    const char *input = NULL, *outdir = NULL;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-o") == 0 || strcmp(a, "--output-dir") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "calc: option '%s' needs a value\n", a);
                return 1;
            }
            outdir = argv[i];
        } else if (a[0] == '-' && a[1] != '\0') {
            fprintf(stderr, "calc: unknown option '%s'\n", a);
            usage();
            return 1;
        } else if (input != NULL) {
            fprintf(stderr, "calc: too many input files\n");
            usage();
            return 1;
        } else {
            input = a;
        }
    }
    if (input == NULL) {
        usage();
        return 1;
    }

    text = read_file(input);
    if (text == NULL) {
        fprintf(stderr, "calc: cannot read '%s'\n", input);
        return 1;
    }

    char base[256];
    get_base(input, base, sizeof base);

    char default_out[4096];
    if (outdir == NULL) {
        snprintf(default_out, sizeof default_out, "%s_%s_%s", base, FIRSTNAME, STUDENTID);
        outdir = default_out;
    }
    if (make_dir(outdir) != 0) {
        fprintf(stderr, "calc: cannot create output folder '%s'\n", outdir);
        free(text);
        return 1;
    }

    char out_path[8192];
    snprintf(out_path, sizeof out_path, "%s/%s_%s_%s_%s.txt", outdir, base, FIRSTNAME, LASTNAME,
             STUDENTID);
    FILE *out = fopen(out_path, "w");
    if (out == NULL) {
        fprintf(stderr, "calc: cannot write '%s'\n", out_path);
        free(text);
        return 1;
    }

    size_t error_at = evaluate(out);
    if (error_at != 0) {
        fclose(out);
        out = fopen(out_path, "w");
        if (out == NULL) {
            fprintf(stderr, "calc: cannot write '%s'\n", out_path);
            free(text);
            return 1;
        }
        fprintf(out, "ERROR:%zu\n", error_at);
    }

    int status = 0;
    if (fclose(out) != 0) {
        fprintf(stderr, "calc: cannot write '%s'\n", out_path);
        status = 1;
    }
    free(text);
    return status;
}