#define LOG_TAG         "Selftest_ini"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sysfs.h"
#include "cts_selftest.h"
#include "cts_selftest_ini.h"
#include <linux/string.h>

static int read_line(char **ptr, char *line)
{
	int len = 0;
	char c;

	for (; **ptr == ' ' || **ptr == '\t' || **ptr == '#' || **ptr == '\r' || **ptr == '\n'; *ptr += 1)
		;

	if (**ptr == '\0') {
		return -EPERM;
	}

	while (**ptr != '\0') {
		c = **ptr;
		*ptr += 1;
		if (c != '\n' && c != '\r' && c != ' ') {
			len++;
			*line++ = c;
			*line = '\0';
		} else if (c == '\n') {
			break;
		}
	}

	return len;
}

int cts_add_ini_keyword(PT_SelftestData selftestdata, char *field, char *key, char *val)
{
	PT_KeyWord pkeyword;

	/*cts_info("cts_add_ini_keyword %s:%s:%s", field, key, val); */
	if (selftestdata->keyword_num >= CTS_KEYWORD_MAX_NUM) {
		cts_err("too much keyword");
		return -EPERM;
	}
	pkeyword = &selftestdata->keyword[selftestdata->keyword_num];
	strlcpy(pkeyword->field, field, CTS_KEYWORD_FIELD_MAXLEN - 1);
	strlcpy(pkeyword->key, key, CTS_KEYWORD_KEY_MAXLEN - 1);
	strlcpy(pkeyword->val, val, CTS_KEYWORD_VAL_MAXLEN - 1);

	selftestdata->keyword_num += 1;
	return 0;
}

int cts_show_ini_keyword(PT_SelftestData selftestdata)
{
	int i;

	for (i = 0; i < selftestdata->keyword_num; i++) {
		cts_info("No: %d field:%s, key:%s, val:%s", i,
			 selftestdata->keyword[i].field, selftestdata->keyword[i].key, selftestdata->keyword[i].val);
	}

	return 0;
}

int cts_init_keyword(PT_SelftestData selftestdata)
{
	char *ptr;
	char line[CTS_KEYWORD_LINE_MAXLEN];
	char field[CTS_KEYWORD_FIELD_MAXLEN];
	char *kstr;
	int len;
	int i, j;
	char c;

	ptr = selftestdata->ini_file_buf;
	memset(field, 0, sizeof(field));
	while (1) {
		memset(line, 0, CTS_KEYWORD_LINE_MAXLEN);
		len = read_line(&ptr, line);
		if (len < 0) {
			break;
		}
		cts_info("%s", line);

		if (line[0] == '[') {
			j = 0;
			for (i = 1; line[i] != '\0'; i++) {
				c = line[i];
				if (c != ']') {
					field[j++] = c;
					field[j] = '\0';
				}
			}
		} else {
			kstr = strnstr(line, "=", CTS_KEYWORD_LINE_MAXLEN);
			if (kstr == NULL) {
				continue;
			}
			*kstr = 0;
			cts_add_ini_keyword(selftestdata, field, line, kstr + 1);
		}
	}

	cts_show_ini_keyword(selftestdata);

	return 0;
}

int cts_find_ini_word(PT_SelftestData selftestdata, char *field, char *key)
{
	int i;

	for (i = 0; i < selftestdata->keyword_num; i++) {
		if (strcmp(selftestdata->keyword[i].field, field) == 0
		    && strcmp(selftestdata->keyword[i].key, key) == 0) {
			break;
		}
	}
	if (i == selftestdata->keyword_num) {
		return -EPERM;
	}
	return i;
}
