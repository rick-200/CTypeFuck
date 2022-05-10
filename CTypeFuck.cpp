#include <cassert>
#include <deque>
#include <fstream>
#include <iostream>
#include <list>
#include <random>
#include <string>
#include <vector>

#include "memory_pool.h"

template <size_t Size>
class PoolAlloc {
  static rrlib::MemoryPool* pool;

 public:
  PoolAlloc() {
    if (pool == nullptr) pool = new rrlib::MemoryPool(Size, 1024 * 1024 * 16);
  }
  void free(void* p) { pool->Free(p); }
  void* alloc() { return pool->Allocate(); }
};
template <size_t Size>
rrlib::MemoryPool* PoolAlloc<Size>::pool = nullptr;

template <class _Ty>
class MyAlloc : public PoolAlloc<sizeof(_Ty)> {
 public:
  static_assert(!std::is_const_v<_Ty>,
                "The C++ Standard forbids containers of const elements "
                "because allocator<const T> is ill-formed.");

  using value_type = _Ty;

  using size_type = size_t;
  using difference_type = ptrdiff_t;

  using propagate_on_container_move_assignment = std::true_type;

  MyAlloc() noexcept {}

  MyAlloc(const MyAlloc&) noexcept {}
  template <class _Other>
  MyAlloc(const MyAlloc<_Other>&) noexcept {}
  ~MyAlloc() = default;
  MyAlloc& operator=(const MyAlloc&) = default;

  void deallocate(_Ty* const _Ptr, const size_t _Count) { this->free(_Ptr); }

  _Ty* allocate(const size_t _Count) {
    assert(_Count == 1);
    return static_cast<_Ty*>(this->alloc());
  }
};

int depth;
int param_max = 5;
bool no_cv = false;
bool only_int = false;
std::string out_file;

bool random(double x) {
  static std::mt19937_64 ran;
  static std::uniform_real_distribution<double> dis(0.0, 1.0);
  return dis(ran) < x;
}
int random(int l, int r) {
  // static std::mt19937_64 ran(1331);
  static std::mt19937_64 ran(std::random_device{}());
  std::uniform_int_distribution<int> dis(l, r);
  return dis(ran);
}
enum class TypeID { Normal, NormalPointer, FuncPointer };
class Type {
 public:
  virtual std::list<const char*, MyAlloc<const char*>> GetDef(
      const char* name) const = 0;
  virtual TypeID GetID() const = 0;
  virtual std::list<const char*, MyAlloc<const char*>> AsReturn(
      std::list<const char*, MyAlloc<const char*>>&& outer) const = 0;
};
class NormalType : public Type {
  const char* _s;

 public:
  NormalType(const char* s) : _s(s) {}
  virtual std::list<const char*, MyAlloc<const char*>> GetDef(
      const char* name) const override {
    std::list<const char*, MyAlloc<const char*>> ret;
    ret.push_back(_s);
    if (name != nullptr) {
      ret.push_back(name);
    }
    return ret;
  }
  virtual std::list<const char*, MyAlloc<const char*>> AsReturn(
      std::list<const char*, MyAlloc<const char*>>&& outer) const override {
    std::list<const char*, MyAlloc<const char*>> ret = std::move(outer);
    ret.push_front(_s);
    return ret;
  }
  virtual TypeID GetID() const override { return TypeID::Normal; }
};
class NormalPointerType : public Type {
  std::unique_ptr<Type> _inner;
  bool _const, _volatile;

 public:
  NormalPointerType(std::unique_ptr<Type>&& inner, bool is_const,
                    bool is_volatile)
      : _inner(std::move(inner)), _const(is_const), _volatile(is_volatile) {
    assert(_inner->GetID() != TypeID::FuncPointer);
  }
  virtual std::list<const char*, MyAlloc<const char*>> GetDef(
      const char* name) const override {
    assert(_inner->GetID() != TypeID::FuncPointer);
    std::list<const char*, MyAlloc<const char*>> ret = _inner->GetDef(nullptr);
    ret.push_back("*");
    if (_const && _volatile) {
      ret.push_back(" const volatile ");
    } else if (_const) {
      ret.push_back(" const ");
    } else if (_const) {
      ret.push_back(" volatile ");
    }
    if (name != nullptr) {
      ret.push_back(name);
    }
    return ret;
  }
  virtual std::list<const char*, MyAlloc<const char*>> AsReturn(
      std::list<const char*, MyAlloc<const char*>>&& outer) const override {
    auto ret = GetDef(nullptr);
    ret.splice(ret.end(), outer);
    return ret;
  }
  virtual TypeID GetID() const override { return TypeID::NormalPointer; }
};
class FuncPointer : public Type {
  std::unique_ptr<Type> _ret;
  std::vector<std::unique_ptr<Type>> _params;

