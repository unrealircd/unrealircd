/*
 * IRC - Internet Relay Chat, antirandom.c
 * (C) Copyright 2004-2016, Bram Matthys (Syzop) <syzop@vulnscan.org>
 *
 * Contains ideas from Keith Dunnett <keith@dunnett.org>
 * Most of the detection mechanisms come from SpamAssassin FVGT_Tripwire.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

/* You can change this '//#undef' into '#define' if you want to see quite
 * a flood for every user that connects (and on-load if cfg.fullstatus_on_load).
 * Obviously only recommended for testing, use with care!
 */
#undef  DEBUGMODE

/** Change this 'undef' to 'define' to get performance information.
 * This really only meant for debugging purposes.
 */
#undef TIMING

ModuleHeader MOD_HEADER(antirandom)
  = {
	"antirandom",
	"v1.4",
	"Randomness detector",
	"3.2-b8-1",
	NULL 
    };

#ifndef MAX
 #define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

#ifndef _WIN32
typedef struct {
    char *regex;
    int score;
} ScoreTable;
#endif

#ifndef BAN_ACT_WARN
 #define BAN_ACT_WARN 11
#endif

#ifndef _WIN32
/* You can define regexes here.. the format is:
 * {"<REGEX>", SCORE},
 */
ScoreTable regex_scores[] = {
	/* These have all been moved to internal digit/vowel/consonant checks.
	 * But I've left the regex ability here, in case someone else uses it.
	 */
	{NULL, 0}
};
#endif

