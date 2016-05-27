#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>

enum type_t { no, num, k, id, str };
const char *types[] = { "null", "number", "key", "identifier", "string" };
const int buf_size = 256;
const char *keywords[] = { "if", "then", "else", "while", "do", "buy",
	"sell", "prod", "build", "print", "turn", "==", "!=", ">=",
	"<=", 0 };

struct Lexem {
	unsigned line;
	type_t type;
	char *str;	
	Lexem(const char *s, unsigned l, type_t t) : line(l), type(t)
		{ str = new char[strlen(s)+1]; strcpy(str, s); }
	Lexem(const Lexem& L);
	~Lexem() { delete[] str; }
	bool operator==(const char *s) { return (strcmp(str, s) == 0); }
	bool operator!=(const char *s) { return (strcmp(str, s) != 0); }
};

struct lexem_list {
	Lexem *l;
	lexem_list *next;
};

const char *msg[] = {
	"Unexpected end of file",
	"Too long lexem",
	"Error in integer constant",
	"Wrong keyword",
	"Unmatching brackets",
	"Function name without parentheses",
	"Unmatching parentheses",
	"Invalid expression",
	"Invalid_variable",
	"Missing relation sign",
	"Missing '=' sign in assignment statement",
	"Missing ';' at the end of the statement",
	"Missing 'then' within conditional statement",
	"Missing ',' between arguments",
	"Unmathcing curly braces",
	"Missing curly brace at the begining of the block",
	"Missing 'do' within loop statement",
	"Invalid Statement",
	"No such function"
};

enum errors { unexpected_eof, too_long_lexem, wrong_number, wrong_keyword,
	unmatching_brackets, no_paren, unmatching_paren,
	incorrect_expr, incorrect_var, no_rel_sign, no_assign,
	no_semicolon, no_then, no_comma, unmatching_curly, no_curly, no_do,
	invalid_statement, nonexistent_function
};

class Error {
	errors code;
	unsigned line;
	char *s;
public:
	Error(errors n, unsigned l, const char *in = ""): code(n), line(l)
		{ s = new char[strlen(in)+1]; strcpy(s, in); }
	void PrintError() const
	{
		fprintf(stderr, "error on line %d:", line); 
		if (s[0])
			fprintf(stderr, "'%s': ", s);
		fprintf(stderr, " %s \n", msg[code]);
	}
	~Error() { delete[] s; }
};

class LexicalAnalizer {
	enum st { home, number, identifier, string, key, sep,
		sep2, error };
	Lexem *lexem;
	bool is_ready;
	st state;
	char buf[buf_size];
	int pos;
	int current_char, saved_char;
	unsigned current_line;
	type_t current_type;
	Error *err;

public:
	LexicalAnalizer(): lexem(0), is_ready(false), state(home), pos(0),
		current_char('\0'), saved_char(' '), current_line(1),
		err(0) { memset(buf, 0, buf_size); }
	void FeedChar(char c) { current_char = c; Step(); saved_char = c; }
	Lexem *GetLexem();
	Error *GetError() const { return (state == error ? err : 0); }
private:
	void ClearBuf() { pos = 0; };
	void LoadLexem();
	void Push(char c);
	bool IsDelimiter(char c), IsSep(char c);
	bool IsKey(const char *s);
	void Step();
	void Home(), Key(),
		Number(), Sep(), Sep2(), String(), Identifier();
};

typedef long long Int;

struct variable_tab {
	char *name;
	Int val;
	variable_tab *next;
};

class VariableTable {
	variable_tab *first;
public:
	VariableTable(): first(0) {}
	variable_tab* Get(const char *name);
	~VariableTable()
	{
		while (first) {
			variable_tab *t = first->next;
			delete[] first->name;
			delete first;
			first = t;
		}
	}
private:
	variable_tab* Include(const char *name);
	variable_tab* Search(const char *name);
};

struct Status {
	int month;
	int pl_count;
	int sell_n;
	int min_price;
	int buy_n;
	int max_price;
	void Print();
};

enum auc_act { sold, bought };
struct Auction {
	int player;
	auc_act action;
	int cost;
	int ammount;
	void Print();
	Auction(int p, auc_act a, int am, int c): player(p), action(a),
		cost(c), ammount(am) {}
};

struct auction_list {
	Auction *auc;
	auction_list *next;
};

class Player {
protected:
	int number;
	int money;
	int products;
	int material;
	int factories;
	int building_factories;
	bool makes_turn;
public:
	Player(int n, int mon=10000, int prod=2, int mat=4, int fact=2,
		int build=0, bool mt=false) : number(n), money(mon),
		products(prod), material(mat), factories(fact),
		building_factories(build), makes_turn(mt) {}
	int GetProducts() const { return products; }
	int GetMoney() const { return money; }
	int GetFactories() const { return factories; }
	int GetMaterial() const { return material; }
	int GetNumber() const { return number; }
	bool IsTurning() const { return makes_turn; }
	void Print();
	~Player() {}
};
	
const int in_buf_size = 1024;
const int out_buf_size = 32;

enum finished { not_yet, victory, defeat };
class Robot: public Player {
	int sd;
	int position;
	int turn;
	int players_n;
	finished is_finished;
	auction_list *yesterday_auc;
	Player **today_players, **yesterday_players;
	Status *today_market;
	char in_buf[in_buf_size];
	char out_buf[out_buf_size];
public:
	Robot(int fd, int n, int k): Player(n), sd(fd), position(0),
		turn(1), players_n(k), is_finished(not_yet),
		yesterday_auc(0), today_players(0), yesterday_players(0),
		today_market(0)
	{
		memset(in_buf, 0, in_buf_size);
		memset(out_buf, 0, out_buf_size);
		Run();
	}
	void Produce(int k) { Send("prod", k); Update(); }
	void Buy(int k, int price) { Send("buy", k, price); Update(); }
	void Sell(int k, int price) { Send("sell", k, price); Update(); }
	void Build() { Send("build"); Update(); }
	void EndTurn();
	Status* MarketSituation();
	Player* PlayerInfo(int number);
	Player** AllPlayersInfo();
	auction_list* AuctionResults();
	void Update();
	void DeleteAuc(auction_list *ptr);
	int GetTurn() const { return turn; }
	int GetPlayers() const { return players_n; } 
	Player** GetTodayPlayers() const { return today_players; }
	Player** GetYesterdayPlayers() const { return yesterday_players; }
	auction_list* GetAuction() const { return yesterday_auc; }
	Status* GetMarket() const { return today_market; }
	finished IsFinished() const { return is_finished; }
private:
	void Send(const char *command, int arg1 = 0, int arg2 = 0);
	char* Recieve(const char *str1, const char *str2 = 0);
	void ShiftBuf(int from);
	void DeletePlayersInfo(Player **p);
	void Run();
	void NewMonth();
};

class IPNElem;
struct IPNItem {
	IPNElem *elem;
	IPNItem *next;
};

class IPNElem {
public:
	virtual void Evaluate(IPNItem **stack, IPNItem **cur_cmd, 
		VariableTable *vt, Robot& bot) const = 0;
	virtual ~IPNElem() {}
	virtual void Print() = 0;
protected:
	static void Push(IPNItem **stack, IPNElem *elem);
	static IPNElem *Pop(IPNItem **stack);
};

