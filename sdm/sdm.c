#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>

struct stardict_index {
	char *word;
	u32int offset;
	u32int size;
};

unsigned long
ntohl(int x)
{
	unsigned long n;
	unsigned char *p;

	n = x;
	p = (unsigned char*)&n;
	return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
}

int stardict_idx_read(Biobuf *idx_bio, struct stardict_index *idxbuf) {
	idxbuf->word = Brdstr(idx_bio, '\0', 1);
	if (idxbuf->word == nil) return -1;
	u32int tmp;
	if (Bread(idx_bio, &tmp, sizeof(tmp)) != sizeof(tmp)) return -1;
	idxbuf->offset = ntohl(tmp);
	if (Bread(idx_bio, &tmp, sizeof(tmp)) != sizeof(tmp)) return -1;
	idxbuf->size = ntohl(tmp);
	return 1;
}

int stardict_idx_search(Biobuf *idx_bio, char *word,
						struct stardict_index *idxbuf) {
	Bseek(idx_bio, 0, 0);
	int found = -1;
	while (stardict_idx_read(idx_bio, idxbuf) >= 0) {
		if (strcmp(word, idxbuf->word) == 0)
			found = 1;
		free(idxbuf->word);
		if (found > 0) break;
	}
	return found;
}

int stardict_dict_print(Biobuf *dict_bio, struct stardict_index idx) {
	if (Bseek(dict_bio, idx.offset, 0) != idx.offset) return -1;
	char buf[512];
	long readed = 0;
	long r;
	while (readed < idx.size) {
		r = idx.size - readed;
		if (r > 512) r = 512;
		if (Bread(dict_bio, buf, r) != r) return -1;
		if (write(1, buf, r) != r) return -1;
		readed += r;
	}
	return 1;
}

int getword(Biobuf *in, char *buf, int bufsize) {
	int c;

	// skip space
	do {
		c = Bgetc(in);
		if (c < 0) return c;
	} while (isspace(c));
	Bungetc(in);

	int i;
	for (i=0;i<bufsize-1;i++) {
		c = Bgetc(in);
		if (c < 0) return c;
		if (isspace(c)) break;
		buf[i] = c;
	}
	buf[i] = '\0';
	return 1;
}

#define MSG_NOTFOUND "WORD NOT FOUND\n"

void main(int argc, char *argv[]) {
	char *idx_fn = getenv("sdm_index");
	if (idx_fn == nil) idx_fn = "/lib/stardict/default.idx";
	Biobuf *idx_bio = Bopen(idx_fn, OREAD);
	if (idx_bio == nil) sysfatal("idx open failed: %r");
	char *dict_fn = getenv("sdm_dict");
	if (dict_fn == nil) dict_fn = "/lib/stardict/default.dict";
	Biobuf *dict_bio = Bopen(dict_fn, OREAD);
	if (dict_bio == nil) sysfatal("dict open failed: %r");
	Biobuf *stdin_bio = Bfdopen(0, OREAD);

	struct stardict_index idx_cur;
	int i;
	for (i=1;i<argc;i++) {
		if (stardict_idx_search(idx_bio, argv[i], &idx_cur) < 0) {
				fprint(2, MSG_NOTFOUND);
		} else {
				stardict_dict_print(dict_bio, idx_cur);
		}
	}

	if (argc == 1) {
		if (stdin_bio == nil) sysfatal("stdin open failed: %r");
		char word[256];
		while (getword(stdin_bio, word, 256) > 0) {
			if (stardict_idx_search(idx_bio, word, &idx_cur) < 0) {
				fprint(2, MSG_NOTFOUND);
			} else {
				stardict_dict_print(dict_bio, idx_cur);
			}
		}
	}

	exits(nil);
}