/* "<char1><char2>" followed by "<rest>" */
static char *triples_txt[] = {
	"aj", "fqtvxz",
	"aq", "deghjkmnprtxyz",
	"av", "bfhjqwxz",
	"az", "jwx",
	"bd", "bghjkmpqvxz",
	"bf", "bcfgjknpqvwxyz",
	"bg", "bdfghjkmnqstvxz",
	"bh", "bfhjkmnqvwxz",
	"bj", "bcdfghjklmpqtvwxyz",
	"bk", "dfjkmqrvwxyz",
	"bl", "bgpqwxz",
	"bm", "bcdflmnqz",
	"bn", "bghjlmnpqtvwx",
	"bp", "bfgjknqvxz",
	"bq", "bcdefghijklmnopqrstvwxyz",
	"bt", "dgjkpqtxz",
	"bv", "bfghjklnpqsuvwxz",
	"bw", "bdfjknpqsuwxyz",
	"bx", "abcdfghijklmnopqtuvwxyz",
	"bz", "bcdfgjklmnpqrstvwxz",
	"cb", "bfghjkpqyz",
	"cc", "gjqxz",
	"cd", "hjkqvwxz",
	"cf", "gjknqvwyz",
	"cg", "bdfgjkpqvz",
	"cl", "fghjmpqxz",
	"cm", "bjkqv",
	"cn", "bghjkpqwxz",
	"cp", "gjkvxyz",
	"cq", "abcdefghijklmnopqsvwxyz",
	"cr", "gjqx",
	"cs", "gjxz",
	"cv", "bdfghjklmnquvwxyz",
	"cx", "abdefghjklmnpqrstuvwxyz",
	"cy", "jqy",
	"cz", "bcdfghjlpqrtvwxz",
	"db", "bdgjnpqtxz",
	"dc", "gjqxz",
	"dd", "gqz",
	"df", "bghjknpqvxyz",
	"dg", "bfgjqvxz",
	"dh", "bfkmnqwxz",
	"dj", "bdfghjklnpqrwxz",
	"dk", "cdhjkpqrtuvwxz",
	"dl", "bfhjknqwxz",
	"dm", "bfjnqw",
	"dn", "fgjkmnpqvwz",
	"dp", "bgjkqvxz",
	"dq", "abcefghijkmnopqtvwxyz",
	"dr", "bfkqtvx",
	"dt", "qtxz",
	"dv", "bfghjknqruvwyz",
	"dw", "cdfjkmnpqsvwxz",
	"dx", "abcdeghjklmnopqrsuvwxyz",
	"dy", "jyz",
	"dz", "bcdfgjlnpqrstvxz",
	"eb", "jqx",
	"eg", "cjvxz",
	"eh", "hxz",
	"ej", "fghjpqtwxyz",
	"ek", "jqxz",
	"ep", "jvx",
	"eq", "bcghijkmotvxyz",
	"ev", "bfpq",
	"fc", "bdjkmnqvxz",
	"fd", "bgjklqsvyz",
	"fg", "fgjkmpqtvwxyz",
	"fh", "bcfghjkpqvwxz",
	"fj", "bcdfghijklmnpqrstvwxyz",
	"fk", "bcdfghjkmpqrstvwxz",
	"fl", "fjkpqxz",
	"fm", "dfhjlmvwxyz",
	"fn", "bdfghjklnqrstvwxz",
	"fp", "bfjknqtvwxz",
	"fq", "abcefghijklmnopqrstvwxyz",
	"fr", "nqxz",
	"fs", "gjxz",
	"ft", "jqx",
	"fv", "bcdfhjklmnpqtuvwxyz",
	"fw", "bcfgjklmpqstuvwxyz",
	"fx", "bcdfghjklmnpqrstvwxyz",
	"fy", "ghjpquvxy",
	"fz", "abcdfghjklmnpqrtuvwxyz",
	"gb", "bcdknpqvwx",
	"gc", "gjknpqwxz",
	"gd", "cdfghjklmqtvwxz",
	"gf", "bfghjkmnpqsvwxyz",
	"gg", "jkqvxz",
	"gj", "bcdfghjklmnpqrstvwxyz",
	"gk", "bcdfgjkmpqtvwxyz",
	"gl", "fgjklnpqwxz",
	"gm", "dfjkmnqvxz",
	"gn", "jkqvxz",
	"gp", "bjknpqtwxyz",
	"gq", "abcdefghjklmnopqrsvwxyz",
	"gr", "jkqt",
	"gt", "fjknqvx",
	"gu", "qwx",
	"gv", "bcdfghjklmpqstvwxyz",
	"gw", "bcdfgjknpqtvwxz",
	"gx", "abcdefghjklmnopqrstvwxyz",
	"gy", "jkqxy",
	"gz", "bcdfgjklmnopqrstvxyz",
	"hb", "bcdfghjkqstvwxz",
	"hc", "cjknqvwxz",
	"hd", "fgjnpvz",
	"hf", "bfghjkmnpqtvwxyz",
	"hg", "bcdfgjknpqsxyz",
	"hh", "bcgklmpqrtvwxz",
	"hj", "bcdfgjkmpqtvwxyz",
	"hk", "bcdgkmpqrstvwxz",
	"hl", "jxz",
	"hm", "dhjqrvwxz",
	"hn", "jrxz",
	"hp", "bjkmqvwxyz",
	"hq", "abcdefghijklmnopqrstvwyz",
	"hr", "cjqx",
	"hs", "jqxz",
	"hv", "bcdfgjklmnpqstuvwxz",
	"hw", "bcfgjklnpqsvwxz",
	"hx", "abcdefghijklmnopqrstuvwxyz",
	"hz", "bcdfghjklmnpqrstuvwxz",
	"ib", "jqx",
	"if", "jqvwz",
	"ih", "bgjqx",
	"ii", "bjqxy",
	"ij", "cfgqxy",
	"ik", "bcfqx",
	"iq", "cdefgjkmnopqtvxyz",
	"iu", "hiwxy",
	"iv", "cfgmqx",
	"iw", "dgjkmnpqtvxz",
	"ix", "jkqrxz",
	"iy", "bcdfghjklpqtvwx",
	"jb", "bcdghjklmnopqrtuvwxyz",
	"jc", "cfgjkmnopqvwxy",
	"jd", "cdfghjlmnpqrtvwx",
	"jf", "abcdfghjlnopqrtuvwxyz",
	"jg", "bcdfghijklmnopqstuvwxyz",
	"jh", "bcdfghjklmnpqrxyz",
	"jj", "bcdfghjklmnopqrstuvwxyz",
	"jk", "bcdfghjknqrtwxyz",
	"jl", "bcfghjmnpqrstuvwxyz",
	"jm", "bcdfghiklmnqrtuvwyz",
	"jn", "bcfjlmnpqsuvwxz",
	"jp", "bcdfhijkmpqstvwxyz",
	"jq", "abcdefghijklmnopqrstuvwxyz",
	"jr", "bdfhjklpqrstuvwxyz",
	"js", "bfgjmoqvxyz",
	"jt", "bcdfghjlnpqrtvwxz",
	"jv", "abcdfghijklpqrstvwxyz",
	"jw", "bcdefghijklmpqrstuwxyz",
	"jx", "abcdefghijklmnopqrstuvwxyz",
	"jy", "bcdefghjkpqtuvwxyz",
	"jz", "bcdfghijklmnopqrstuvwxyz",
	"kb", "bcdfghjkmqvwxz",
	"kc", "cdfgjknpqtwxz",
	"kd", "bfghjklmnpqsvwxyz",
	"kf", "bdfghjkmnpqsvwxyz",
	"kg", "cghjkmnqtvwxyz",
	"kh", "cfghjkqx",
	"kj", "bcdfghjkmnpqrstwxyz",
	"kk", "bcdfgjmpqswxz",
	"kl", "cfghlmqstwxz",
	"km", "bdfghjknqrstwxyz",
	"kn", "bcdfhjklmnqsvwxz",
	"kp", "bdfgjkmpqvxyz",
	"kq", "abdefghijklmnopqrstvwxyz",
	"kr", "bcdfghjmqrvwx",
	"ks", "jqx",
	"kt", "cdfjklqvx",
	"ku", "qux",
	"kv", "bcfghjklnpqrstvxyz",
	"kw", "bcdfgjklmnpqsvwxz",
	"kx", "abcdefghjklmnopqrstuvwxyz",
	"ky", "vxy",
	"kz", "bcdefghjklmnpqrstuvwxyz",
	"lb", "cdgkqtvxz",
	"lc", "bqx",
	"lg", "cdfgpqvxz",
	"lh", "cfghkmnpqrtvx",
	"lk", "qxz",
	"ln", "cfjqxz",
	"lp", "jkqxz",
	"lq", "bcdefhijklmopqrstvwxyz",
	"lr", "dfgjklmpqrtvwx",
	"lv", "bcfhjklmpwxz",
	"lw", "bcdfgjknqxz",
	"lx", "bcdfghjklmnpqrtuwz",
	"lz", "cdjptvxz",
	"mb", "qxz",
	"md", "hjkpvz",
	"mf", "fkpqvwxz",
	"mg", "cfgjnpqsvwxz",
	"mh", "bchjkmnqvx",
	"mj", "bcdfghjknpqrstvwxyz",
	"mk", "bcfgklmnpqrvwxz",
	"ml", "jkqz",
	"mm", "qvz",
	"mn", "fhjkqxz",
	"mq", "bdefhjklmnopqtwxyz",
	"mr", "jklqvwz",
	"mt", "jkq",
	"mv", "bcfghjklmnqtvwxz",
	"mw", "bcdfgjklnpqsuvwxyz",
	"mx", "abcefghijklmnopqrstvwxyz",
	"mz", "bcdfghjkmnpqrstvwxz",
	"nb", "hkmnqxz",
	"nf", "bghqvxz",
	"nh", "fhjkmqtvxz",
	"nk", "qxz",
	"nl", "bghjknqvwxz",
	"nm", "dfghjkqtvwxz",
	"np", "bdjmqwxz",
	"nq", "abcdfghjklmnopqrtvwxyz",
	"nr", "bfjkqstvx",
	"nv", "bcdfgjkmnqswxz",
	"nw", "dgjpqvxz",
	"nx", "abfghjknopuyz",
	"nz", "cfqrxz",
	"oc", "fjvw",
	"og", "qxz",
	"oh", "fqxz",
	"oj", "bfhjmqrswxyz",
	"ok", "qxz",
	"oq", "bcdefghijklmnopqrstvwxyz",
	"ov", "bfhjqwx",
	"oy", "qxy",
	"oz", "fjpqtvx",
	"pb", "fghjknpqvwz",
	"pc", "gjq",
	"pd", "bgjkvwxz",
	"pf", "hjkmqtvwyz",
	"pg", "bdfghjkmqsvwxyz",
	"ph", "kqvx",
	"pk", "bcdfhjklmpqrvx",
	"pl", "ghkqvwx",
	"pm", "bfhjlmnqvwyz",
	"pn", "fjklmnqrtvwz",
	"pp", "gqwxz",
	"pq", "abcdefghijklmnopqstvwxyz",
	"pr", "hjkqrwx",
	"pt", "jqxz",
	"pv", "bdfghjklquvwxyz",
	"pw", "fjkmnpqsuvwxz",
	"px", "abcdefghijklmnopqrstuvwxyz",
	"pz", "bdefghjklmnpqrstuvwxyz",
	"qa", "ceghkopqxy",
	"qb", "bcdfghjklmnqrstuvwxyz",
	"qc", "abcdfghijklmnopqrstuvwxyz",
	"qd", "defghijklmpqrstuvwxyz",
	"qe", "abceghjkmopquwxyz",
	"qf", "abdfghijklmnopqrstuvwxyz",
	"qg", "abcdefghijklmnopqrtuvwxz",
	"qh", "abcdefghijklmnopqrstuvwxyz",
	"qi", "efgijkmpwx",
	"qj", "abcdefghijklmnopqrstuvwxyz",
	"qk", "abcdfghijklmnopqrsuvwxyz",
	"ql", "abcefghjklmnopqrtuvwxyz",
	"qm", "bdehijklmnoqrtuvxyz",
	"qn", "bcdefghijklmnoqrtuvwxyz",
	"qo", "abcdefgijkloqstuvwxyz",
	"qp", "abcdefghijkmnopqrsuvwxyz",
	"qq", "bcdefghijklmnopstwxyz",
	"qr", "bdefghijklmnoqruvwxyz",
	"qs", "bcdefgijknqruvwxz",
	"qt", "befghjklmnpqtuvwxz",
	"qu", "cfgjkpwz",
	"qv", "abdefghjklmnopqrtuvwxyz",
	"qw", "bcdfghijkmnopqrstuvwxyz",
	"qx", "abcdefghijklmnopqrstuvwxyz",
	"qy", "abcdefghjklmnopqrstuvwxyz",
	"qz", "abcdefghijklmnopqrstuvwxyz",
	"rb", "fxz",
	"rg", "jvxz",
	"rh", "hjkqrxz",
	"rj", "bdfghjklmpqrstvwxz",
	"rk", "qxz",
	"rl", "jnq",
	"rp", "jxz",
	"rq", "bcdefghijklmnopqrtvwxy",
	"rr", "jpqxz",
	"rv", "bcdfghjmpqrvwxz",
	"rw", "bfgjklqsvxz",
	"rx", "bcdfgjkmnopqrtuvwxz",
	"rz", "djpqvxz",
	"sb", "kpqtvxz",
	"sd", "jqxz",
	"sf", "bghjkpqw",
	"sg", "cgjkqvwxz",
	"sj", "bfghjkmnpqrstvwxz",
	"sk", "qxz",
	"sl", "gjkqwxz",
	"sm", "fkqwxz",
	"sn", "dhjknqvwxz",
	"sq", "bfghjkmopstvwxz",
	"sr", "jklqrwxz",
	"sv", "bfhjklmnqtwxyz",
	"sw", "jkpqvwxz",
	"sx", "bcdefghjklmnopqrtuvwxyz",
	"sy", "qxy",
	"sz", "bdfgjpqsvxz",
	"tb", "cghjkmnpqtvwx",
	"tc", "jnqvx",
	"td", "bfgjkpqtvxz",
	"tf", "ghjkqvwyz",
	"tg", "bdfghjkmpqsx",
	"tj", "bdfhjklmnpqstvwxyz",
	"tk", "bcdfghjklmpqvwxz",
	"tl", "jkqwxz",
	"tm", "bknqtwxz",
	"tn", "fhjkmqvwxz",
	"tp", "bjpqvwxz",
	"tq", "abdefhijklmnopqrstvwxyz",
	"tr", "gjqvx",
	"tv", "bcfghjknpquvwxz",
	"tw", "bcdfjknqvz",
	"tx", "bcdefghjklmnopqrsuvwxz",
	"tz", "jqxz",
	"uc", "fjmvx",
	"uf", "jpqvx",
	"ug", "qvx",
	"uh", "bcgjkpvxz",
	"uj", "wbfghklmqvwx",
	"uk", "fgqxz",
	"uq", "bcdfghijklmnopqrtwxyz",
	"uu", "fijkqvwyz",
	"uv", "bcdfghjkmpqtwxz",
	"uw", "dgjnquvxyz",
	"ux", "jqxz",
	"uy", "jqxyz",
	"uz", "fgkpqrx",
	"vb", "bcdfhijklmpqrtuvxyz",
	"vc", "bgjklnpqtvwxyz",
	"vd", "bdghjklnqvwxyz",
	"vf", "bfghijklmnpqtuvxz",
	"vg", "bcdgjkmnpqtuvwxyz",
	"vh", "bcghijklmnpqrtuvwxyz",
	"vj", "abcdfghijklmnpqrstuvwxyz",
	"vk", "bcdefgjklmnpqruvwxyz",
	"vl", "hjkmpqrvwxz",
	"vm", "bfghjknpquvxyz",
	"vn", "bdhjkmnpqrtuvwxz",
	"vp", "bcdeghjkmopqtuvwyz",
	"vq", "abcdefghijklmnopqrstvwxyz",
	"vr", "fghjknqrtvwxz",
	"vs", "dfgjmqz",
	"vt", "bdfgjklmnqtx",
	"vu", "afhjquwxy",
	"vv", "cdfghjkmnpqrtuwxz",
	"vw", "abcdefghijklmnopqrtuvwxyz",
	"vx", "abcefghjklmnopqrstuvxyz",
	"vy", "oqx",
	"vz", "abcdefgjklmpqrstvwxyz",
	"wb", "bdfghjpqtvxz",
	"wc", "bdfgjkmnqvwx",
	"wd", "dfjpqvxz",
	"wf", "cdghjkmqvwxyz",
	"wg", "bcdfgjknpqtvwxyz",
	"wh", "cdghjklpqvwxz",
	"wj", "bfghijklmnpqrstvwxyz",
	"wk", "cdfgjkpqtuvxz",
	"wl", "jqvxz",
	"wm", "dghjlnqtvwxz",
	"wp", "dfgjkpqtvwxz",
	"wq", "abcdefghijklmnopqrstvwxyz",
	"wr", "cfghjlmpqwx",
	"wt", "bdgjlmnpqtvx",
	"wu", "aikoquvwy",
	"wv", "bcdfghjklmnpqrtuvwxyz",
	"ww", "bcdgkpqstuvxyz",
	"wx", "abcdefghijklmnopqrstuvwxz",
	"wy", "jquwxy",
	"wz", "bcdfghjkmnopqrstuvwxz",
	"xa", "ajoqy",
	"xb", "bcdfghjkmnpqsvwxz",
	"xc", "bcdgjkmnqsvwxz",
	"xd", "bcdfghjklnpqstuvwxyz",
	"xf", "bcdfghjkmnpqtvwxyz",
	"xg", "bcdfghjkmnpqstvwxyz",
	"xh", "cdfghjkmnpqrstvwxz",
	"xi", "jkqy",
	"xj", "abcdefghijklmnopqrstvwxyz",
	"xk", "abcdfghjkmnopqrstuvwxyz",
	"xl", "bcdfghjklmnpqrvwxz",
	"xm", "bcdfghjknpqvwxz",
	"xn", "bcdfghjklmnpqrvwxyz",
	"xp", "bcfjknpqvxz",
	"xq", "abcdefghijklmnopqrstvwxyz",
	"xr", "bcdfghjklnpqrsvwyz",
	"xs", "bdfgjmnqrsvxz",
	"xt", "jkpqvwxz",
	"xu", "fhjkquwx",
	"xv", "bcdefghjklmnpqrsuvwxyz",
	"xw", "bcdfghjklmnpqrtuvwxyz",
	"xx", "bcdefghjkmnpqrstuwyz",
	"xy", "jxy",
	"xz", "abcdefghjklmnpqrstuvwxyz",
	"yb", "cfghjmpqtvwxz",
	"yc", "bdfgjmpqsvwx",
	"yd", "chjkpqvwx",
	"yf", "bcdghjmnpqsvwx",
	"yg", "cfjkpqtxz",
	"yh", "bcdfghjkpqx",
	"yi", "hjqwxy",
	"yj", "bcdfghjklmnpqrstvwxyz",
	"yk", "bcdfgpqvwxz",
	"ym", "dfgjqvxz",
	"yp", "bcdfgjkmqxz",
	"yq", "abcdefghijklmnopqrstvwxyz",
	"yr", "jqx",
	"yt", "bcfgjnpqx",
	"yv", "bcdfghjlmnpqstvwxz",
	"yw", "bfgjklmnpqstuvwxz",
	"yx", "bcdfghjknpqrstuvwxz",
	"yy", "bcdfghjklpqrstvwxz",
	"yz", "bcdfjklmnpqtvwx",
	"zb", "dfgjklmnpqstvwxz",
	"zc", "bcdfgjmnpqstvwxy",
	"zd", "bcdfghjklmnpqstvwxy",
	"zf", "bcdfghijkmnopqrstvwxyz",
	"zg", "bcdfgjkmnpqtvwxyz",
	"zh", "bcfghjlpqstvwxz",
	"zj", "abcdfghjklmnpqrstuvwxyz",
	"zk", "bcdfghjklmpqstvwxz",
	"zl", "bcdfghjlnpqrstvwxz",
	"zm", "bdfghjklmpqstvwxyz",
	"zn", "bcdfghjlmnpqrstuvwxz",
	"zp", "bcdfhjklmnpqstvwxz",
	"zq", "abcdefghijklmnopqrstvwxyz",
	"zr", "bcfghjklmnpqrstvwxyz",
	"zs", "bdfgjmnqrsuwxyz",
	"zt", "bcdfgjkmnpqtuvwxz",
	"zu", "ajqx",
	"zv", "bcdfghjklmnpqrstuvwxyz",
	"zw", "bcdfghjklmnpqrstuvwxyz",
	"zx", "abcdefghijklmnopqrstuvwxyz",
	"zy", "fxy",
	"zz", "cdfhjnpqrvx",
	NULL, NULL
};