class SyntaxAnalizer {
	lexem_list *list;
	Lexem current;
	IPNItem *first, **cur_cmd;
public:
	SyntaxAnalizer(lexem_list *l): list(l), current(*list->l)
		{ first = new IPNItem; cur_cmd = &first; }
	void Run() { CompoundStatement(); printf("OK\n"); }
	IPNItem* GetIPN() { return first; }
private:
	void Next();
	IPNItem* NewCmd(IPNElem* p);
	IPNItem* HoldCmd();
	void StatementsList(), Statement(), PrintStatement(), PrintList(),
		PrintElem(), GameStatement(), WhileStatement(),
		IfStatement(), CompoundStatement(), AssignmentStatement(),
		Expression(), AndExpression(), RelationExpression(),
		RelExpr(), RelationSign(), SumExpression(),
		Sum(), MulExpression(), Mul(), UnaryExpression(),
		PrimeExpression(), FunctionCall(), ArgList(), Variable(),
		Index();
	bool IsRel();
};

class IPNEx {
public:
	virtual ~IPNEx() {}
	virtual void Print() = 0;
};

class IPNExNotLabel: public IPNEx {
	IPNElem *p;
public:
	IPNExNotLabel(IPNElem *a): p(a) {}
	virtual ~IPNExNotLabel() {}
	void Print()
	{
		printf("\n");
		p->Print();
		printf(" is not a label\n");
	}
};

class IPNExNotInt: public IPNEx {
	IPNElem *p;
public:
	IPNExNotInt(IPNElem *a): p(a) {}
	virtual ~IPNExNotInt() {}
	void Print()
	{
		printf("\n'");
		p->Print();
		printf("' is not an integer\n");
	}
};

class IPNExNotVar: public IPNEx {
	IPNElem *p;
public:
	IPNExNotVar(IPNElem *a): p(a) {}
	virtual ~IPNExNotVar() {}
	void Print()
	{
		printf("\n");
		p->Print();
		printf(" is not a variable\n");
	}
};

class IPNExNotForPrint: public IPNEx {
	IPNElem *p;
public:
	IPNExNotForPrint(IPNElem *a): p(a) {}
	virtual ~IPNExNotForPrint() {}
	void Print()
	{
		printf("\n");
		p->Print();
		printf(" is not for print\n");
	}
};

class IPNExZeroDivisor: public IPNEx {
public:
	virtual ~IPNExZeroDivisor() {}
	void Print() { printf("attempt to divide by zero\n"); }
};
	
class IPNConst: public IPNElem {
public:
	virtual IPNElem* Clone() const = 0;
	virtual ~IPNConst() {}
	void Evaluate(IPNItem **stack, IPNItem **cur_cmd,
		VariableTable *vt, Robot& bot) const
	{
		Push(stack, Clone());
		*cur_cmd = (*cur_cmd)->next;
	}
};

class IPNOperation: public IPNElem {
public:
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const = 0;
	virtual ~IPNOperation() {}
	virtual void Evaluate(IPNItem **stack, IPNItem **cur_cmd, 
		VariableTable *vt, Robot& bot) const
	{
		IPNElem *res = DoOperation(stack, vt, bot);
		if (res)
			Push(stack, res);
		*cur_cmd = (*cur_cmd)->next;
	}
};

class IPNInt: public IPNConst {
	Int value;
public:
	IPNInt(Int a): value(a) {}
	void Print() { printf("%lld", value); }
	virtual ~IPNInt() {}
	virtual IPNElem* Clone() const
		{ return new IPNInt(value); }
	Int Get() const { return value; }
};

class IPNLabel: public IPNConst {
	IPNItem *value;
public:
	IPNLabel(IPNItem *a): value(a) {}
	void Print() { printf("label %p", value); }
	virtual ~IPNLabel() {}
	virtual IPNElem* Clone() const
		{ return new IPNLabel(value); }
	IPNItem* Get() const { return value; }
};

class IPNString: public IPNConst {
	char *value;
public:
	IPNString(const char *s)
	{
		value = new char[strlen(s)+1];
		strcpy(value, s);
	}
	void Print() { printf("%s", value); }
	virtual ~IPNString() { delete[] value; }
	virtual IPNElem* Clone() const
		{ return new IPNString(value); }
	char* Get() const { return value; }
};

typedef IPNString IPNVarName;

class IPNVar: public IPNConst {
	variable_tab *value;
public:
	IPNVar(variable_tab *a): value(a) {}
	void Print() { printf("%s", value->name); }
	virtual ~IPNVar() {}
	virtual IPNElem* Clone() const
		{ return new IPNVar(value); }
	variable_tab* Get() const { return value; }
};

class IPNPrint: public IPNOperation {
public:
	virtual ~IPNPrint() {}
	void Print() { printf("print"); }
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		Write(stack);
		printf("\n");
		return 0;
	}
	void Write(IPNItem **stack) const
	{
		IPNElem *op;
		if ((op = Pop(stack))) {
			IPNInt *ip = dynamic_cast<IPNInt*>(op);
			IPNString *sp = dynamic_cast<IPNString*>(op);
			if (!ip && !sp)
				throw IPNExNotForPrint(op);
			Write(stack);
			if (ip)
				printf("%lld ", ip->Get());
			else
				printf("%s ", sp->Get());
			delete op;
		}
	}
};

class IPNOp2: public IPNOperation {
public:
	virtual ~IPNOp2() {}
	virtual Int Perform(Int op1, Int op2) const = 0; 
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		IPNElem *operand2 = Pop(stack);
		IPNInt *op2 = dynamic_cast<IPNInt*>(operand2);
		if (!op2)
			throw IPNExNotInt(operand2);
		IPNElem *operand1 = Pop(stack);
		IPNInt *op1 = dynamic_cast<IPNInt*>(operand1);
		if (!op1)
			throw IPNExNotInt(operand1);
		Int res = Perform(op1->Get(), op2->Get());
		delete operand1;
		delete operand2;
		return new IPNInt(res);
	}
};

class IPNGameOp2: public IPNOperation {
public:
	virtual ~IPNGameOp2() {}
	virtual void Perform(Robot& bot, Int op1, Int op2) const = 0; 
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		IPNElem *operand2 = Pop(stack);
		IPNInt *op2 = dynamic_cast<IPNInt*>(operand2);
		if (!op2)
			throw IPNExNotInt(operand2);
		IPNElem *operand1 = Pop(stack);
		IPNInt *op1 = dynamic_cast<IPNInt*>(operand1);
		if (!op1)
			throw IPNExNotInt(operand1);
		Perform(bot, op1->Get(), op2->Get());
		delete operand1;
		delete operand2;
		return 0;
	}
};

class IPNOp1: public IPNOperation {
public:
	virtual ~IPNOp1() {}
	virtual Int Perform(Int op) const = 0;
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		IPNElem *operand = Pop(stack);
		IPNInt *op = dynamic_cast<IPNInt*>(operand);
		if (!op)
			throw IPNExNotInt(operand);
		Int res = Perform(op->Get());
		delete operand;
		return new IPNInt(res);
	}
};

