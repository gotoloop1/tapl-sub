#include <bits/stdc++.h>
#include <variant>
#include "json11.hpp"
using namespace std;
using namespace json11;

class WrongFormat{};
class WrongType{};

class Primitive;
class Lambda;
class Record;
class Variant;
using TypeRaw = variant<Primitive, Lambda, Record, Variant>;
using Type = shared_ptr<TypeRaw>;

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
class Record {
public:
  map<string, Type> body;
  Record(map<string, Type> b): body(b) {}
};
class Variant {
public:
  map<string, Type> body;
  Variant(map<string, Type> b): body(b) {}
};

template<typename T, typename R> R eval1_(function<R(T*)> f, Type a, R d) {
	if(auto* p = get_if<T>(a.get())) {
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
	if(auto* p = get_if<T>(a.get())) {
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
	if(auto* p = get_if<T>(a.get())) {
		if(auto* q = get_if<U>(b.get())) {
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

bool operator<=(Type a, Type b) {
  return
    eval2([](Primitive* x, Primitive* y){
      return x->name == y->name;
    }, a, b) ||
    eval2([](Lambda* x, Lambda* y){
      return y->arg <= x->arg && x->body <= y->body;
    }, a, b) ||
    eval2([](Record* x, Record* y){
      for(auto i : y->body) {
        auto it = x->body.find(i.first);
        if(it == x->body.end() || !(it->second <= i.second)) {
          return false;
        }
      }
      return true;
    }, a, b) ||
    eval2([](Variant* x, Variant* y){
      for(auto i : x->body) {
        auto it = y->body.find(i.first);
        if(it == y->body.end() || !(i.second <= it->second)) {
          return false;
        }
      }
      return true;
    }, a, b);
}
bool operator>=(Type a, Type b) {
  return b <= a;
}
bool operator==(Type a, Type b) {
  return a <= b && b <= a;
}
bool operator!=(Type a, Type b) {
  return !(a == b);
}

pair<string, bool> print_(const Type& t) {
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
  auto rr = eval1([](Record* x){
    string r;
    r += "{";
    int c = 0;
    for(auto i : x->body) {
      r += i.first + ": " + print_(i.second).first;
      if(c < (int)x->body.size() - 1) {
        r += ", ";
      }
      c++;
    }
    r += "}";
    return make_pair(r, true);
  }, t, make_pair((string)"", false));
  auto rv = eval1([](Variant* x){
    string r;
    r += "<";
    int c = 0;
    for(auto i : x->body) {
      r += i.first + ": " + print_(i.second).first;
      if(c < (int)x->body.size() - 1) {
        r += ", ";
      }
      c++;
    }
    r += ">";
    return make_pair(r, true);
  }, t, make_pair((string)"", false));
  return make_pair(
    rp.first + rl.first + rr.first + rv.first,
    rp.second || rl.second || rr.second || rv.second
  );
}
string print(const Type& t) {
  return print_(t).first;
}

Type annotation(const Json& code) {
  string rule = code["rule"].string_value();
  if(rule == "lambda") {
    return wrap(Lambda(annotation(code["arg"]), annotation(code["body"])));
  }
  if(rule == "primitive") {
    return wrap(Primitive(code["value"].string_value()));
  }
  if(rule == "record") {
    map<string, Type> m;
    for(auto i : code["body"].array_items()) {
      if(m.find(i["label"].string_value()) != m.end()) {
        throw WrongFormat();
      }
      m[i["label"].string_value()] = annotation(i["type"]);
    }
    return wrap(Record(m));
  }
  if(rule == "variant") {
    map<string, Type> m;
    for(auto i : code["body"].array_items()) {
      m[i["label"].string_value()] = annotation(i["type"]);
    }
    return wrap(Variant(m));
  }
  throw WrongFormat();
}

Type join(Type a, Type b);
Type meet(Type a, Type b) {
  auto rp = eval2([&a](Primitive* x, Primitive* y){
    if(x->name == y->name) {
      return make_pair(true, a);
    }
    return make_pair(false, Type());
  }, a, b);
  if(rp.first) {
    return rp.second;
  }
  auto rl = eval2([](Lambda* x, Lambda* y){
    return make_pair(true, wrap(Lambda(join(x->arg, y->arg), meet(x->body, y->body))));
  }, a, b);
  if(rl.first) {
    return rl.second;
  }
  auto rr = eval2([](Record* x, Record* y){
    map<string, Type> m;
    for(auto i : x->body) {
      auto it = y->body.find(i.first);
      if(it == y->body.end()) {
        m[i.first] = i.second;
      }
      else {
        m[i.first] = meet(i.second, it->second);
      }
    }
    for(auto i : y->body) {
      auto it = x->body.find(i.first);
      if(it == x->body.end()) {
        m[i.first] = i.second;
      }
    }
    return make_pair(true, wrap(Record(m)));
  }, a, b);
  if(rr.first) {
    return rr.second;
  }
  auto rv = eval2([](Variant* x, Variant* y){
    map<string, Type> m;
    for(auto i : x->body) {
      auto it = y->body.find(i.first);
      if(it != y->body.end()) {
        try {
          m[i.first] = meet(i.second, it->second);
        }
        catch(WrongType _) {}
      }
    }
    return make_pair(true, wrap(Variant(m)));
  }, a, b);
  if(rv.first) {
    return rv.second;
  }
  throw WrongType();
}
Type join(Type a, Type b) {
  auto rp = eval2([&a](Primitive* x, Primitive* y){
    if(x->name == y->name) {
      return make_pair(true, a);
    }
    return make_pair(false, Type());
  }, a, b);
  if(rp.first) {
    return rp.second;
  }
  auto rl = eval2([](Lambda* x, Lambda* y){
    return make_pair(true, wrap(Lambda(meet(x->arg, y->arg), join(x->body, y->body))));
  }, a, b);
  if(rl.first) {
    return rl.second;
  }
  auto rr = eval2([](Record* x, Record* y){
    map<string, Type> m;
    for(auto i : x->body) {
      auto it = y->body.find(i.first);
      if(it != y->body.end()) {
        try {
          m[i.first] = join(i.second, it->second);
        }
        catch(WrongType _) {}
      }
    }
    return make_pair(true, wrap(Record(m)));
  }, a, b);
  if(rr.first) {
    return rr.second;
  }
  auto rv = eval2([](Variant* x, Variant* y){
    map<string, Type> m;
    for(auto i : x->body) {
      auto it = y->body.find(i.first);
      if(it == y->body.end()) {
        m[i.first] = i.second;
      }
      else {
        m[i.first] = join(i.second, it->second);
      }
    }
    for(auto i : y->body) {
      auto it = x->body.find(i.first);
      if(it == x->body.end()) {
        m[i.first] = i.second;
      }
    }
    return make_pair(true, wrap(Variant(m)));
  }, a, b);
  if(rv.first) {
    return rv.second;
  }
  throw WrongType();
}

bool unique_check(const map<string, Type>& context, string name) {
  return context.find(name) == context.end();
}
Type check(const Json& code, map<string, Type>& context) {
  string rule = code["rule"].string_value();
  if(rule == "let") {
    string name;
    if(code["var"]["rule"].string_value() == "variable") {
      name = code["var"]["value"].string_value();
    }
    if(name != "" && unique_check(context, name)) {
      auto t = check(code["content"], context);
      context[name] = t;
      auto r = check(code["body"], context);
      context.erase(name);
      return r;
    }
    throw WrongFormat();
  }
  if(rule == "lambda") {
    string name;
    Type argt;
    if(
      code["arg"]["rule"].string_value() == "annotation" &&
      code["arg"]["body"]["rule"].string_value() == "variable"
    ) {
      name = code["arg"]["body"]["value"].string_value();
      argt = annotation(code["arg"]["type"]);
    }
    if(name != "" && unique_check(context, name)) {
      context[name] = argt;
      auto r = check(code["body"], context);
      context.erase(name);
      return wrap(Lambda(argt, r));
    }
    throw WrongFormat();
  }
  if(rule == "apply") {
    auto funct = check(code["func"], context);
    auto argt = check(code["arg"], context);
    auto r = eval1([&argt](Lambda* l){
      if(argt <= l->arg) {
        return make_pair(true, l->body);
      }
      return make_pair(false, Type());
    }, funct);
    if(r.first) {
      return r.second;
    }
    throw WrongType();
  }
  if(rule == "variable") {
    string name = code["value"].string_value();
    if(context.find(name) != context.end()) {
      return context[name];
    }
    throw WrongFormat();
  }
  if(rule == "constant") {
    return wrap(Primitive(code["type"].string_value()));
  }
  if(rule == "record") {
    map<string, Type> m;
    for(auto i : code["body"].array_items()) {
      if(m.find(i["label"].string_value()) != m.end()) {
        throw WrongFormat();
      }
      m[i["label"].string_value()] = check(i["content"], context);
    }
    return wrap(Record(m));
  }
  if(rule == "projrcd") {
    auto vart = check(code["var"], context);
    auto r = eval1([&code](Record* r){
      auto it = r->body.find(code["label"].string_value());
      if(it != r->body.end()) {
        return make_pair(true, it->second);
      }
      return make_pair(false, Type());
    }, vart);
    if(r.first) {
      return r.second;
    }
    throw WrongType();
  }
  if(rule == "variant") {
    map<string, Type> m;
    m[code["label"].string_value()] = check(code["content"], context);
    return wrap(Variant(m));
  }
  if(rule == "if") {
    auto condt = check(code["cond"], context);
    if(condt == wrap(Primitive("Bool"))) {
      auto t = check(code["true"], context);
      auto f = check(code["false"], context);
      return join(t, f);
    }
  }
  if(rule == "case") {
    auto t = check(code["content"], context);
    auto r = eval1([&code, &context](Variant* v){
      map<string, pair<string, Json>> m;
      for(auto i : code["body"].array_items()) {
        string name = i["label"].string_value();
        if(m.find(name) != m.end()) {
          throw WrongFormat();
        }
        m[name] = make_pair(i["var"].string_value(), i["content"]);
      }
      Type t = nullptr;
      for(auto i : v->body) {
        auto it = m.find(i.first);
        if(it != m.end()) {
          string name = it->second.first;
          if(unique_check(context, name)) {
            context[name] = i.second;
            auto r = check(it->second.second, context);
            context.erase(name);
            if(t == nullptr) {
              t = r;
            }
            else {
              t = join(t, r);
            }
            continue;
          }
        }
        throw WrongType();
      }
      if(v->body.size() < m.size()) {
        cerr << "there are unused case" << endl;
      }
      if(t == nullptr) {
        throw WrongType();
      }
      return make_pair(true, t);
    }, t);
    if(r.first) {
      return r.second;
    }
    throw WrongType();
  }
  throw WrongFormat();
}

int main() {
  string json; getline(cin, json);
  string error;
  auto code = Json::parse(json, error);
  for(auto i : code.array_items()) {
    try{
      map<string, Type> context;
      cout << print(check(i, context)) << endl;
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
