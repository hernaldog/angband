/**
 * \file mon-desc.c
 * \brief Monster description
 *
 * Copyright (c) 1997-2007 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "game-input.h"
#include "lang.h"
#include "mon-desc.h"
#include "mon-predicate.h"

/**
 * Heuristic for the Spanish grammatical gender of a monster's race name,
 * used to choose between "el " and "la ".  Monster names are typically
 * "noun word [word...]" (e.g. "serpiente blanca grande" or "hormiga
 * soldado"), and the leading noun's own ending is usually the most
 * reliable signal ("-a" feminine, "-o" masculine).  Only when that ending
 * is ambiguous (e.g. "serpiente") do we fall back to checking whether the
 * next word is an adjective that agrees in gender ("-o"/"-a"), or finally
 * to the monster race's RF_FEMALE flag; that
 * fallback must come second, since the second word is sometimes another
 * noun in apposition (e.g. "soldado" in "hormiga soldado") rather than an
 * agreeing adjective.  This is not perfect - Spanish has irregular nouns -
 * but covers the common cases in the bestiary.
 */
/**
 * Some monster names are loanwords that never take Spanish gender
 * agreement in their accompanying adjectives (e.g. "naga negro", "naga
 * rojo", never "naga negra"/"naga roja"), so neither "el"/"un" nor
 * "la"/"una" reads correctly for them. Definite and indefinite
 * descriptions of these monsters drop the article entirely.
 */
static bool monster_name_is_ungendered_es(const char *name)
{
	static const char *ungendered[] = { "naga", "monedas", "tengu", "snaga",
		"espíritu de la tierra" };
	size_t i;

	for (i = 0; i < N_ELEMENTS(ungendered); i++) {
		if (strchr(ungendered[i], ' ')) {
			/* Multi-word name: match it in full */
			if (my_stricmp(name, ungendered[i]) == 0) {
				return true;
			}
		} else {
			/* Single word: match the first noun of the name */
			const char *space = strchr(name, ' ');
			size_t noun_len = space ? (size_t) (space - name) : strlen(name);

			if (strlen(ungendered[i]) == noun_len
					&& my_strnicmp(name, ungendered[i],
						(int) noun_len) == 0) {
				return true;
			}
		}
	}
	return false;
}

static bool monster_name_is_feminine_es(const char *name,
		const bitflag *flags)
{
	/* Nouns ending in "-a" that are nonetheless masculine */
	static const char *masc_exceptions[] = {
		"fantasma", "ilusionista", "carterista", "idiota", "druida",
		"guardia", "patriarca"
	};
	/* Nouns ending in "-e" that are nonetheless feminine */
	static const char *fem_exceptions[] = { "serpiente" };
	/* Epicene nouns whose gender is set by the monster's flag */
	static const char *flag_governed[] = { "maia" };
	const char *space = strchr(name, ' ');
	size_t noun_len = space ? (size_t) (space - name) : strlen(name);
	size_t i;
	char noun_last;

	for (i = 0; i < N_ELEMENTS(masc_exceptions); i++) {
		if (strlen(masc_exceptions[i]) == noun_len
				&& my_strnicmp(name, masc_exceptions[i],
					(int) noun_len) == 0) {
			return false;
		}
	}

	for (i = 0; i < N_ELEMENTS(fem_exceptions); i++) {
		if (strlen(fem_exceptions[i]) == noun_len
				&& my_strnicmp(name, fem_exceptions[i],
					(int) noun_len) == 0) {
			return true;
		}
	}

	/* Epicene nouns defer to the race's gender flag */
	for (i = 0; i < N_ELEMENTS(flag_governed); i++) {
		if (strlen(flag_governed[i]) == noun_len
				&& my_strnicmp(name, flag_governed[i],
					(int) noun_len) == 0) {
			return (flags != NULL) && rf_has(flags, RF_FEMALE);
		}
	}

	/* The noun's own ending, when it is distinctly "-a" or "-o" */
	noun_last = (noun_len > 0) ?
		(char) tolower((unsigned char) name[noun_len - 1]) : '\0';
	if (noun_last == 'a') return true;
	if (noun_last == 'o') return false;

	/* Ambiguous noun ending: check the next word for gender agreement,
	 * when it is distinctly "-o" or "-a" (many words, like "verde" or
	 * "salvaje", don't change with gender and are not useful here). */
	if (space) {
		const char *adj = space + 1;
		const char *adj_space = strchr(adj, ' ');
		size_t adj_len = adj_space ?
			(size_t) (adj_space - adj) : strlen(adj);

		if (adj_len > 0) {
			char last = (char) tolower((unsigned char)
				adj[adj_len - 1]);

			if (last == 'a') return true;
			if (last == 'o') return false;
		}
	}

	/* Fallback to race flag when name is ambiguous */
	if (flags && rf_has(flags, RF_FEMALE))
		return true;

	/* Default */
	return false;
}

