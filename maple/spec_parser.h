#ifndef _SPEC_PARSER_H_
#define _SPEC_PARSER_H_

struct header;
struct header *spec_parser_string(const char *s, int length);
struct header *spec_parser_file(const char *filename);

#endif /* _SPEC_PARSER_H_ */