 public:
  FuncPointer(std::unique_ptr<Type>&& ret,
              std::vector<std::unique_ptr<Type>>&& params)
      : _ret(std::move(ret)), _params(std::move(params)) {}
  std::list<const char*, MyAlloc<const char*>> ParamsToString() const {
    static const char* px[] = {"p0",  "p1",  "p2",  "p3",  "p4",  "p5",  "p6",
                               "p7",  "p8",  "p9",  "p10", "p11", "p12", "p13",
                               "p14", "p15", "p16", "p17", "p18", "p19", "p20",
                               "p21", "p22", "p23", "p24", "p25", "p26", "p27",
                               "p28", "p29", "p30", "p31"};
    std::list<const char*, MyAlloc<const char*>> s;
    s.push_back("(");
    for (size_t i = 0; i < _params.size(); i++) {
      s.splice(s.end(), _params[i]->GetDef(px[i]));
      if (i != _params.size() - 1) s.push_back(", ");
    }
    s.push_back(")");
    return s;
  }
  std::list<const char*, MyAlloc<const char*>> GetDefWithoutReturnType(
      std::list<const char*, MyAlloc<const char*>>&& inner) const {
    std::list<const char*, MyAlloc<const char*>> ret;
    ret.push_back("(*");
    ret.splice(ret.end(), std::move(inner));
    ret.push_back(")");
    ret.splice(ret.end(), ParamsToString());
    return ret;
  }
  virtual std::list<const char*, MyAlloc<const char*>> AsReturn(
      std::list<const char*, MyAlloc<const char*>>&& outer) const override {
    return _ret->AsReturn(GetDefWithoutReturnType(std::move(outer)));
  }
  virtual std::list<const char*, MyAlloc<const char*>> GetDef(
      const char* name) const override {
    std::list<const char*, MyAlloc<const char*>> name_list;
    name_list.push_back(name);
    return _ret->AsReturn(GetDefWithoutReturnType(std::move(name_list)));
  }
  virtual TypeID GetID() const override { return TypeID::FuncPointer; }
};
std::unique_ptr<NormalType> generate_normal_type() {
  // static int x = 0;
  // if (++x % 1000000 == 0) printf("%d\n", x);
  const char* type_icv[] = {"int ", "const int ", "const volatile int "};
  const char* type_s[] = {"char ",
                          "short ",
                          "int ",
                          "long ",
                          "long long ",
                          "unsigned char ",
                          "unsigned short ",
                          "unsigned int ",
                          "unsigned long ",
                          "unsigned long long ",
                          "float ",
                          "double ",
                          "long double "};
  const char* type_s_cv[] = {"char ",
                             "short ",
                             "int ",
                             "long ",
                             "long long ",
                             "unsigned char ",
                             "unsigned short ",
                             "unsigned int ",
                             "unsigned long ",
                             "unsigned long long ",
                             "float ",
                             "double ",
                             "long double ",
                             "const char ",
                             "const short ",
                             "const int ",
                             "const long ",
                             "const long long ",
                             "const unsigned char ",
                             "const unsigned short ",
                             "const unsigned int ",
                             "const unsigned long ",
                             "const unsigned long long ",
                             "const float ",
                             "const double ",
                             "const long double ",
                             "volatile char ",
                             "volatile short ",
                             "volatile int ",
                             "volatile long ",
                             "volatile long long ",
                             "volatile unsigned char ",
                             "volatile unsigned short ",
                             "volatile unsigned int ",
                             "volatile unsigned long ",
                             "volatile unsigned long long ",
                             "volatile float ",
                             "volatile double ",
                             "volatile long double ",
                             "const volatile char ",
                             "const volatile short ",
                             "const volatile int ",
                             "const volatile long ",
                             "const volatile long long ",
                             "const volatile unsigned char ",
                             "const volatile unsigned short ",
                             "const volatile unsigned int ",
                             "const volatile unsigned long ",
                             "const volatile unsigned long long ",
                             "const volatile float ",
                             "const volatile double ",
                             "const volatile long double "};
  const char* s;
  if (only_int) {
    if (no_cv) {
      s = "int ";
    } else {
      s = type_icv[random(0, sizeof(type_icv) / sizeof(void*) - 1)];
    }
  } else {
    if (no_cv) {
      s = type_s[random(0, sizeof(type_s) / sizeof(void*) - 1)];
    } else {
      s = type_s_cv[random(0, sizeof(type_s_cv) / sizeof(void*) - 1)];
    }
  }
  return std::make_unique<NormalType>(s);
}
std::unique_ptr<Type> _generate_type(int dep, int dep_max);
std::unique_ptr<Type> _generate_type_not_funcptr(int dep, int dep_max);
std::unique_ptr<NormalPointerType> generate_normal_pointer_type(int dep,
                                                                int dep_max) {
  // std::unique_ptr<Type> pinner = __generate_type(dep, dep_max);
  return std::make_unique<NormalPointerType>(
      _generate_type_not_funcptr(dep + 1, dep_max), random(0.3), random(0.3));
}
std::unique_ptr<FuncPointer> generate_func_pointer_type(int dep, int dep_max) {
  int cnt = random(0, param_max);
  std::vector<std::unique_ptr<Type>> params;
  for (int i = 0; i < cnt; i++)
    params.push_back(_generate_type(dep + 1, dep_max));
  return std::make_unique<FuncPointer>(_generate_type(dep + 1, dep_max),
                                       std::move(params));
}
std::unique_ptr<Type> _generate_type_not_funcptr(int dep, int dep_max) {
  assert(dep <= dep_max);
  if (dep >= dep_max) return generate_normal_type();
  switch (random(0, 1)) {
    case 0:
      return generate_normal_type();
    case 1:
      return generate_normal_pointer_type(dep, dep_max);
  }
}
std::unique_ptr<Type> _generate_type(int dep, int dep_max) {
  assert(dep <= dep_max);
  if (dep >= dep_max) return generate_normal_type();
  if (random(1)) return generate_func_pointer_type(dep, dep_max);
  switch (random(0, 1)) {
    case 0:
      return generate_normal_type();
    case 1:
      return generate_normal_pointer_type(dep, dep_max);
  }
}
std::string generate_type(const std::string& name, int dep_max) {
  auto l = _generate_type(1, dep_max)->GetDef(name.data());
  std::string s;
  for (auto& ls : l) {
    s.append(ls);
  }
  return s;
}
void help() {
  std::cout << "usage: ctypefuck <depth> [flags]" << std::endl;
  std::cout << "depth: recursive depth" << std::endl;
  std::cout << "flages:" << std::endl;
  std::cout << "-no-cv : do not generate const and volidate modifier"
            << std::endl;
  std::cout << "-only-int : restrict type except function pointer to int"
            << std::endl;
  std::cout << "-param <max_count> : set maxium parameter amount for "
               "generated function pointer (max_count<=32)"
            << std::endl;
  std::cout << "-out <file_path> : set output stream to a specific file"
            << std::endl;
}

