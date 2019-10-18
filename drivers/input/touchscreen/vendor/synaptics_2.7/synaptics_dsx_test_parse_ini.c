#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>

#include "synaptics_dsx_test_parse_ini.h"

static char CFG_SSL = '[';  /* Section Symbol --Can be defined according to the special need to change. */
static char CFG_SSR = ']';  /*  Section Symbol --Can be defined according to the special need to change. */
/* static char CFG_NIS = ':';  //Separator between name and index */
static char CFG_NTS = '#';  /* annotator */
static char CFG_EQS = '=';  /* The equal sign */

/* ST_INI_FILE_DATA g_st_ini_file_data[MAX_KEY_NUM]; */
ST_INI_FILE_DATA *g_st_ini_file_data = NULL;
int g_used_key_num = 0;

/* Works only for digits and letters, but small and fast */
#define TOLOWER(x) ((x) | 0x20)

#define FTS_MALLOC_TYPE		1 /* 0: kmalloc, 1: vmalloc */
enum enum_malloc_mode {
	kmalloc_mode = 0,
	vmalloc_mode = 1,
};

void *tpd_malloc(size_t size)
{
	if (kmalloc_mode == FTS_MALLOC_TYPE)
		return kmalloc(size, GFP_ATOMIC);
	else if (vmalloc_mode == FTS_MALLOC_TYPE)
		return vmalloc(size);

	TPD_TEST_DBG("invalid malloc.\n");

	return NULL;
}

void tpd_free(void *p)
{
	if (kmalloc_mode == FTS_MALLOC_TYPE)
		return kfree(p);
	else if (vmalloc_mode == FTS_MALLOC_TYPE)
		return vfree(p);

	TPD_TEST_DBG("invalid free.\n");

}

int tpd_strncmp(const char *cs, const char *ct, size_t count)
{
	unsigned char c1 = 0, c2 = 0;

	while (count) {
		c1 = TOLOWER(*cs++);
		c2 = TOLOWER(*ct++);
		if (c1 != c2)
			return c1 < c2 ? -1 : 1;
		if (!c1)
			break;
		count--;
	}
	return 0;
}

/*************************************************************
Function:  Get the value of key
Input: char *ini_param; char *section; char *key
Output: char *value¡¡key
Return: 0		SUCCESS
		-1		can not find section
		-2		can not find key
		-10	File open failed
		-12	File read  failed
		-14	File format error
		-22	Out of buffer size
Note:
*************************************************************/
int ini_get_key_value(char *section, char *key, char *value)
{
	int i = 0;
	int ret = -2;

	for (i = 0; i < g_used_key_num; i++) {
		if (tpd_strncmp(section, g_st_ini_file_data[i].pSectionName,
						g_st_ini_file_data[i].iSectionNameLen) != 0)
			continue;
		if (tpd_strncmp(key, g_st_ini_file_data[i].pKeyName,  strlen(key)) == 0) {
			memcpy(value, g_st_ini_file_data[i].pKeyValue, g_st_ini_file_data[i].iKeyValueLen);
			ret = 0;
			break;
		}
	}

	return ret;
}

/*************************************************************
Function: Remove empty character on the right side of the string
Input:	char *buf --String pointer
Output:
Return: String pointer
Note:
*************************************************************/
char *syna_ini_str_trim_r(char *buf)
{
	int len, i;
	char tmp[512];

	memset(tmp, 0, sizeof(tmp));
	len = strlen(buf) + 1;

	memset(tmp, 0x00, len);
	for (i = 0; i < len; i++) {
		if (buf[i] != ' ')
			break;
	}

	if (i < len)
		strlcpy(tmp, (buf + i), (len - i));

	strlcpy(buf, tmp, len);

	return buf;
}

/*************************************************************
Function: Remove empty character on the left side of the string
Input: char *buf --String pointer
Output:
Return: String pointer
Note:
*************************************************************/
char *syna_ini_str_trim_l(char *buf)
{
	int len, i;
	char tmp[512];

	memset(tmp, 0, sizeof(tmp));
	len = strlen(buf) + 1;

	memset(tmp, 0x00, len);

	for (i = 0; i < len; i++) {
		if (buf[len - i - 1] != ' ')
			break;
	}

	if (i < len)
		strlcpy(tmp, buf, len - i);

	strlcpy(buf, tmp, len);

	return buf;
}

/*************************************************************
Function: Read a line from file
Input:	FILE *fp; int maxlen-- Maximum length of buffer
Output: char *buffer --  A string
Return: >0		Actual read length
		-1		End of file
		-2		Error reading file
Note:
*************************************************************/
static int ini_file_get_line(char *ini_param, char *buffer, int maxlen)
{
	int i = 0;
	int j = 0;
	int iRetNum = -1;
	char ch1 = '\0';

	for (i = 0, j = 0; i < maxlen; j++) {
		ch1 = ini_param[j];
		iRetNum = j + 1;
		if (ch1 == '\n' || ch1 == '\r') {    /* line end */
			ch1 = ini_param[j + 1];
			if (ch1 == '\n' || ch1 == '\r')
				iRetNum++;

			break;    /* line breaks */
		} else if (ch1 == 0x00)	{
			iRetNum = -1;
			break;    /* file end */
		}

		buffer[i++] = ch1;	  /* ignore carriage return */
	}
	buffer[i] = '\0';

	return iRetNum;
}

