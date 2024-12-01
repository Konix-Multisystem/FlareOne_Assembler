%{
	#include "ast.h"
        #include <cstdio>
        #include <cstdlib>

	NodeList *g_ProgramBlock; /* the top level root node of our final AST */

	std::map<std::string,CSymbol*> symbolTable;
	std::map<std::string,CGlobal*> globalSymbol;
	std::vector<CSegmentMap> addressRanges;

	extern unsigned int g_CurLine;

	extern std::string g_FileName;
	std::string lastLabel;

	bool usingOverride=false;
	uint64_t overrideAddress=0;

	#define YYERROR_VERBOSE	/* Better error reporting */
/*
	CExpression* AssumeSegCS=NULL;
	CExpression* AssumeSegDS=NULL;
	CExpression* AssumeSegES=NULL;
	CExpression* AssumeSegSS=NULL;
*/
	uint64_t execAddress;

	extern int yylex();
	void yyerror(const char *s) { std::cerr << "Error: " << s << " (line " << g_CurLine << ") (file " << g_FileName << ")" << std::endl; }
%}

%union {
	NodeList* nodeList;
	ExpressionList* expressionList;
	CNode* node;
        CData* data;
	CSymbol* symbol;
	//CMemory* memory;
	CInstruction* instruction;
	CExpression* expression;
	std::string* string;
	int token;
}

%token <int> TOK_PREPROC

%token <int> TOK_IGNORE_SEND TOK_IGNORE_Z80 TOK_EXTERNAL TOK_IGNORE_CSEG TOK_IGNORE_FREE

%token <int> TOK_ORG TOK_BANK TOK_IF TOK_ELSE TOK_ENDIF TOK_ERROR TOK_EXEC TOK_LOOP TOK_DO TOK_REPEAT TOK_GLOBAL

%token <string> TOK_IDENTIFIER TOK_NUMBER TOK_STRING TOK_CHAR TOK_LOCAL_LABEL

%token <int> TOK_COMMA TOK_EQU TOK_EQUALS TOK_DEFW TOK_DEFB TOK_DEFS TOK_DEFM TOK_EOL TOK_COLON 

%token <int> TOK_OADD TOK_OSUB TOK_OMUL TOK_BRKOPEN TOK_BRKCLOSE TOK_DOLLAR TOK_OGREATER TOK_OLESS

%token <int> TOK_R_A TOK_R_B TOK_R_C TOK_R_D TOK_R_E TOK_R_F TOK_R_H TOK_R_L TOK_R_R TOK_R_I

%token <int> TOK_RP_AF TOK_RP_BC TOK_RP_DE TOK_RP_HL TOK_RP_IX TOK_RP_IY TOK_RP_AF_ TOK_RP_SP

%token <int> TOK_CC_C TOK_CC_NC TOK_CC_Z TOK_CC_NZ TOK_CC_M TOK_CC_P TOK_CC_PE TOK_CC_PO 

%token <int> TOK_INS_ADC TOK_INS_ADD TOK_INS_AND
%token <int> TOK_INS_BIT
%token <int> TOK_INS_CALL TOK_INS_CCF TOK_INS_CP TOK_INS_CDP TOK_INS_CPDR TOK_INS_CPI TOK_INS_CPIR TOK_INS_CPL
%token <int> TOK_INS_DAA TOK_INS_DEC TOK_INS_DI TOK_INS_DJNZ
%token <int> TOK_INS_EI TOK_INS_EX TOK_INS_EXX
%token <int> TOK_INS_HALT
%token <int> TOK_INS_IM TOK_INS_IN TOK_INS_INC TOK_INS_IND TOK_INS_INDR TOK_INS_INI TOK_INS_INIR
%token <int> TOK_INS_JP TOK_INS_JR
%token <int> TOK_INS_LD TOK_INS_LDD TOK_INS_LDDR TOK_INS_LDI TOK_INS_LDIR
%token <int> TOK_INS_NEG TOK_INS_NOP
%token <int> TOK_INS_OR TOK_INS_OTDR TOK_INS_OTIR TOK_INS_OUT TOK_INS_OUTD TOK_INS_OUTI
%token <int> TOK_INS_POP TOK_INS_PUSH
%token <int> TOK_INS_RES TOK_INS_RET TOK_INS_RETI TOK_INS_RETN TOK_INS_RL TOK_INS_RLA TOK_INS_RLC TOK_INS_RLCA TOK_INS_RLD TOK_INS_RR TOK_INS_RRA TOK_INS_RRC TOK_INS_RRCA TOK_INS_RRD TOK_INS_RST
%token <int> TOK_INS_SBC TOK_INS_SCF TOK_INS_SET TOK_INS_SLA TOK_INS_SRA TOK_INS_SRL TOK_INS_SUB
%token <int> TOK_INS_XOR

