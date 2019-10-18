#ifndef __POWER_ZTE_MISC__
#define __POWER_ZTE_MISC__

enum battery_sts {
	BATTERY_CHARGING = 0,
	BATTERY_DISCHARGING,/*set icl min value and disable charger*/
	BATTERY_NOT_CHARGING,/*only disable charger*/
	BATTERY_UNKNOWN
};

enum charging_policy_sts {
	NORMAL_CHARGING_POLICY = BIT(0),
	DEMO_CHARGING_POLICY = BIT(1),
	EXPIRED_CHARGING_POLICY = BIT(2),/*depends on EXPIRED_CHARGING_POLICY_ENABLE*/
	UNKNOWN_CHARGING_POLICY = BIT(3)
};

enum charger_types_oem {
	CHARGER_TYPE_DEFAULT = -1,
	CHARGER_TYPE_UNKNOWN = 0,
	CHARGER_TYPE_HVDCP = 1,
	CHARGER_TYPE_DCP = 2,
	CHARGER_TYPE_DCP_SLOW = 3,
	CHARGER_TYPE_SDP_NBC1P2 = 4,
	CHARGER_TYPE_SDP_NBC1P2_SLOW = 5,
	CHARGER_TYPE_SDP_NBC1P2_CHARGR_ERR = 6,
};

#define DEFAULT_CHARGING_POLICY NORMAL_CHARGING_POLICY
#define MIN_BATTERY_PROTECTED_PERCENT 50
#define MAX_BATTERY_PROTECTED_PERCENT 70
#define EXPIRED_CHARGING_POLICY_ENABLE 0
#define CHARGING_EXPIRATION_TIME_MS 64800000 /*18 h*/
#define CHARGING_POLICY_PERIOD 60000 /*60s*/
#define CHARGING_POLICY_LOWTEMP_THRESHOLD 150

struct charging_policy_ops {
	int battery_status;
	int charging_policy_status;
	int (*charging_policy_demo_sts_set)(struct charging_policy_ops *charging_policy, bool enable);
	int (*charging_policy_demo_sts_get)(struct charging_policy_ops *charging_policy);
	int (*charging_policy_expired_mode_set)(struct charging_policy_ops *charging_policy, bool enable);
	int (*charging_policy_expired_mode_get)(struct charging_policy_ops *charging_policy);
	int (*charging_policy_expired_sts_get)(struct charging_policy_ops *charging_policy);
	int (*charging_policy_expired_sec_set)(struct charging_policy_ops *charging_policy, int sec);
	int (*charging_policy_expired_sec_get)(struct charging_policy_ops *charging_policy);
	int (*charging_policy_lowtemp_en_set)(struct charging_policy_ops *charging_policy, bool enable);
	int (*charging_policy_lowtemp_en_get)(struct charging_policy_ops *charging_policy);
	int (*charging_policy_cap_min_set)(struct charging_policy_ops *charging_policy, int cap);
	int (*charging_policy_cap_min_get)(struct charging_policy_ops *charging_policy);
	int (*charging_policy_cap_max_set)(struct charging_policy_ops *charging_policy, int cap);
	int (*charging_policy_cap_max_get)(struct charging_policy_ops *charging_policy);
};

int zte_misc_register_charging_policy_ops(struct charging_policy_ops *ops);

#endif
