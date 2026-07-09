/**
 * \file lang.c
 * \brief Internationalization - .po file loader + unit display configuration
 *
 * .po format subset:
 *   # comment
 *   msgid  "original english"
 *   msgstr "translation"
 *
 * units.ini format:
 *   # comment
 *   [locale]    <- section matching lang code
 *   Key=Value
 */

#include "angband.h"
#include "z-file.h"
#include "lang.h"

char lang_current[8] = "en";

/* ------------------------------------------------------------------ */
/* Translation table                                                   */
/* ------------------------------------------------------------------ */

struct trans_entry {
	char *msgid;
	char *msgstr;
};

static struct trans_entry *translations = NULL;
static int trans_count = 0;
static int trans_capacity = 0;

static char *unescape_po(const char *src)
{
	size_t len = strlen(src);
	char *dst = mem_alloc(len + 1);
	size_t i = 0, j = 0;

	while (i < len) {
		if (src[i] == '\\' && i + 1 < len) {
			i++;
			switch (src[i]) {
				case 'n':  dst[j++] = '\n'; break;
				case 't':  dst[j++] = '\t'; break;
				case '"':  dst[j++] = '"';  break;
				case '\\': dst[j++] = '\\'; break;
				default:   dst[j++] = '\\'; dst[j++] = src[i]; break;
			}
		} else {
			dst[j++] = src[i];
		}
		i++;
	}
	dst[j] = '\0';
	return dst;
}

static char *extract_quoted(const char *line)
{
	const char *start = strchr(line, '"');
	const char *end;
	char *raw, *result;
	size_t len;

	if (!start) return NULL;
	start++;
	end = strrchr(start, '"');
	if (!end || end <= start) return (*start == '"') ? string_make("") : NULL;

	len = (size_t)(end - start);
	if (len == 0) return string_make("");

	raw = mem_alloc(len + 1);
	memcpy(raw, start, len);
	raw[len] = '\0';
	result = unescape_po(raw);
	mem_free(raw);
	return result;
}

/* ------------------------------------------------------------------ */
/* Unit globals with defaults (English)                                */
/* ------------------------------------------------------------------ */

char  lang_unit_weight[16]     = "lb";
long  lang_unit_weight_mult    = 1;
long  lang_unit_weight_div     = 1;
int   lang_unit_charweight_stones = 1;
long  lang_unit_charweight_div    = 14;
char  lang_unit_charweight[16] = "lb";
long  lang_unit_charweight_mult   = 1;
long  lang_unit_charweight_div2   = 1;
int   lang_unit_height_ftin    = 1;
long  lang_unit_height_ftdiv   = 12;
char  lang_unit_height[16]     = "in";
long  lang_unit_height_mult    = 1;
long  lang_unit_height_div     = 1;
char  lang_unit_infra[16]      = "ft";
long  lang_unit_infra_mult     = 10;
long  lang_unit_infra_div      = 1;
int   lang_unit_infra_dec      = 0;

/* ------------------------------------------------------------------ */
/* units.ini loader                                                    */
/* ------------------------------------------------------------------ */

static void set_unit_str(char *field, size_t fsize, const char *val)
{
	my_strcpy(field, val, fsize);
}

static long parse_long(const char *val)
{
	return atol(val);
}

