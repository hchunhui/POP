#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include "packet_parser.h"

/* symbol table entry */
struct entry {
	struct header *h;
	int ref;
	bool decl;
};

/* Tokens */
enum token {
	T_EOI,    // End of input
	T_BAD,    // Bad token
	T_START,  // "start"
	T_HEADER, // "header"
	T_FIELDS, // "fields"
	T_LENGTH, // "length"
	T_NEXT,   // "next"
	T_SELECT, // "select"
	T_CASE,   // "case"
	T_BEGIN,  // "{"
	T_END,    // "}"
	T_LP,     // "("
	T_RP,     // ")"
	T_COL,    // ":"
	T_SCOL,   // ";"
	T_STAR,   // "*"
	T_IDENT,  // [_a-zA-Z][_a-zA-Z0-9]*
	T_NUMBER, // 0[0-7]* | 0[xX][0-9a-fA-F]+ | 0[bB][01]+ | [1-9][0-9]*
};

/* Parse state */
struct parse_ctx {
	const char *data;      // input buffer
	int length;            // length of buffer
	const char *curr;      // current position
	int row, col;          // row and column for current char
	int tok_row, tok_col;  // row and column for current token
	char ch;               // current char
	enum token tok;        // current token
	unsigned long long val;// attribute for current token
	char buf[80];          // attribute for current token

	struct entry symtab[100];
	int tabsize;
	struct header *curr_h;
	int curr_offset;
};

/* Get next char from input buffer */
static void next_ch(struct parse_ctx *pctx)
{
	if(pctx->curr >= pctx->data + pctx->length)
		pctx->ch = 0;
	pctx->ch = *(pctx->curr++);
	if(pctx->ch == '\n') {
		pctx->row++;
		pctx->col = 0;
	} else if(pctx->ch == '\t') {
		pctx->col += 8 - (pctx->col % 8);
	} else pctx->col++;
}

/* Get next token (Lexer) */
/*
  L: Look
  N: Next
  R: Return
*/
static void next_tok(struct parse_ctx *pctx)
{
#define L (pctx->ch)
#define N (next_ch(pctx))
#define R(x) do { pctx->tok=(x); return; } while (0)
	for(;;) {
		pctx->tok_row = pctx->row;
		pctx->tok_col = pctx->col;
		switch(L) {
		case ' ':  case '\t':  case '\n':  case '\r':
			N;
			continue;
		case '/':
			N;
			if(L == '*') {
				char l;
				N;
				do {
					l = L;
					N;
				} while(l != '*' || L != '/');
				N;
				continue;
			} else if(L == '/') {
				do N; while(L != '\n');
				N;
				continue;
			} else R(T_BAD);
			break;
		case '{': N; R(T_BEGIN);
		case '}': N; R(T_END);
		case '(': N; R(T_LP);
		case ')': N; R(T_RP);
		case ':': N; R(T_COL);
		case ';': N; R(T_SCOL);
		case '*': N; R(T_STAR);
		case '\0': R(T_EOI);
		default:
			if(isalpha(L) || L == '_') {
				char *p = pctx->buf;
				*p++ = L;
				while(p - pctx->buf < 31) {
					N;
					if(isalpha(L) || isdigit(L) || L == '_')
						*p++ = L;
					else break;
				}
				*p = 0;
				if(strcmp(pctx->buf, "start") == 0)
					R(T_START);
				else if(strcmp(pctx->buf, "header") == 0)
					R(T_HEADER);
				else if(strcmp(pctx->buf, "fields") == 0)
					R(T_FIELDS);
				else if(strcmp(pctx->buf, "length") == 0)
					R(T_LENGTH);
				else if(strcmp(pctx->buf, "next") == 0)
					R(T_NEXT);
				else if(strcmp(pctx->buf, "select") == 0)
					R(T_SELECT);
				else if(strcmp(pctx->buf, "case") == 0)
					R(T_CASE);
				R(T_IDENT);
			} else if(isdigit(L)) {
				if(L == '0') {
					N;
					if(L == 'x' || L == 'X') {
						char *p = pctx->buf;
						while(p - pctx->buf < 16) {
							N;
							if((L >= '0' && L <= '9') ||
							   (L >= 'a' && L <= 'f') ||
							   (L >= 'A' && L <= 'F'))
								*p++ = L;
							else break;
						}
						*p = 0;
						pctx->val = strtoull(pctx->buf, NULL, 16);
					} else if(L == 'b' || L == 'B') {
						char *p = pctx->buf;
						while(p - pctx->buf < 64) {
							N;
							if(L >= '0' && L <= '1')
								*p++ = L;
							else break;
						}
						*p = 0;
						pctx->val = strtoull(pctx->buf, NULL, 2);
					} else if(L >= '0' && L <= '7') {
						char *p = pctx->buf;
						*p++ = L;
						while(p - pctx->buf < 22) {
							N;
							if(L >= '0' && L <= '7')
								*p++ = L;
							else break;
						}
						*p = 0;
						pctx->val = strtoull(pctx->buf, NULL, 8);
					}
				} else {
					char *p = pctx->buf;
					*p++ = L;
					while(p - pctx->buf < 20) {
						N;
						if(L >= '0' && L <= '9')
							*p++ = L;
						else break;
					}
					*p = 0;
					pctx->val = strtoull(pctx->buf, NULL, 10);
				}
				R(T_NUMBER);
			}
			R(T_BAD);
		}
	}
#undef L
#undef N
#undef R
}

