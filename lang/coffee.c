/*
 * Coffee script language mode for QEmacs.
 *
 * Copyright (c) 2000-2020 Charlie Gordon.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "qe.h"

/*---------------- Coffee coloring ----------------*/

static char const coffee_keywords[] = {
    // keywords common with Javascript:
    "true|false|null|this|new|delete|typeof|in|instanceof|"
    "return|throw|break|continue|debugger|yield|if|else|"
    "switch|for|while|do|try|catch|finally|class|extends|super|"
    // CoffeeScript only keywords:
    "undefined|then|unless|until|loop|of|by|when|"
    // aliasses
    "and|or|is|isnt|not|yes|no|on|off|"
    // reserved: should be flagged as errors
    "case|default|function|var|void|with|const|let|enum|export|import|"
    "native|implements|interface|package|private|protected|public|static|"
    // proscribed in strict mode
    "arguments|eval|yield*|"
};

enum {
    IN_COFFEE_STRING       = 0x100,
    IN_COFFEE_STRING2      = 0x200,
    IN_COFFEE_REGEX        = 0x400,
    IN_COFFEE_LONG_STRING  = 0x01,
    IN_COFFEE_LONG_STRING2 = 0x02,
    IN_COFFEE_LONG_REGEX   = 0x04,
    IN_COFFEE_REGEX_CCLASS = 0x08,
    IN_COFFEE_JSTOKEN      = 0x10,
    IN_COFFEE_LONG_COMMENT = 0x20,
};

enum {
    COFFEE_STYLE_TEXT =     QE_STYLE_DEFAULT,
    COFFEE_STYLE_COMMENT =  QE_STYLE_COMMENT,
    COFFEE_STYLE_STRING =   QE_STYLE_STRING,
    COFFEE_STYLE_REGEX =    QE_STYLE_STRING,
    COFFEE_STYLE_JSTOKEN =  QE_STYLE_STRING,
    COFFEE_STYLE_NUMBER =   QE_STYLE_NUMBER,
    COFFEE_STYLE_KEYWORD =  QE_STYLE_KEYWORD,
    COFFEE_STYLE_FUNCTION = QE_STYLE_FUNCTION,
    COFFEE_STYLE_ERROR =    QE_STYLE_ERROR,
};

