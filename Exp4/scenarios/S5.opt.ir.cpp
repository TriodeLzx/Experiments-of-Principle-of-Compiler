#include <cstdlib>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static int __cast_int(int v) { return v; }
static int __cast_int(double v) { return static_cast<int>(v); }
static int __cast_int(bool v) { return v ? 1 : 0; }
static int __cast_int(const std::string& v) { return std::atoi(v.c_str()); }
static double __cast_float(int v) { return static_cast<double>(v); }
static double __cast_float(double v) { return v; }
static double __cast_float(bool v) { return v ? 1.0 : 0.0; }
static double __cast_float(const std::string& v) { return std::strtod(v.c_str(), nullptr); }
static bool __cast_bool(int v) { return v != 0; }
static bool __cast_bool(double v) { return std::fabs(v) > 1e-12; }
static bool __cast_bool(bool v) { return v; }
static bool __cast_bool(const std::string& v) { return !v.empty() && v != "0" && v != "false"; }
template <typename T>
static std::string __num_to_string(T v) { std::ostringstream out; out << v; return out.str(); }
static std::string __cast_str(int v) { return __num_to_string(v); }
static std::string __cast_str(double v) { return __num_to_string(v); }
static std::string __cast_str(bool v) { return v ? "true" : "false"; }
static std::string __cast_str(const std::string& v) { return v; }

static void __print_value(int v) { std::cout << v << std::endl; }
static void __print_value(double v) { std::cout << v << std::endl; }
static void __print_value(bool v) { std::cout << (v ? "true" : "false") << std::endl; }
static void __print_value(const std::string& v) { std::cout << v << std::endl; }
template <typename T>
static void __print_value(const std::vector<T>& values) {
  std::cout << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0) std::cout << ", ";
    std::cout << values[i];
  }
  std::cout << "]" << std::endl;
}

static int __fn_array_ops_demo();
static int __fn_main();

static int __fn_array_ops_demo() {
  int __ret = 0;
  std::vector<int> nums__1 = std::vector<int>();
  std::vector<int> __t1 = std::vector<int>();
  int i__2 = 0;
  int sum__3 = 0;
  bool __t2 = false;
  int __t3 = 0;
  int __t4 = 0;
  int __t5 = 0;
  int __t6 = 0;
  int __t7 = 0;
  std::vector<std::vector<int>> mat__4 = std::vector<std::vector<int>>();
  std::vector<int> __t8 = std::vector<int>();
  std::vector<int> __t9 = std::vector<int>();
  std::vector<std::vector<int>> __t10 = std::vector<std::vector<int>>();
  int diag__5 = 0;
  int __t11 = 0;
  int __t12 = 0;
  int __t13 = 0;
  int out__6 = 0;
  int __t14 = 0;

  __t1 = std::vector<int>{1, 2, 3, 4, 5};
  nums__1 = __t1;
  i__2 = 0;
  sum__3 = 0;
while_begin_1:
  __t2 = i__2 < 5;
  if (!(__t2)) goto while_end_2;
  __t3 = nums__1[i__2];
  __t4 = __t3 * 2;
  nums__1[i__2] = __t4;
  __t5 = nums__1[i__2];
  __t6 = sum__3 + __t5;
  sum__3 = __t6;
  __t7 = i__2 + 1;
  i__2 = __t7;
  goto while_begin_1;
while_end_2:
  __t8 = std::vector<int>{1, 2};
  __t9 = std::vector<int>{3, 4};
  __t10 = std::vector<std::vector<int>>{__t8, __t9};
  mat__4 = __t10;
  __t11 = mat__4[0][0];
  __t12 = mat__4[1][1];
  __t13 = __t11 + __t12;
  diag__5 = __t13;
  __t14 = sum__3 + diag__5;
  out__6 = __t14;
  __ret = out__6;
  goto __func_end_array_ops_demo;
__func_end_array_ops_demo:
  return __ret;
}

static int __fn_main() {
  int __ret = 0;
  int result__7 = 0;
  int __t15 = 0;

  __t15 = __fn_array_ops_demo();
  result__7 = __t15;
  __ret = result__7;
  goto __func_end_main;
__func_end_main:
  return __ret;
}

int main() {
  auto __result = __fn_main();
  __print_value(__result);
  return 0;
}
