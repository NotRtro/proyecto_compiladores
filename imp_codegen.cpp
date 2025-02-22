#include "imp_codegen.hh"

void ImpCodeGen::codegen(string label, string instr) {
  if (label !=  nolabel)
    code << label << ": ";
  code << instr << endl;
}

void ImpCodeGen::codegen(string label, string instr, int arg) {
  if (label !=  nolabel)
    code << label << ": ";
  code << instr << " " << arg << endl;
}

void ImpCodeGen::codegen(string label, string instr, string jmplabel) {
  if (label !=  nolabel)
    code << label << ": ";
  code << instr << " " << jmplabel << endl;
}

string ImpCodeGen::next_label() {
  string l = "L";
  string n = to_string(current_label++);
  l.append(n);
  return l;
}

void ImpCodeGen::codegen(Program* p, string outfname) {
  nolabel = "";
  current_label = 0;
  siguiente_direccion = 0;
  mem_locals = 0;
  p->accept(this);
  ofstream outfile;
  outfile.open(outfname);
  outfile << code.str();
  outfile.close();
  cout << "Memoria variables locales: " << mem_locals << endl;
  return;
}

int ImpCodeGen::visit(Program* p) {
  p->body->accept(this);
  return 0;
}

int ImpCodeGen::visit(Body * b) {
  int dir = siguiente_direccion;
  direcciones.add_level();  
  b->var_decs->accept(this);
  b->slist->accept(this);
  direcciones.remove_level();
  if (siguiente_direccion > mem_locals) mem_locals = siguiente_direccion;
  siguiente_direccion = dir;
  return 0;
}

int ImpCodeGen::visit(VarDecList* s) {
  list<VarDec*>::iterator it;
  for (it = s->vdlist.begin(); it != s->vdlist.end(); ++it) {
    (*it)->accept(this);
  }  
  return 0;
}
			  
int ImpCodeGen::visit(VarDec* vd) {
  list<string>::iterator it;
  for (it = vd->vars.begin(); it != vd->vars.end(); ++it){
    direcciones.add_var(*it, siguiente_direccion++);
  }
  return 0;
}

int ImpCodeGen::visit(StatementList* s) {
  list<Stm*>::iterator it;
  for (it = s->slist.begin(); it != s->slist.end(); ++it) {
    (*it)->accept(this);
  }
  return 0;
}

int ImpCodeGen::visit(AssignStatement* s) {
  s->rhs->accept(this);
  codegen(nolabel,"store",direcciones.lookup(s->id));
  return 0;
}

int ImpCodeGen::visit(PrintStatement* s) {
  s->e->accept(this);
  code << "print" << endl;;
  return 0;
}

int ImpCodeGen::visit(IfStatement* s) {
  string l1 = next_label();
  string l2 = next_label();
  
  s->cond->accept(this);
  codegen(nolabel,"jmpz",l1);
  s->tbody->accept(this);
  codegen(nolabel,"goto",l2);
  codegen(l1,"skip");
  if (s->fbody!=NULL) {
    s->fbody->accept(this);
  }
  codegen(l2,"skip");
 
  return 0;
}

int ImpCodeGen::visit(WhileStatement* s) {//3
  string l1 = next_label();
  string l2 = next_label();

  buclepila.push_back(LoopEntry(s,l1,l2));

  codegen(l1,"skip");
  s->cond->accept(this);
  codegen(nolabel,"jmpz",l2);
  s->body->accept(this);
  codegen(nolabel,"goto",l1);
  codegen(l2,"skip");
  buclepila.pop_back();
  return 0;
}

