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

ModuleHeader MOD_HEADER
  = {
	"antirandom",
	"1.5",
	"Detect and ban users with random names",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

#ifndef MAX
 #define MAX(x,y) ((x) > (y) ? (x) : (y))
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

/* Used for parsed triples: */
#define TRIPLES_REST_SIZE	32
typedef struct Triples Triples;
struct Triples {
	Triples *next;
	char two[3];
	char rest[TRIPLES_REST_SIZE];
};

Triples *triples = NULL;

struct {
	int threshold;
	int ban_action;
	int ban_reason;
	int ban_time;
} req;

struct {
	int threshold;
	BanAction *ban_action;
	char *ban_reason;
	long ban_time;
	int convert_to_lowercase;
	int show_failedconnects;
	SecurityGroup *except;
} cfg;

/* Forward declarations */
static int init_stuff(void);
static int init_triples(void);
static void free_stuff(void);
static void free_config(void);
int antirandom_config_test(ConfigFile *, ConfigEntry *, int, int *);
int antirandom_config_run(ConfigFile *, ConfigEntry *, int);
int antirandom_config_posttest(int *);
int antirandom_preconnect(Client *client);
static int is_exempt(Client *client);

MOD_TEST()
{
	memset(&req, 0, sizeof(req));
	memset(&cfg, 0, sizeof(cfg));
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, antirandom_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, antirandom_config_posttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	if (!init_stuff())
	{
		config_error("antirandom: loading aborted");
		free_stuff();
		return MOD_FAILED;
	}
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, 0, antirandom_preconnect);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, antirandom_config_run);

	/* Some default values: */
	cfg.convert_to_lowercase = 1;
	cfg.except = safe_alloc(sizeof(SecurityGroup));
	cfg.except->webirc = 1;

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	free_stuff();
	free_config();
	return MOD_SUCCESS;
}

static void free_config(void)
{
	safe_free(cfg.ban_reason);
	free_security_group(cfg.except);
	safe_free_all_ban_actions(cfg.ban_action);
	memset(&cfg, 0, sizeof(cfg)); /* needed! */
}

int antirandom_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::antirandom... */
	if (!ce || !ce->name || strcmp(ce->name, "antirandom"))
		return 0;
	
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "except"))
		{
			test_match_block(cf, cep, &errors);
		} else
		if (!strcmp(cep->name, "except-hosts"))
		{
		} else
		if (!strcmp(cep->name, "except-webirc"))
		{
			/* This should normally be UNDER the generic 'set::antirandom::%s with no value'
			 * stuff but I put it here because people may think it's a hostlist and then
			 * the error can be a tad confusing. -- Syzop
			 */
			if (!cep->value)
			{
				config_error("%s:%i: set::antirandom::except-webirc should be 'yes' or 'no'",
				             cep->file->filename, cep->line_number);
				errors++;
			}
		} else
		if (!cep->value)
		{
			config_error("%s:%i: set::antirandom::%s with no value",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
		} else
		if (!strcmp(cep->name, "threshold"))
		{
			req.threshold = 1;
		} else
		if (!strcmp(cep->name, "ban-action") || !strcmp(cep->name, "action"))
		{
			req.ban_action = 1;
			errors += test_ban_action_config(cep);
		} else
		if (!strcmp(cep->name, "ban-reason"))
		{
			req.ban_reason = 1;
		} else
		if (!strcmp(cep->name, "ban-time"))
		{
			req.ban_time = 1;
		} else
		if (!strcmp(cep->name, "convert-to-lowercase"))
		{
		} else
		if (!strcmp(cep->name, "show-failedconnects"))
		{
		} else
		{
			config_error("%s:%i: unknown directive set::antirandom::%s",
				cep->file->filename, cep->line_number, cep->name);
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
	if (!ce || !ce->name || strcmp(ce->name, "antirandom"))
		return 0;
	
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "except"))
		{
			conf_match_block(cf, cep, &cfg.except);
		} else
		if (!strcmp(cep->name, "except-hosts"))
		{
			/* backwards compatible with set::antirandom::except */
			for (cep2 = cep->items; cep2; cep2 = cep2->next)
				unreal_add_masks(&cfg.except->mask, cep2);
		} else
		if (!strcmp(cep->name, "except-webirc"))
		{
			/* backwards compatible with set::antirandom::except */
			cfg.except->webirc = config_checkval(cep->value, CFG_YESNO);
		} else
		if (!strcmp(cep->name, "threshold"))
		{
			cfg.threshold = atoi(cep->value);
		} else
		if (!strcmp(cep->name, "ban-action") || !strcmp(cep->name, "action"))
		{
			cfg.ban_action = parse_ban_action_config(cep);
		} else
		if (!strcmp(cep->name, "ban-reason"))
		{
			safe_strdup(cfg.ban_reason, cep->value);
		} else
		if (!strcmp(cep->name, "ban-time"))
		{
			cfg.ban_time = config_checkval(cep->value, CFG_TIME);
		} else
		if (!strcmp(cep->name, "convert-to-lowercase"))
		{
			cfg.convert_to_lowercase = config_checkval(cep->value, CFG_YESNO);
		}
		if (!strcmp(cep->name, "show-failedconnects"))
		{
			cfg.show_failedconnects = config_checkval(cep->value, CFG_YESNO);
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
	if (!init_triples())
		return 0;
	return 1;
}