int my_tpd_atoi(const char *str)
{
	int result = 0;
	int signal = 1; /* The default is positive number*/

	if ((*str >= '0' && *str <= '9') || *str == '-' || *str == '+') {
		if (*str == '-' || *str == '+') {
			if (*str == '-')
				signal = -1; /*enter negative number*/
			str++;
		}
	} else
		return 0;
	/*start transform*/
	while (*str >= '0' && *str <= '9')
		result = result * 10 + (*str++ - '0');

	return signal*result;
}

int _tpd_isspace(int x)
{
	if (x == ' ' || x == '\t' || x == '\n' || x == '\f' || x == '\b' || x == '\r')
		return 1;
	else
		return 0;
}

int _tpd_isdigit(int x)
{
	if (x <= '9' && x >= '0')
		return 1;
	else
		return 0;

}

static long _tpd_atol(char *nptr)
{
	int c; /* current char */
	long total; /* current total */
	int sign; /* if ''-'', then negative, otherwise positive */
	/* skip whitespace */
	while (_tpd_isspace((int)(unsigned char)*nptr))
		++nptr;
	c = (int)(unsigned char)*nptr++;
	sign = c; /* save sign indication */
	if (c == '-' || c == '+')
		c = (int)(unsigned char)*nptr++; /* skip sign */
	total = 0;
	while (_tpd_isdigit(c)) {
		total = 10 * total + (c - '0'); /* accumulate digit */
		c = (int)(unsigned char)*nptr++; /* get next char */
	}
	if (sign == '-')
		return -total;
	else
		return total; /* return result, negated if necessary */
}

int tpd_atoi(char *nptr)
{
	return (int)_tpd_atol(nptr);
}

/* malloc ini section/key/value data space. */
int init_key_data_space(void)
{
	int i = 0;

	g_used_key_num = 0;

	g_st_ini_file_data = NULL;
	if (g_st_ini_file_data == NULL)
		g_st_ini_file_data = tpd_malloc(sizeof(ST_INI_FILE_DATA)*MAX_KEY_NUM);

	if (g_st_ini_file_data == NULL) {
		TPD_TEST_DBG("tpd_malloc failed in function.");
		return -EINVAL;
	}

	for (i = 0; i < MAX_KEY_NUM; i++) {
		memset(g_st_ini_file_data[i].pSectionName, 0, MAX_KEY_NAME_LEN);
		memset(g_st_ini_file_data[i].pKeyName, 0, MAX_KEY_NAME_LEN);
		memset(g_st_ini_file_data[i].pKeyValue, 0, MAX_KEY_VALUE_LEN);
		g_st_ini_file_data[i].iSectionNameLen = 0;
		g_st_ini_file_data[i].iKeyNameLen = 0;
		g_st_ini_file_data[i].iKeyValueLen = 0;
	}

	return 0;
}

int release_key_data_space(void)
{
	if (g_st_ini_file_data != NULL) {
		tpd_free(g_st_ini_file_data);
		g_st_ini_file_data = NULL;
	}

	return 0;
}