/* Parser */
/*
  F(g): Fail and return if parse operation `g' fail.
  T(t): Test if current token is `t'.
  M(t): Match terminal symbol(token) `t'.
  P(f): Parse non-terminal symbol `f'.
  D(f): Define non-terminal symbol `f'.
*/
#define F(g) do {							\
		 int ret;						\
		 if((ret = (g))) {					\
			fprintf(stderr, "Fail at (%d, %d) in %s.\n",	\
				pctx->tok_row,				\
				pctx->tok_col, #g);			\
			return ret;					\
		 }							\
	} while(0)
#define T(t) (parse_tok(pctx, t) == 0)
#define M(t) F(parse_tok(pctx, t))
#define P(f) F(parse_##f(pctx))
#define D(f) static int parse_##f(struct parse_ctx *pctx)
#define CTX (pctx)

static int parse_tok(struct parse_ctx *pctx, enum token tok)
{
	if(pctx->tok == tok) {
#if 0
		switch(tok) {
		case T_BEGIN: puts("{"); break;
		case T_END: puts("}"); break;
		case T_LP: puts("("); break;
		case T_RP: puts(")"); break;
		case T_COL: puts(":"); break;
		case T_SCOL: puts(";"); break;
		case T_STAR: puts("*"); break;
		case T_BAD: puts(" BAD "); break;
		case T_EOI: puts("\n"); break;
		case T_NUMBER: printf("%llu\n", pctx->val); break;
		default:
			printf("%s\n", pctx->buf);
		}
		fflush(stdout);
#endif
		next_tok(pctx);
		return 0;
	}
	return 1;
}

/*
Syntax for Packet Header Spec:
  prog ::= headers start
  start ::= START IDENT SCOL
  headers ::= e | header headers
  header ::= HEADER IDENT (SCOL | BEGIN fields length next END)
  fields ::= FIELDS BEGIN items END
  items ::= e | IDENT COL (NUMBER | STAR) SCOL items
  length ::= e | LENGTH COL NUMBER
  next ::= e | NEXT SELECT LP IDENT RP BEGIN cases END
  cases ::= e | CASE NUMBER COL IDENT SCOL cases
*/

static struct entry *symtab_find(struct parse_ctx *pctx, char *name)
{
	int i;
	for(i = 0; i < pctx->tabsize; i++)
		if(strcmp(name, header_get_name(pctx->symtab[i].h)) == 0)
			return pctx->symtab + i;
	return NULL;
}

static void symtab_enter(struct parse_ctx *pctx, struct header *h)
{
	int i = pctx->tabsize;
	assert(i < 100);
	pctx->symtab[i].h = h;
	pctx->symtab[i].ref = 0;
	pctx->symtab[i].decl = true;
	pctx->tabsize++;
}

D(cases)
{
	if(T(T_CASE)) {
		unsigned long long val;
		struct entry *e;
		int slen;
		M(T_NUMBER);
		val = CTX->val;
		M(T_COL);
		M(T_IDENT);
		e = symtab_find(CTX, CTX->buf);
		if(!e) {
			fprintf(stderr, "Header %s not found.\n", CTX->buf);
			return 2;
		}
		slen = header_get_sel_length(CTX->curr_h);
		e->ref++;
		if(slen <= 8)
			header_add_next(CTX->curr_h, value_from_8(val), e->h);
		else if(slen == 16)
			header_add_next(CTX->curr_h, value_from_16(val), e->h);
		else if(slen == 32)
			header_add_next(CTX->curr_h, value_from_32(val), e->h);
		else if(slen == 48)
			header_add_next(CTX->curr_h, value_from_48(val), e->h);
		else if(slen == 64)
			header_add_next(CTX->curr_h, value_from_64(val), e->h);
		else {
			fprintf(stderr, "Unable to handle length: %d\n", slen);
			return 2;
		}
		M(T_SCOL);
		P(cases);
		return 0;
	} else {
		return 0;
	}
}