#ifndef _WIN32
/* Used for parsed sregexes */
typedef struct _regexlist RegexList;
struct _regexlist {
	RegexList *next;
	regex_t regex;
#ifdef DEBUGMODE
	char *regextxt;
#endif
	int score;
};
#endif

/* Used for parsed triples: */
#define TRIPLES_REST_SIZE	32
typedef struct _triples Triples;
struct _triples {
	Triples *next;
	char two[3];
	char rest[TRIPLES_REST_SIZE];
};

#ifndef _WIN32
RegexList *sregexes = NULL;
#endif
Triples *triples = NULL;

struct {
	int threshold;
	int ban_action;
	int ban_reason;
	int ban_time;
} req;

struct {
	int threshold;
	int ban_action;
	char *ban_reason;
	long ban_time;
	int convert_to_lowercase;
	int show_failedconnects;
	int fullstatus_on_load;
	ConfigItem_mask *except_hosts;
} cfg;

/* Forward declarations */
static int init_stuff(void);
static int init_sregexes(void);
static int init_triples(void);
static void free_stuff(void);
static void free_config(void);
int antirandom_config_test(ConfigFile *, ConfigEntry *, int, int *);
int antirandom_config_run(ConfigFile *, ConfigEntry *, int);
int antirandom_config_posttest(int *);
int antirandom_preconnect(aClient *sptr);
static int is_except_host(aClient *sptr);