class IPNGo: public IPNElem {
public:
	void Print() { printf("go-operator"); }
	virtual ~IPNGo() {}
	virtual void Evaluate(IPNItem **stack, IPNItem **cur_cmd, 
		VariableTable *vt, Robot& bot) const
	{
		IPNElem *op1 = Pop(stack);
		IPNLabel *label = dynamic_cast<IPNLabel*>(op1);
		if (!label)
			throw IPNExNotLabel(op1);
		IPNItem *addr = label->Get();
		*cur_cmd = addr;
		delete op1;
	}
};

class IPNGoFalse: public IPNElem {
public:
	void Print() { printf("go-false-operator"); }
	virtual ~IPNGoFalse() {}
	virtual void Evaluate(IPNItem **stack, IPNItem **cur_cmd,
		VariableTable *vt, Robot& bot) const
	{
		IPNElem *where = Pop(stack);
		IPNLabel *label = dynamic_cast<IPNLabel*>(where);
		if (!label)
			throw IPNExNotLabel(where);
		IPNElem *condition = Pop(stack);
		IPNInt *cond = dynamic_cast<IPNInt*>(condition);
		if (!cond)
			throw IPNExNotInt(condition);
		if (!cond->Get()) {
			IPNItem *addr = label->Get();
			*cur_cmd = addr;
		} else {
			*cur_cmd = (*cur_cmd)->next;
		}
		delete condition;
		delete where;
	}
};
class IPNVarValue: public IPNOperation {
public:
	virtual ~IPNVarValue() {}
	void Print() { printf("var-value"); }
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		IPNElem *op = Pop(stack);
		IPNVar *var_addr = dynamic_cast<IPNVar*>(op);
		if (!var_addr)
			throw IPNExNotVar(op);
		IPNInt *res = new IPNInt(var_addr->Get()->val);
		delete op;
		return res;
	}
};

class IPNBrackets: public IPNOperation {
	virtual ~IPNBrackets() {}
	void Print() { printf("[]"); }
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		IPNElem *op2 = Pop(stack);
		IPNInt *idx = dynamic_cast<IPNInt*>(op2);
		if (!idx)
			throw IPNExNotInt(op2);
		IPNElem *op1 = Pop(stack);
		IPNVarName *var_name = dynamic_cast<IPNVarName*>(op1);
		if (!var_name)
			throw IPNExNotVar(op1);
		char *s = new char[64+strlen(var_name->Get())];
		sprintf(s, "%s[%lld]", var_name->Get(), idx->Get());
		delete op1;
		delete op2;
		return new IPNVarName(s);
	}
};
		
class IPNMakeVar: public IPNOperation {
	virtual ~IPNMakeVar() {}
	void Print() { printf("make-var"); }
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		IPNElem *op = Pop(stack);
		IPNVarName *var_name = dynamic_cast<IPNVarName*>(op);
		if (!var_name)
			throw IPNExNotVar(op);
		IPNVar *res = new IPNVar(vt->Get(var_name->Get()));
		delete op;
		return res;
	}
};

class IPNAssign: public IPNOperation {
	virtual ~IPNAssign() {}
	void Print() { printf("="); }
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		IPNElem *op2 = Pop(stack);
		IPNInt *val = dynamic_cast<IPNInt*>(op2);
		if (!val)
			throw IPNExNotInt(op2);
		IPNElem *op1 = Pop(stack);
		IPNVar *var = dynamic_cast<IPNVar*>(op1);
		if (!var)
			throw IPNExNotVar(op1);
		var->Get()->val = val->Get();
		delete op1;
		delete op2;
		return 0;
	}
};

class IPNPlus: public IPNOp2 {
public:
	void Print() { printf("+"); }
	virtual ~IPNPlus() {}
	Int Perform(Int op1, Int op2) const { return op1+op2; }
};

class IPNMinus: public IPNOp2 {
public:
	void Print() { printf("-"); }
	virtual ~IPNMinus() {}
	Int Perform(Int op1, Int op2) const { return op1-op2; }
};

class IPNMultiply: public IPNOp2 {
public:
	void Print() { printf("*"); }
	virtual ~IPNMultiply() {}
	Int Perform(Int op1, Int op2) const { return op1*op2; }
};

class IPNDivide: public IPNOp2 {
public:
	void Print() { printf("/"); }
	virtual ~IPNDivide() {}
	Int Perform(Int op1, Int op2) const
	{
		if (!op2)
			throw IPNExZeroDivisor();
		return op1/op2;
	}
};

class IPNMod: public IPNOp2 {
public:
	void Print() { printf("%%"); }
	virtual ~IPNMod() {}
	Int Perform(Int op1, Int op2) const
	{
		if (op2)
			throw IPNExZeroDivisor();
		return op1%op2;
	}
};

class IPNOr: public IPNOp2 {
public:
	void Print() { printf("|"); }
	virtual ~IPNOr() {}
	Int Perform(Int op1, Int op2) const { return op1 || op2; }
};

class IPNAnd: public IPNOp2 {
public:
	void Print() { printf("&"); }
	virtual ~IPNAnd() {}
	Int Perform(Int op1, Int op2) const { return op1 && op2; }
};

class IPNEqual: public IPNOp2 {
public:
	void Print() { printf("=="); }
	virtual ~IPNEqual() {}
	Int Perform(Int op1, Int op2) const { return op1 == op2; }
};

class IPNNotEqual: public IPNOp2 {
public:
	void Print() { printf("!="); }
	virtual ~IPNNotEqual() {}
	Int Perform(Int op1, Int op2) const { return op1 != op2; }
};

class IPNLess: public IPNOp2 {
public:
	void Print() { printf("<"); }
	virtual ~IPNLess() {}
	Int Perform(Int op1, Int op2) const { return op1 < op2; }
};

class IPNLessEqual: public IPNOp2 {
public:
	void Print() { printf("<="); }
	virtual ~IPNLessEqual() {}
	Int Perform(Int op1, Int op2) const { return op1 <= op2; }
};

class IPNGreater: public IPNOp2 {
public:
	void Print() { printf(">"); }
	virtual ~IPNGreater() {}
	Int Perform(Int op1, Int op2) const { return op1 > op2; }
};

class IPNGreaterEqual: public IPNOp2 {
public:
	void Print() { printf(">="); }
	virtual ~IPNGreaterEqual() {}
	Int Perform(Int op1, Int op2) const { return op1 >= op2; }
};

class IPNBuy: public IPNGameOp2 {
public:
	void Print() { printf("buy"); }
	virtual ~IPNBuy() {}
	void Perform(Robot& bot, Int op1, Int op2) const
	{
		bot.Buy(op1, op2);
	}
};

class IPNSell: public IPNGameOp2 {
public:
	void Print() { printf("sell"); }
	virtual ~IPNSell() {}
	void Perform(Robot& bot, Int op1, Int op2) const
	{
		bot.Sell(op1, op2);
	}
};

class IPNProd: public IPNOperation {
public:
	void Print() { printf("prod"); }
	virtual ~IPNProd() {}
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		IPNElem *operand = Pop(stack);
		IPNInt *op = dynamic_cast<IPNInt*>(operand);
		if (!op)
			throw IPNExNotInt(operand);
		bot.Produce(op->Get());
		delete operand;
		return 0;
	}
};

