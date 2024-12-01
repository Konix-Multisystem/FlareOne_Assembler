%{
#include <string>
#include <ctype.h>
#if OS_WINDOWS
#include <io.h>
#endif
#include "ast.h"
#include "parser.hpp"

//#define DEBUG printf
#define DEBUG(x) {}

#define SAVE_TOKEN {int mkUpper; std::string t(""); for (mkUpper=0;mkUpper<yyleng;mkUpper++) { t+=toupper(yytext[mkUpper]); }  yylval.string = new std::string(t); DEBUG(" [%s] ",yylval.string->c_str());}
#define SAVE_TOKEN_NOUPPER { yylval.string = new std::string(yytext); DEBUG(" [%s] ",yylval.string->c_str()); }
#define TOKEN(t) { if (t == TOK_EOL) {DEBUG(" " #t "\n");} else {DEBUG(" " #t "");} return yylval.token = t;}

unsigned int g_CurLine=1;		/* For error handling */
std::string g_FileName;
extern "C" int yywrap() { return 1; }
%}

%s INS
%s CHECKINS

%%

"##!PREPROC!## "				{ DEBUG(" <CHECKINS> "); BEGIN(CHECKINS); TOKEN(TOK_PREPROC); }		/* Special preprocessor macro (should never occur in source file) */

(?i:DEFB)				{ TOKEN(TOK_DEFB); }
(?i:DB)					{ TOKEN(TOK_DEFB); }
(?i:DEFW)				{ TOKEN(TOK_DEFW); }
(?i:DW)					{ TOKEN(TOK_DEFW); }
(?i:DEFS)				{ TOKEN(TOK_DEFS); }
(?i:DS)					{ TOKEN(TOK_DEFS); }
(?i:BLKB)				{ TOKEN(TOK_DEFS); }
(?i:DEFM)				{ TOKEN(TOK_DEFM); }
(?i:EQU)				{ TOKEN(TOK_EQU); }
"="					{ TOKEN(TOK_EQUALS); }

(?i:FREE)               { TOKEN(TOK_IGNORE_FREE); }
(?i:SEND)				{ TOKEN(TOK_IGNORE_SEND); }
(?i:.Z80)				{ TOKEN(TOK_IGNORE_Z80); }
(?i:CSEG)				{ TOKEN(TOK_IGNORE_CSEG); }

<INS>(?i:GLOBAL)			{ TOKEN(TOK_GLOBAL); }
<INS>(?i:EXTERNAL)			{ TOKEN(TOK_EXTERNAL); }
<INS>(?i:EXEC)				{ TOKEN(TOK_EXEC); }
<INS>(?i:ORG)				{ TOKEN(TOK_ORG); }
<INS>(?i:BANK)				{ TOKEN(TOK_BANK); }
<INS>(?i:IF)					{ TOKEN(TOK_IF); }
<INS>(?i:ELSE)				{ TOKEN(TOK_ELSE); }
<INS>(?i:ENDIF)				{ TOKEN(TOK_ENDIF); }
<INS>(?i:ERROR)				{ TOKEN(TOK_ERROR); }
<INS>(?i:DO)					{ TOKEN(TOK_DO); }
<INS>(?i:LOOP)				{ TOKEN(TOK_LOOP); }
<INS>(?i:REPEAT)				{ TOKEN(TOK_REPEAT); }

(?i:I)					{ TOKEN(TOK_R_I); }
(?i:R)					{ TOKEN(TOK_R_R); }
(?i:A)					{ TOKEN(TOK_R_A); }
(?i:B)					{ TOKEN(TOK_R_B); }
(?i:C)					{ TOKEN(TOK_R_C); }
(?i:D)					{ TOKEN(TOK_R_D); }
(?i:E)					{ TOKEN(TOK_R_E); }
(?i:F)					{ TOKEN(TOK_R_F); }
(?i:H)					{ TOKEN(TOK_R_H); }
(?i:L)					{ TOKEN(TOK_R_L); }

