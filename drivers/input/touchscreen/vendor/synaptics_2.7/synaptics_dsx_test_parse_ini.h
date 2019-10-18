#ifndef _SYNAPTICS_DSX_TEST_PARSE_INI_
#define _SYNAPTICS_DSX_TEST_PARSE_INI_

#define FORCETOUCH_ROW	2
#define BUFFER_LENGTH	256

#define MAX_KEY_NUM					300
#define MAX_KEY_NAME_LEN			50
#define MAX_KEY_VALUE_LEN			360

#define MAX_CFG_BUF					480
#define SUCCESS						0
/* return value */
#define CFG_OK						SUCCESS
#define CFG_SECTION_NOT_FOUND		-1
#define CFG_KEY_NOT_FOUND			-2
#define CFG_ERR						-10

#define CFG_ERR_OPEN_FILE			-10
#define CFG_ERR_CREATE_FILE			-11
#define CFG_ERR_READ_FILE			-12
#define CFG_ERR_WRITE_FILE			-13
#define CFG_ERR_FILE_FORMAT			-14
#define CFG_ERR_TOO_MANY_KEY_NUM	-15
#define CFG_ERR_OUT_OF_LEN			-16

#define CFG_ERR_EXCEED_BUF_SIZE		-22

#define COPYF_OK					SUCCESS
#define COPYF_ERR_OPEN_FILE			-10
#define COPYF_ERR_CREATE_FILE		-11
#define COPYF_ERR_READ_FILE			-12
#define COPYF_ERR_WRITE_FILE		-13

#define TPD_TEST_DEBUG_EN     1
#if (TPD_TEST_DEBUG_EN)
#define TPD_TEST_DBG(fmt, args...) pr_info("[tpd] %s. line: %d.  "fmt"\n", __func__, __LINE__, ##args)
#define TPD_TEST_PRINT(fmt, args...) pr_info("" fmt, ## args)
#else
#define TPD_TEST_DBG(fmt, args...) do {} while (0)
#define TPD_TEST_PRINT(fmt, args...) do {} while (0)
#endif

typedef struct _ST_INI_FILE_DATA {
	char pSectionName[MAX_KEY_NAME_LEN];
	char pKeyName[MAX_KEY_NAME_LEN];
	char pKeyValue[MAX_KEY_VALUE_LEN];
	int iSectionNameLen;
	int iKeyNameLen;
	int iKeyValueLen;
} ST_INI_FILE_DATA;


void *tpd_malloc(size_t size);
void tpd_free(void *p);

char *syna_ini_str_trim_r(char *buf);
char *syna_ini_str_trim_l(char *buf);
int get_key_value_string(char *section, char *ItemName, char *defaultvalue, char *returnValue);
int syna_ini_get_key_data(char *ini_param);
int release_key_data_space(void);
int tpd_atoi(char *nptr);
#endif