/**
 * Return true if a Spanish monster race name is grammatically plural, so
 * messages about it must use plural verb forms.  The head noun is the first
 * word of the name and a Spanish plural noun ends in "s".  A few invariable
 * or proper names also end in "s" but stay singular ("ciempiés",
 * "estegociempiés", "catoblepas").
 */
bool monster_name_is_plural_es(const char *name)
{
	static const char *singular_ending_in_s[] = {
		"catoblepas", "ciempiés", "estegociempiés"
	};
	const char *space = strchr(name, ' ');
	size_t noun_len = space ? (size_t) (space - name) : strlen(name);
	size_t i;

	if (noun_len == 0)
		return false;

	/* Plural nouns end in "s". */
	if (name[noun_len - 1] != 's')
		return false;

	/* Invariable names that end in "s" but stay singular. */
	for (i = 0; i < N_ELEMENTS(singular_ending_in_s); i++) {
		if (strlen(singular_ending_in_s[i]) == noun_len
				&& my_strnicmp(name, singular_ending_in_s[i],
					(int) noun_len) == 0) {
			return false;
		}
	}

	return true;
}

/**
 * In Spanish, return a pointer to the name with any leading article
 * ("el ", "la ", "un ", "una ") removed.  Used for messages that read
 * better with just the species name (e.g. "Has matado a Arquero Kobold").
 * In other languages the name is returned unchanged.
 */
const char *monster_name_strip_article_es(const char *name)
{
	static const char *articles[] = { "el ", "la ", "un ", "una " };
	size_t i;

	if (!streq(lang_current, "es")) {
		return name;
	}

	for (i = 0; i < N_ELEMENTS(articles); i++) {
		size_t alen = strlen(articles[i]);

		if (my_strnicmp(name, articles[i], (int) alen) == 0) {
			return name + alen;
		}
	}

	return name;
}

/**
 * In Spanish, capitalise the first letter of each word of a species
 * name (like a label), except for short particles ("de", "del", "la",
 * "el", ...) that stay in lower case.
 */
void es_species_name_title_case(char *name)
{
	static const char *particles[] = {
		"a", "al", "con", "de", "del", "e", "el", "en", "la", "las",
		"los", "o", "por", "un", "una", "y"
	};
	char *s = name;
	bool at_word_start = true;

	while (*s) {
		if (isspace((unsigned char) *s)) {
			at_word_start = true;
			s++;
			continue;
		}

		if (at_word_start) {
			char *end = s;
			size_t len;

			while (*end && !isspace((unsigned char) *end)) {
				end++;
			}
			len = (size_t) (end - s);

			/* Keep particles (except the first word) in lower case */
			if (s != name) {
				size_t i;

				for (i = 0; i < N_ELEMENTS(particles); i++) {
					if (strlen(particles[i]) == len
							&& my_strnicmp(s, particles[i],
								(int) len) == 0) {
						goto skip;
					}
				}
			}

			*s = toupper((unsigned char) *s);
		}

		skip:
		at_word_start = false;
		s++;
	}
}

/**
 * Write the Spanish plural of a single word to out.  Handles the common
 * Spanish pluralization rules used by monster names.  Returns the number of
 * bytes written to out.
 */