class IPNUnaryMinus: public IPNOp1 {
public:
	void Print() { printf("-"); }
	virtual ~IPNUnaryMinus() {}
	Int Perform(Int op) const { return -op; }
};

class IPNNot: public IPNOp1 {
public:
	void Print() { printf("!"); }
	virtual ~IPNNot() {}
	Int Perform(Int op) const { return !op; }
};

class IPNNoOp: public IPNOperation {
public:
	void Print() { printf("NoOp"); }
	virtual ~IPNNoOp() {}
	IPNElem* DoOperation(IPNItem **stack, VariableTable *vt,
		Robot& bot) const
	{
		return 0;
	}	
};

class IPNBuild: public IPNOperation {
public:
	void Print() { printf("build"); }
	virtual ~IPNBuild() {}
	IPNElem* DoOperation(IPNItem **stack, VariableTable *vt,
		Robot& bot) const
	{
		bot.Build();
		return 0;
	}
};

class IPNTurn: public IPNOperation {
public:
	void Print() { printf("turn"); }
	virtual ~IPNTurn() {}
	IPNElem* DoOperation(IPNItem **stack, VariableTable *vt,
		Robot& bot) const
	{
		bot.EndTurn();
		return 0;
	}
};

class IPNFuncMyId: public IPNOperation {
public:
	virtual void Print() { printf("?my_id"); }
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		return new IPNInt(bot.GetNumber());
	}
	virtual ~IPNFuncMyId() {}
};

class IPNFuncTurn: public IPNOperation {
public:
	virtual void Print() { printf("?turn"); }
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		return new IPNInt(bot.GetTurn());
	}
	virtual ~IPNFuncTurn() {}
};

class IPNFuncSupply: public IPNOperation {
public:
	virtual void Print() { printf("?supply"); }
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		return new IPNInt(bot.GetMarket()->sell_n);
	}
	virtual ~IPNFuncSupply() {}
};

class IPNFuncMaterialPrice: public IPNOperation {
public:
	virtual void Print() { printf("?material_price"); }
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		return new IPNInt(bot.GetMarket()->min_price);
	}
	virtual ~IPNFuncMaterialPrice() {}
};

class IPNFuncDemand: public IPNOperation {
public:
	virtual void Print() { printf("?demand"); }
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		return new IPNInt(bot.GetMarket()->buy_n);
	}
	virtual ~IPNFuncDemand() {}
};

class IPNFuncProductionPrice: public IPNOperation {
public:
	virtual void Print() { printf("?production_price"); }
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		return new IPNInt(bot.GetMarket()->max_price);
	}
	virtual ~IPNFuncProductionPrice() {}
};

class IPNFuncPlayers: public IPNOperation {
public:
	virtual void Print() { printf("?players"); }
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		return new IPNInt(bot.GetPlayers());
	}
	virtual ~IPNFuncPlayers() {}
};

class IPNFuncActivePlayers: public IPNOperation {
public:
	virtual void Print() { printf("?active_players"); }
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		return new IPNInt(bot.GetMarket()->pl_count);
	}
	virtual ~IPNFuncActivePlayers() {}
};

class IPNFuncIsFinished: public IPNOperation {
public:
	virtual void Print() { printf("?is_finished"); }
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		return new IPNInt((bot.IsFinished() == not_yet) ? 0 : 1);
	}
	virtual ~IPNFuncIsFinished() {}
};

class IPNFunc1: public IPNOperation {
public:
	virtual ~IPNFunc1() {}
	virtual Int Perform(Robot& bot, Int op) const = 0;
	virtual IPNElem*
		DoOperation(IPNItem **stack, VariableTable *vt,
			Robot& bot) const
	{
		IPNElem *operand = Pop(stack);
		IPNInt *op = dynamic_cast<IPNInt*>(operand);
		if (!op)
			throw IPNExNotInt(operand);
		Int res = Perform(bot, op->Get());
		delete operand;
		return new IPNInt(res);
	}
};

class IPNFuncMoney: public IPNFunc1 {
public:
	virtual void Print() { printf("?money"); }
	virtual Int Perform(Robot& bot, Int op) const
	{
		/*don't use today_players, need newer info*/
		Player *p = bot.PlayerInfo(op);
		if (!p)
			return 0;
		int res = p->GetMoney();
		delete p;
		return res;
	}
	virtual ~IPNFuncMoney() {}
};

class IPNFuncMaterial: public IPNFunc1 {
public:
	virtual void Print() { printf("?material"); }
	virtual Int Perform(Robot& bot, Int op) const
	{
		Player *p = bot.PlayerInfo(op);
		if (!p)
			return 0;
		int res = p->GetMaterial();
		delete p;
		return res;
	}
	virtual ~IPNFuncMaterial() {}
};

class IPNFuncProduction: public IPNFunc1 {
public:
	virtual void Print() { printf("?production"); }
	virtual Int Perform(Robot& bot, Int op) const
	{
		Player *p = bot.PlayerInfo(op);
		if (!p)
			return 0;
		int res = p->GetProducts();
		delete p;
		return res;
	}
	virtual ~IPNFuncProduction() {}
};


class IPNFuncFactories: public IPNFunc1 {
public:
	virtual void Print() { printf("?factories"); }
	virtual Int Perform(Robot& bot, Int op) const
	{
		Player *p = bot.PlayerInfo(op);
		if (!p)
			return 0;
		int res = p->GetFactories();
		delete p;
		return res;
	}
	virtual ~IPNFuncFactories() {}
};

class IPNFuncManufactured: public IPNFunc1 {
public:
	virtual void Print() { printf("?manufactured"); }
	virtual Int Perform(Robot& bot, Int op) const
	{
		if (!bot.GetYesterdayPlayers())
			return 0;
		Player *yest = (bot.GetYesterdayPlayers())[op-1];
		Player *today = (bot.GetTodayPlayers())[op-1];
		if (today) {
			Int old = yest->GetProducts();
			Int new_a = today->GetProducts();
			return new_a-old;
		} else
			return 0;
	}
	virtual ~IPNFuncManufactured() {}
};

class IPNFuncResultMaterialBought: public IPNFunc1 {
/*  number of material, bought from the bank */
public:
	virtual void Print() { printf("?result_material_bought"); }
	virtual Int Perform(Robot& bot, Int op) const
	{
		auction_list *a = bot.GetAuction();
		Int res = 0;
		while (a) {
			if (a->auc->player == op &&
				a->auc->action == bought)
			{
				res += a->auc->ammount;
			}
			a = a->next;
		}
		return res;
	}
	virtual ~IPNFuncResultMaterialBought() {}
};

class IPNFuncResultMaterialPrice: public IPNFunc1 {
/* price of material, bought from the bank */
public:
	virtual void Print() { printf("?result_material_price"); }
	virtual Int Perform(Robot& bot, Int op) const
	{
		auction_list *a = bot.GetAuction();
		Int res = 0;
		while (a) {
			if (a->auc->player == op &&
				a->auc->action == bought)
			{
				res = a->auc->cost;

			}
			a = a->next;
		}
		return res;
	}
	virtual ~IPNFuncResultMaterialPrice() {}
};
	
