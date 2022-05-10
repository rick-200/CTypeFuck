#include <cassert>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

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
  virtual std::string GetDef(const std::string& name) const = 0;
  virtual TypeID GetID() const = 0;
  virtual std::string AsReturn(const std::string& outer) const = 0;
};
class NormalType : public Type {
  std::string _s;

 public:
  NormalType(const std::string& s) : _s(s) {}
  NormalType(std::string&& s) : _s(std::move(s)) {}
  virtual std::string GetDef(const std::string& name) const override {
    std::string ret = _s;
    if (!name.empty()) ret.append(" ").append(name);
    return ret;
  }
  virtual std::string AsReturn(const std::string& outer) const override {
    return _s + outer;
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
  virtual std::string GetDef(const std::string& name) const override {
    assert(_inner->GetID() != TypeID::FuncPointer);
    std::string ret = _inner->GetDef("").append("*");
    if (_const) ret.append(" const");
    if (_volatile) ret.append(" volatile");
    if (!name.empty()) ret.append(" ").append(name);
    return ret;
  }
  virtual std::string AsReturn(const std::string& outer) const override {
    return GetDef("") + outer;
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
  std::string ParamsToString() const {
    std::string s = "(";
    for (size_t i = 0; i < _params.size(); i++) {
      s.append(_params[i]->GetDef("p" + std::to_string(i)));
      if (i != _params.size() - 1) s.append(", ");
    }
    s.append(")");
    return s;
  }
  std::string GetDefWithoutReturnType(const std::string& inner) const {
    return std::string("(*").append(inner).append(")").append(ParamsToString());
  }
  virtual std::string AsReturn(const std::string& outer) const override {
    return _ret->AsReturn(GetDefWithoutReturnType(outer));
  }
  virtual std::string GetDef(const std::string& name) const override {
    return _ret->AsReturn(GetDefWithoutReturnType(name));
  }
  virtual TypeID GetID() const override { return TypeID::FuncPointer; }
};
std::unique_ptr<NormalType> generate_normal_type() {
  const char* type_s[] = {"char",
                          "short",
                          "int",
                          "long",
                          "long long",
                          "unsigned char",
                          "unsigned short",
                          "unsigned int",
                          "unsigned long",
                          "unsigned long long",
                          "float",
                          "double",
                          "long double"};
  std::string s =
      only_int ? "int" : type_s[random(0, sizeof(type_s) / sizeof(void*) - 1)];
  if (!no_cv && random(0.3)) s.append(" const");
  if (!no_cv && random(0.3)) s.append(" volatile");
  return std::make_unique<NormalType>(std::move(s));
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
  return _generate_type(1, dep_max)->GetDef(name);
}
void help() {
  std::cout << "usage: ctypefuck <depth> [flags]" << std::endl;
  std::cout << "depth: recursive depth" << std::endl;
  std::cout << "flages:" << std::endl;
  std::cout << "-no-cv : do not generate const and volidate modifier"
            << std::endl;
  std::cout << "-only-int : restrict type except function pointer to int" << std::endl;
  std::cout << "-param <max_count> : set maxium parameter amount for "
               "generated function pointer"
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
      if (p >= argc || !to_int(argv[p], &param_max)) {
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