%type <expressionList> expr_list expr_stringlist dir_expr
%type <data> directive
%type <string> concat_string
%type <symbol> equate 
%type <node> line label structured org
%type <instruction> instruction
%type <expression> expr pair single cond fullcond
%type <nodeList> lines program element 

%left TOK_OLESS TOK_OGREATER
%left TOK_OAND TOK_OOR TOK_OXOR
%left TOK_OSUB TOK_OADD
%left TOK_OMUL TOK_ODIV TOK_OMOD
%left NEG

%start program

%%

program : lines { g_ProgramBlock = $1; }
		;

PreLine : TOK_PREPROC TOK_STRING TOK_PREPROC TOK_NUMBER TOK_PREPROC 		{ g_CurLine=atoi($4->c_str()); g_FileName=*$2; }

lines : element					{ $$ = $1; }
      | lines element				{ $$->insert($$->end(),$2->begin(),$2->end()); }
      ;

structured : TOK_IF expr TOK_EOL lines PreLine TOK_ELSE TOK_EOL lines PreLine TOK_ENDIF		{ $$ = new CStructured($2,$4,$8); }
           | TOK_IF expr TOK_EOL lines PreLine TOK_ENDIF					{ $$ = new CStructured($2,$4,NULL); }
	   | TOK_DO expr TOK_EOL lines PreLine TOK_LOOP						{ $$ = new CDoLoop($2,$4); }
	   | TOK_REPEAT expr TOK_EOL PreLine instruction 					{ $$ = new CRepeat($2,$5); }
	   ;

element : PreLine line	TOK_EOL			{ $$ = new NodeList(); $$->push_back($2); }
	| PreLine instruction TOK_EOL		{ $$ = new NodeList(); $$->push_back($2); }
	| PreLine directive TOK_EOL		{ $$ = new NodeList(); $$->push_back($2); }
	| PreLine org TOK_EOL			{ $$ = new NodeList(); $$->push_back($2); }
	| PreLine label instruction TOK_EOL	{ $$ = new NodeList(); $$->push_back($2); $$->push_back($3); }
	| PreLine label directive TOK_EOL	{ $$ = new NodeList(); $$->push_back($2); $$->push_back($3); }
	| PreLine label org TOK_EOL		{ $$ = new NodeList(); $$->push_back($2); $$->push_back($3); }
	| PreLine label TOK_EOL			{ $$ = new NodeList(); $$->push_back($2); }
        | PreLine structured TOK_EOL		{ $$ = new NodeList(); $$->push_back($2); }
	| PreLine TOK_EOL			{ $$ = new NodeList(); }
	;

org : TOK_ORG expr				{ $$ = new COrg($2); }
    | TOK_ORG expr TOK_COMMA expr		{ $$ = new COrg($2,$4);}		/* Second address acts like org, however when writing to memory the first address is used */
    ;

line : equate					{ $$ = $1; }
     | TOK_IGNORE_CSEG              { $$ = new CNull(); }
     | TOK_IGNORE_Z80               { $$ = new CNull(); }
     | TOK_IGNORE_SEND expr			{ $$ = new CNull(); }
     | TOK_IGNORE_FREE expr         { $$ = new CNull(); }
     | TOK_GLOBAL TOK_IDENTIFIER    { $$ = new CGlobal(*$2,true); } 
     | TOK_EXTERNAL TOK_IDENTIFIER  { $$ = new CGlobal(*$2,false); } 
     | TOK_BANK expr				{ $$ = new CBank($2); }
     | TOK_ERROR TOK_STRING			{ $$ = new CNTODO();}
     | TOK_EXEC expr				{ $$ = new CExec($2); }
     ;