class IPNFuncResultProdSold: public IPNFunc1 {
/* number of production, sold to the bank */
public:
	virtual void Print() { printf("?result_prod_sold"); }
	virtual Int Perform(Robot& bot, Int op) const
	{
		auction_list *a = bot.GetAuction();
		Int res = 0;
		while (a) {
			if (a->auc->player == op &&
				a->auc->action == sold)
			{
				res += a->auc->ammount;
			}
			a = a->next;
		}
		return res;
	}
	virtual ~IPNFuncResultProdSold() {}
};

class IPNFuncResultProdPrice: public IPNFunc1 {
/* price of production, sold to the bank */
public:
	virtual void Print() { printf("?result_prod_price"); }
	virtual Int Perform(Robot& bot, Int op) const
	{
		auction_list *a = bot.GetAuction();
		Int res = 0;
		while (a) {
			if (a->auc->player == op &&
				a->auc->action == sold)
			{
				res += a->auc->cost;
			}
			a = a->next;
		}
		return res;
	}
	virtual ~IPNFuncResultProdPrice() {}
};

enum {bufsize = 128};

int take_a_number(int sd, int *players_n)
{
	char buf[bufsize];
	unsigned int n, k, rc;
	const char *greeting;
	rc = read(sd, buf, bufsize);
	if (1 != sscanf(buf, greeting = "Welcome to my game!\n"
		"Your number is %d\n"
		"Type 'help' to get help\n",
		&n))
	{
		throw "can't join the game\n";
	}
	if (rc <= strlen(greeting))
		read(sd, buf+rc, bufsize-rc);
	if (2 != sscanf(strstr(buf, "help\n")+strlen("help\n"),
		"Now there are %d/%d players",
		&k, players_n))
	{
		throw "can't join the game\n";
	}
	return n;
}



int validate(int argc, char **argv, sockaddr_in *addr)
{
	bool is_number(const char *);
	if (argc<4 || !inet_aton(argv[1], &(addr->sin_addr))
		|| !is_number(argv[2]))
	{
		fprintf(stderr, "Usage: %s ip port file\n", argv[0]);
		exit(1);
	}
	addr->sin_family = AF_INET;
	addr->sin_port = htons(atoi(argv[2]));
	return 0;
}
	
int create_connection(sockaddr_in *addr)
{
	int sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1)
		throw "can't create socket\n";
	if (connect(sd, (sockaddr *)addr, sizeof(*addr)) == -1)
		throw "can't connect to the server\n";
	return sd;
}
	

int lexical_go(FILE *f, lexem_list **list)
{
	LexicalAnalizer la;
	Lexem *ptr;
	unsigned line;
	while (!feof(f)) {
		int c = fgetc(f);
		la.FeedChar(c);
		if (Error *p = la.GetError()) {
			p->PrintError();
			return -1;
		}
		if ((ptr = la.GetLexem())) {
			*list = new lexem_list;
			(*list)->l = ptr;
			line = ptr->line;
			(*list)->next = 0;
			list = &((*list)->next);
		}
	}
	// add the sign of end of list
	*list = new lexem_list;
	(*list)->l = new Lexem("", line, no);
	(*list)->next = 0;
	return 0;
}

IPNItem* syntax_go(lexem_list *list)
{
	SyntaxAnalizer sa(list);
	try {
		sa.Run();
		IPNItem *ipn = sa.GetIPN();
		IPNItem *p = ipn;
		while (p) {
			p->elem->Print();
			printf("         \t %p\n", p);
			p = p->next;
		}
		return ipn;
	}
	catch (const Error& err) {
		err.PrintError();
		exit(1);
	}
}

void execute_ipn(IPNItem *ipn, Robot& robot)
{
	try {
		IPNItem *stack = 0;
		VariableTable vt;
		while (ipn)
			ipn->elem->Evaluate(&stack, &ipn, &vt, robot);
	}
	catch (IPNEx& p) {
		p.Print();
		exit(1);
	}
}	

void play_scenario(Robot& robot, const char *file)
{
	void delete_list(lexem_list *), delete_ipn(IPNItem *);
	FILE *f = fopen(file, "r");
	if (!f) {
		fprintf(stderr, "Bad file\n");
		exit(1);
	}
	lexem_list *list = 0;
	int k = lexical_go(f, &list);
	fclose(f);
	if (k == -1)
		exit(1);
	IPNItem *ipn = syntax_go(list);
	execute_ipn(ipn, robot);
	delete_ipn(ipn);
	delete_list(list);
}

int main(int argc, char **argv)
{
	sockaddr_in addr;
	try {
		validate(argc, argv, &addr);
		int sd = create_connection(&addr);
		int players_n;
		int num = take_a_number(sd, &players_n);
		Robot robot(sd, num, players_n);
		play_scenario(robot, argv[3]);
	}
	catch (const char *s) {
		fprintf(stderr, "%s", s);
		return 1;
	}
	return 0;
}	

Lexem::Lexem(const Lexem& l)
{
	str = new char[strlen(l.str)+1];
	strcpy(str, l.str);
	line = l.line;
	type = l.type;
}

Lexem* LexicalAnalizer::GetLexem()
{
	bool tmp = is_ready;
	is_ready = false;
	return (tmp ? lexem : 0);
}

void LexicalAnalizer::Step()
{
	switch (state) {
	case home:	Home(); return;
	case number:	Number(); return;
	case identifier: Identifier(); return;
	case string:	String(); return;
	case sep:	Sep(); return;
	case sep2:	Sep2(); return;
	case key:	Key(); return;
	case error:	return;
	}
}

void LexicalAnalizer::Push(char c)
{
	if (pos < buf_size - 1)
		buf[pos++] = c;
	else {
		state = error;
		//too long lexem
		err = new Error(too_long_lexem, current_line);
	}
}

bool LexicalAnalizer::IsSep(char c)
{
	return (c == '+' || c == '-' || c == '*' || c == '/' ||
		c == '%' || c == '<' || c == '>' || c == '=' ||
		c == '&' || c == '|' || c == '!' || c == '(' ||
		c == ')' || c == '[' || c == ']' || c == ';' ||
		c == '{' || c == '}' || c == ',');
}

bool LexicalAnalizer::IsDelimiter(char c)
{
	return (isspace(c) || c == '\"' || IsSep(c));
}

bool LexicalAnalizer::IsKey(const char *s)
{
	if (IsSep(s[0]) && s[1] == '\0')
		return true;
	for (unsigned i=0; keywords[i]; i++) {
		if (strcmp(s, keywords[i]) == 0)
			return true;
	}
	return false;
}

void LexicalAnalizer::LoadLexem()
{
	Push('\0');
	if (current_type == k && !IsKey(buf)) {
		err = new Error(wrong_keyword, current_line, buf);
		state = error;
		return;
	}
	lexem = new Lexem(buf, current_line, current_type);
	is_ready = true;
	ClearBuf();
}