int debug_print_key_data(void)
{
	int i = 0;


	TPD_TEST_DBG("g_used_key_num = %d",  g_used_key_num);
	for (i = 0; i < MAX_KEY_NUM; i++)
		TPD_TEST_DBG("pSectionName_%d:%s, pKeyName_%d:%s\n, pKeyValue_%d:%s",
					 i, g_st_ini_file_data[i].pSectionName,
					 i, g_st_ini_file_data[i].pKeyName,
					 i, g_st_ini_file_data[i].pKeyValue);

	return 1;
}
/*************************************************************
Function: Read all the parameters and values to the structure.
Return: Returns the number of key. If you go wrong, return a negative number.
Note:
*************************************************************/
int syna_ini_get_key_data(char *ini_param)
{
	char cfg_onebuf[MAX_CFG_BUF + 1] = {0};
	int line_len = 0;
	int retval = 0; /* n_sections = 0, */
	int offset = 0;
	int iEqualSign = 0;
	int i = 0;
	char tmpSectionName[MAX_CFG_BUF + 1] = {0};

	/* malloc ini section/key/value data space. */
	retval = init_key_data_space();
	if (retval < 0) {
		TPD_TEST_DBG("init_key_data_space failed return:%d.", retval);
		retval = -1;
		goto cfg_scts_end;
	}

	g_used_key_num = 0;
	while (1) {
		retval = CFG_ERR_READ_FILE;

		line_len = ini_file_get_line(ini_param + offset, cfg_onebuf, MAX_CFG_BUF);
		if (line_len < -1)
			goto cfg_scts_end;
		if (line_len < 0)
			break;/* file end */
		if (line_len >= MAX_CFG_BUF) {
			TPD_TEST_DBG("Error Length:%d\n",  line_len);
			goto cfg_scts_end;
		}
		offset += line_len;

		/* Remove empty character. */
		line_len = strlen(syna_ini_str_trim_l(syna_ini_str_trim_r(cfg_onebuf)));
		if (line_len == 0 || cfg_onebuf[0] == CFG_NTS)/* A blank line or a comment line */
			continue;

		retval = CFG_ERR_FILE_FORMAT;

		/* get section name */
		if (line_len > 2 && ((cfg_onebuf[0] == CFG_SSL && cfg_onebuf[line_len - 1] != CFG_SSR))) {
			TPD_TEST_DBG("Bad Section:%s\n",  cfg_onebuf);
			goto cfg_scts_end; /* bad section */
		}

		if (cfg_onebuf[0] == CFG_SSL) {
			g_st_ini_file_data[g_used_key_num].iSectionNameLen = line_len - 2;
			if (g_st_ini_file_data[g_used_key_num].iSectionNameLen > MAX_KEY_NAME_LEN) {
				retval = CFG_ERR_OUT_OF_LEN;
				TPD_TEST_DBG("MAX_KEY_NAME_LEN: CFG_ERR_OUT_OF_LEN\n");
				goto cfg_scts_end;
			}

			cfg_onebuf[line_len - 1] = 0x00;
			strlcpy(tmpSectionName, cfg_onebuf + 1, sizeof(tmpSectionName));

			continue;
		}
		/* get section name end */

		strlcpy(g_st_ini_file_data[g_used_key_num].pSectionName, tmpSectionName,
			sizeof(g_st_ini_file_data[g_used_key_num].pSectionName));
		g_st_ini_file_data[g_used_key_num].iSectionNameLen = strlen(tmpSectionName);

		iEqualSign = 0;
		for (i = 0; i < line_len; i++) {
			if (cfg_onebuf[i] == CFG_EQS) {
				iEqualSign = i;
				break;
			}
		}
		if (iEqualSign == 0)
			continue;

		/* before equal sign is assigned to the key name*/
		g_st_ini_file_data[g_used_key_num].iKeyNameLen = iEqualSign;
		if (g_st_ini_file_data[g_used_key_num].iKeyNameLen > MAX_KEY_NAME_LEN) {
			retval = CFG_ERR_OUT_OF_LEN;
			TPD_TEST_DBG("MAX_KEY_NAME_LEN: CFG_ERR_OUT_OF_LEN\n");
			goto cfg_scts_end;
		}
		memcpy(g_st_ini_file_data[g_used_key_num].pKeyName,
			   cfg_onebuf, g_st_ini_file_data[g_used_key_num].iKeyNameLen);

		/* After equal sign is assigned to the key value*/
		g_st_ini_file_data[g_used_key_num].iKeyValueLen = line_len - iEqualSign - 1;
		if (g_st_ini_file_data[g_used_key_num].iKeyValueLen > MAX_KEY_VALUE_LEN) {
			retval = CFG_ERR_OUT_OF_LEN;
			TPD_TEST_DBG("MAX_KEY_VALUE_LEN: CFG_ERR_OUT_OF_LEN\n");
			goto cfg_scts_end;
		}
		memcpy(g_st_ini_file_data[g_used_key_num].pKeyValue,
			   cfg_onebuf + iEqualSign + 1, g_st_ini_file_data[g_used_key_num].iKeyValueLen);


		retval = g_used_key_num;

		g_used_key_num++;	/*Parameter number accumulation*/
		if (g_used_key_num > MAX_KEY_NUM) {
			retval = CFG_ERR_TOO_MANY_KEY_NUM;
			TPD_TEST_DBG("MAX_KEY_NUM: CFG_ERR_TOO_MANY_KEY_NUM\n");
			goto cfg_scts_end;
		}
	}

	return 0;

cfg_scts_end:

	return retval;
}

int get_key_value_string(char *section, char *ItemName, char *defaultvalue, char *returnValue)
{
	char value[BUFFER_LENGTH] = {0};
	int len = 0;

	if (returnValue == NULL) {
		TPD_TEST_DBG("returnValue==NULL in function %s.", __func__);
		return 0;
	}

	if (ini_get_key_value(section, ItemName, value) < 0) {
		if (defaultvalue != NULL)
			memcpy(value, defaultvalue, strlen(defaultvalue));

		len = snprintf(returnValue, BUFFER_LENGTH, "%s", value);
		return 0;
	}

	len = snprintf(returnValue, BUFFER_LENGTH, "%s", value);

	return len;
}

