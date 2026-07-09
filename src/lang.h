/**
 * \file lang.h
 * \brief Internationalization (i18n) - gettext-style translation + unit display
 */

#ifndef INCLUDED_LANG_H
#define INCLUDED_LANG_H

#include <stddef.h>

/**
 * Initialize the translation system.
 * Loads lib/locale/<lang_code>.po  and lib/locale/units.ini from lib_dir.
 * For "en" (English), no .po file is loaded and _() returns the key unchanged.
 */
void lang_init(const char *lang_code, const char *lib_dir);

/**
 * Free all translation resources.
 */
void lang_cleanup(void);

/**
 * Return the translation for key, or key itself if not found.
 */
const char *lang_get(const char *key);

/**
 * Current language code ("en", "es", ...). Set by lang_init().
 */
extern char lang_current[8];

/**
 * Translate a string. In English mode returns the string unchanged.
 */
#define _(str) lang_get(str)

/* ------------------------------------------------------------------ */
/* Unit display configuration (loaded from lib/locale/units.ini)      */
/* ------------------------------------------------------------------ */

/* Weight (burden/overweight): internal = tenths of lbs */
extern char  lang_unit_weight[16];   /* display unit label, e.g. "kg" or "lb" */
extern long  lang_unit_weight_mult;  /* multiply internal by mult/div for tenths of display unit */
extern long  lang_unit_weight_div;

/* Character weight (player->wt): internal = lbs */
extern int   lang_unit_charweight_stones; /* 1 = use "Nst Mlb", 0 = decimal */
extern long  lang_unit_charweight_div;    /* for STONES: stones divisor (14) */
extern char  lang_unit_charweight[16];    /* for DECIMAL: unit label */
extern long  lang_unit_charweight_mult;   /* for DECIMAL: mult */
extern long  lang_unit_charweight_div2;   /* for DECIMAL: div (tenths of unit) */

/* Height (player->ht): internal = inches */
extern int   lang_unit_height_ftin;  /* 1 = "N'M\"" format, 0 = decimal */
extern long  lang_unit_height_ftdiv; /* for FTIN: feet divisor (12) */
extern char  lang_unit_height[16];   /* for DECIMAL: unit label */
extern long  lang_unit_height_mult;  /* for DECIMAL: mult */
extern long  lang_unit_height_div;   /* for DECIMAL: div (gives integer cm) */

/* Infravision (see_infra): internal = 10-foot units */
extern char  lang_unit_infra[16];    /* display unit label */
extern long  lang_unit_infra_mult;
extern long  lang_unit_infra_div;
extern int   lang_unit_infra_dec;    /* 0 = integer, 1 = one decimal */

/* ------------------------------------------------------------------ */
/* Format helpers                                                      */
/* ------------------------------------------------------------------ */

/**
 * Format weight from tenths of lbs into buf (e.g. "0.7 lb" or "0.3 kg").
 * For negative values (overweight), prepend '-'.
 */
void lang_fmt_weight(char *buf, size_t max, int tenths_lbs);

/**
 * Format character weight from lbs into buf.
 * English: "6st 0lb"   Spanish: "38.0 kg"
 */
void lang_fmt_charweight(char *buf, size_t max, int lbs);

/**
 * Format height from inches into buf.
 * English: "6'0\""     Spanish: "182 cm"
 */
void lang_fmt_height(char *buf, size_t max, int inches);

/**
 * Format infravision from 10-foot units into buf.
 * English: "30 ft"     Spanish: "0.9 mt"
 */
void lang_fmt_infra(char *buf, size_t max, int ten_ft_units);

#endif /* INCLUDED_LANG_H */