void LexicalAnalizer::Home()
{
	if (current_char == EOF || IsDelimiter(current_char)) {
		if (!isspace(saved_char))
			LoadLexem();
		if (isspace(current_char)) {
			if (current_char == '\n')
				current_line++;
		} else if (IsSep(current_char)) {
			state = sep;
			Sep();
		} else if (current_char == '\"') {
			state = string;
		}
	} else {
		if (IsSep(saved_char)) {
			LoadLexem();
		}
		if (isalpha(current_char)) {
			state = key;
			Key();
		} else if (isdigit(current_char)) {
			state = number;
			Number();
		} else if (current_char == '$' || current_char == '?') {
			state = identifier;
			Identifier();
		}
	}

}

void LexicalAnalizer::Key()
{
	current_type = k;
	if (isalpha(current_char)) {
		Push(current_char);
	} else if (IsDelimiter(current_char)) {
		Push('\0');
		if (IsKey(buf)) {
			state = home;
			Home();
		} else {
			state = error;
			err = new Error(wrong_keyword, current_line, buf);
		}
	} else {
		state = error;
		//wrong keyword
		err = new Error(wrong_keyword, current_line);
	}
}

void LexicalAnalizer::Number()
{
	current_type = num;
	if (isdigit(current_char)) {
		Push(current_char);
	} else if (IsDelimiter(current_char)) {
		state = home;
		Home();
	} else {
		state = error;
		//wrong number
		err = new Error(wrong_number, current_line);
	}
}

void LexicalAnalizer::Sep()
{
	current_type = k;
	Push(current_char);
	if (current_char == '!' || current_char == '=' ||
		current_char == '<' || current_char == '>') {
		state = sep2;
	} else {
		state = home;
	}
}

void LexicalAnalizer::Sep2()
{
	if (current_char == '=') {
		Push(current_char);
		state = home;
	} else {
		state = home;
		Home();
	}
}

void LexicalAnalizer::String()
{
	current_type = str;
	if (current_char == '\"') {
		state = home;
	} else {
		Push(current_char);
	}
}

void LexicalAnalizer::Identifier()
{
	current_type = id;
	if (isalnum(current_char) || current_char == '$' ||
		current_char == '?' || current_char == '_')
	{
		Push(current_char);
	} else if (IsDelimiter(current_char)) {
		state = home;
		Home();
	}
}

void print_lexem_list(lexem_list *list)
{
	printf("%5s, %10s: %s\n", "line", "type", "lexem");
	while (list) {
		if (list->l->str[0]) {
			printf("%5d, %10s: %s\n", list->l->line,
				types[list->l->type], list->l->str);
		}
		list = list->next;
	}
}

void delete_list(lexem_list *list)
{
	if (list) {
		delete list->l;
		delete_list(list->next);
		delete list;
	}
}

void delete_ipn(IPNItem *ipn)
{
	if (ipn) {
		delete ipn->elem;
		delete_ipn(ipn->next);
		delete ipn;
	}
}

void SyntaxAnalizer::StatementsList()
{
	while (current != "}") {
		Statement();
	}
}

void SyntaxAnalizer::Statement()
{
	if (current == "print")
		PrintStatement();
	else if (current == "turn" || current == "prod" ||
		current == "buy" || current == "sell" ||
		current == "build")
	{
		GameStatement();
	} else if (current == "while")
		WhileStatement();
	else if (current == "if")
		IfStatement();
	else if (current == "{")
		CompoundStatement();
	else if (current.type == id)
		AssignmentStatement();
	else
		throw Error(invalid_statement, current.line, current.str);
}

void SyntaxAnalizer::PrintStatement()
{
	Next();
	PrintList();
	if (current != ";")
		throw Error(no_semicolon, current.line);
	NewCmd(new IPNPrint);
	Next();
}

void SyntaxAnalizer::PrintList()
{
	PrintElem();
	while (current == ",") {
		Next();
		PrintElem();
	}
}

void SyntaxAnalizer::PrintElem()
{
	if (current.type == str) {
		NewCmd(new IPNString(current.str));
		Next();
	} else {
		Expression();
	}
}

void SyntaxAnalizer::GameStatement()
{
	if (current == "turn") {
		Next();
		if (current != ";")
			throw Error(no_semicolon, current.line);
		NewCmd(new IPNTurn);
		Next();
	} else if (current == "build") {
		Next();
		if (current != ";")
			throw Error(no_semicolon, current.line);
		NewCmd(new IPNBuild);
		Next();
	} else if (current == "prod") {
		Next();
		SumExpression();
		if (current != ";")
			throw Error(no_semicolon, current.line);
		NewCmd(new IPNProd);
		Next();
	} else if (current == "sell") {
		Next();
		SumExpression();
		if (current != ",")
			throw Error(no_comma, current.line);
		Next();
		SumExpression();
		if (current != ";")
			throw Error(no_semicolon, current.line);
		NewCmd(new IPNSell);
		Next();
	} else if (current == "buy") {
		Next();
		SumExpression();
		if (current != ",")
			throw Error(no_comma, current.line);
		Next();
		SumExpression();
		if (current != ";")
			throw Error(no_semicolon, current.line);
		NewCmd(new IPNBuy);
		Next();
	}
}

void SyntaxAnalizer::WhileStatement()
{
	Next();
	IPNItem *condition_addr = NewCmd(new IPNNoOp);
	Expression();
	if (current != "do")
		throw Error(no_do, current.line);
	IPNItem *label = HoldCmd();
	NewCmd(new IPNGoFalse);
	Next();
	Statement();
	NewCmd(new IPNLabel(condition_addr));
	NewCmd(new IPNGo);
	label->elem = new IPNLabel(NewCmd(new IPNNoOp));
}

void SyntaxAnalizer::IfStatement()
{
	Next();
	Expression();
	if (current != "then")
		throw Error(no_then, current.line);
	Next();
	IPNItem *label1 = HoldCmd();
	IPNItem *label2 = label1;
	NewCmd(new IPNGoFalse);
	Statement();
	if (current == "else") {
		label2 = HoldCmd();
		NewCmd(new IPNGo);
		IPNItem *addr1 = NewCmd(new IPNNoOp);
		label1->elem = new IPNLabel(addr1);
		Next();
		Statement();
	}
	IPNItem *addr2 = NewCmd(new IPNNoOp);
	label2->elem = new IPNLabel(addr2);
	
}

void SyntaxAnalizer::CompoundStatement()
{
	if (current != "{")
		throw Error(no_curly, current.line);
	Next();
	StatementsList();
	if (current != "}")
		throw Error(unmatching_curly, current.line);
	Next();
}

void SyntaxAnalizer::AssignmentStatement()
{
	Variable();
	if (current != "=")
		throw Error(no_assign, current.line, current.str);
	Next();
	Expression();
	if (current != ";")
		throw Error(no_semicolon, current.line);
	Next();
	NewCmd(new IPNAssign);
}

void SyntaxAnalizer::Expression()
{
	AndExpression();
	while (current == "|") {
		Next();
		AndExpression();
		NewCmd(new IPNOr);
	}
}

void SyntaxAnalizer::AndExpression()
{
	RelationExpression();
	while (current == "&") {
		Next();
		RelationExpression();
		NewCmd(new IPNAnd);
	}
}

void SyntaxAnalizer::RelationExpression()
{
	SumExpression();
	RelExpr();
}