bool to_int(const std::string_view& s, int* out) {
  char* pend;
  *out = strtol(s.data(), &pend, 0);
  return pend == s.data() + s.length();
}

int main(int argc, char* argv[]) {
  // only_int = 1;
  // param_max = 0;
  // no_cv = 1;
  // std::cout << generate_type("T", 2);
  if (argc == 1 || !to_int(argv[1], &depth)) {
    help();
    return 0;
  }
  int p = 2;
  while (p < argc) {
    if (strcmp(argv[p], "-no-cv") == 0) {
      no_cv = true;
    } else if (strcmp(argv[p], "-only-int") == 0) {
      only_int = true;
    } else if (strcmp(argv[p], "-param") == 0) {
      ++p;
      if (p >= argc || !to_int(argv[p], &param_max) || param_max > 32) {
        help();
        return 0;
      }
    } else if (strcmp(argv[p], "-out") == 0) {
      ++p;
      if (p >= argc) {
        help();
        return 0;
      }
      out_file = argv[p];
    } else {
      help();
      return 0;
    }
    ++p;
  }
  if (out_file.empty()) {
    std::cout << "typedef " << generate_type("T", depth) << ";" << std::endl;
  } else {
    std::ofstream of(out_file);
    of << "typedef " << generate_type("T", depth) << ";" << std::endl;
    of.close();
  }

  return 0;
}