static void load_units(const char *lib_dir, const char *lang_code)
{
	char path[1024];
	char line[256];
	ang_file *f;
	bool in_section = false;
	char section[16] = "";

	path_build(path, sizeof(path), lib_dir, "locale");
	strnfmt(path, sizeof(path), "%s%sunits.ini", path, PATH_SEP);

	f = file_open(path, MODE_READ, FTYPE_TEXT);
	if (!f) return;

	while (file_getl(f, line, sizeof(line))) {
		char *p = line;

		/* Skip leading whitespace */
		while (*p == ' ' || *p == '\t') p++;

		/* Skip blank lines and comments */
		if (!*p || *p == '#' || *p == ';') continue;

		/* Section header [lang] */
		if (*p == '[') {
			char *end = strchr(p + 1, ']');
			if (end) {
				size_t len = (size_t)(end - p - 1);
				if (len >= sizeof(section)) len = sizeof(section) - 1;
				memcpy(section, p + 1, len);
				section[len] = '\0';
				in_section = (strcmp(section, lang_code) == 0);
			}
			continue;
		}

		if (!in_section) continue;

		/* Key=Value */
		char *eq = strchr(p, '=');
		if (!eq) continue;
		*eq = '\0';
		char *key = p;
		char *val = eq + 1;

		/* Trim trailing whitespace from key */
		char *kend = key + strlen(key) - 1;
		while (kend > key && (*kend == ' ' || *kend == '\t' || *kend == '\r')) *kend-- = '\0';

		/* Trim trailing newline/cr from val */
		char *vend = val + strlen(val) - 1;
		while (vend >= val && (*vend == '\r' || *vend == '\n' || *vend == ' ')) *vend-- = '\0';

		/* Apply key */
		if      (strcmp(key, "WeightUnit")          == 0) set_unit_str(lang_unit_weight, sizeof(lang_unit_weight), val);
		else if (strcmp(key, "WeightMult")          == 0) lang_unit_weight_mult = parse_long(val);
		else if (strcmp(key, "WeightDiv")           == 0) lang_unit_weight_div  = parse_long(val);
		else if (strcmp(key, "CharWeightFmt")       == 0) lang_unit_charweight_stones = (strcmp(val, "STONES") == 0) ? 1 : 0;
		else if (strcmp(key, "CharWeightStonesDiv") == 0) lang_unit_charweight_div    = parse_long(val);
		else if (strcmp(key, "CharWeightUnit")      == 0) set_unit_str(lang_unit_charweight, sizeof(lang_unit_charweight), val);
		else if (strcmp(key, "CharWeightMult")      == 0) lang_unit_charweight_mult   = parse_long(val);
		else if (strcmp(key, "CharWeightDiv")       == 0) lang_unit_charweight_div2   = parse_long(val);
		else if (strcmp(key, "HeightFmt")           == 0) lang_unit_height_ftin = (strcmp(val, "FTIN") == 0) ? 1 : 0;
		else if (strcmp(key, "HeightFtDiv")         == 0) lang_unit_height_ftdiv = parse_long(val);
		else if (strcmp(key, "HeightUnit")          == 0) set_unit_str(lang_unit_height, sizeof(lang_unit_height), val);
		else if (strcmp(key, "HeightMult")          == 0) lang_unit_height_mult = parse_long(val);
		else if (strcmp(key, "HeightDiv")           == 0) lang_unit_height_div  = parse_long(val);
		else if (strcmp(key, "InfraUnit")           == 0) set_unit_str(lang_unit_infra, sizeof(lang_unit_infra), val);
		else if (strcmp(key, "InfraMult")           == 0) lang_unit_infra_mult = parse_long(val);
		else if (strcmp(key, "InfraDiv")            == 0) lang_unit_infra_div  = parse_long(val);
		else if (strcmp(key, "InfraDecimals")       == 0) lang_unit_infra_dec  = (int)parse_long(val);
	}

	file_close(f);
}

/* ------------------------------------------------------------------ */
/* Format helpers                                                      */
/* ------------------------------------------------------------------ */

void lang_fmt_weight(char *buf, size_t max, int tenths_lbs)
{
	long val = (long)tenths_lbs * lang_unit_weight_mult / (lang_unit_weight_div ? lang_unit_weight_div : 1);
	int neg = (val < 0);
	if (neg) val = -val;
	strnfmt(buf, max, "%s%ld.%ld %s", neg ? "-" : "", val / 10, val % 10, lang_unit_weight);
}

