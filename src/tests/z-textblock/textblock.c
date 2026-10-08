/* z-quark/quark.c */

#include "unit-test.h"
#include "z-color.h"
#include "z-textblock.h"
#include "z-util.h"

/**
 * Minimal UTF-8 to wide character conversion, installed as text_mbcs_hook so
 * tests of multibyte handling do not depend on the locale.
 * As mbstowcs(), the returned count does not include the terminating null.
 */
static size_t test_utf8_mbstowcs(wchar_t *dest, const char *src, int n)
{
	const unsigned char *s = (const unsigned char *)src;
	size_t count = 0;

	while (*s) {
		wchar_t wc;
		int extra, i;

		if (*s < 0x80) {
			wc = (wchar_t)*s;
			extra = 0;
		} else if ((*s & 0xE0) == 0xC0) {
			wc = (wchar_t)(*s & 0x1F);
			extra = 1;
		} else if ((*s & 0xF0) == 0xE0) {
			wc = (wchar_t)(*s & 0x0F);
			extra = 2;
		} else if ((*s & 0xF8) == 0xF0) {
			wc = (wchar_t)(*s & 0x07);
			extra = 3;
		} else {
			return (size_t)-1;
		}

		s++;
		for (i = 0; i < extra; i++) {
			if ((*s & 0xC0) != 0x80) return (size_t)-1;
			wc = (wchar_t)((wc << 6) | (*s & 0x3F));
			s++;
		}

		if (dest) {
			if ((int)count >= n) break;
			dest[count] = wc;
		}
		count++;
	}

	if (dest) dest[count] = L'\0';

	return count;
}

int setup_tests(void **state) {
	text_mbcs_hook = test_utf8_mbstowcs;
	ok;
}

int teardown_tests(void *state) {
	text_mbcs_hook = NULL;
	ok;
}

static int test_alloc(void *state) {
	textblock *tb = textblock_new();

	require(tb);

	textblock_free(tb);

	ok;
}

static int test_append(void *state) {
	textblock *tb = textblock_new();

	require(!wcscmp(textblock_text(tb), L""));

	textblock_append(tb, "Hello");
	require(!wcscmp(textblock_text(tb), L"Hello"));

	textblock_append(tb, "%d", 20);
	require(!wcscmp(textblock_text(tb), L"Hello20"));

	textblock_free(tb);

	ok;
}

static int test_append_utf8(void *state) {
	textblock *tb = textblock_new();

	require(tb);

	/* A complete multibyte character at the end has to be kept. */
	textblock_append(tb, "teletransportar hacia s\xC3\xAD");
	require(!wcscmp(textblock_text(tb),
		L"teletransportar hacia s" L"\x00ed"));
	textblock_append(tb, ", drenar man\xC3\xA1");
	require(!wcscmp(textblock_text(tb),
		L"teletransportar hacia s" L"\x00ed" L", drenar man" L"\x00e1"));

	/* An incomplete sequence at the end still has to be discarded. */
	textblock_append(tb, " \xC3");
	require(!wcscmp(textblock_text(tb),
		L"teletransportar hacia s" L"\x00ed" L", drenar man" L"\x00e1"
		L" "));

	textblock_free(tb);

	ok;
}

static int test_colour(void *state) {
	textblock *tb = textblock_new();

	const char text[] = "two";
	const uint8_t attrs[] = { COLOUR_L_GREEN, COLOUR_L_GREEN, COLOUR_L_GREEN };

	textblock_append_c(tb, COLOUR_L_GREEN, text);

	require(!memcmp(textblock_attrs(tb), attrs, 3));

	textblock_free(tb);

	ok;
}

static int test_length(void *state) {
	textblock *tb = textblock_new();

	const char text[] = "1234567";
	const wchar_t test_text[] = L"1234567";
	int i;

	const wchar_t *tb_text;

	/* Add it 32 times to make sure that appending definitely works */
	for (i = 0; i < 32; i++) {
		textblock_append(tb, text);
	}

	/* Now make sure it's all right */
	tb_text = textblock_text(tb);
	for (i = 0; i < 32; i++) {
		int n = N_ELEMENTS(text) - 1;
		int offset = i * n;

	 	require(!wmemcmp(tb_text + offset, test_text, n));
	}

	textblock_free(tb);

	ok;
}

static int test_append_textblock(void *state) {
	const uint8_t attrs[] = { COLOUR_L_BLUE, COLOUR_L_BLUE, COLOUR_L_BLUE,
		COLOUR_L_GREEN, COLOUR_L_GREEN, COLOUR_L_GREEN, COLOUR_L_GREEN };
	textblock *tb1 = textblock_new();
	textblock *tb2 = textblock_new();

	textblock_append_c(tb1, COLOUR_L_BLUE, "Hey");
	textblock_append_c(tb2, COLOUR_L_GREEN, " you");
	textblock_append_textblock(tb1, tb2);
	require(!wcscmp(textblock_text(tb1), L"Hey you"));
	require(!memcmp(textblock_attrs(tb1), attrs, sizeof(attrs)));
	require(!wcscmp(textblock_text(tb2), L" you"));
	require(!memcmp(textblock_attrs(tb2), attrs + 3,
		sizeof(attrs) - 3 * sizeof(*attrs)));

	textblock_free(tb2);
	textblock_free(tb1);

	ok;
}

const char *suite_name = "z-textblock/textblock";
struct test tests[] = {
	{ "alloc", test_alloc },
	{ "append", test_append },
	{ "append_utf8", test_append_utf8 },
	{ "colour", test_colour },
	{ "length", test_length },
	{ "append_textblock", test_append_textblock },
	{ NULL, NULL }
};