MOD_TEST(antirandom)
{
	memset(&req, 0, sizeof(req));
	memset(&cfg, 0, sizeof(cfg));
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, antirandom_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, antirandom_config_posttest);
	return MOD_SUCCESS;
}

MOD_INIT(antirandom)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	if (!init_stuff())
	{
		config_error("antirandom: loading aborted");
		free_stuff();
		return MOD_FAILED;
	}
	cfg.fullstatus_on_load = 1; /* default */
	cfg.convert_to_lowercase = 1; /* default */
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, 0, antirandom_preconnect);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, antirandom_config_run);

	return MOD_SUCCESS;
}

void check_all_users(void);

MOD_LOAD(antirandom)
{
	if (cfg.fullstatus_on_load)
		check_all_users();
	return MOD_SUCCESS;
}

MOD_UNLOAD(antirandom)
{
	free_stuff();
	free_config();
	return MOD_SUCCESS;
}

/* Sends a message to all (local) opers AND logs to the ircdlog (as LOG_ERROR) */
static void multi_log(char *fmt, ...)
{
va_list vl;
static char buf[2048];

	va_start(vl, fmt);
	vsnprintf(buf, sizeof(buf), fmt, vl);
	va_end(vl);
	
	sendto_realops("%s", buf);
	ircd_log(LOG_ERROR, "%s", buf);
}