static size_t spanish_plural_word(const char *word, size_t wlen,
		char *out, size_t outmax)
{
	unsigned char last;

	if (wlen >= outmax)
		wlen = outmax - 1;
	memcpy(out, word, wlen);
	if (wlen == 0)
		return 0;

	/* Words ending in "s" are treated as invariable (e.g. ciempiés). */
	if (out[wlen - 1] == 's')
		return wlen;

	/* Words ending in "ll" (mostly English loanwords such as troll) keep
	 * the double ell and take a simple "s" in the plural (troll -> trolls). */
	if (wlen >= 2 && out[wlen - 2] == 'l' && out[wlen - 1] == 'l') {
		if (wlen + 1 >= outmax)
			return wlen;
		out[wlen] = 's';
		return wlen + 1;
	}

	/* Words ending in "z": pez -> peces. */
	if (out[wlen - 1] == 'z') {
		if (wlen + 2 >= outmax)
			return wlen;
		out[wlen - 1] = 'c';
		out[wlen] = 'e';
		out[wlen + 1] = 's';
		return wlen + 2;
	}

	/* Words ending in an accented vowel followed by "n" (dragón, capitán,
	 * avispón): drop the accent in the plural (dragones, capitanes,
	 * avispones). */
	if (wlen >= 3 && out[wlen - 1] == 'n'
			&& (unsigned char)out[wlen - 3] == 0xC3) {
		const char *plain = NULL;
		switch ((unsigned char)out[wlen - 2]) {
			case 0xA1: plain = "a"; break; /* á */
			case 0xAD: plain = "i"; break; /* í */
			case 0xB3: plain = "o"; break; /* ó */
			case 0xBA: plain = "u"; break; /* ú */
		}
		if (plain != NULL && wlen + 1 < outmax) {
			out[wlen - 3] = plain[0];
			out[wlen - 2] = 'n';
			out[wlen - 1] = 'e';
			out[wlen] = 's';
			return wlen + 1;
		}
	}

	/* Words ending in a vowel (accented or not): add "s" (mago -> magos,
	 * araña -> arañas, bebé -> bebés). */
	last = (unsigned char)out[wlen - 1];
	if (strchr("aeiou", out[wlen - 1]) != NULL
			|| (wlen >= 2 && (unsigned char)out[wlen - 2] == 0xC3
				&& (last == 0xA1 || last == 0xA9 || last == 0xAD
					|| last == 0xB3 || last == 0xBA))) {
		if (wlen + 1 >= outmax)
			return wlen;
		out[wlen] = 's';
		return wlen + 1;
	}

	/* Any other consonant: add "es" (acechador -> acechadores). */
	if (wlen + 2 >= outmax)
		return wlen;
	out[wlen] = 'e';
	out[wlen + 1] = 's';
	return wlen + 2;
}

/**
 * Perform simple pluralization on a monster name.
 *
 * English simply appends "s" (or "es") to the end of the name.  In Spanish
 * the head noun (first word) is also pluralized, while the last word keeps
 * the simple "s"/"es" suffix; names containing a "de"/"del" connector only
 * pluralize the head noun.
 */
void plural_aux(char *name, size_t max)
{
	size_t name_len = strlen(name);
	assert(name_len != 0);

	if (streq(lang_current, "es")) {
		char *space = strchr(name, ' ');
		if (space != NULL) {
			bool skip_tail = (strstr(name, " de ") != NULL)
					|| (strstr(name, " del ") != NULL);
			char head[128];
			size_t head_len = space - name;
			size_t plural_len = spanish_plural_word(name, head_len,
					head, sizeof(head));

			/* Make room for the pluralized head word. */
			memmove(name + plural_len, space, name_len - head_len + 1);
			memcpy(name, head, plural_len);
			name_len += plural_len - head_len;

			/* "x de y" / "x del y" names leave the tail alone. */
			if (skip_tail)
				return;
		}
	}

	/* Simple pluralization of the last word. */
	if (name[name_len - 1] == 's')
		my_strcat(name, "es", max);
	else
		my_strcat(name, "s", max);
}


/**
 * Helper function for display monlist.  Prints the number of creatures,
 * followed by either a singular or plural version of the race name as
 * appropriate.
 */
