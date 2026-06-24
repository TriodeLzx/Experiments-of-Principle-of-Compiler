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

static int __fn_add_int(int a__1, int b__2);
static int __fn_type_convert_demo();
static int __fn_main();

static int __fn_add_int(int a__1, int b__2) {
  int __ret = 0;
  int c__3 = 0;
  int __t1 = 0;

  __t1 = a__1 + b__2;
  c__3 = __t1;
  __ret = c__3;
  goto __func_end_add_int;
__func_end_add_int:
  return __ret;
}

static int __fn_type_convert_demo() {
  int __ret = 0;
  int n__4 = 0;
  double f__5 = 0.0;
  std::string s__6 = "";
  bool flag__7 = false;
  double n_to_f__8 = 0.0;
  double __t2 = 0.0;
  int f_to_i__9 = 0;
  int __t3 = 0;
  int s_to_i__10 = 0;
  int __t4 = 0;
  std::string n_to_s__11 = "";
  std::string __t5 = "";
  bool n_to_b__12 = false;
  bool __t6 = false;
  int sum_int__13 = 0;
  int __t7 = 0;
  double sum_float__14 = 0.0;
  double __t8 = 0.0;
  bool ok__15 = false;
  bool __t9 = false;
  bool __t10 = false;
  int out__16 = 0;
  int __t11 = 0;
  int __t12 = 0;

  s_to_i__10 = 12;
  __t7 = __fn_add_int(7, 3);
  sum_int__13 = __t7;
  sum_float__14 = 10.5;
  __t9 = sum_int__13 > 0;
  __t10 = __t9;
  ok__15 = __t10;
  if (!(ok__15)) goto if_next_2;
  __t11 = __cast_int(sum_float__14);
  __t12 = __t11 + s_to_i__10;
  out__16 = __t12;
  __ret = out__16;
  goto __func_end_type_convert_demo;
if_next_2:
  __ret = 0;
  goto __func_end_type_convert_demo;
__func_end_type_convert_demo:
  return __ret;
}

static int __fn_main() {
  int __ret = 0;
  int result__17 = 0;
  int __t13 = 0;

  __t13 = __fn_type_convert_demo();
  result__17 = __t13;
  __ret = result__17;
  goto __func_end_main;
__func_end_main:
  return __ret;
}

int main() {
  auto __result = __fn_main();
  __print_value(__result);
  return 0;
}
