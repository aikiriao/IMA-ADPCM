#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include "test.h"

/* ログ出力関数（暫定） */
#define Test_LogOutput printf

/* 式文字列の最大長 */
#define TEST_MAX_LEN_EXPRESSION_STRING (100)

/* テスト関数リスト */
struct TestFuncList {
  struct TestFuncList *next;
  const char*         test_name;    /* テスト名   */
  TestFunctionType    test_func;    /* テスト関数 */
};

/* テストスイート */
struct TestSuite {
  const char*             suite_name;   /* スイート名 */
  void*                   obj;          /* 任意のオブジェクト */
  TestInitFinFunctionType init_func;    /* 初期化関数 */
  TestInitFinFunctionType fin_func;     /* 終了関数   */
  struct TestFuncList*    tests;        /* テスト関数リスト */
  uint32_t                num_asserts;  /* アサート数 */
  uint32_t                num_failure;  /* 失敗数 */
  float                   epsilon;      /* 判定用イプシロン */
};

/* テストスイートのリスト */
struct TestSuiteList {
  struct TestSuiteList *next;
  struct TestSuite     suite;         /* note: テストスイートは公開するためリストのメンバとする */
};

/* テスト失敗ケース */
typedef enum TestFailureTypeTag {
  TEST_FAILURE_TYPE_CONDITION,                  /* 条件式                                   */
  TEST_FAILURE_TYPE_EQAL_EXPECT,                /* 期待した値                               */
  TEST_FAILURE_TYPE_NOT_EQAL_EXPECT,            /* 期待しない値                             */
  TEST_FAILURE_TYPE_FLOAT32_EPSION_EQUAL,       /* 32bit浮動小数のイプシロン以下  */
  TEST_FAILURE_TYPE_FLOAT32_NOT_EPSION_EQUAL    /* 32bit浮動小数のイプシロンより大きい  */
} TestFailureType;

/* テスト失敗情報 */
struct TestFailureList {
  struct TestFailureList*    next;
  TestFailureType            type;                                          /* 失敗ケース       */
  const char*                file_name;                                     /* ファイル名       */
  const char*                function_name;                                 /* テスト関数名     */
  uint32_t                   line_no;                                       /* 失敗ケース行     */
  union {
    struct {
      const char*            expression;                                    /* 条件式文字列     */
    } condition;
    struct {
      int64_t                expected_value;                                /* 期待した値       */
      int64_t                actual_value;                                  /* 実際の値         */
      char                   expected_exp[TEST_MAX_LEN_EXPRESSION_STRING];  /* 期待した値文字列 */
      char                   actual_exp[TEST_MAX_LEN_EXPRESSION_STRING];    /* 実際の値文字列   */
    } expect_value;
    struct {
      float                  expected_value;                                /* 期待した値         */
      float                  actual_value;                                  /* 実際の値           */
      float                  epsilon;                                       /* 判定用のイプシロン */
      char                   expected_exp[TEST_MAX_LEN_EXPRESSION_STRING];  /* 期待した値文字列   */
      char                   actual_exp[TEST_MAX_LEN_EXPRESSION_STRING];    /* 実際の値文字列     */
    } float32_epsilon;
  } fail_u;
  const struct TestSuite*    suite; /* 失敗したスイート */
};

/* テストランナー */
struct TestRunner {
  struct TestSuite       *current_suite; /* 現在テスト中のスイート */
  struct TestSuiteList   *suite_list;    /* 全テストスイートリスト */
  struct TestFailureList *failure_list;  /* 失敗リスト */
};

/* テストスイートの実行 */
static int test_RunTestSuite(struct TestSuite *test_suite);
/* 失敗ケースの追加 */
static int test_AddNewFailure(struct TestRunner *runner, 
    struct TestFailureList *new_failure);
/* 期待する数値アサート・サブルーチン */
static void test_AssertEqualSub(
    TestFailureType type, int64_t expect, int64_t actual,
    const char* file_name, const char* function_name, 
    uint32_t    line_no, const char* expected_exp,
    const char* actual_exp);

/* テストランナーの実体 */
static struct TestRunner st_test_runner;

/* テストの初期化 */
void Test_Initialize(void)
{
  st_test_runner.suite_list   = NULL;
  st_test_runner.failure_list = NULL;
}

