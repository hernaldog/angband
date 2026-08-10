/* monster/blows */

#include "unit-test.h"

#include "lang.h"
#include "mon-blows.h"
#include "z-virt.h"

static struct blow_message test_msg;
static struct blow_method test_method;

int setup_tests(void **state) {
	test_msg.act_msg = "{target} golpea";
	test_msg.next = NULL;
	test_method.messages = &test_msg;
	test_method.num_messages = 1;
	*state = NULL;
	return 0;
}

int teardown_tests(void *state) {
	return 0;
}

static int test_plural_action_spanish(void *state) {
	char *act;

	/*
	 * Run in Spanish.  With a plural subject the action string is looked up
	 * in the translation table for the plural verb form; without a table
	 * loaded here the singular form is kept as a fallback.
	 */
	my_strcpy(lang_current, "es", sizeof(lang_current));

	act = monster_blow_method_action(&test_method, -1, true);
	require(act);
	require(streq(act, "te golpea"));
	string_free(act);

	act = monster_blow_method_action(&test_method, -1, false);
	require(act);
	require(streq(act, "te golpea"));
	string_free(act);

	my_strcpy(lang_current, "en", sizeof(lang_current));
	ok;
}

const char *suite_name = "monster/blows";
struct test tests[] = {
	{ "plural action", test_plural_action_spanish },
	{ NULL, NULL },
};