void SyntaxAnalizer::RelExpr()
{
	if (IsRel()) {
		const char *s = current.str;
		Next();
		SumExpression();
		if (strcmp(s, "==") == 0)
			NewCmd(new IPNEqual);
		else if (strcmp(s,"!=") == 0)
			NewCmd(new IPNNotEqual);
		else if (strcmp(s, "<") == 0)
			NewCmd(new IPNLess);
		else if (strcmp(s, "<=") == 0)
			NewCmd(new IPNLessEqual);
		else if (strcmp(s, ">") == 0)
			NewCmd(new IPNGreater);
		else if (strcmp(s, ">=") == 0)
			NewCmd(new IPNGreaterEqual);
	}
}

bool SyntaxAnalizer::IsRel()
{
	if (current != "==" && current != "!=" && current != ">" &&
		current != "<" && current != ">=" && current != "<=")
	{
		return false;
	} else
		return true;
}

void SyntaxAnalizer::SumExpression()
{
	MulExpression();
	Sum();
}

void SyntaxAnalizer::Sum()
{
	if (current == "+") {
		Next();
		MulExpression();
		NewCmd(new IPNPlus);
		Sum();
	} else if (current == "-") {
		Next();
		MulExpression();
		NewCmd(new IPNMinus);
		Sum();
	}
}

void SyntaxAnalizer::MulExpression()
{
	UnaryExpression();
	Mul();
}

void SyntaxAnalizer::Mul()
{
	if (current == "*") {
		Next();
		UnaryExpression();
		NewCmd(new IPNMultiply);
		Mul();
	} else if (current == "/") {
		Next();
		UnaryExpression();
		NewCmd(new IPNDivide);
		Mul();
	} else if (current == "%") {
		Next();
		UnaryExpression();
		NewCmd(new IPNMod);
		Mul();
	}
}

void SyntaxAnalizer::UnaryExpression()
{
	if (current == "-") {
		Next();
		PrimeExpression();
		NewCmd(new IPNUnaryMinus);
	} else if (current == "!") {
		Next();
		PrimeExpression();
		NewCmd(new IPNNot);
	} else {
		PrimeExpression();
	}
}

void SyntaxAnalizer::PrimeExpression()
{
	if (current == "(") {
		Next();
		Expression();
		if (current != ")")
			throw Error(unmatching_paren, current.line);
		Next();
	} else if (current.type == num) {
		NewCmd(new IPNInt(atoll(current.str)));
		Next();
	} else if (current.type == id) {
		if (current.str[0] == '$') {
			Variable();
			NewCmd(new IPNVarValue);
		} else if (current.str[0] == '?') {
			FunctionCall();
		}
	} else if (current.type == str) {
		NewCmd(new IPNString(current.str));
		Next();
	} else
		throw Error(incorrect_expr, current.line, current.str);
}

void SyntaxAnalizer::FunctionCall()
{
	char *s = current.str;
	Next();
	if (current != "(")
		throw Error(no_paren, current.line);
	Next();
	ArgList();
	if (current != ")")
		throw Error(unmatching_paren, current.line);
	Next();
	if (strcmp(s, "?my_id") == 0)
		NewCmd(new IPNFuncMyId);
	else if (strcmp(s, "?turn") == 0)
		NewCmd(new IPNFuncTurn);
	else if (strcmp(s, "?players") == 0)
		NewCmd(new IPNFuncPlayers);
	else if (strcmp(s, "?active_players") == 0)
		NewCmd(new IPNFuncActivePlayers);
	else if (strcmp(s, "?is_finished") == 0)
		NewCmd(new IPNFuncIsFinished);
	else if (strcmp(s, "?supply") == 0)
		NewCmd(new IPNFuncSupply);
	else if (strcmp(s, "?material_price") == 0)
		NewCmd(new IPNFuncMaterialPrice);
	else if (strcmp(s, "?demand") == 0)
		NewCmd(new IPNFuncDemand);
	else if (strcmp(s, "?production_price") == 0)
		NewCmd(new IPNFuncProductionPrice);
	else if (strcmp(s, "?money") == 0)
		NewCmd(new IPNFuncMoney);
	else if (strcmp(s, "?material") == 0)
		NewCmd(new IPNFuncMaterial);
	else if (strcmp(s, "?production") == 0)
		NewCmd(new IPNFuncProduction);
	else if (strcmp(s, "?factories") == 0)
		NewCmd(new IPNFuncFactories);
	else if (strcmp(s, "?manufactured") == 0)
		NewCmd(new IPNFuncManufactured);
	else if (strcmp(s, "?result_material_bought") == 0)
		NewCmd(new IPNFuncResultMaterialBought);
	else if (strcmp(s, "?result_material_price") == 0)
		NewCmd(new IPNFuncResultMaterialPrice);
	else if (strcmp(s, "?result_prod_sold") == 0)
		NewCmd(new IPNFuncResultProdSold);
	else if (strcmp(s, "?result_prod_price") == 0)
		NewCmd(new IPNFuncResultProdPrice);
	else
		throw Error(nonexistent_function, current.line);
}

void SyntaxAnalizer::ArgList()
{
	while (current != ")") {
		Expression();
		while (current == ",") {
			Next();
			Expression();
		}
	}
}

void SyntaxAnalizer::Variable()
{
	if (current.type != id || current.str[0] != '$')
		throw Error(incorrect_var, current.line); 
	NewCmd(new IPNVarName(current.str));
	Next();
	Index();
	NewCmd(new IPNMakeVar);
}

void SyntaxAnalizer::Index()
{
	if (current == "[") {
		Next();
		SumExpression();
		if (current != "]")
			throw Error(unmatching_brackets, current.line);
		NewCmd(new IPNBrackets);
		Next();
	}
}

void SyntaxAnalizer::Next()
{ 
	if (list->next) {
		list = list->next;
		current = *list->l;
	}
}

IPNItem* SyntaxAnalizer::NewCmd(IPNElem *p)
{
	if (!*cur_cmd)
		*cur_cmd = new IPNItem;
	(*cur_cmd)->elem = p;
	(*cur_cmd)->next = 0;
	IPNItem *t = *cur_cmd;
	cur_cmd = &(*cur_cmd)->next;
	return t;
}	

IPNItem* SyntaxAnalizer::HoldCmd()
{
	*cur_cmd = new IPNItem;
	(*cur_cmd)->elem = 0;
	(*cur_cmd)->next = 0;
	IPNItem *t = *cur_cmd;
	cur_cmd = &(*cur_cmd)->next;
	return t;
}

variable_tab* VariableTable::Include(const char *name)
{
	if (!Search(name)) {
		variable_tab *p = new variable_tab;
		p->name = new char[strlen(name)+1];
		strcpy(p->name, name);
		p->val = 0;
		p->next = first;
		first = p;
		return p;
	}
	return 0;
}

variable_tab* VariableTable::Search(const char *name)
{
	variable_tab *p = first;
	while (p) {
		if (strcmp(p->name, name) == 0)
			return p;
		p = p->next;
	}
	return 0;
}

variable_tab* VariableTable::Get(const char *name)
{
	variable_tab *p = Search(name);
	if (!p)
		p = Include(name);
	return p;
}