int ImpCodeGen::visit(ForStatement* s) {//p2
  int sig_dic= siguiente_direccion;
  direcciones.add_level();
  direcciones.add_var(s->id, siguiente_direccion++);
  
  s->e1->accept(this);
  codegen(nolabel,"store",direcciones.lookup(s->id));

  s->e2->accept(this);

  string lcheck = next_label();
  string lend= next_label();
  int iterator_adress= direcciones.lookup(s->id);
  buclepila.push_back(LoopEntry(s,lcheck,lend));
  codegen(lcheck,"skip");
  codegen(nolabel,"dup");
  codegen(nolabel,"load",iterator_adress);
  codegen(nolabel,"ge");
  codegen(nolabel,"jmpz",lend);
  s->body->accept(this);
  codegen(nolabel,"load",iterator_adress);
  codegen(nolabel,"push",1);
  codegen(nolabel,"add");
  codegen(nolabel,"store",iterator_adress);
  codegen(nolabel,"goto",lcheck);
  codegen(lend,"skip");
  codegen(nolabel,"pop");
  direcciones.remove_level();
  siguiente_direccion=sig_dic;
  buclepila.pop_back();
  return 0;
}

int ImpCodeGen::visit(DowhileStatement* s) {//p2
  string l1 = next_label();
  string l2 = next_label();
  string l3 = next_label();
  buclepila.push_back(LoopEntry(s,l3,l2));
  codegen(l1,"skip");
  s->body->accept(this);
  codegen(l3,"skip");
  s->cond->accept(this);
  codegen(nolabel,"jmpn",l1);
  codegen(l2,"skip");
  buclepila.pop_back();
  return 0;
}

int ImpCodeGen::visit(LoopSkipStatement* s){
  if (s->type == LoopSkipType::BREAK)
    codegen(nolabel,"goto",buclepila.back().lbreak);
  else
    codegen(nolabel,"goto",buclepila.back().lcont);
  return 0;
}

int ImpCodeGen::visit(BinaryExp* e) {
  e->left->accept(this);
  e->right->accept(this);
  string op = "";
  switch(e->op) {
  case PLUS: op =  "add"; break;
  case MINUS: op = "sub"; break;
  case MULT:  op = "mul"; break;
  case DIV:  op = "div"; break;
  case LT:  op = "lt"; break;
  case LTEQ: op = "le"; break;
  case EQ:  op = "eq"; break;
  case AND:  op = "and"; break;//p2
  case OR:  op = "or"; break;//p2
  
  default: cout << "binop " << Exp::binopToString(e->op) << " not implemented" << endl;
  }
  codegen(nolabel, op);
  return 0;
}

int ImpCodeGen::visit(UnaryExp* e) {
  e->e->accept(this);
  if (e->op == NEG) {
    codegen(nolabel,"push",0);
    codegen(nolabel,"swap");
    codegen(nolabel,"sub");
  } else {
    string l1 = next_label();
    string l2 = next_label();
    codegen(nolabel,"jmpz",l1);
    codegen(nolabel,"push", 0);
    codegen(nolabel,"goto", l2);
    codegen(l1,"skip", 0);
    codegen(nolabel,"push", 1);
    codegen(l2,"skip");
  }
  return 0;
}

int ImpCodeGen::visit(NumberExp* e) {
  codegen(nolabel,"push ",e->value);
  return 0;
}

int ImpCodeGen::visit(BoolConstExp* e) {//pregunta2
  if(e->b)
    codegen(nolabel,"push",1);
  else
    codegen(nolabel,"push",0);
  return 0;
}

int ImpCodeGen::visit(IdExp* e) {
  codegen(nolabel,"load",direcciones.lookup(e->id));
  return 0;
}

int ImpCodeGen::visit(ParenthExp* ep) {
  ep->e->accept(this);
  return 0;
}

int ImpCodeGen::visit(CondExp* e) {
  string l1 = next_label();
  string l2 = next_label();
 
  e->cond->accept(this);
  codegen(nolabel, "jmpz", l1);
  e->etrue->accept(this);
  codegen(nolabel, "goto", l2);
  codegen(l1,"skip");
  e->efalse->accept(this);
  codegen(l2, "skip");
  return 0;
}