D(next)
{
	if(T(T_NEXT)) {
		M(T_SELECT);
		M(T_LP);
		M(T_IDENT);
		header_set_sel(CTX->curr_h, CTX->buf);
		M(T_RP);
		M(T_BEGIN);
		P(cases);
		M(T_END);
		return 0;
	} else {
		return 0;
	}
}

D(length)
{
	if(T(T_LENGTH)) {
		M(T_COL);
		M(T_NUMBER);
		header_set_length(CTX->curr_h, CTX->val);
		M(T_SCOL);
		return 0;
	} else {
		header_set_length(CTX->curr_h, CTX->curr_offset);
		return 0;
	}
}

D(items)
{
	char name[32];
	if(T(T_IDENT)) {
		strcpy(name, CTX->buf);
		M(T_COL);
		if(T(T_NUMBER)) {
			header_add_field(CTX->curr_h,
					 name,
					 CTX->curr_offset, CTX->val);
			CTX->curr_offset += CTX->val;
		} else {
			M(T_STAR);
			header_add_field(CTX->curr_h,
					 name,
					 CTX->curr_offset, 0);
		}
		M(T_SCOL);
		P(items);
		return 0;
	} else {
		return 0;
	}
}

D(fields)
{
	M(T_FIELDS);
	M(T_BEGIN);
	P(items);
	M(T_END);
	return 0;
}

D(headers)
{
	char name[32];
	if(T(T_HEADER)) {
		M(T_IDENT);
		strcpy(name, CTX->buf);
		if(T(T_SCOL)) {
			struct entry *e = symtab_find(CTX, name);
			if(!e) {
				struct header *h = header(name);
				symtab_enter(CTX, h);
			}
			P(headers);
			return 0;
		} else {
			struct entry *e = symtab_find(CTX, name);
			if(!e) {
				struct header *h = header(name);
				symtab_enter(CTX, h);
				e = symtab_find(CTX, name);
				e->decl = false;
				CTX->curr_h = h;
				CTX->curr_offset = 0;
			} else {
				if(e->decl) {
					e->decl = false;
					CTX->curr_h = e->h;
					CTX->curr_offset = 0;
				} else {
					fprintf(stderr, "Conflict header %s.\n", name);
					return 2;
				}
			}
			M(T_BEGIN);
			P(fields);
			P(length);
			P(next);
			M(T_END);
			P(headers);
			return 0;
		}
	} else {
		return 0;
	}
}

D(start)
{
	struct entry *e;
	M(T_START);
	M(T_IDENT);
	e = symtab_find(CTX, CTX->buf);
	e->ref++;
	CTX->curr_h = e->h;
	M(T_SCOL);
	return 0;
}

D(prog)
{
	P(headers);
	P(start);
	M(T_EOI);
	return 0;
}

static int parse(struct parse_ctx *pctx)
{
	next_ch(pctx);
	next_tok(pctx);
	return parse_prog(pctx);
}

#undef F
#undef T
#undef M
#undef P
#undef D
#undef CTX

/* Interface */
struct header *parse_string(char *s, int length)
{
	int i;
	int ret;
	struct parse_ctx ctx;
	ctx.data = s;
	ctx.curr = ctx.data;
	ctx.length = length;
	ctx.row = 1;
	ctx.col = 0;
	ctx.tabsize = 0;
	ret = parse(&ctx);
	if(ret)
		return NULL;
	for(i = 0; i < ctx.tabsize; i++)
		if(ctx.symtab[i].ref == 0) {
			fprintf(stderr, "Warning: Defined but not used Header %s.\n",
				header_get_name(ctx.symtab[i].h));
			header_free(ctx.symtab[i].h);
		}
	return ctx.curr_h;
}

/* For test */
#if 1
int main(int argc, char *argv[])
{
	FILE *fp;
	int size;
	char *s;
	struct header *h;
	fp = fopen("scripts/header.spec", "r");
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	s = malloc(size+1);
	fread(s, size, 1, fp);
	s[size] = 0;
	fclose(fp);
	h = parse_string(s, size);
	if(h)
		printf("accept, start is %s.\n", header_get_name(h));
	else
		printf("reject.\n");
	free(s);
	return 0;
}
#endif