label : TOK_IDENTIFIER TOK_COLON		{ $$ = new CLabel(*$1,false); }
      | TOK_IDENTIFIER				{ $$ = new CLabel(*$1,false); }
      | TOK_LOCAL_LABEL				{ $$ = new CLabel(*new std::string(lastLabel+*$1),true); }
	;

equate : TOK_IDENTIFIER TOK_EQU expr		{ $$ = new CSymbolExpression(*$1,$3,false); }
       | TOK_IDENTIFIER TOK_EQUALS expr		{ $$ = new CSymbolExpression(*$1,$3,true); }
       ;

single: TOK_R_A							{ $$ = new CExpressionRegSingle(REG_A); }
      | TOK_R_B							{ $$ = new CExpressionRegSingle(REG_B); }
      | TOK_R_C							{ $$ = new CExpressionRegSingle(REG_C); }
      | TOK_R_D							{ $$ = new CExpressionRegSingle(REG_D); }
      | TOK_R_E							{ $$ = new CExpressionRegSingle(REG_E); }
      | TOK_R_F							{ $$ = new CExpressionRegSingle(REG_F); }
      | TOK_R_H							{ $$ = new CExpressionRegSingle(REG_H); }
      | TOK_R_L							{ $$ = new CExpressionRegSingle(REG_L); }
      | TOK_R_R							{ $$ = new CExpressionRegSingle(REG_R); }
      | TOK_R_I							{ $$ = new CExpressionRegSingle(REG_I); }
      | TOK_BRKOPEN TOK_R_C TOK_BRKCLOSE			{ $$ = new CExpressionRegSingle(REG_M_C_M); }		/* Used by in and out */
      | TOK_BRKOPEN TOK_RP_BC TOK_BRKCLOSE			{ $$ = new CExpressionRegSingle(REG_M_BC_M); }
      | TOK_BRKOPEN TOK_RP_DE TOK_BRKCLOSE			{ $$ = new CExpressionRegSingle(REG_M_DE_M); }
      | TOK_BRKOPEN TOK_RP_HL TOK_BRKCLOSE			{ $$ = new CExpressionRegSingle(REG_M_HL_M); }
      | TOK_BRKOPEN TOK_RP_SP TOK_BRKCLOSE			{ $$ = new CExpressionRegSingle(REG_M_SP_M); }
      | TOK_BRKOPEN TOK_RP_IX TOK_OADD expr TOK_BRKCLOSE	{ $$ = new CExpressionRegSingle(REG_M_IX_M,$4); }
      | TOK_BRKOPEN TOK_RP_IY TOK_OADD expr TOK_BRKCLOSE	{ $$ = new CExpressionRegSingle(REG_M_IY_M,$4); }
      | TOK_BRKOPEN TOK_RP_IX expr TOK_BRKCLOSE			{ $$ = new CExpressionRegSingle(REG_M_IX_M,$3); }
      | TOK_BRKOPEN TOK_RP_IY expr TOK_BRKCLOSE			{ $$ = new CExpressionRegSingle(REG_M_IY_M,$3); }
      ;

pair: TOK_RP_AF					{ $$ = new CExpressionRegPair(REG_AF); }
      | TOK_RP_BC				{ $$ = new CExpressionRegPair(REG_BC); }
      | TOK_RP_DE				{ $$ = new CExpressionRegPair(REG_DE); }
      | TOK_RP_HL				{ $$ = new CExpressionRegPair(REG_HL); }
      | TOK_RP_IX				{ $$ = new CExpressionRegPair(REG_IX); }
      | TOK_RP_IY				{ $$ = new CExpressionRegPair(REG_IY); }
      | TOK_RP_SP				{ $$ = new CExpressionRegPair(REG_SP); }
      | TOK_RP_AF_				{ $$ = new CExpressionRegPair(REG_AF_); }
      ;