/** Initializes the triples list. */
static int init_triples(void)
{
	char **s;
	Triples *e, *last=NULL;
	int cnt=0;

	for (s=triples_txt; *s; s++)
	{
		cnt++;
		e = safe_alloc(sizeof(Triples));
		if (strlen(*s) > 2)
		{
			config_error("init_triples: error parsing triples_txt, cnt=%d, item='%s' (length>2)",
				cnt, *s);
			return 0;
		}
		strcpy(e->two, *s); /* (SAFE) */
		s++;
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
 * - sregexes (not used)
 * - triples (three-letter combinations)
 */
static int internal_getscore(char *str)
{
	Triples *t;
	register char *s;
	int score = 0;
	int highest_vowels=0, highest_consonants=0, highest_digits=0;
	int vowels=0, consonants=0, digits=0;

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
		score += digits;
	}
	if (vowels >= 4)
	{
		score += vowels;
	}
	if (consonants >= 4)
	{
		score += consonants;
	}
	
	for (t=triples; t; t=t->next)
	{
		for (s=str; *s; s++)
			if ((t->two[0] == s[0]) && (t->two[1] == s[1]) && s[2] && strchr(t->rest, s[2]))
			{
				score++; /* OK */
			}
	}

	
	
	return score;
}

/** Returns "spam score".
 * @note a user is expected, do not call for anything else (eg: servers)
 */
static int get_spam_score(Client *client)
{
	char *nick = client->name;
	char *user = client->user->username;
	char *gecos = client->info;
	char nbuf[NICKLEN+1], ubuf[USERLEN+1], rbuf[REALLEN+1];
	int nscore, uscore, gscore, score;

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

	return score;
}

int antirandom_preconnect(Client *client)
{
	int score;

	if (is_exempt(client))
		return HOOK_CONTINUE;

	score = get_spam_score(client);
	if (score > cfg.threshold)
	{
		int n = take_action(client, cfg.ban_action, cfg.ban_reason, cfg.ban_time, 0);
		if ((n == BAN_ACT_WARN) || (n == BAN_ACT_SOFT_WARN))
		{
			unreal_log(ULOG_INFO, "antirandom", "ANTIRANDOM_DENIED_USER", client,
			           "[antirandom] would have denied access to user with score $score: $client.details:$client.user.realname",
			           log_data_integer("score", score));
		} else
		if (n <= 0)
		{
			/* No action / exempt */
		} else
		{
			if (cfg.show_failedconnects)
			{
				unreal_log(ULOG_INFO, "antirandom", "ANTIRANDOM_DENIED_USER", client,
					   "[antirandom] denied access to user with score $score: $client.details:$client.user.realname",
					   log_data_integer("score", score));
			}
			return HOOK_DENY;
		}
	}
	return HOOK_CONTINUE;
}

static void free_stuff(void)
{
	Triples *t, *t_next;

	for (t=triples; t; t=t_next)
	{
		t_next = t->next;
		safe_free(t);
	}
	triples = NULL;
}

/** Is this user exempt from antirandom interventions? */
static int is_exempt(Client *client)
{
	if (user_allowed_by_security_group(client, cfg.except))
		return 1;

	if (find_tkl_exception(TKL_ANTIRANDOM, client))
		return 1;

	/* Soft ban and logged in? */
	if (only_soft_actions(cfg.ban_action) && IsLoggedIn(client))
		return 1;

	return 0;
}