/* テストの終了 */
void Test_Finalize(void)
{
  struct TestSuiteList *suite, *tmp_suite;
  struct TestFuncList *test, *tmp_test;
  struct TestFailureList *fail, *tmp_fail;

  /* 全スイートとテストを解放 */
  for (suite = st_test_runner.suite_list;
       suite != NULL; ) {
    for (test = suite->suite.tests;
         test != NULL; ) {
      /* テストケースが一つもない */
      if (test == NULL) {
        break;
      }
      tmp_test = test->next;
      free(test);
      test = tmp_test;
    }
    tmp_suite = suite->next;
    free(suite);
    suite = tmp_suite;
  }

  /* 失敗リストを解放 */
  for (fail = st_test_runner.failure_list;
       fail != NULL; ) {
      /* 失敗が一つもない（素晴らしい） */
      if (fail == NULL) {
        break;
      }
      tmp_fail = fail->next;
      free(fail);
      fail = tmp_fail;
  }
}

struct TestSuite* 
Test_AddTestSuite(const char *suite_name,
    void *obj,
    TestInitFinFunctionType init_func,
    TestInitFinFunctionType fin_func)
{
  struct TestSuiteList *list_pos;
  struct TestSuiteList *new_entry;

  /* 新しいスイートの作成 */
  new_entry 
    = (struct TestSuiteList *)malloc(sizeof(struct TestSuiteList));
  new_entry->next               = NULL;
  new_entry->suite.suite_name   = suite_name;
  new_entry->suite.obj          = obj;
  new_entry->suite.init_func    = init_func;
  new_entry->suite.fin_func     = fin_func;
  new_entry->suite.tests        = NULL;
  new_entry->suite.num_asserts  = 0;
  new_entry->suite.num_failure  = 0;
  new_entry->suite.epsilon      = 0.0f;

  if (st_test_runner.suite_list == NULL) {
    /* 初回のみ先頭に追加 */
    st_test_runner.suite_list = new_entry;
  } else {
    /* 末尾に至るまでリストを辿る */
    for (list_pos        = st_test_runner.suite_list;
         list_pos->next != NULL;
         list_pos        = list_pos->next) ;

    /* リストに追加 */
    list_pos->next = new_entry;
  }

  return &(new_entry->suite);
}

void Test_AddTestWithName(
    struct TestSuite* test_suite, 
    const char* test_name,
    TestFunctionType test_func)
{
  struct TestFuncList *list_pos;
  struct TestFuncList *new_entry;

  if (test_suite == NULL) {
    return;
  }

  /* 新しいテストの作成 */
  new_entry 
    = (struct TestFuncList *)malloc(sizeof(struct TestFuncList));
  new_entry->next       = NULL;
  new_entry->test_name  = test_name;
  new_entry->test_func  = test_func;

  /* リストに追加 */
  if (test_suite->tests == NULL) {
    /* 初回のみ先頭に追加 */
    test_suite->tests    = new_entry;
  } else {
    for (list_pos        = test_suite->tests;
         list_pos->next != NULL;
         list_pos        = list_pos->next) ;
    list_pos->next = new_entry;
  }

}

/* 全失敗情報の印字 */
int Test_PrintAllFailures(void)
{
  struct TestFailureList* fail;

  Test_LogOutput("====== %-30s ======\n", "Failures");

  /* 失敗が一つもない */
  if (st_test_runner.failure_list == NULL) {
    Test_LogOutput("There are no failures. \n");
    return 0;
  }

  for (fail = st_test_runner.failure_list;
       fail != NULL;
       fail = fail->next) {
    Test_LogOutput("File:%s(line:%d) In function %s \n",
        fail->file_name, fail->line_no, fail->function_name);
    switch (fail->type) {
      case TEST_FAILURE_TYPE_CONDITION:
        Test_LogOutput("Expression %s evaluated as FALSE. \n",
            fail->fail_u.condition.expression);
        break;
      case TEST_FAILURE_TYPE_EQAL_EXPECT:
        Test_LogOutput("Expression %s(=%ld) is not equal to %s(=%ld). \n",
            fail->fail_u.expect_value.expected_exp, (long)fail->fail_u.expect_value.expected_value,
            fail->fail_u.expect_value.actual_exp, (long)fail->fail_u.expect_value.actual_value);
        break;
      case TEST_FAILURE_TYPE_NOT_EQAL_EXPECT:
        Test_LogOutput("Expression %s(=%ld) is equal to %s(=%ld). \n",
            fail->fail_u.expect_value.expected_exp, (long)fail->fail_u.expect_value.expected_value,
            fail->fail_u.expect_value.actual_exp, (long)fail->fail_u.expect_value.actual_value);
        break;
      case TEST_FAILURE_TYPE_FLOAT32_EPSION_EQUAL:
        Test_LogOutput("Expression %s(=%f) is not equal to %s(=%f) (in epsilon %f). \n",
            fail->fail_u.float32_epsilon.expected_exp, fail->fail_u.float32_epsilon.expected_value,
            fail->fail_u.float32_epsilon.actual_exp, fail->fail_u.float32_epsilon.actual_value,
            fail->fail_u.float32_epsilon.epsilon);
        break;
      case TEST_FAILURE_TYPE_FLOAT32_NOT_EPSION_EQUAL:
        Test_LogOutput("Expression %s(=%f) is equal to %s(=%f) (in epsilon %f). \n",
            fail->fail_u.float32_epsilon.expected_exp, fail->fail_u.float32_epsilon.expected_value,
            fail->fail_u.float32_epsilon.actual_exp, fail->fail_u.float32_epsilon.actual_value,
            fail->fail_u.float32_epsilon.epsilon);
        break;
      default:
        Test_LogOutput("Not Supported Fail Type. \n");
        break;
    }
  }

  return 0;

}