cond: TOK_R_C					{ $$ = new CExpressionCondition(REG_CC_C); }
    | TOK_CC_NC					{ $$ = new CExpressionCondition(REG_CC_NC); }
    | TOK_CC_Z					{ $$ = new CExpressionCondition(REG_CC_Z); }
    | TOK_CC_NZ					{ $$ = new CExpressionCondition(REG_CC_NZ); }
    ;

fullcond: cond					{ $$ = $1; }
	| TOK_CC_M				{ $$ = new CExpressionCondition(REG_CC_M); }
	| TOK_CC_P				{ $$ = new CExpressionCondition(REG_CC_P); }
	| TOK_CC_PE				{ $$ = new CExpressionCondition(REG_CC_PE); }
	| TOK_CC_PO				{ $$ = new CExpressionCondition(REG_CC_PO); }
	;

  /* Reminder, change expr below to operand */

instruction : TOK_INS_ADC expr				{ $$ = new CInstructionAlu(new CExpressionRegSingle(REG_A),$2,1); }
	    | TOK_INS_ADC expr TOK_COMMA expr		{ $$ = new CInstructionAlu($2,$4,1); }
            | TOK_INS_ADD expr				{ $$ = new CInstructionAlu(new CExpressionRegSingle(REG_A),$2,0); }
            | TOK_INS_ADD expr TOK_COMMA expr		{ $$ = new CInstructionAlu($2,$4,0); }
            | TOK_INS_AND expr				{ $$ = new CInstructionAlu(new CExpressionRegSingle(REG_A),$2,4); }
            | TOK_INS_BIT expr TOK_COMMA expr		{ $$ = new CInstructionResSetBit($2,$4,2); }
            | TOK_INS_CALL expr				{ $$ = new CInstructionCall(NULL,$2); }
            | TOK_INS_CALL fullcond TOK_COMMA expr	{ $$ = new CInstructionCall($2,$4); }
            | TOK_INS_CCF				{ $$ = new CInstructionCcf(); }
            | TOK_INS_CP expr				{ $$ = new CInstructionAlu(new CExpressionRegSingle(REG_A),$2,7); }
            | TOK_INS_CPL				{ $$ = new CInstructionCpl(); }
            | TOK_INS_DAA				{ $$ = new CInstructionDaa(); }
            | TOK_INS_DEC expr				{ $$ = new CInstructionIncDec($2,false); }
            | TOK_INS_DI				{ $$ = new CInstructionDi(); }
            | TOK_INS_DJNZ expr				{ $$ = new CInstructionDjnz($2); }
            | TOK_INS_EI				{ $$ = new CInstructionEi(); }
            | TOK_INS_EX expr TOK_COMMA expr		{ $$ = new CInstructionEx($2,$4); }
            | TOK_INS_EXX				{ $$ = new CInstructionExx(); }
            | TOK_INS_HALT				{ $$ = new CInstructionHalt(); }
            | TOK_INS_IN expr TOK_COMMA expr		{ $$ = new CInstructionIn($2,$4); }
            | TOK_INS_INC expr				{ $$ = new CInstructionIncDec($2,true); }
            | TOK_INS_JP expr				{ $$ = new CInstructionJp(NULL,$2); }
            | TOK_INS_JP fullcond TOK_COMMA expr	{ $$ = new CInstructionJp($2,$4); }
            | TOK_INS_JR expr				{ $$ = new CInstructionJr(NULL,$2); }
            | TOK_INS_JR cond TOK_COMMA expr		{ $$ = new CInstructionJr($2,$4); }
            | TOK_INS_LD expr TOK_COMMA expr		{ $$ = new CInstructionLd($2,$4); }
	    | TOK_INS_LDD				{ $$ = new CInstructionLdd(); }
	    | TOK_INS_LDDR				{ $$ = new CInstructionLddr(); }
	    | TOK_INS_LDI				{ $$ = new CInstructionLdi(); }
	    | TOK_INS_LDIR				{ $$ = new CInstructionLdir(); }
	    | TOK_INS_NEG				{ $$ = new CInstructionNeg(); }
	    | TOK_INS_NOP				{ $$ = new CInstructionNop(); }
	    | TOK_INS_OR expr				{ $$ = new CInstructionAlu(new CExpressionRegSingle(REG_A),$2,6); }
            | TOK_INS_OUT expr TOK_COMMA expr		{ $$ = new CInstructionOut($2,$4); }
            | TOK_INS_POP expr				{ $$ = new CInstructionPop($2); }
            | TOK_INS_PUSH expr				{ $$ = new CInstructionPush($2); }
            | TOK_INS_RES expr TOK_COMMA expr		{ $$ = new CInstructionResSetBit($2,$4,0); }
            | TOK_INS_RET				{ $$ = new CInstructionRet(NULL); }
            | TOK_INS_RET fullcond			{ $$ = new CInstructionRet($2); }
            | TOK_INS_RETI				{ $$ = new CInstructionReti(); }
            | TOK_INS_RL expr				{ $$ = new CInstructionRot($2,2); }
            | TOK_INS_RLA				{ $$ = new CInstructionRla(); }
            | TOK_INS_RLC expr				{ $$ = new CInstructionRot($2,0); }
            | TOK_INS_RLCA				{ $$ = new CInstructionRlca(); }
            | TOK_INS_RR expr				{ $$ = new CInstructionRot($2,3); }
            | TOK_INS_RRA				{ $$ = new CInstructionRra(); }
            | TOK_INS_RRC expr				{ $$ = new CInstructionRot($2,1); }
            | TOK_INS_RRCA				{ $$ = new CInstructionRrca(); }
            | TOK_INS_SBC expr TOK_COMMA expr		{ $$ = new CInstructionAlu($2,$4,3); }
            | TOK_INS_SCF				{ $$ = new CInstructionScf(); }
            | TOK_INS_SET expr TOK_COMMA expr		{ $$ = new CInstructionResSetBit($2,$4,1); }
            | TOK_INS_SLA expr				{ $$ = new CInstructionRot($2,4); }
            /*| TOK_INS_SLL expr				{ $$ = new CInstructionRot($2,6); }   undocumented- unlikely to be used */
            | TOK_INS_SRA expr				{ $$ = new CInstructionRot($2,5); }
            | TOK_INS_SRL expr				{ $$ = new CInstructionRot($2,7); }
            | TOK_INS_SUB expr				{ $$ = new CInstructionAlu(new CExpressionRegSingle(REG_A),$2,2); }
            | TOK_INS_XOR expr				{ $$ = new CInstructionAlu(new CExpressionRegSingle(REG_A),$2,5); }
	    ;