(?i:AF)					{ TOKEN(TOK_RP_AF); }
(?i:AF')				{ TOKEN(TOK_RP_AF_); }
(?i:BC)					{ TOKEN(TOK_RP_BC); }
(?i:DE)					{ TOKEN(TOK_RP_DE); }
(?i:HL)					{ TOKEN(TOK_RP_HL); }
(?i:IX)					{ TOKEN(TOK_RP_IX); }
(?i:IY)					{ TOKEN(TOK_RP_IY); }
(?i:SP)					{ TOKEN(TOK_RP_SP); }


<INS>(?i:ADC)				{ TOKEN(TOK_INS_ADC); }
<INS>(?i:ADD)				{ TOKEN(TOK_INS_ADD); }
<INS>(?i:AND)				{ TOKEN(TOK_INS_AND); }
<INS>(?i:BIT)				{ TOKEN(TOK_INS_BIT); }
<INS>(?i:CALL)				{ TOKEN(TOK_INS_CALL); }
<INS>(?i:CCF)				{ TOKEN(TOK_INS_CCF); }
<INS>(?i:CP)					{ TOKEN(TOK_INS_CP); }
<INS>(?i:CDP)				{ TOKEN(TOK_INS_CDP); }
<INS>(?i:CPDR)				{ TOKEN(TOK_INS_CPDR); }
<INS>(?i:CPI)				{ TOKEN(TOK_INS_CPI); }
<INS>(?i:CPIR)				{ TOKEN(TOK_INS_CPIR); }
<INS>(?i:CPL)				{ TOKEN(TOK_INS_CPL); }
<INS>(?i:DAA)				{ TOKEN(TOK_INS_DAA); }
<INS>(?i:DEC)				{ TOKEN(TOK_INS_DEC); }
<INS>(?i:DI)					{ TOKEN(TOK_INS_DI); }
<INS>(?i:DJNZ)				{ TOKEN(TOK_INS_DJNZ); }
<INS>(?i:EI)					{ TOKEN(TOK_INS_EI); }
<INS>(?i:EX)					{ TOKEN(TOK_INS_EX); }
<INS>(?i:EXX)				{ TOKEN(TOK_INS_EXX); }
<INS>(?i:HALT)				{ TOKEN(TOK_INS_HALT); }
<INS>(?i:IM)					{ TOKEN(TOK_INS_IM); }
<INS>(?i:IN)					{ TOKEN(TOK_INS_IN); }
<INS>(?i:INC)				{ TOKEN(TOK_INS_INC); }
<INS>(?i:IND)				{ TOKEN(TOK_INS_IND); }
<INS>(?i:INDR)				{ TOKEN(TOK_INS_INDR); }
<INS>(?i:INI)				{ TOKEN(TOK_INS_INI); }
<INS>(?i:INIR)				{ TOKEN(TOK_INS_INIR); }
<INS>(?i:JP)					{ TOKEN(TOK_INS_JP); }
<INS>(?i:JR)					{ TOKEN(TOK_INS_JR); }
<INS>(?i:LD)					{ TOKEN(TOK_INS_LD); }
<INS>(?i:LDD)				{ TOKEN(TOK_INS_LDD); }
<INS>(?i:LDDR)				{ TOKEN(TOK_INS_LDDR); }
<INS>(?i:LDI)				{ TOKEN(TOK_INS_LDI); }
<INS>(?i:LDIR)				{ TOKEN(TOK_INS_LDIR); }
<INS>(?i:NEG)				{ TOKEN(TOK_INS_NEG); }
<INS>(?i:NOP)				{ TOKEN(TOK_INS_NOP); }
<INS>(?i:OR)					{ TOKEN(TOK_INS_OR); }
<INS>(?i:OTDR)				{ TOKEN(TOK_INS_OTDR); }
<INS>(?i:OTIR)				{ TOKEN(TOK_INS_OTIR); }
<INS>(?i:OUT)				{ TOKEN(TOK_INS_OUT); }
<INS>(?i:OUTD)				{ TOKEN(TOK_INS_OUTD); }
<INS>(?i:OUTI)				{ TOKEN(TOK_INS_OUTI); }
<INS>(?i:POP)				{ TOKEN(TOK_INS_POP); }
<INS>(?i:PUSH)				{ TOKEN(TOK_INS_PUSH); }
<INS>(?i:RES)				{ TOKEN(TOK_INS_RES); }
<INS>(?i:RET)				{ TOKEN(TOK_INS_RET); }
<INS>(?i:RETI)				{ TOKEN(TOK_INS_RETI); }
<INS>(?i:RETN)				{ TOKEN(TOK_INS_RETN); }
<INS>(?i:RL)					{ TOKEN(TOK_INS_RL); }
<INS>(?i:RLA)				{ TOKEN(TOK_INS_RLA); }
<INS>(?i:RLC)				{ TOKEN(TOK_INS_RLC); }
<INS>(?i:RLCA)				{ TOKEN(TOK_INS_RLCA); }
<INS>(?i:RLD)				{ TOKEN(TOK_INS_RLD); }
<INS>(?i:RR)					{ TOKEN(TOK_INS_RR); }
<INS>(?i:RRA)				{ TOKEN(TOK_INS_RRA); }
<INS>(?i:RRC)				{ TOKEN(TOK_INS_RRC); }
<INS>(?i:RRCA)				{ TOKEN(TOK_INS_RRCA); }
<INS>(?i:RRD)				{ TOKEN(TOK_INS_RRD); }
<INS>(?i:RST)				{ TOKEN(TOK_INS_RST); }
<INS>(?i:SBC)				{ TOKEN(TOK_INS_SBC); }
<INS>(?i:SCF)				{ TOKEN(TOK_INS_SCF); }
<INS>(?i:SET)				{ TOKEN(TOK_INS_SET); }
<INS>(?i:SLA)				{ TOKEN(TOK_INS_SLA); }
<INS>(?i:SRA)				{ TOKEN(TOK_INS_SRA); }
<INS>(?i:SRL)				{ TOKEN(TOK_INS_SRL); }
<INS>(?i:SUB)				{ TOKEN(TOK_INS_SUB); }
<INS>(?i:XOR)				{ TOKEN(TOK_INS_XOR); }

(?i:NC)					{ TOKEN(TOK_CC_NC); }
(?i:Z)					{ TOKEN(TOK_CC_Z); }
(?i:NZ)					{ TOKEN(TOK_CC_NZ); }
(?i:M)					{ TOKEN(TOK_CC_M); }
(?i:P)					{ TOKEN(TOK_CC_P); }
(?i:PE)					{ TOKEN(TOK_CC_PE); }
(?i:PO)					{ TOKEN(TOK_CC_PO); }

[a-zA-Z_][a-zA-Z0-9_$.]*		{ SAVE_TOKEN; return TOK_IDENTIFIER; }
$[0-9]+					{ SAVE_TOKEN; return TOK_LOCAL_LABEL; }

"'"[^']"'"				{ SAVE_TOKEN_NOUPPER; return TOK_CHAR; }
["][^"]["]				{ SAVE_TOKEN_NOUPPER; return TOK_CHAR; }

"'"[^']*"'"				{ SAVE_TOKEN_NOUPPER; return TOK_STRING; }
["][^"]*["]				{ SAVE_TOKEN_NOUPPER; return TOK_STRING; }

#[0-9a-fA-F]+				{ SAVE_TOKEN; return TOK_NUMBER; }
%[0-1]+					{ SAVE_TOKEN; return TOK_NUMBER; }
[0-9a-fA-F]+[Hh]			{ SAVE_TOKEN; return TOK_NUMBER; }
[0-1]+[Bb]				{ SAVE_TOKEN; return TOK_NUMBER; }
[0-9]+					{ SAVE_TOKEN; return TOK_NUMBER; }

<CHECKINS>[ \t]+	{ DEBUG(" <INS> "); BEGIN(INS); }

<INS,INITIAL>[ \t\r]					{ DEBUG(" <INITIAL> "); BEGIN(INITIAL); /*Consume white space*/  }
";"[^\n]*				{  /*Ignore comments*/ }

":"					{ TOKEN(TOK_COLON); }
","					{ TOKEN(TOK_COMMA); }

"/"					{ TOKEN(TOK_ODIV); }
"*"					{ TOKEN(TOK_OMUL); }
"+"					{ TOKEN(TOK_OADD); }
"-"					{ TOKEN(TOK_OSUB); }
"("					{ TOKEN(TOK_BRKOPEN); }
")"					{ TOKEN(TOK_BRKCLOSE); }
"$"					{ TOKEN(TOK_DOLLAR); }
"&"					{ TOKEN(TOK_OAND); }
"|"					{ TOKEN(TOK_OOR); }
"~"					{ TOKEN(TOK_OXOR); }
">"					{ TOKEN(TOK_OGREATER); }
"<"					{ TOKEN(TOK_OLESS); }

[\n]					{ DEBUG(" <INITIAL> "); BEGIN(INITIAL); TOKEN(TOK_EOL); }

.					{ printf("Unrecognised character in input : %c  (file %s - line %d)\n",yytext[0],g_FileName.c_str(),g_CurLine); yyterminate(); }

%%