/* 全テストスイートの実行 */
int Test_RunAllTestSuite(void)
{
  int ret = 0, tmp_ret;
  struct TestSuiteList *suite;

  for (suite  = st_test_runner.suite_list;
       suite != NULL;
       suite  = suite->next) {

    /* 現在実行中のスイートに変更
     * マルチスレッド動作時は test_RunTestSuite も含め
     * 排他する必要がある */
    st_test_runner.current_suite = &suite->suite;

    /* 異常終了の回数だけインクリメント */
    tmp_ret = test_RunTestSuite(&suite->suite); 
    if (tmp_ret != 0) {
      ret++;
    }

  }

  Test_LogOutput("%-30s Done. [Total Failures(Suites):%d]\n", "Test", ret);

  return ret;
}

/* 一つのテストスイートを実行するサブルーチン */
static int test_RunTestSuite(struct TestSuite *test_suite)
{
  struct TestFuncList* func_list;

  Test_LogOutput("====== %-30s ======\n", test_suite->suite_name);

  for (func_list = test_suite->tests;
       func_list != NULL;
       func_list = func_list->next) {

    /* 初期化関数実行 */
    test_suite->init_func(test_suite->obj);
    /* テスト実行 */
    func_list->test_func(test_suite->obj);
    /* 終了関数実行 */
    test_suite->fin_func(test_suite->obj);

    /* 結果表示 */
    Test_LogOutput("%-30s Done.\n", func_list->test_name);

  }

  Test_LogOutput("%-30s Done. [Failures:%d/Asserts:%d] \n",
      test_suite->suite_name, test_suite->num_failure, test_suite->num_asserts);

  return test_suite->num_failure;
}

static int test_AddNewFailure(
    struct TestRunner *runner,
    struct TestFailureList *new_failure)
{
  struct TestFailureList* fail_pos;

  /* 引数チェック */
  if (runner == NULL || new_failure == NULL) {
    return -1;
  }

  /* スイートの失敗回数を増加 */
  runner->current_suite->num_failure++;

  /* どのスイートで失敗したのか */
  new_failure->suite = runner->current_suite;

  /* リストに追加 */
  if (runner->failure_list == NULL) {
    /* 初回のみ先頭に追加 */
    runner->failure_list = new_failure;
  } else {
    for (fail_pos        = runner->failure_list;
         fail_pos->next != NULL;
         fail_pos        = fail_pos->next) ;
    fail_pos->next = new_failure;
  }

  return 0;
}

/* 条件式アサート */
void Test_AssertConditionFunc(int32_t condition, 
    const char* file_name, 
    const char* function_name, 
    uint32_t    line_no,
    const char* cond_expression)
{
  struct TestFailureList* fail;

  /* アサート回数をインクリメント */
  st_test_runner.current_suite->num_asserts++;

  /* 成功していれば即時リターン */
  if (condition != 0) {
    return;
  }

  /* 失敗情報を生成 */
  fail = (struct TestFailureList *)malloc(sizeof(struct TestFailureList));
  fail->next          = NULL;
  fail->type          = TEST_FAILURE_TYPE_CONDITION;
  fail->file_name     = file_name;
  fail->function_name = function_name;
  fail->line_no       = line_no;
  fail->fail_u.condition.expression = cond_expression;
  test_AddNewFailure(&st_test_runner, fail);
}