static void free_config(void)
{
	safefree(cfg.ban_reason);
	unreal_delete_masks(cfg.except_hosts);
	memset(&cfg, 0, sizeof(cfg)); /* needed! */
}

int antirandom_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
int errors = 0;
ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::antirandom... */
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "antirandom"))
		return 0;
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_varname)
		{
			config_error("%s:%i: blank set::antirandom item",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
		} else
		if (!strcmp(cep->ce_varname, "except-hosts"))
		{
		} else
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: set::antirandom::%s with no value",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
		} else
		if (!strcmp(cep->ce_varname, "threshold"))
		{
			req.threshold = 1;
		} else
		if (!strcmp(cep->ce_varname, "ban-action"))
		{
			if (!banact_stringtoval(cep->ce_vardata) && strcmp(cep->ce_vardata, "warn"))
			{
				config_error("%s:%i: set::antirandom::ban-action: unknown action '%s'",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_vardata);
				errors++;
			} else
				req.ban_action = 1;
		} else
		if (!strcmp(cep->ce_varname, "ban-reason"))
		{
			req.ban_reason = 1;
		} else
		if (!strcmp(cep->ce_varname, "ban-time"))
		{
			req.ban_time = 1;
		} else
		if (!strcmp(cep->ce_varname, "convert-to-lowercase"))
		{
		} else
		if (!strcmp(cep->ce_varname, "fullstatus-on-load"))
		{
		} else
		if (!strcmp(cep->ce_varname, "show-failedconnects"))
		{
		} else
		{
			config_error("%s:%i: unknown directive set::antirandom::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
		}
	}
	*errs = errors;
	return errors ? -1 : 1;
}

