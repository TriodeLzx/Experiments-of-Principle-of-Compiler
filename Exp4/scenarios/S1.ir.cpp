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

static int __fn_fibonacci(int n__1);
static int __fn_main();

static int __fn_fibonacci(int n__1) {
  int __ret = 0;
  bool __t1 = false;
  int a__2 = 0;
  int b__3 = 0;
  int i__4 = 0;
  bool __t2 = false;
  int next__5 = 0;
  int __t3 = 0;
  int __t4 = 0;

  __t1 = n__1 <= 1;
  if (!(__t1)) goto if_next_2;
  __ret = n__1;
  goto __func_end_fibonacci;
if_next_2:
  a__2 = 0;
  b__3 = 1;
  i__4 = 2;
while_begin_3:
  __t2 = i__4 <= n__1;
  if (!(__t2)) goto while_end_4;
  __t3 = a__2 + b__3;
  next__5 = __t3;
  a__2 = b__3;
  b__3 = next__5;
  __t4 = i__4 + 1;
  i__4 = __t4;
  goto while_begin_3;
while_end_4:
  __ret = b__3;
  goto __func_end_fibonacci;
__func_end_fibonacci:
  return __ret;
}

static int __fn_main() {
  int __ret = 0;
  int n__6 = 0;
  int result__7 = 0;
  int __t5 = 0;

  n__6 = 10;
  __t5 = __fn_fibonacci(n__6);
  result__7 = __t5;
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