static void coffee_colorize_line(QEColorizeContext *cp,
                                 unsigned int *str, int n, ModeDef *syn)
{
    int i = 0, start = i, c, style = 0, sep, prev, i1;
    int state = cp->colorize_state;
    char kbuf[64];

    if (state & IN_COFFEE_STRING) {
        sep = '\'';
        goto parse_string;
    }
    if (state & IN_COFFEE_STRING2) {
        sep = '\"';
        goto parse_string;
    }
    if (state & IN_COFFEE_REGEX) {
        goto parse_regex;
    }
    if (state & IN_COFFEE_LONG_STRING) {
        sep = '\'';
        goto parse_long_string;
    }
    if (state & IN_COFFEE_LONG_STRING2) {
        sep = '\"';
        goto parse_long_string;
    }
    if (state & IN_COFFEE_LONG_REGEX) {
        goto parse_regex;
    }
    if (state & IN_COFFEE_JSTOKEN) {
        goto parse_jstoken;
    }
    if (state & IN_COFFEE_LONG_COMMENT) {
        goto parse_long_comment;
    }

    while (i < n) {
        start = i;
        c = str[i++];
        switch (c) {
        case '#':
            if (str[i] == '#' && str[i + 1] == '#') {
                /* multi-line block comments with ### */
                state = IN_COFFEE_LONG_COMMENT;
            parse_long_comment:
                while (i < n) {
                    c = str[i++];
                    if (c == '#' && str[i] == '#' && str[i + 1] == '#') {
                        i += 2;
                        state = 0;
                        break;
                    }
                }
            } else {
                i = n;
            }
            style = COFFEE_STYLE_COMMENT;
            break;

        case '\'':
        case '\"':
            /* parse string constant */
            i--;
            sep = str[i++];
            if (str[i] == (unsigned int)sep && str[i + 1] == (unsigned int)sep) {
                /* long string */
                state = (sep == '\"') ? IN_COFFEE_LONG_STRING2 :
                        IN_COFFEE_LONG_STRING;
                i += 2;
            parse_long_string:
                while (i < n) {
                    c = str[i++];
                    if (c == '\\') {
                        if (i < n) {
                            i += 1;
                        }
                    } else
                    if (c == sep && str[i] == (unsigned int)sep && str[i + 1] == (unsigned int)sep) {
                        i += 2;
                        state = 0;
                        break;
                    }
                }
            } else {
                state = (sep == '\"') ? IN_COFFEE_STRING2 : IN_COFFEE_STRING;
            parse_string:
                while (i < n) {
                    c = str[i++];
                    if (c == '\\') {
                        if (i < n) {
                            i += 1;
                        }
                    } else
                    if (c == sep) {
                        state = 0;
                        break;
                    }
                }
                if (state) {
                    state = 0;
                    // unterminated string literal, should flag unless
                    // point is at end of line.
                    style = COFFEE_STYLE_ERROR;
                    break;
                }
            }
            style = COFFEE_STYLE_STRING;
            break;

        case '`':
            /* parse multi-line JS token */
            state = IN_COFFEE_JSTOKEN;
        parse_jstoken:
            while (i < n) {
                c = str[i++];
                if (c == '\\') {
                    if (i < n) {
                        i += 1;
                    }
                } else
                if (c == '`') {
                    state = 0;
                    break;
                }
            }
            style = COFFEE_STYLE_JSTOKEN;
            break;

        case '.':
            if (qe_isdigit(str[i]))
                goto parse_decimal;
            if (str[i] == '.') /* .. range operator */
                i++;
            if (str[i] == '.') /* ... range operator */
                i++;
            continue;

        case '/':
            /* XXX: should use more context to tell regex from divide */
            if (str[i] == '/') {
                i++;
                if (str[i] == '/') {
                    /* multiline /// regex */
                    state = IN_COFFEE_LONG_REGEX;
                    i++;
                    goto parse_regex;
                } else {
                    /* floor divide // operator */
                    break;
                }
            }
            prev = ' ';
            for (i1 = start; i1 > 0; ) {
                prev = str[--i1] & CHAR_MASK;
                if (!qe_isblank(prev))
                    break;
            }
            if (qe_findchar(" [({},;=<>!~^&|*/%?:", prev)
            ||  qe_findchar("^\\?.[{},;<>!~&|*%:", str[i])
            ||  (str[i] == '=' && str[i + 1] == '/')
            ||  (str[i] == '(' && str[i + 1] == '?')
            ||  (str[i1] >> STYLE_SHIFT) == COFFEE_STYLE_KEYWORD
            ||  (str[i] != ' ' && (str[i] != '=' || str[i + 1] != ' ')
            &&   !(qe_isalnum(prev) || qe_findchar(")]}\"\'?:", prev)))) {
                state = IN_COFFEE_REGEX;
            parse_regex:
                style = COFFEE_STYLE_REGEX;
                while (i < n) {
                    c = str[i++];
                    if (c == '\\') {
                        if (i < n) {
                            i += 1;
                        }
                    } else
                    if (state & IN_COFFEE_REGEX_CCLASS) {
                        if (c == ']') {
                            state &= ~IN_COFFEE_REGEX_CCLASS;
                        }
                        /* ignore '/' inside char classes */
                    } else {
                        if (c == '[') {
                            state |= IN_COFFEE_REGEX_CCLASS;
                            if (str[i] == '^')
                                i++;
                            if (str[i] == ']')
                                i++;
                        } else
                        if (state & IN_COFFEE_LONG_REGEX) {
                            if (c == '/' && str[i] == '/' && str[i + 1] == '/') {
                                i += 2;
                                state = 0;
                                while (qe_isalpha(str[i]))
                                    i++;
                                break;
                            } else
                            if (qe_isblank(c) && str[i] == '#' && str[i+1] != '{') {
                                SET_COLOR(str, start, i, style);
                                start = i;
                                i = n;
                                style = COFFEE_STYLE_COMMENT;
                                break;
                            }
                        } else {
                            if (c == '/') {
                                state = 0;
                                while (qe_isalpha(str[i]))
                                    i++;
                                break;
                            }
                        }
                    }
                }
                if (state & ~IN_COFFEE_LONG_REGEX) {
                    state = 0;
                    // unterminated regex literal, should flag unless
                    // point is at end of line.
                    style = COFFEE_STYLE_ERROR;
                    break;
                }
                break;
            }
            continue;

        default:
            if (qe_isdigit(c)) {
                if (c == '0' && str[i] == 'b') {
                    /* binary numbers */
                    for (i += 1; qe_isbindigit(str[i]); i++)
                        continue;
                } else
                if (c == '0' && str[i] == 'o') {
                    /* octal numbers */
                    for (i += 1; qe_isoctdigit(str[i]); i++)
                        continue;
                } else
                if (c == '0' && str[i] == 'x') {
                    /* hexadecimal numbers */
                    for (i += 1; qe_isxdigit(str[i]); i++)
                        continue;
                } else {
                    /* decimal numbers */
                    for (; qe_isdigit(str[i]); i++)
                        continue;
                    if (str[i] == '.' && qe_isdigit(str[i + 1])) {
                        i++;
                    parse_decimal:
                        /* decimal floats require a digit after the '.' */
                        for (; qe_isdigit(str[i]); i++)
                            continue;
                    }
                    if (str[i] == 'e') {
                        int k = i + 1;
                        if (str[k] == '+' || str[k] == '-')
                            k++;
                        if (qe_isdigit(str[k])) {
                            for (i = k + 1; qe_isdigit(str[i]); i++)
                                continue;
                        }
                    }
                }

                /* XXX: should detect malformed number constants */
                style = COFFEE_STYLE_NUMBER;
                break;
            }
            if (qe_isalpha_(c)) {
                i += ustr_get_identifier(kbuf, countof(kbuf), c, str, i, n);
                if (strfind(syn->keywords, kbuf)) {
                    style = COFFEE_STYLE_KEYWORD;
                    break;
                }
                if (check_fcall(str, i)) {
                    style = COFFEE_STYLE_FUNCTION;
                    break;
                }
                continue;
            }
            continue;
        }
        if (style) {
            SET_COLOR(str, start, i, style);
            style = 0;
        }
    }
    cp->colorize_state = state;
}

static int coffee_mode_probe(ModeDef *mode, ModeProbeData *p)
{
    if (match_extension(p->filename, mode->extensions)
    ||  match_shell_handler(cs8(p->buf), mode->shell_handlers)
    ||  stristart(p->filename, "Cakefile", NULL)) {
        return 80;
    }
    return 1;
}

static ModeDef coffee_mode = {
    .name = "CoffeeScript",
    .alt_name = "coffee",
    .extensions = "coffee",
    .shell_handlers = "coffee",
    .mode_probe = coffee_mode_probe,
    .keywords = coffee_keywords,
    .colorize_func = coffee_colorize_line,
};

static int coffee_init(void)
{
    qe_register_mode(&coffee_mode, MODEF_SYNTAX);

    return 0;
}

qe_module_init(coffee_init);