dir_expr : expr								{ $$ = new ExpressionList(); $$->push_back($1); }
/*	| TOK_QUESTION							{ $$ = new ExpressionList(); $$->push_back(new CExpressionConst(*new CInteger(0))); }
	| TOK_NUMBER TOK_DUP TOK_BRKOPEN TOK_QUESTION TOK_BRKCLOSE	{ $$ = new ExpressionList(); {CInteger t(*$1); uint64_t a; for (a=0;a<t.getValue();a++) $$->push_back(new CExpressionConst(*new CInteger(0x00))); } }
	| TOK_NUMBER TOK_DUP TOK_BRKOPEN TOK_NUMBER TOK_BRKCLOSE	{ $$ = new ExpressionList(); {CInteger t(*$1); uint64_t a; for (a=0;a<t.getValue();a++) $$->push_back(new CExpressionConst(*new CInteger(*$4))); } }
	| expr TOK_DUP TOK_BRKOPEN TOK_QUESTION TOK_BRKCLOSE		{ $$ = new ExpressionList(); $$->push_back(new CExpressionConstList($1,*new CInteger(0x00))); }
	| expr TOK_DUP TOK_BRKOPEN TOK_NUMBER TOK_BRKCLOSE		{ $$ = new ExpressionList(); $$->push_back(new CExpressionConstList($1,*new CInteger(*$4)));}*/
	;

expr_list : dir_expr							{ $$ = new ExpressionList(); $$->insert($$->end(),$1->begin(),$1->end()); }
	  | expr_list TOK_COMMA dir_expr				{ $$->insert($$->end(),$3->begin(),$3->end()); }
          | expr_list TOK_COMMA						{ $$ = $1; }
	  ; 

