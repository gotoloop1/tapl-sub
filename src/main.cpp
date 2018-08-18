#include <bits/stdc++.h>
#include <variant>
#include "json11.hpp"
using namespace std;
using namespace json11;

template<typename T> vector<T> operator+(const vector<T>& a, const vector<T>& b) {
  vector<T> r = a;
  r.insert(r.end(), b.begin(), b.end());
  return r;
}

class WrongFormat{};
class WrongType{};

class Primitive;
class Lambda;
class Unknown;
using TypeRaw = variant<Primitive, Lambda, Unknown>;
using Type = shared_ptr<TypeRaw>;
using Condition = vector<pair<Type, Type>>;

template<typename T> Type wrap(T x) {
  return Type(new TypeRaw(x));
}

class Primitive {
public:
  string name;
  Primitive(string n): name(n) {}
};
class Lambda {
public:
  Type arg, body;
  Lambda(Type a, Type b): arg(a), body(b) {}
};
class Unknown {
public:
  static int count;
  int num;
  string name;
  Type link;
  Unknown(): num(count++) {}
};
int Unknown::count = 0;

Type find_parent(Type t) {
  if(auto* p = get_if<Unknown>(t.get())) {
    if(p->link == nullptr) {
      return t;
    }
    return p->link = find_parent(p->link);
	}
  else {
    return t;
  }
}

template<typename T, typename R> R eval1_(function<R(T*)> f, Type a, R d) {
	if(auto* p = get_if<T>(find_parent(a).get())) {
		return f(p);
	}
	return d;
}
template<typename T, typename R> R eval1_(function<R(T*)> f, Type a) {
  return eval1_(f, a, R());
}
template<typename T, typename R> R eval1(T f, Type a, R d) {
	return eval1_(function(f), a, d);
}
template<typename T> auto eval1(T f, Type a) -> decltype(eval1_(function(f), a)) {
	return eval1_(function(f), a);
}

template<typename T, typename U, typename R> R eval1p1_(function<R(T*, U)> f, Type a, U b, R d) {
	if(auto* p = get_if<T>(find_parent(a).get())) {
		return f(p, b);
	}
	return d;
}
template<typename T, typename U, typename R> R eval1p1_(function<R(T*, U)> f, Type a, U b) {
	return eval1p1_(f, a, b, R());
}
template<typename T, typename U, typename R> R eval1p1(T f, Type a, U b, R d) {
	return eval1p1_(function(f), a, b, d);
}
template<typename T, typename U> auto eval1p1(T f, Type a, U b) -> decltype(eval1p1_(function(f), a, b)) {
	return eval1p1_(function(f), a, b);
}

template<typename T, typename U, typename R> R eval2_(function<R(T*, U*)> f, Type a, Type b, R d) {
	if(auto* p = get_if<T>(find_parent(a).get())) {
		if(auto* q = get_if<U>(find_parent(b).get())) {
			return f(p, q);
		}
	}
	return d;
}
template<typename T, typename U, typename R> R eval2_(function<R(T*, U*)> f, Type a, Type b) {
  return eval2_(f, a, b, R());
}
template<typename T, typename R> R eval2(T f, Type a, Type b, R d) {
	return eval2_(function(f), a, b, d);
}
template<typename T> auto eval2(T f, Type a, Type b) -> decltype(eval2_(function(f), a, b)) {
	return eval2_(function(f), a, b);
}

bool operator==(Type a, Type b) {
  return
    eval2([](Primitive* x, Primitive* y){
      return x->name == y->name;
    }, a, b) ||
    eval2([](Lambda* x, Lambda* y){
      return x->arg == y->arg && x->body == y->body;
    }, a, b) ||
    eval2([](Unknown* x, Unknown* y){
      return x->num == y->num;
    }, a, b);
}
bool operator!=(Type a, Type b) {
  return !(a == b);
}

pair<string, bool> print_(const Type& t) {
  static int now = 0;
  auto rp = eval1([](Primitive* x){
    return make_pair(x->name, true);
  }, t, make_pair((string)"", false));
  auto rl = eval1([](Lambda* x){
    auto ar = print_(x->arg);
    auto br = print_(x->body);
    if(ar.second) {
      return make_pair(ar.first + " -> " + br.first, false);
    }
    else {
      return make_pair("(" + ar.first + ") -> " + br.first, false);
    }
  }, t, make_pair((string)"", false));
  auto ru = eval1([](Unknown* x){
    if(x->name == "") {
      x->name = "`" + string(1, 'A' + now);
      now++;
    }
    return make_pair(x->name, true);
  }, t, make_pair((string)"", false));
  return make_pair(rp.first + rl.first + ru.first, rp.second || rl.second || ru.second);
}
string print(const Type& t) {
  return print_(t).first;
}

Type annotation(const Json& code) {
  string rule = code["rule"].string_value();
  if(rule == "lambda") {
    return wrap(Lambda(annotation(code["arg"]), annotation(code["body"])));
  }
  else if(rule == "primitive") {
    return wrap(Primitive(code["value"].string_value()));
  }
  else {
    throw WrongFormat();
  }
}

void unify(Condition c);

