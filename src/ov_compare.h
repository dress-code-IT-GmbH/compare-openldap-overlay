#define CMP_EQUAL 0
#define CMP_ALTB -1
#define CMP_BLTA 1
#define EXISTANCE_EQUAL_BOTH_EXIST CMP_EQUAL
#define EXISTANCE_EQUAL_BOTH_DONTEXIST CMP_EQUAL
#define EXISTANCE_ALTB_ONLY_B_EXISTS CMP_ALTB
#define EXISTANCE_BLTA_ONLY_A_EXISTS CMP_BLTA

typedef struct ov_compare_attrs_s {
  AttributeDescription *attr;
} ov_compare_attrs_t;

typedef struct ov_compare_attributes_s {
  ov_compare_attrs_t *attribs[2];
} ov_compare_attributes_t;

typedef int ov_compare_function_result_t;

enum {
  OV_COMPARE_SUCCESS = 0,
  OV_COMPARE_MORE_THAN_TWO_ARGUMENTS_ERROR = 1,
  OV_COMPARE_SLAP_CONFIG_EMIT_CALLED_ERROR = 2,
  OV_COMPARE_NOT_THE_SAME_ORDERING = 3,
};

enum {
  OV_COMPARE_ATTR = 1,
};

static ConfigDriver ov_compare_cf_gen;

static AttributeDescription *ad_idnSyncDiff;

static struct schema_info {
  char *def;
  AttributeDescription **ad;
} ov_compare_OpSchema[] = {
  {
  "( 1.3.6.1.4.1.453.19.2.42 "
      "NAME 'idnSyncDiff' "
      "DESC 'ideninetics sync difference' "
      "EQUALITY integerMatch "
      "ORDERING integerOrderingMatch "
      "SYNTAX OMsInteger "
      "SINGLE-VALUE NO-USER-MODIFICATION USAGE dsaOperation )",
      &ad_idnSyncDiff}, {
  NULL, NULL}
};



static ConfigTable ov_comparecfg[] = {
  {"ov_compare_attributes", "attribute...",
   2, 0, 0, ARG_MAGIC | OV_COMPARE_ATTR, ov_compare_cf_gen,
   "( OLcfgOvAt:10.3 NAME 'olcOvCompareAttribute' "
   "DESC 'Attributes for which the value will be compared' "
   "EQUALITY caseIgnoreMatch "
   "ORDERING caseIgnoreOrderingMatch "
   "SUBSTR caseIgnoreSubstringsMatch "
   "SYNTAX OMsDirectoryString )", NULL, NULL},
  {NULL, NULL, 0, 0, 0, ARG_IGNORED}
};

static ConfigOCs ov_compareocs[] = {
  {"( OLcfgOvOc:10.3 "
   "NAME 'olcOvCompareConfig' "
   "DESC 'Attribute value comparison' "
   "SUP olcOverlayConfig " "MAY ( olcOvCompareAttribute ) )",
   Cft_Overlay, ov_comparecfg},
  {NULL, 0, NULL}
};