concat_string : TOK_STRING						{ $$ = $1; }
	      | concat_string TOK_STRING				{ $$ = new std::string($1->substr(0,$1->length()-1) + *$2); }
              ;


expr_stringlist : dir_expr						{ $$ = new ExpressionList(); $$->insert($$->end(),$1->begin(),$1->end()); }
		| concat_string						{ $$ = new ExpressionList(); $$->push_back(new CExpressionString(*$1)); }
	  	| expr_stringlist TOK_COMMA dir_expr			{ $$->insert($$->end(),$3->begin(),$3->end()); }
	  	| expr_stringlist TOK_COMMA TOK_STRING			{ $$->push_back(new CExpressionString(*$3)); }
          	| expr_stringlist TOK_COMMA				{ $$ = $1; }
	  	; 

directive : TOK_DEFB expr_stringlist			{ $$ = new CData(*$2,8); }
	  | TOK_DEFW expr_list				{ $$ = new CData(*$2,16); }
	  | TOK_DEFM expr_stringlist			{ $$ = new CData(*$2,8); }
	  | TOK_DEFS expr TOK_COMMA dir_expr		{ $$ = new CDataSpace($2,*$4,8); }
	  | TOK_DEFS expr				{ {ExpressionList* t = new ExpressionList(); t->push_back(new CExpressionConst(*new CInteger((uint64_t)0))); $$ = new CDataSpace($2,*t,8);} }
	  ;

expr: TOK_NUMBER			{ $$ = new CExpressionConst(*new CInteger(*$1)); }
    | TOK_IDENTIFIER			{ $$ = new CExpressionIdent(*$1); }
    | TOK_LOCAL_LABEL			{ $$ = new CExpressionIdent(*new std::string(lastLabel+*$1)); }
    | TOK_CHAR				{ $$ = new CExpressionConst(*new CInteger($1->at(1))); }
    | TOK_DOLLAR			{ $$ = new CExpressionHere(); }
    | single				{ $$ = $1; }
    | pair				{ $$ = $1; }
    | expr TOK_OAND expr		{ $$ = new CExpressionOperator(EO_AND,$1,$3); }
    | expr TOK_EQUALS expr		{ $$ = new CExpressionOperator(EO_EQU,$1,$3); }
    | expr TOK_OGREATER expr		{ $$ = new CExpressionOperator(EO_GRT,$1,$3); }
    | expr TOK_OLESS expr		{ $$ = new CExpressionOperator(EO_LES,$1,$3); }
    | TOK_OGREATER expr			{ $$ = new CExpressionOperator(EO_HIGH,$2,$2); }
    | TOK_OLESS expr			{ $$ = new CExpressionOperator(EO_LOW,$2,$2); }
/*    | expr TOK_OOR expr			{ $$ = new CExpressionOperator(EO_OR,$1,$3); }*/
    | expr TOK_OXOR expr		{ $$ = new CExpressionOperator(EO_XOR,$1,$3); }
    | expr TOK_OSUB expr		{ $$ = new CExpressionOperator(EO_SUB,$1,$3); }
    | expr TOK_OADD expr		{ $$ = new CExpressionOperator(EO_ADD,$1,$3); }
    | expr TOK_OMUL expr		{ $$ = new CExpressionOperator(EO_MUL,$1,$3); }
/*    | expr TOK_INS_SHL expr		{ $$ = new CExpressionOperator(EO_SHL,$1,$3); }*/
    | expr TOK_ODIV expr		{ $$ = new CExpressionOperator(EO_DIV,$1,$3); }
/*    | expr TOK_OMOD expr		{ $$ = new CExpressionOperator(EO_MOD,$1,$3); }*/
    | TOK_OSUB expr %prec NEG		{ $$ = new CExpressionOperator(EO_SUB,new CExpressionConst(*new CInteger((uint64_t)0)),$2); }
    | TOK_BRKOPEN expr TOK_BRKCLOSE	{ $$ = new CExpressionMemRef($2); }
/*    | TOK_HIGH expr			{ $$ = new CExpressionHigh8($2); }*/
    ;



%%