void get_mon_name(char *buf, size_t buflen,
				  const struct monster_race *race, int num)
{
	assert(race != NULL);

    /* Unique names don't have a number */
	if (rf_has(race->flags, RF_UNIQUE)) {
		strnfmt(buf, buflen, "[U] %s", race->name);
    } else {
	    strnfmt(buf, buflen, "%3d ", num);

	    if (num == 1) {
	        my_strcat(buf, race->name, buflen);
	    } else if (race->plural != NULL) {
	        my_strcat(buf, race->plural, buflen);
	    } else {
	        char race_name[128];
	        my_strcpy(race_name, race->name, sizeof(race_name));
	        plural_aux(race_name, sizeof(race_name));
	        my_strcat(buf, race_name, buflen);
	    }
    }
}

/**
 * Builds a string describing a monster in some way.
 *
 * We can correctly describe monsters based on their visibility.
 * We can force all monsters to be treated as visible or invisible.
 * We can build nominatives, objectives, possessives, or reflexives.
 * We can selectively pronominalize hidden, visible, or all monsters.
 * We can use definite or indefinite descriptions for hidden monsters.
 * We can use definite or indefinite descriptions for visible monsters.
 *
 * Pronominalization involves the gender whenever possible and allowed,
 * so that by cleverly requesting pronominalization / visibility, you
 * can get messages like "You hit someone.  She screams in agony!".
 *
 * Reflexives are acquired by requesting Objective plus Possessive.
 *
 * Note that "offscreen" monsters will get a special "(offscreen)"
 * notation in their name if they are visible but offscreen.  This
 * may look silly with possessives, as in "the rat's (offscreen)".
 * Perhaps the "offscreen" descriptor should be abbreviated.
 *
 * Mode Flags:
 *   0x01 --> Objective (or Reflexive)
 *   0x02 --> Possessive (or Reflexive)
 *   0x04 --> Use indefinites for hidden monsters ("something")
 *   0x08 --> Use indefinites for visible monsters ("a kobold")
 *   0x10 --> Pronominalize hidden monsters
 *   0x20 --> Pronominalize visible monsters
 *   0x40 --> Assume the monster is hidden
 *   0x80 --> Assume the monster is visible
 *  0x100 --> Capitalise monster name
 *  0x200 --> Add a comma if the name includes an unterminated phrase,
 *            "Wormtongue, Agent of Saruman" is an example
 *
 * Useful Modes:
 *   0x00 --> Full nominative name ("the kobold") or "it"
 *   0x04 --> Full nominative name ("the kobold") or "something"
 *   0x80 --> Banishment resistance name ("the kobold")
 *   0x88 --> Killing name ("a kobold")
 *   0x22 --> Possessive, genderized if visable ("his") or "its"
 *   0x23 --> Reflexive, genderized if visable ("himself") or "itself"
 */
