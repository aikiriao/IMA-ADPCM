#ifndef _TEST_H_INCLUDED_
#define _TEST_H_INCLUDED_

/* C89標準では __func__ が定義されていない */
#if !defined(__STDC_VERSION__) && !defined(__func__)
#define __func__ "<cannot get function name>"
#endif

#include <stdint.h>

/* 未使用引数警告回避マクロ */
#define TEST_UNUSED_PARAMETER(arg) \
  if (&(arg) == &(arg)) { ; }

/* テスト追加（関数名付き） */
#define Test_AddTest(suite, function) \
  Test_AddTestWithName(suite, #function, function)

/* 条件式アサート */
#define Test_AssertCondition(condition)     \
  Test_AssertConditionFunc(condition,       \
      __FILE__, __func__, __LINE__, #condition)

/* 期待する数値アサート */
#define Test_AssertEqual(expect, actual)    \
  Test_AssertEqualFunc(expect, actual,      \
      __FILE__, __func__, __LINE__,         \
      #expect, #actual)

/* 期待しない数値アサート */
#define Test_AssertNotEqual(not_expect, actual) \
  Test_AssertNotEqualFunc(not_expect, actual,   \
      __FILE__, __func__, __LINE__,             \
      #not_expect, #actual)

/* イプシロンの範囲内アサート */
#define Test_AssertFloat32EpsilonEqual(expect, actual)        \
  Test_AssertFloat32EpsilonEqualFunc(expect, actual,          \
      __FILE__, __func__, __LINE__,                           \
      #expect, #actual)

/* イプシロンの範囲外アサート */
#define Test_AssertFloat32NotEpsilonEqual(not_expect, actual) \
  Test_AssertFloat32NotEpsilonEqualFunc(not_expect, actual,   \
      __FILE__, __func__, __LINE__,                           \
      #not_expect, #actual)

/* 初期化/終了関数型 */
typedef int (*TestInitFinFunctionType)(void *obj);

/* テスト関数 */
typedef void (*TestFunctionType)(void *obj);

/* テストスイート構造体 */
struct TestSuite;

#ifdef __cplusplus
extern "C" {
#endif

/* テストの初期化 */
void Test_Initialize(void);

/* テストの終了 */
void Test_Finalize(void);

/* テストスイートの作成 */
struct TestSuite* 
Test_AddTestSuite(const char *suite_name,
    void *obj,
    TestInitFinFunctionType init_func,
    TestInitFinFunctionType fin_func);

/* テストスイートにテストを追加 */
void Test_AddTestWithName(
    struct TestSuite* test_suite,
    const char *test_name, 
    TestFunctionType test_func);

/* 全テストスイートの実行 */
int Test_RunAllTestSuite(void);

/* 全失敗情報の印字 */
int Test_PrintAllFailures(void);

/* イプシロン値の設定 */
/* 設定したイプシロンは同一テストスイート内で有効 */
void Test_SetFloat32Epsilon(float epsilon);

/* 条件式アサート */
void Test_AssertConditionFunc(
    int32_t condition, 
    const char* file_name, const char* function_name, 
    uint32_t    line_no, const char* cond_expression);

/* 期待する数値アサート */
void Test_AssertEqualFunc(
    int64_t expect, int64_t actual,
    const char* file_name, const char* function_name, 
    uint32_t    line_no,
    const char* expected_exp, const char* actual_exp);

/* 期待しない数値アサート */
void Test_AssertNotEqualFunc(
    int64_t not_expect, int64_t actual,
    const char* file_name, const char* function_name, 
    uint32_t    line_no,
    const char* expected_exp, const char* actual_exp);

/* イプシロンの範囲内アサート */
void Test_AssertFloat32EpsilonEqualFunc(
    float expect, float actual,
    const char* file_name, 
    const char* function_name, 
    uint32_t    line_no,
    const char* expected_exp,
    const char* actual_exp);

/* イプシロンの範囲外アサート */
void Test_AssertFloat32NotEpsilonEqualFunc(
    float not_expect, float actual,
    const char* file_name, 
    const char* function_name, 
    uint32_t    line_no,
    const char* expected_exp,
    const char* actual_exp);

#ifdef __cplusplus
}
#endif

#endif /* _TEST_H_INCLUDED_ */