int antirandom_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cep2;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::antirandom... */
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "antirandom"))
		return 0;
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "except-hosts"))
		{
			for (cep2 = cep->ce_entries; cep2; cep2 = cep2->ce_next)
				unreal_add_masks(&cfg.except_hosts, cep2);
		} else
		if (!strcmp(cep->ce_varname, "threshold"))
		{
			cfg.threshold = atoi(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "ban-action"))
		{
			cfg.ban_action = banact_stringtoval(cep->ce_vardata);
			if (!cfg.ban_action && strcmp(cep->ce_vardata, "warn"))
				cfg.ban_action = BAN_ACT_WARN;
		} else
		if (!strcmp(cep->ce_varname, "ban-reason"))
		{
			if (cfg.ban_reason)
				MyFree(cfg.ban_reason);
			cfg.ban_reason = strdup(cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "ban-time"))
		{
			cfg.ban_time = config_checkval(cep->ce_vardata, CFG_TIME);
		} else
		if (!strcmp(cep->ce_varname, "convert-to-lowercase"))
		{
			cfg.convert_to_lowercase = config_checkval(cep->ce_vardata, CFG_YESNO);
		}
		if (!strcmp(cep->ce_varname, "show-failedconnects"))
		{
			cfg.show_failedconnects = config_checkval(cep->ce_vardata, CFG_YESNO);
		} else
		if (!strcmp(cep->ce_varname, "fullstatus-on-load"))
		{
			cfg.fullstatus_on_load = config_checkval(cep->ce_vardata, CFG_YESNO);
		}
	}
	return 1;
}