void lang_fmt_charweight(char *buf, size_t max, int lbs)
{
	if (lang_unit_charweight_stones) {
		long div = lang_unit_charweight_div ? lang_unit_charweight_div : 14;
		strnfmt(buf, max, "%ldst %ldlb", (long)lbs / div, (long)lbs % div);
	} else {
		long val = (long)lbs * lang_unit_charweight_mult /
		           (lang_unit_charweight_div2 ? lang_unit_charweight_div2 : 1);
		strnfmt(buf, max, "%ld.%ld %s", val / 10, val % 10, lang_unit_charweight);
	}
}

void lang_fmt_height(char *buf, size_t max, int inches)
{
	if (lang_unit_height_ftin) {
		long div = lang_unit_height_ftdiv ? lang_unit_height_ftdiv : 12;
		strnfmt(buf, max, "%ld'%ld\"", (long)inches / div, (long)inches % div);
	} else {
		long val = (long)inches * lang_unit_height_mult /
		           (lang_unit_height_div ? lang_unit_height_div : 1);
		strnfmt(buf, max, "%ld %s", val, lang_unit_height);
	}
}

void lang_fmt_infra(char *buf, size_t max, int ten_ft_units)
{
	long val = (long)ten_ft_units * lang_unit_infra_mult /
	           (lang_unit_infra_div ? lang_unit_infra_div : 1);
	if (lang_unit_infra_dec) {
		strnfmt(buf, max, "%ld.%ld %s", val / 10, val % 10, lang_unit_infra);
	} else {
		strnfmt(buf, max, "%ld %s", val, lang_unit_infra);
	}
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void lang_init(const char *lang_code, const char *lib_dir)
{
	char locale_dir[1024];
	char po_file[1024];
	char line[4096];
	ang_file *f;
	char *pending_id = NULL;

	my_strcpy(lang_current, lang_code, sizeof(lang_current));

	/* Load unit config for this locale */
	load_units(lib_dir, lang_code);

	/* English is the source language — no .po file needed */
	if (strcmp(lang_code, "en") == 0) return;

	path_build(locale_dir, sizeof(locale_dir), lib_dir, "locale");
	strnfmt(po_file, sizeof(po_file), "%s%s%s.po", locale_dir, PATH_SEP, lang_code);

	f = file_open(po_file, MODE_READ, FTYPE_TEXT);
	if (!f) {
		plog_fmt("i18n: no se pudo abrir el archivo de idioma: %s", po_file);
		return;
	}

	trans_capacity = 512;
	translations = mem_alloc((size_t)trans_capacity * sizeof(struct trans_entry));
	trans_count = 0;

	while (file_getl(f, line, sizeof(line))) {
		if (line[0] == '\0' || line[0] == '#') continue;

		if (strncmp(line, "msgid ", 6) == 0) {
			mem_free(pending_id);
			pending_id = extract_quoted(line + 6);
		} else if (strncmp(line, "msgstr ", 7) == 0) {
			char *str = extract_quoted(line + 7);
			if (pending_id && str && str[0] != '\0') {
				if (trans_count >= trans_capacity) {
					trans_capacity *= 2;
					translations = mem_realloc(translations,
						(size_t)trans_capacity * sizeof(struct trans_entry));
				}
				translations[trans_count].msgid  = pending_id;
				translations[trans_count].msgstr = str;
				trans_count++;
				pending_id = NULL;
				str = NULL;
			}
			mem_free(str);
		}
	}

	mem_free(pending_id);
	file_close(f);
}

void lang_cleanup(void)
{
	int i;
	for (i = 0; i < trans_count; i++) {
		mem_free(translations[i].msgid);
		mem_free(translations[i].msgstr);
	}
	mem_free(translations);
	translations = NULL;
	trans_count = 0;
	trans_capacity = 0;
}

const char *lang_get(const char *key)
{
	int i;
	if (!key) return "";
	if (!translations) return key;
	for (i = 0; i < trans_count; i++) {
		if (strcmp(translations[i].msgid, key) == 0)
			return translations[i].msgstr;
	}
	return key;
}
