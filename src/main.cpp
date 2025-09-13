#include <bits/stdc++.h>
using namespace std;

#if __has_include(<sysexits.h>)
  #include <sysexits.h>
  constexpr int EC_OK        = EXIT_SUCCESS;
  constexpr int EC_USAGE     = EX_USAGE;       // bad CLI usage
  constexpr int EC_IO        = EX_NOINPUT;     // input file problems
  constexpr int EC_COMPILE   = EX_DATAERR;     // parse/type errors
  constexpr int EC_TOOLCHAIN = EX_UNAVAILABLE; // assembler/linker not available
#else
  enum : int {
    EC_OK        = 0,
    EC_USAGE     = 2,
    EC_IO        = 3,
    EC_COMPILE   = 1,
    EC_TOOLCHAIN = 4
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

static inline bool idStart(char c){ return isalpha((unsigned char)c)||c=='_'; }
static inline bool idChar (char c){ return isalnum((unsigned char)c)||c=='_'; }

static string ident(Src& z){
  z.skip(); if(!idStart(z.p())) throw runtime_error("expected identifier");
  string r; r.push_back(z.g()); while(idChar(z.p())) r.push_back(z.g()); return r;
}
static long long number(Src& z){
  z.skip(); if(!isdigit((unsigned char)z.p())) throw runtime_error("expected integer");
  long long v=0; while(isdigit((unsigned char)z.p())) v=v*10+(z.g()-'0'); return v;
}
static void expect(Src& z, char c){
  z.skip(); if(z.p()!=c) throw runtime_error(string("expected '")+c+"'");
  z.g();
}

struct Emitter {
  ostream& out;
  unordered_set<string> vars; // names declared via 'let'
  explicit Emitter(ostream& o): out(o) {}
  string slot(const string& n) const { return "var_"+n; }
  void noteVar(const string& n){ vars.insert(n); }
};

static void genExpr(Src& z, Emitter& em, const unordered_set<string>& declared);
static void genTerm(Src& z, Emitter& em, const unordered_set<string>& declared);
static void genFactor(Src& z, Emitter& em, const unordered_set<string>& declared);

static void genFactor(Src& z, Emitter& em, const unordered_set<string>& declared){
  z.skip();
  if(isdigit((unsigned char)z.p())){
    long long v = number(z);
    em.out << "    mov rax, " << v << "\n";
    return;
  }
  if(idStart(z.p())){
    string n = ident(z);
    if(!declared.count(n)) throw runtime_error("undeclared: "+n);
    em.out << "    mov rax, [" << em.slot(n) << "]\n";
    return;
  }
  if(z.p()=='('){
    z.g();
    genExpr(z, em, declared);
    expect(z,')');
    return;
  }
  throw runtime_error("expected factor");
}

static void genTerm(Src& z, Emitter& em, const unordered_set<string>& declared){
  genFactor(z, em, declared);
  while(true){
    z.skip(); char c=z.p();
    if(c=='*' || c=='/'){
      z.g();
      em.out << "    push rax\n";
      genFactor(z, em, declared);
      em.out << "    mov rbx, rax\n";
      em.out << "    pop rax\n";
      if(c=='*'){
        em.out << "    imul rax, rbx\n";
      }else{
        em.out << "    cqo\n";
        em.out << "    idiv rbx\n";
      }
    } else break;
  }
}

static void genExpr(Src& z, Emitter& em, const unordered_set<string>& declared){
  genTerm(z, em, declared);
  while(true){
    z.skip(); char c=z.p();
    if(c=='+' || c=='-'){
      z.g();
      em.out << "    push rax\n";
      genTerm(z, em, declared);
      em.out << "    mov rbx, rax\n";
      em.out << "    pop  rax\n";
      if(c=='+') em.out << "    add rax, rbx\n";
      else       em.out << "    sub rax, rbx\n";
    } else break;
  }
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

  stringstream body;
  Emitter em(body);
  unordered_set<string> declared;
  bool saw_yield = false;

  try{
    while(true){
      z.skip(); if(!z.p()) break;
      if(!idStart(z.p())) throw runtime_error("expected statement");
      string kw = ident(z);

      if(kw=="let"){
        z.skip(); string name = ident(z);
        expect(z,'=');
        genExpr(z, em, declared);
        expect(z,';');
        em.noteVar(name);
        declared.insert(name);
        body << "    mov [" << em.slot(name) << "], rax\n";
        continue;
      }

      if(kw=="yield"){
        expect(z,'(');
        genExpr(z, em, declared);
        expect(z,')'); expect(z,';');
        // exit( (RAX) & 255 )
        body << "    and rax, 255\n";
        body << "    mov rdi, rax\n";
        body << "    mov rax, 60\n";
        body << "    syscall\n";
        saw_yield = true;
        continue;
      }

      throw runtime_error("unexpected identifier: "+kw);
    }

    ofstream asmfile("out.asm");
    if(!asmfile){ cerr<<"cannot create out.asm\n"; return EC_IO; }

    asmfile << "global _start\n";
    if(!em.vars.empty()){
      asmfile << "section .bss\n";
      for(const auto& v: em.vars) asmfile << em.slot(v) << ": resq 1\n";
    }
    asmfile << "section .text\n_start:\n";
    asmfile << body.str();
    if(!saw_yield){
      asmfile << "    mov rdi, 0\n    mov rax, 60\n    syscall\n";
    }
    asmfile.close();

    int a = system("nasm -felf64 out.asm -o out.o");
    if(a!=0) throw runtime_error("toolchain: nasm failed");

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