/* 期待する数値アサート・サブルーチン */
static void test_AssertEqualSub(
    TestFailureType type,
    int64_t expect, int64_t actual,
    const char* file_name, 
    const char* function_name, 
    uint32_t    line_no,
    const char* expected_exp,
    const char* actual_exp)
{
  struct TestFailureList* fail;

  /* アサート回数をインクリメント */
  st_test_runner.current_suite->num_asserts++;

  /* 成功していれば即時リターン */
  if (((type == TEST_FAILURE_TYPE_EQAL_EXPECT) && (expect == actual))
      || ((type == TEST_FAILURE_TYPE_NOT_EQAL_EXPECT) && (expect != actual))) {
    return;
  }

  /* 失敗情報を生成 */
  fail = (struct TestFailureList *)malloc(sizeof(struct TestFailureList));
  fail->next            = NULL;
  fail->type            = type;
  fail->file_name       = file_name;
  fail->function_name   = function_name;
  fail->line_no         = line_no;
  fail->fail_u.expect_value.expected_value  = expect;
  fail->fail_u.expect_value.actual_value    = actual;
  strncpy(fail->fail_u.expect_value.expected_exp, expected_exp, TEST_MAX_LEN_EXPRESSION_STRING);
  strncpy(fail->fail_u.expect_value.actual_exp, actual_exp, TEST_MAX_LEN_EXPRESSION_STRING);
  test_AddNewFailure(&st_test_runner, fail);
}

void Test_AssertEqualFunc(
    int64_t expect, int64_t actual,
    const char* file_name, 
    const char* function_name, 
    uint32_t    line_no,
    const char* expected_exp,
    const char* actual_exp)
{
  test_AssertEqualSub(
      TEST_FAILURE_TYPE_EQAL_EXPECT,
      expect, actual,
      file_name, function_name, line_no,
      expected_exp, actual_exp);
}

void Test_AssertNotEqualFunc(
    int64_t not_expect, int64_t actual,
    const char* file_name, 
    const char* function_name, 
    uint32_t    line_no,
    const char* expected_exp,
    const char* actual_exp)
{
  test_AssertEqualSub(
      TEST_FAILURE_TYPE_NOT_EQAL_EXPECT,
      not_expect, actual,
      file_name, function_name, line_no,
      expected_exp, actual_exp);
}

/* イプシロン値の設定 */
void Test_SetFloat32Epsilon(float epsilon)
{
  if (epsilon >= 0.0f) {
    st_test_runner.current_suite->epsilon = epsilon;
  }
}

/* イプシロン値アサート */
static void test_AssertFloat32EpsilonEqualSub(
    TestFailureType type,
    float expect, float actual,
    const char* file_name, 
    const char* function_name, 
    uint32_t    line_no,
    const char* expected_exp,
    const char* actual_exp)
{
  struct TestFailureList* fail;
  float error;
  float epsilon = st_test_runner.current_suite->epsilon;

  /* アサート回数をインクリメント */
  st_test_runner.current_suite->num_asserts++;

  /* 誤差を計算 */
  error = fabs(expect - actual);

  /* 成功していれば即時リターン */
  if (((type == TEST_FAILURE_TYPE_FLOAT32_EPSION_EQUAL) && (error <= epsilon))
      || ((type == TEST_FAILURE_TYPE_FLOAT32_NOT_EPSION_EQUAL) && (expect > epsilon))) {
    return;
  }

  /* 失敗情報を生成 */
  fail = (struct TestFailureList *)malloc(sizeof(struct TestFailureList));
  fail->next            = NULL;
  fail->type            = type;
  fail->file_name       = file_name;
  fail->function_name   = function_name;
  fail->line_no         = line_no;
  fail->fail_u.float32_epsilon.expected_value  = expect;
  fail->fail_u.float32_epsilon.actual_value    = actual;
  fail->fail_u.float32_epsilon.epsilon         = epsilon;
  strncpy(fail->fail_u.float32_epsilon.expected_exp, expected_exp, TEST_MAX_LEN_EXPRESSION_STRING);
  strncpy(fail->fail_u.float32_epsilon.actual_exp, actual_exp, TEST_MAX_LEN_EXPRESSION_STRING);
  test_AddNewFailure(&st_test_runner, fail);
}

void Test_AssertFloat32EpsilonEqualFunc(
    float expect, float actual,
    const char* file_name, 
    const char* function_name, 
    uint32_t    line_no,
    const char* expected_exp,
    const char* actual_exp)
{
  test_AssertFloat32EpsilonEqualSub(
      TEST_FAILURE_TYPE_FLOAT32_EPSION_EQUAL,
      expect, actual,
      file_name, function_name, line_no,
      expected_exp, actual_exp);
}

void Test_AssertFloat32NotEpsilonEqualFunc(
    float not_expect, float actual,
    const char* file_name, 
    const char* function_name, 
    uint32_t    line_no,
    const char* expected_exp,
    const char* actual_exp)
{
  test_AssertFloat32EpsilonEqualSub(
      TEST_FAILURE_TYPE_FLOAT32_NOT_EPSION_EQUAL,
      not_expect, actual,
      file_name, function_name, line_no,
      expected_exp, actual_exp);
}