int antirandom_config_posttest(int *errs)
{
int errors = 0;

	if (!req.threshold) { config_error("set::antirandom::threshold missing"); errors++; }
	if (!req.ban_action) { config_error("set::antirandom::ban-action missing"); errors++; }
	if (!req.ban_time) { config_error("set::antirandom::ban-time missing"); errors++; }
	if (!req.ban_reason) { config_error("set::antirandom::ban-reason missing"); errors++; }
	
	*errs = errors;
	return errors ? -1 : 1;
}

static int init_stuff(void)
{
	if (!init_sregexes() || !init_triples())
		return 0;
	return 1;
}

/** Initializes the sregexes regex list */
static int init_sregexes(void)
{
#ifndef _WIN32
ScoreTable *s = &regex_scores[0];
RegexList *e, *last=NULL;
int cnt=0, n;
char *res;

	for (s=&regex_scores[0]; s->regex; s++)
	{
		cnt++;
		e = MyMallocEx(sizeof(RegexList));
		/* validate regex */
		res = unreal_checkregex(s->regex, 0, 1);
		if (res)
		{
			config_error("init_sregexes: sregexes_txt contains invalid regex (nr %d): %s",
				cnt, res);
			return 0;
		}
		/* parse regex here (should go fine, checked above) */
		n = regcomp(&e->regex, s->regex, REG_ICASE|REG_EXTENDED);
		if (n)
		{
			/* should never happen (yes I'm too lazy to get the errormsg) */
			config_error("init_sregexes: weird regcomp() failure: item=%d, errorcode=%d, aborting...",
				cnt, n);
			return 0;
		}
#ifdef DEBUGMODE
		e->regextxt = strdup(s->regex);
#endif

		e->score = s->score;

		/* Append at end of list (to keep it in order, not importent yet, but..) */
		if (last)
			last->next = e;
		else
			sregexes = e; /*(head)*/
		last = e;
	}
#endif
	return 1;
}

/** Initializes the triples list. */
static int init_triples(void)
{
char **s;
Triples *e, *last=NULL;
int cnt=0;

	for (s=triples_txt; *s; *s++)
	{
		cnt++;
		e = MyMallocEx(sizeof(Triples));
		if (strlen(*s) > 2)
		{
			config_error("init_triples: error parsing triples_txt, cnt=%d, item='%s' (length>2)",
				cnt, *s);
			return 0;
		}
		strcpy(e->two, *s); /* (SAFE) */
		*s++;
		if (!*s)
		{
			config_error("init_triples: error parsing triples_txt, cnt=%d, got NULL expected param",
				cnt);
			return 0;
		}
		if (strlen(*s) > TRIPLES_REST_SIZE-1)
		{
			config_error("init_triples: error parsing triples_txt, cnt=%d, item='%s' (length>%d)",
				cnt, *s, TRIPLES_REST_SIZE-1);
			return 0;
		}
		strcpy(e->rest, *s); /* (SAFE) */

		/* Append at end of list (to keep it in order, not importent yet, but..) */
		if (last)
			last->next = e;
		else
			triples = e; /*(head)*/
		last = e;
	}
	return 1;
}

/** Run the actual tests over this string.
 * There are 3 tests:
 * - weird chars (not used)
 * - sregexes (easy stuff)
 * - triples (three-letter combinations)
 */
static int internal_getscore(char *str)
{
#ifndef _WIN32
RegexList *r;
#endif
Triples *t;
register char *s;
int score = 0;
int highest_vowels=0, highest_consonants=0, highest_digits=0;
int vowels=0, consonants=0, digits=0;

#ifndef _WIN32
	for (r=sregexes; r; r=r->next)
	{
		if (!regexec(&r->regex, str, 0, NULL, 0))
		{
			score += r->score; /* note: in the draft this returns the # of occurances, not 1 */
#ifdef DEBUGMODE
			multi_log("score@'%s': MATCH for '%s'", str, r->regextxt);
#endif
		}
	}
#endif

	/* Fast digit/consonant/vowel checks... */
	for (s=str; *s; s++)
	{
		if ((*s >= '0') && (*s <= '9'))
			digits++;
		else {
			highest_digits = MAX(highest_digits, digits);
			digits = 0;
		}
		if (strchr("bcdfghjklmnpqrstvwxz", *s))
			consonants++;
		else {
			highest_consonants = MAX(highest_consonants, consonants);
			consonants = 0;
		}
		if (strchr("aeiou", *s))
			vowels++;
		else {
			highest_vowels = MAX(highest_vowels, vowels);
			vowels = 0;
		}
	}

	digits = MAX(highest_digits, digits);
	consonants = MAX(highest_consonants, consonants);
	vowels = MAX(highest_vowels, vowels);
	
	if (digits >= 5)
	{
		score += 5 + (digits - 5);
#ifdef DEBUGMODE
		multi_log("score@'%s': MATCH for digits check", str);
#endif
	}
	if (vowels >= 4)
	{
		score += 4 + (vowels - 4);
#ifdef DEBUGMODE
		multi_log("score@'%s': MATCH for vowels check", str);
#endif
	}
	if (consonants >= 4)
	{
		score += 4 + (consonants - 4);
#ifdef DEBUGMODE
		multi_log("score@'%s': MATCH for consonants check", str);
#endif
	}
	
	for (t=triples; t; t=t->next)
	{
		for (s=str; *s; s++)
			if ((t->two[0] == s[0]) && (t->two[1] == s[1]) && s[2] && strchr(t->rest, s[2]))
			{
				score++; /* OK */
#ifdef DEBUGMODE
				multi_log("score@'%s': MATCH for '%s[%s]' %c/%c/%c", str, t->two, t->rest,
					s[0], s[1], s[2]);
#endif
			}
	}

	
	
	return score;
}