Type type_clone_(Type x, int b, map<int, Type>& to) {
  return
    eval1([&x](Primitive* _){return x;}, x,
    eval1([&b, &to](Lambda* l){return wrap(Lambda(type_clone_(l->arg, b, to), type_clone_(l->body, b, to)));}, x,
    eval1([&x, &b, &to](Unknown* u){
      if(u->num < b) {
        if(to.find(u->num) == to.end()) {
          to[u->num] = wrap(Unknown());
        }
        return to[u->num];
      }
      else {
        return x;
      }
    }, x
  )));
}
Type type_clone(Type x) {
  map<int, Type> to;
  return type_clone_(x, Unknown::count, to);
}

bool unique_check(const map<string, Type>& context, const map<string, Type>& def, string name) {
  return context.find(name) == context.end() && def.find(name) == def.end();
}
pair<Type, Condition> check(const Json& code, map<string, Type>& context, map<string, Type>& def) {
  string rule = code["rule"].string_value();
  if(rule == "let") {
    string name;
    Type argt;
    if(
      code["var"]["rule"].string_value() == "annotation" &&
      code["var"]["body"]["rule"].string_value() == "variable"
    ) {
      name = code["var"]["body"]["value"].string_value();
      argt = annotation(code["var"]["type"]);
    }
    else if(code["var"]["rule"].string_value() == "variable") {
      name = code["var"]["value"].string_value();
      argt = wrap(Unknown());
    }
    if(name != "" && unique_check(context, def, name)) {
      auto r = check(code["content"], context, def);
      r.second.push_back(make_pair(r.first, argt));
      unify(r.second);
      context[name] = r.first;
      auto ret = check(code["body"], context, def);
      context.erase(name);
      return ret;
    }
    throw WrongFormat();
  }
  else if(rule == "lambda") {
    string name;
    Type argt;
    if(
      code["arg"]["rule"].string_value() == "annotation" &&
      code["arg"]["body"]["rule"].string_value() == "variable"
    ) {
      name = code["arg"]["body"]["value"].string_value();
      argt = annotation(code["arg"]["type"]);
    }
    else if(code["arg"]["rule"].string_value() == "variable") {
      name = code["arg"]["value"].string_value();
      argt = wrap(Unknown());
    }
    if(name != "" && unique_check(context, def, name)) {
      context[name] = argt;
      auto r = check(code["body"], context, def);
      context.erase(name);
      return make_pair(wrap(Lambda(argt, r.first)), r.second);
    }
    throw WrongFormat();
  }
  else if(rule == "apply") {
    auto funct = check(code["func"], context, def);
    auto argt = check(code["arg"], context, def);
    auto ret = wrap(Unknown());
    return make_pair(
      ret,
      argt.second +
      funct.second +
      Condition{make_pair(funct.first, wrap(Lambda(argt.first, ret)))}
    );
  }
  else if(rule == "variable") {
    string name = code["value"].string_value();
    if(context.find(name) != context.end()) {
      return make_pair(context[name], Condition());
    }
    else if(def.find(name) != def.end()) {
      return make_pair(type_clone(def[name]), Condition());
    }
    throw WrongFormat();
  }
  else if(rule == "constant") {
    return make_pair(wrap(Primitive(code["type"].string_value())), Condition());
  }
  else {
    throw WrongFormat();
  }
}

bool is_contain(Unknown* x, Type a) {
  return
    eval1([](Primitive* p){return false;}, a) ||
    eval1([&x](Lambda* l){return is_contain(x, l->arg) || is_contain(x, l->body);}, a) ||
    eval1([&x](Unknown* u){return x->num == u->num;}, a);
}

void unify(Condition c) {
  while(!c.empty()) {
    auto s = c.back().first;
    auto t = c.back().second;
    c.pop_back();
    if(s == t) {
      continue;
    }
    auto f = [](Unknown* x, Type a){
      if(!is_contain(x, a)) {
        x->link = a;
        return true;
      }
      return false;
    };
    if(eval1p1(f, s, t)) {
      continue;
    }
    if(eval1p1(f, t, s)) {
      continue;
    }
    if(eval2([&c](Lambda* x, Lambda* y){
      c.push_back(make_pair(x->arg, y->arg));
      c.push_back(make_pair(x->body, y->body));
      return true;
    }, s, t)) {
      continue;
    }
    throw WrongType();
  }
}

int main() {
  string json; getline(cin, json);
  string error;
  auto code = Json::parse(json, error);
  map<string, Type> def;
  for(auto i : code.array_items()) {
    try{
      map<string, Type> context;
      if(i["rule"].string_value() == "def") {
        string name;
        Type argt;
        if(
          i["var"]["rule"].string_value() == "annotation" &&
          i["var"]["body"]["rule"].string_value() == "variable"
        ) {
          name = i["var"]["body"]["value"].string_value();
          argt = annotation(i["var"]["type"]);
        }
        else if(i["var"]["rule"].string_value() == "variable") {
          name = i["var"]["value"].string_value();
          argt = wrap(Unknown());
        }
        if(name != "" && unique_check(context, def, name)) {
          auto r = check(i["content"], context, def);
          r.second.push_back(make_pair(r.first, argt));
          unify(r.second);
          def[name] = r.first;
          continue;
        }
        throw WrongFormat();
      }
      else {
        auto r = check(i, context, def);
        unify(r.second);
        cout << print(r.first) << endl;
        continue;
      }
    }
    catch(WrongFormat) {
      cout << "wrong format" << endl;
    }
    catch(WrongType) {
      cout << "wrong type" << endl;
    }
    catch(...) {
      cout << "wrong program" << endl;
    }
  }
}
