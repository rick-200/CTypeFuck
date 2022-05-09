#include <cassert>
#include <iostream>
#include <random>
#include <string>
#include <vector>
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
};
class NormalType : public Type {
  std::string _s;

 public:
  NormalType(const std::string& s) : _s(s) {}
  virtual std::string GetDef(const std::string& name) const override {
    std::string ret = _s;
    if (!name.empty()) ret.append(" ").append(name);
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
  virtual std::string GetDef(const std::string& name) const override {
    if (_inner->GetID() == TypeID::FuncPointer) {
      std::string s = "*";
      if (_const) s.append("const ");
      if (_volatile) s.append("volatile ");
      s.append(name);
      return _inner->GetDef(s);
    }
    std::string ret = _inner->GetDef("").append("*");
    if (_const) ret.append(" const");
    if (_volatile) ret.append(" volatile");
    if (!name.empty()) ret.append(" ").append(name);
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
  std::string AsReturn(const FuncPointer* outer,
                       const std::string& name) const {
    assert(!name.empty());
    if (_ret->GetID() == TypeID::FuncPointer)
      return static_cast<FuncPointer*>(_ret.get())->AsReturn(this, name);
    return _ret->GetDef("").append(
        GetDefWithoutReturnType(outer->GetDefWithoutReturnType(name)));
  }
  virtual std::string GetDef(const std::string& name) const override {
    assert(!name.empty());
    if (_ret->GetID() == TypeID::FuncPointer)
      return static_cast<FuncPointer*>(_ret.get())->AsReturn(this, name);
    return _ret->GetDef("").append(GetDefWithoutReturnType(name));
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
  std::string s = type_s[random(0, sizeof(type_s) / sizeof(void*) - 1)];
  if (random(0.3)) s += " const";
  if (random(0.3)) s += " volatile";
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
  int cnt = random(0, 5);
  std::vector<std::unique_ptr<Type>> params;
  for (int i = 0; i < cnt; i++)
    params.push_back(_generate_type(dep + 1, dep_max));
  return std::make_unique<FuncPointer>(_generate_type(dep + 1, dep_max),
                                       std::move(params));
}
std::unique_ptr<Type> _generate_type_not_funcptr(int dep, int dep_max) {
  if (dep >= dep_max) return generate_normal_type();
  switch (random(0, 1)) {
    case 0:
      return generate_normal_type();
    case 1:
      return generate_normal_pointer_type(dep, dep_max);
  }
}
std::unique_ptr<Type> _generate_type(int dep, int dep_max) {
  if (dep >= dep_max) return generate_normal_type();
  if (random(0.8)) return generate_func_pointer_type(dep, dep_max);
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
int main() {
  int dep;
  std::cin >> dep;
  std::cout << "typedef " << generate_type("T", dep) << ";" << std::endl;
  return 0;
}