void strtolower_safe(char *dst, char *src, int size)
{
int i;

	if (!size)
		return; /* size of 0 is unworkable */
	size--; /* for \0 */
	
	for (; *src && size; src++)
	{
		*dst++ = tolower(*src);
		size--;
	}
	*dst = '\0';
}

/** Returns "spam score".
 * @note a user is expected, do not call for anything else (eg: servers)
 */
static int get_spam_score(aClient *sptr)
{
char *nick = sptr->name;
char *user = sptr->user->username;
char *gecos = sptr->info;
char nbuf[NICKLEN+1], ubuf[USERLEN+1], rbuf[REALLEN+1];
int nscore, uscore, gscore, score;
#ifdef TIMING
struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif

	if (cfg.convert_to_lowercase)
	{
		strtolower_safe(nbuf, nick, sizeof(nbuf));
		strtolower_safe(ubuf, user, sizeof(ubuf));
		strtolower_safe(rbuf, gecos, sizeof(rbuf));
		nick = nbuf;
		user = ubuf;
		gecos = rbuf;
	}

	nscore = internal_getscore(nick);
	uscore = internal_getscore(user);
	gscore = internal_getscore(gecos);
	score = nscore + uscore + gscore;

#ifdef TIMING
	gettimeofday(&tv_beta, NULL);
	ircd_log(LOG_ERROR, "AntiRandom Timing: %ld microseconds",
		((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec));
#endif
#ifdef DEBUGMODE
	multi_log("got score: %d/%d/%d = %d",
		nscore, uscore, gscore, score);
#endif

	return score;
}

void check_all_users(void)
{
	aClient *acptr;
	int i, matches=0, score;
	
	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (IsPerson(acptr))
		{
			if (is_except_host(acptr))
				continue;

			score = get_spam_score(acptr);
			if (score > cfg.threshold)
			{
				if (!matches)
					sendto_realops("[antirandom] Full status report follows:");
				sendto_realops("%d points: %s!%s@%s:%s",
					score, acptr->name, acptr->user->username, acptr->user->realhost, acptr->info);
				matches++;
			}
		}
	}
	if (matches)
		sendto_realops("[antirandom] %d match%s", matches, matches == 1 ? "" : "es");
}

int antirandom_preconnect(aClient *sptr)
{
int score;

	if (!is_except_host(sptr))
	{
		score = get_spam_score(sptr);
		if (score > cfg.threshold)
		{
			if (cfg.ban_action == BAN_ACT_WARN)
			{
				multi_log("[antirandom] would have denied access to user with score %d: %s!%s@%s:%s",
					score, sptr->name, sptr->user->username, sptr->user->realhost, sptr->info);
				return 0;
			}
			if (cfg.show_failedconnects)
				multi_log("[antirandom] denied access to user with score %d: %s!%s@%s:%s",
					score, sptr->name, sptr->user->username, sptr->user->realhost, sptr->info);
			return place_host_ban(sptr, cfg.ban_action, cfg.ban_reason, cfg.ban_time);
		}
	}
	return 0;
}

static void free_stuff(void)
{
#ifndef _WIN32
RegexList *r, *r_next;
#endif
Triples *t, *t_next;

#ifndef _WIN32
	for (r=sregexes; r; r=r_next)
	{
		r_next = r->next;
		regfree(&r->regex);
#ifdef DEBUGMODE
		if (r->regextxt)
			MyFree(r->regextxt);
#endif
		MyFree(r);
	}
	sregexes = NULL;
#endif

	for (t=triples; t; t=t_next)
	{
		t_next = t->next;
		MyFree(t);
	}
	triples = NULL;
}

/** Finds out if the host is on the except list. 1 if yes, 0 if no */
static int is_except_host(aClient *sptr)
{
	return unreal_mask_match(sptr, cfg.except_hosts);
}