void monster_desc(char *desc, size_t max, const struct monster *mon, int mode)
{
	assert(mon != NULL);

	/* Can we see it? (forced, or not hidden + visible) */
	bool seen = (mode & MDESC_SHOW) ||
		(!(mode & MDESC_HIDE) && monster_is_visible(mon));

	/* Sexed pronouns (seen and forced, or unseen and allowed) */
	bool use_pronoun = (seen && (mode & MDESC_PRO_VIS)) ||
			(!seen && (mode & MDESC_PRO_HID));

	/* First, try using pronouns, or describing hidden monsters */
	if (!seen || use_pronoun) {
		const char *choice = _("it");

		/* an encoding of the monster "sex" */
		int msex = 0x00;

		/* Extract the gender (if applicable) */
		if (use_pronoun) {
			if (rf_has(mon->race->flags, RF_FEMALE)) {
				msex = 0x20;
			} else if (rf_has(mon->race->flags, RF_MALE)) {
				msex = 0x10;
			}
		}

		/* Brute force: split on the possibilities */
		switch (msex + (mode & 0x07)) {
			/* Neuter */
			case 0x00: choice = _("it"); break;
			case 0x01: choice = _("it"); break;
			case 0x02: choice = _("its"); break;
			case 0x03: choice = _("itself"); break;
			case 0x04: choice = _("something"); break;
			case 0x05: choice = _("something"); break;
			case 0x06: choice = _("something's"); break;
			case 0x07: choice = _("itself"); break;

			/* Male */
			case 0x10: choice = _("he"); break;
			case 0x11: choice = _("him"); break;
			case 0x12: choice = _("his"); break;
			case 0x13: choice = _("himself"); break;
			case 0x14: choice = _("someone"); break;
			case 0x15: choice = _("someone"); break;
			case 0x16: choice = _("someone's"); break;
			case 0x17: choice = _("himself"); break;

			/* Female */
			case 0x20: choice = _("she"); break;
			case 0x21: choice = _("her"); break;
			case 0x22: choice = _("her"); break;
			case 0x23: choice = _("herself"); break;
			case 0x24: choice = _("someone"); break;
			case 0x25: choice = _("someone"); break;
			case 0x26: choice = _("someone's"); break;
			case 0x27: choice = _("herself"); break;
		}

		my_strcpy(desc, choice, max);
	} else if ((mode & MDESC_POSS) && (mode & MDESC_OBJE)) {
		/* The monster is visible, so use its gender */
		if (rf_has(mon->race->flags, RF_FEMALE))
			my_strcpy(desc, _("herself"), max);
		else if (rf_has(mon->race->flags, RF_MALE))
			my_strcpy(desc, _("himself"), max);
		else
			my_strcpy(desc, _("itself"), max);
	} else {
		const char *comma_pos;

		/* Unique, indefinite or definite */
		if (monster_is_shape_unique(mon)) {
			/* Start with the name (thus nominative and objective) */
			/*
			 * Strip off descriptive phrase if a possessive will be
			 * added.
			 */
			if ((mode & MDESC_POSS)
					&& rf_has(mon->race->flags, RF_NAME_COMMA)
					&& (comma_pos = strchr(mon->race->name, ','))
					&& comma_pos - mon->race->name < 1024) {
				strnfmt(desc, max, "%.*s",
					(int) (comma_pos - mon->race->name),
					mon->race->name);
			} else {
				my_strcpy(desc, mon->race->name, max);
			}
		} else {
			bool es = streq(lang_current, "es");
			char name_part[200];

			/*
			 * As with uniques, strip off phrase if a possessive
			 * will be added.
			 */
			if ((mode & MDESC_POSS)
					&& rf_has(mon->race->flags, RF_NAME_COMMA)
					&& (comma_pos = strchr(mon->race->name, ','))
					&& comma_pos - mon->race->name < (long)sizeof(name_part)) {
				strnfmt(name_part, sizeof(name_part), "%.*s",
					(int) (comma_pos - mon->race->name),
					mon->race->name);
			} else {
				my_strcpy(name_part, mon->race->name, sizeof(name_part));
			}

			/* Spanish species names are capitalised, like a label */
			if (es && name_part[0]) {
				es_species_name_title_case(name_part);
			}

			if (mode & MDESC_IND_VIS) {
				/* XXX Check plurality for "some" */
				/* Indefinite monsters need an indefinite article */
				if (es && monster_name_is_ungendered_es(mon->race->name)) {
					my_strcpy(desc, "", max);
				} else if (es) {
					bool feminine =
						monster_name_is_feminine_es(mon->race->name,
							mon->race->flags);
					my_strcpy(desc, feminine ? "una " : "un ", max);
				} else {
					my_strcpy(desc,
						is_a_vowel(mon->race->name[0]) ? "an " : "a ",
						max);
				}
			} else if (es && monster_name_is_ungendered_es(mon->race->name)) {
				my_strcpy(desc, "", max);
			} else if (es) {
				/* Definite monsters use a gender-matched article */
				bool feminine =
					monster_name_is_feminine_es(mon->race->name,
						mon->race->flags);
				my_strcpy(desc, feminine ? "la " : "el ", max);
			} else {
				my_strcpy(desc, "", max);
			}

			my_strcat(desc, name_part, max);
		}

		if ((mode & MDESC_COMMA)
				&& rf_has(mon->race->flags, RF_NAME_COMMA)) {
			my_strcat(desc, ",", max);
		}

		/* Handle the possessive */
		/* XXX Check for trailing "s" */
		if (mode & MDESC_POSS) {
			my_strcat(desc, "'s", max);
		}

		/* Mention "offscreen" monsters */
		if (!panel_contains(mon->grid.y, mon->grid.x)) {
			my_strcat(desc, _(" (offscreen)"), max);
		}
	}

	if (mode & MDESC_CAPITAL) {
		my_strcap(desc);
	}
}