void IPNElem::Push(IPNItem **stack, IPNElem *elem)
{
	IPNItem *t = new IPNItem;
	t->elem = elem;
	t->next = *stack;
	*stack = t;
}

IPNElem* IPNElem::Pop(IPNItem **stack)
{
	if (!*stack)
		return 0;
	IPNElem *t = (*stack)->elem;
	IPNItem *p = (*stack)->next;
	delete *stack;
	*stack = p;
	return t;
}

void Robot::Send(const char *command, int arg1, int arg2)
{
	strcpy(out_buf, command);
	if (arg1!=0) {
		char s[10];
		sprintf(s, " %d", arg1);
		strcat(out_buf, s);
		if (arg2!=0) {
			sprintf(s, " %d", arg2);
			strcat(out_buf, s);
		}
	}
	strcat(out_buf, "\n");
	write(sd, out_buf, strlen(out_buf));
}
	
char* Robot::Recieve(const char *str1, const char *str2)
{
	const char *bankrupt = "YOU ARE BANKRUPT";
	const char *win = "You are winner";
	char *ptr = 0, *q = 0;
	while (position == 0 || !(ptr = strstr(in_buf, str1))) {
		if (!ptr) {
			if (str2) {
				q = strstr(in_buf, str2);
				if (q) {
					ShiftBuf(q-in_buf+strlen(str2));
					return in_buf;
				}
			}
			if (strstr(in_buf, bankrupt)) {
				is_finished = defeat;
				return 0;
			}
			if (strstr(in_buf, win)) {
				is_finished = victory;
				return 0;
			}
			int rc = read(sd, in_buf+position,
				in_buf_size-position);
			position += rc;
			in_buf[position] = '\0';
		}
	}
	ShiftBuf(ptr-in_buf+strlen(str1));
	return in_buf;
}

void Player::Print()
{
	printf("Player %d has: dollars product material factories "
		"building; makes turn\n"
		"\t      %d \t %d \t %d\t  %d \t   %d\t     %s \n",
		number, money, products, material, factories,
		building_factories, (makes_turn ? "yes" : "no"));
}

void Robot::ShiftBuf(int from)
{
	int i,j;
	for (i=0,j=from; j<position; i++,j++)
		in_buf[i] = in_buf[j];
	position -= from;
	if (position < 0)
		throw "Broken buffer\n";
	in_buf[position] = '\0';
}

Player* Robot::PlayerInfo(int n)
/* NULL, if bankrupt */
{
	Send("player", n);
	Recieve("turn\n%", "is ");
	if (strncmp(in_buf, "a bankrupt", strlen("a bankrupt")) != 0 &&
		strncmp(in_buf, "no such", strlen("no such") != 0)) {
		int mon, prod, mat, fact, build;
		char mt;
		if (6 != sscanf(in_buf, "%d %d %d %d %d %c",
			&mon, &prod, &mat, &fact, &build, &mt))
		{
			throw "Error on loading player's info\n";
		}
		Player *p = new Player(n, mon, prod, mat, fact, build,
			mt=='y');
		return p;
	}
	return 0;
}

void Robot::Update()
{
	Send("player", number);
	char *p = Recieve("makes ", "Let's play\n");
	if (strncmp(p, "turn\n%", strlen("turn\n%")) == 0) {
		char mt;
		if (6 != sscanf(in_buf, "turn \n%% %d %d %d %d %d %c",
			&money, &products, &material, &factories,
			&building_factories, &mt))
		{
			throw "Updating error\n";
		}
		makes_turn = mt=='y';
	} else if (strncmp(p, "The month", strlen("The month")) == 0) {
		makes_turn = true;
	}
}
	
Status* Robot::MarketSituation()
{
	Status *st = new Status;
	Send("market");
	Recieve("Current month is %");
	if (1 != sscanf(in_buf, "%d", &st->month))
		throw "Error in market\n";
	Recieve("%");
	if (1 != sscanf(in_buf, "%d", &st->pl_count))
		throw "Error in market\n";
	Recieve("%");
	if (2 != sscanf(in_buf, "%d %d", &st->sell_n, &st->min_price))
		throw "Error in market\n";
	Recieve("%");
	if (2 != sscanf(in_buf, "%d %d", &st->buy_n, &st->max_price))
		throw "Error in market\n";
	return st;
}

void Status::Print()
{
	printf("Current month is %d\n"
		"Players still active:\n"
		"  \t     %d\n"
		"bank sells: items min.price\n"
		"  \t     %d       %d\n"
		"bank buys: items max.price\n"
		"  \t     %d       %d\n",
		 month, pl_count, sell_n, min_price, buy_n, max_price);
}	

int is_number(const char *str)
{
	for (unsigned int i=0; i<strlen(str); i++) {
		if (!isdigit(str[i]))
			return 0;
	}
	return 1;
}

void Auction::Print()
{
	printf("Player %d %s %d %s %d dollars\n", player,
		action==sold ? "sold" : "bought", ammount,
		action==sold ? "products and gained" :
		"materials and spent", cost);
}

auction_list* Robot::AuctionResults()
{
	auction_list *first = 0, **last = &first;
	int pl, ammo, mon;
	while (Recieve("#", "The month")) {
		if (3 == sscanf(in_buf, " Player %d sold %d products"
			" and gained %d dollars\n", &pl, &ammo, &mon))
		{
			*last = new auction_list;
			(*last)->auc = new Auction(pl, sold, ammo, mon);
			(*last)->next = 0;
			last = &(*last)->next;
		} else
		if (3 == sscanf(in_buf, " Player %d bought %d materials "
			"and spent %d dollars\n", &pl, &ammo, &mon))
		{
			*last = new auction_list;
			(*last)->auc = new Auction(pl, bought, ammo, mon);
			(*last)->next = 0;
			last = &(*last)->next;
		} else
			break;
	}
	return first;
}
			 
void Robot::DeleteAuc(auction_list *ptr)
{
	if (ptr) {
		delete ptr->auc;
		DeleteAuc(ptr->next);
		delete ptr;
	}
}


int min3(int a, int b, int c)
{
	if (a<b && a<c) {
		return a;
	} else {
		if (b<c) 
			return b;
		else
			return c;
	}
}

void Robot::EndTurn()
{
	Send("turn");
	makes_turn = false;
	yesterday_auc = AuctionResults();
	++turn;
	if (is_finished == not_yet) {
		Update();
		DeletePlayersInfo(yesterday_players);
		yesterday_players = today_players;
		NewMonth();
	} else if (is_finished == victory)
		printf("I am awesome\n");
	else
		printf("I am loser\n");
}

void Robot::NewMonth()
{
	today_players = AllPlayersInfo();
	today_market = MarketSituation();
}

void Robot::DeletePlayersInfo(Player **p)
{
	if (yesterday_players) {
		for (int i=0; i<players_n; i++)
			delete yesterday_players[i];
		delete[] yesterday_players;
	}
}

Player** Robot::AllPlayersInfo()
{
	Player** players = new Player*[players_n];
	for (int i=0; i<players_n; i++) {
		players[i] = PlayerInfo(i+1);
	}
	return players;
}

void Robot::Run()
{
	while (!makes_turn)
		Update();
	NewMonth();
}

