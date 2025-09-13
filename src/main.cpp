#include <bits/stdc++.h>
using namespace std;

#if __has_include(<sysexits.h>)
  #include <sysexits.h>
  // Map to sysexits when available
  constexpr int EC_OK        = EXIT_SUCCESS;
  constexpr int EC_USAGE     = EX_USAGE;       // bad CLI usage
  constexpr int EC_IO        = EX_NOINPUT;     // input file problems
  constexpr int EC_COMPILE   = EX_DATAERR;     // parse/type errors
  constexpr int EC_TOOLCHAIN = EX_UNAVAILABLE; // assembler/linker not available
#else
  enum : int {
    EC_OK        = 0,
    EC_USAGE     = 2, // bad CLI usage
    EC_IO        = 3, // input file/read error
    EC_COMPILE   = 1, // compile (syntax/semantic) error
    EC_TOOLCHAIN = 4  // assembler/linker failures
  };
#endif

struct Src {
  string s; size_t i=0; explicit Src(string t): s(move(t)) {}
  char p(size_t k=0)const{ return (i+k<s.size())? s[i+k] : '\0'; }
  char g(){ return p()? s[i++] : '\0'; }
  void skip(){
    while(true){
      while(isspace((unsigned char)p())) g();
      if(p()=='/'&&p(1)=='/'){ while(p()&&p()!='\n') g(); continue; }
      if(p()=='/'&&p(1)=='*'){ g();g(); while(p()){ if(p()=='*'&&p(1)=='/'){g();g();break;} g(); } continue; }
      break;
    }
  }
};

bool idStart(char c){ return isalpha((unsigned char)c)||c=='_'; }
bool idChar (char c){ return isalnum((unsigned char)c)||c=='_'; }
string ident(Src& z){ if(!idStart(z.p())) throw runtime_error("expected identifier");
  string r; r.push_back(z.g()); while(idChar(z.p())) r.push_back(z.g()); return r; }
long long number(Src& z){ if(!isdigit((unsigned char)z.p())) throw runtime_error("expected integer");
  long long v=0; while(isdigit((unsigned char)z.p())) v=v*10+(z.g()-'0'); return v; }
void expect(Src& z, char c){ z.skip(); if(z.p()!=c) throw runtime_error(string("expected '")+c+"'"); z.g(); }

long long parseExpr(Src& z, unordered_map<string,long long>& env);

long long factor(Src& z, unordered_map<string,long long>& env){
  z.skip();
  if(isdigit((unsigned char)z.p())) return number(z);
  if(idStart(z.p())){ string n=ident(z); if(!env.count(n)) throw runtime_error("undeclared: "+n); return env[n]; }
  if(z.p()=='('){ z.g(); auto v=parseExpr(z,env); expect(z,')'); return v; }
  throw runtime_error("expected factor");
}
long long term(Src& z, unordered_map<string,long long>& env){
  long long v=factor(z,env);
  while(true){
    z.skip(); char c=z.p();
    if(c=='*'||c=='/'){ z.g(); long long r=factor(z,env);
      if(c=='*') v*=r; else{ if(r==0) throw runtime_error("division by zero"); v/=r; } }
    else break;
  }
  return v;
}
long long parseExpr(Src& z, unordered_map<string,long long>& env){
  long long v=term(z,env);
  while(true){
    z.skip(); char c=z.p();
    if(c=='+'||c=='-'){ z.g(); long long r=term(z,env); v = (c=='+')? v+r : v-r; }
    else break;
  }
  return v;
}

int main(int argc, char** argv){
  ios::sync_with_stdio(false); cin.tie(nullptr);

  if(argc!=2){
    cerr << "usage: mint <input.mint>\n";
    return EC_USAGE;
  }

  ifstream in(argv[1]);
  if(!in){
    cerr << "cannot open file\n";
    return EC_IO;
  }

  string src((istreambuf_iterator<char>(in)), {});
  Src z(src);
  unordered_map<string,long long> env;
  optional<long long> retv;

  try{
    while(true){
      z.skip(); if(!z.p()) break;
      if(!idStart(z.p())) throw runtime_error("expected statement");
      string kw = ident(z);

      if(kw=="let"){
        z.skip(); string name = ident(z);
        expect(z,'='); long long val = parseExpr(z,env); expect(z,';');
        env[name]=val; continue;
      }
      if(kw=="yield"){
        expect(z,'('); long long val = parseExpr(z,env); expect(z,')'); expect(z,';');
        retv = val; break;
      }
      throw runtime_error("unexpected identifier: "+kw);
    }
    if(!retv.has_value()) retv = 0;

    long long exit_code = ((*retv % 256) + 256) % 256;

    ofstream asmfile("out.asm");
    asmfile <<
R"(global _start
section .text
_start:
    mov rax, 60
    mov rdi, )" << exit_code << R"(
    syscall
)";
    asmfile.close();

    int a = system("nasm -felf64 out.asm -o out.o");
    if(a!=0){
      throw runtime_error("toolchain: nasm failed");
    }

    int l = system("x86_64-linux-gnu-gcc -nostdlib -static -Wl,-e,_start -o out out.o");
    if(l!=0){
      l = system("x86_64-linux-gnu-ld -m elf_x86_64 -static -e _start -o out out.o");
      if(l!=0) throw runtime_error("toolchain: link failed");
    }

    return EC_OK;

  } catch(const exception& e){
    string msg = e.what();
    cerr << msg << "\n";
    if(msg.rfind("toolchain:", 0) == 0) return EC_TOOLCHAIN;
    return EC_COMPILE;
  }
}