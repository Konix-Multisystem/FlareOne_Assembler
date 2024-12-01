#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stdint.h>

/*

 Ok, it appears the original PDS assembler was a bit of a hack job, the bank command actually moves assembled things into place after they have been assembled


 e.g. 

 	org 16384*3		;Top 16k page --- at present this is the only page that is relocated!

	defm "I have been assembled @ 0xC000+",0

	bank 32



	resulting assembled file

	0x8C000	"I have been assembled @ 0xC000+",0


	This is currently implemented to affect only the upper 16k slot, the lower 48k always assemble at 0x40000-0x4BFFF  - I may have to revisit if this proves wrong

*/


extern unsigned int g_CurLine;		/* For error handling */
extern std::string g_FileName;

enum EOperation
{
	EO_SUB,
	EO_ADD,
	EO_MUL,
	EO_SHL,
	EO_DIV,
	EO_AND,
	EO_OR,
	EO_XOR,
	EO_MOD,
	EO_EQU,
	EO_GRT,
	EO_LES,
	EO_LOW,
	EO_HIGH
};


#define	REG_A		0
#define	REG_B		1
#define	REG_C		2
#define	REG_D		3
#define	REG_E		4
#define	REG_F		5
#define	REG_H		6
#define	REG_L		7
#define REG_M_HL_M	8
#define REG_M_IX_M	9
#define REG_M_IY_M	10
#define REG_M_BC_M	11
#define REG_M_DE_M	12
#define REG_M_SP_M	13
#define REG_R		14
#define REG_I		15
#define REG_M_C_M	16		// USED BY IN / OUT

#define	REG_AF		20
#define	REG_BC		21
#define	REG_DE		22
#define	REG_HL		23
#define	REG_IX		24
#define	REG_IY		25
#define	REG_AF_		26
#define	REG_SP		27

#define	REG_CC_C	40
#define	REG_CC_NC	41
#define	REG_CC_Z	42
#define	REG_CC_NZ	43

#define	REG_CC_M	50
#define	REG_CC_P	51
#define	REG_CC_PE	52
#define	REG_CC_PO	53


#define RS_UNKNOWN	0xFFFF

int isUsingCom();

class CExpression;
/*extern CExpression* AssumeSegCS;
extern CExpression* AssumeSegDS;
extern CExpression* AssumeSegES;
extern CExpression* AssumeSegSS;
*/
class CSegmentMap
{
public:
	uint64_t addressBegin;
	uint64_t addressEnd;
	CSegmentMap(uint64_t address) : addressBegin(address),addressEnd(address) { }

	bool IsInRange(uint64_t address)
	{
		if (address>=addressBegin && address<=addressEnd)
			return true;
		return false;
	}

	void AddRange(CSegmentMap other)
	{
		if (other.addressBegin<addressBegin)
			addressBegin=other.addressBegin;
		if (other.addressEnd>addressEnd)
			addressEnd=other.addressEnd;
	}

	bool CanCombine(CSegmentMap other)
	{
		if (other.addressEnd == addressBegin-1)
			return true;
		if (other.addressBegin == addressEnd+1)
			return true;
		return false;
	}

	void Dump()
	{
		std::cout << "Start : " << std::setfill('0') << std::setw(16) << std::hex << addressBegin << std::endl;
		std::cout << "End : " << std::setfill('0') << std::setw(16) << std::hex << addressEnd << std::endl;
	}

	uint64_t GetBegin() { return addressBegin; }
	uint64_t GetEnd() { return addressEnd; }
};

extern std::vector<CSegmentMap> addressRanges;

extern bool usingOverride;
extern uint64_t overrideAddress;

class CNode
{
protected:

	uint64_t address;
	uint64_t line;
	std::string filename;
	bool surpressError;
public:
	CNode() {address=-1;line=g_CurLine;filename=g_FileName;surpressError=false;}
	virtual ~CNode() {}

	virtual bool PrePass(uint64_t& inAddress)=0;
	virtual bool Assemble(uint8_t* base)=0;
	
	uint64_t GetLineNumber() const
	{
		return line;
	}

	uint64_t GetAddress() const
	{
		return address;
	}

	std::string GetFilename() const
	{
		return filename;
	}

	void SurpressError(bool yes)
	{
		surpressError=yes;
	}

	void FlagError(const char* error) const
	{
		if (!surpressError)
		{
			std::cerr << "Error: " << error << " (line " << (uint32_t)GetLineNumber() << " ) in file '"<<filename<<"'" << std::endl;
		}
	}

	void ExtendRanges(uint64_t offset)
	{
		size_t a,b;
		for (a=0;a<addressRanges.size();a++)
		{
			if (addressRanges[a].IsInRange(offset))
				return;
		}
		CSegmentMap tmp(offset);
		addressRanges.push_back(tmp);

		// Recombine ranges
		for (a=0;a<addressRanges.size();a++)
		{
			bool done=false;
			while (!done)
			{
				done=true;
				std::vector<CSegmentMap>::iterator iter=addressRanges.begin();
				for (b=0;b<addressRanges.size();b++,++iter)
				{
					if (a!=b)
					{
						if (addressRanges[a].CanCombine(addressRanges[b]))
						{
							addressRanges[a].AddRange(addressRanges[b]);
							addressRanges.erase(iter);
							done=false;
							break;
						}
					}
				}
			}
		}
	}

	void OverrideWriteAddress(bool en, uint64_t newStart)
	{
		std::cout << "newstart " << std::hex << newStart << std::endl;
		overrideAddress=newStart;					// BIG ASSUMPTION THAT ADDRESSES ARE WRITTEN IN INCREASING ORDER!!
		usingOverride=en;
	}

	void WriteToBase(uint8_t* base,uint64_t offset,uint8_t value)		// Allows tracking of segment ranges
	{
		if (usingOverride)
		{
			std::cout << std::hex << "Requested : " << offset << " : Using : " << overrideAddress << std::endl;
			offset=overrideAddress;
			overrideAddress++;
		}
		if (!isUsingCom())
		{
			if ((offset&0xC000)==0xC000)
			{
				//upper range - bank command will move these!
				offset&=0x3FFF;
				offset+=64*16384;
			}
			else
			{
				offset&=0xFFFF;
				offset|=0x40000;
			}
		}
		ExtendRanges(offset);
		base[offset]=value;
	}
	uint8_t ReadFromBase(uint8_t* base,uint64_t offset)
	{
		if (!isUsingCom())
		{
			if ((offset&0xC000)==0xC000)
			{
				//upper range - bank command will move these!
				offset&=0x3FFF;
				offset+=64*16384;
			}
			else
			{
				offset&=0xFFFF;
				offset|=0x40000;
			}
		}
		return base[offset];
	}

	void MoveUpperBase(uint8_t* base,uint64_t target)
	{
		std::vector<CSegmentMap>::iterator iter=addressRanges.begin();		// Find range containing the >1Mb boundary

		for (;iter!=addressRanges.end();++iter)
		{
			if ((iter->addressBegin>=16384*64) && (iter->addressEnd<=16384*64+16385))
			{
				uint64_t newLocation=iter->addressBegin&0x3FFF;
				newLocation+=target;

				memcpy(&base[newLocation],&base[iter->addressBegin],iter->addressEnd-iter->addressBegin+1);

				iter->addressBegin=((iter->addressBegin&0x3FFF)+target);
				iter->addressEnd=((iter->addressEnd&0x3FFF)+target);
			}
		}
		
	}

	virtual const char* Identify() const =0;
};

typedef std::vector<CNode*> NodeList;
typedef std::vector<std::string*> StringList;
extern NodeList* g_ProgramBlock;

class CNull : public CNode
{
public:
	CNull() {}
	
	virtual bool PrePass(uint64_t& inAddress) { address=inAddress; return true; }
	virtual bool Assemble(uint8_t* base) { return true; }
	
	virtual const char* Identify() const { return "NULL"; }

};

class CExpression;
typedef std::vector<CExpression*> ExpressionList;

class CInteger : public CNode
{
protected:
	uint64_t value;

public:
	CInteger(std::string& inValue)
	{
		std::string tmp = "";
		switch (inValue[inValue.length()-1])
		{
			case 'H':
			case 'h':
				value=strtol(inValue.c_str(),NULL,16);
				break;
			case 'B':
			case 'b':
				value=strtol(inValue.c_str(),NULL,2);
				break;
			default:
				switch (inValue[0])
				{
					case '%':
						value=strtol(inValue.c_str()+1,NULL,2);
						break;
					case '#':
						value=strtol(inValue.c_str()+1,NULL,16);
						break;
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						value=strtol(inValue.c_str(),NULL,10);
						break;
					default:
						std::cerr<< "EEEK : "  << inValue << std::endl;
						exit(1);
				}
				break;
		}
	}
	CInteger(char c)
	{
		value=c;
	}
	CInteger(uint64_t v)
	{
		value=v;
	}
	
	virtual bool PrePass(uint64_t& inAddress) { FlagError("Error : PrePass Called For Integer!!"); return false; }
	virtual bool Assemble(uint8_t* base) { FlagError("Warning : Assemble Called For Integer!!"); return true; }
	
	virtual const char* Identify() const { return "Integer"; }

	uint64_t getValue() const { return value; }
};

typedef std::vector<CInteger*> IntegerList;

class CSymbol;
class CGlobal;

extern std::map<std::string,CSymbol*> symbolTable;
extern std::map<std::string,CGlobal*> globalSymbol;

class CGlobal : public CNode
{
protected:
	std::string name;
	bool definition;
public:
	CGlobal(std::string& n, bool definition) : name(n),definition(definition) {}
	
	bool IsDefinition() { return definition; }

	virtual bool PrePass(uint64_t& inAddress) 
	{
		std::map<std::string,CGlobal*>::iterator it;
		if ((it=globalSymbol.find(name))!=globalSymbol.end())
		{
			if (globalSymbol[name]->IsDefinition() && IsDefinition())
			{
				FlagError((std::string("Error : Global requested multiple times- Symbol : ") + name).c_str());
				return false;
			}
			else
			{
				if (!globalSymbol[name]->IsDefinition())
					globalSymbol[name] = this;
			}
		}
		else
		{
			globalSymbol[name]=this;
		}
		
		address=inAddress;
		return true; 
	}
	virtual bool Assemble(uint8_t* base) { return true; }
	
	virtual uint64_t getValue() { return 0; };

	virtual const char* Identify() const { return "Global"; }
};

extern std::string g_localSymbolPrefix;

class CSymbol : public CNode
{
protected:
	std::string n;
	std::string name;
	bool label;
	bool redefine;
	int ptrSize;
public:

	static std::string PrefixedSymbol(std::string& n)
	{
		if (globalSymbol.find(n) != globalSymbol.end())
			return n;
		else
			return g_localSymbolPrefix + n;
	}

	CSymbol(std::string& n, bool label, bool redefine) : n(n), label(label), redefine(redefine), ptrSize(0) {}
	
	virtual bool PrePass(uint64_t& inAddress) 
	{
		name = PrefixedSymbol(n);
		std::map<std::string,CSymbol*>::iterator it;
		if ((it=symbolTable.find(name))!=symbolTable.end())
		{
			if (redefine)
			{
				redefine = 2;
//				symbolTable.erase(it);

				//it->second=this;
			}
			else
			{
				FlagError((std::string("Error : Duplicate Symbol : ")+name).c_str());
				return false;
			}
		}
		else
		{
			symbolTable[name]=this;
		}
		
		address=inAddress;
		return true; 
	}
	virtual bool Assemble(uint8_t* base) =0;//{ return true; }
	
	virtual uint64_t getValue() const =0;

	virtual const char* Identify() const { return "Symbol"; }

	bool IsLabel() const { return label; }

	void SetPtrSize(int s) { ptrSize=s; }
	int GetPtrSize() const { return ptrSize; }
};

class CExpression : public CNode
{
protected:

public:
	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress;
		return true; 
	}

	virtual size_t MultiCount() { return 0; }
	virtual char GetMultiValue(int ) { return 0; }

	virtual bool Assemble(uint8_t* base) =0;//{ return true; }
	
	virtual bool getValue(uint64_t& rValue) const =0;

	virtual const char* Identify() const { return "Expression"; }
	
	virtual bool IsLabel(bool& isLabel) const =0;

	virtual bool IsValidValue() { bool answer; uint64_t v; SurpressError(true); answer=getValue(v); SurpressError(false); return answer; }

	virtual int GetPtrSize() const =0;

	virtual bool IsRreg() const { return false; }
	virtual bool IsMbc() const { return false; }
	virtual bool IsMC() const { return false; }		// returns true if operand is (C) aka in / out usage
	virtual bool IsMde() const { return false; }
	virtual bool IsMsp() const { return false; }
	virtual bool IsAF() const { return false; }
	virtual bool IsAF_() const { return false; }
	virtual bool IsRR() const { return false; }
	virtual bool IsRR2() const { return false; }
	virtual bool IsCC() const { return false; }
	virtual bool IsII() const { return false; }
	virtual bool IsIMem() const { return false; }
	virtual bool IsR() const { return false; }
	virtual bool IsOperand() const { return false; }
	virtual bool IsMemory() const { return false; }
	virtual int GetRR() const { return RS_UNKNOWN; }
	virtual int GetRR2() const { return RS_UNKNOWN; }
	virtual int GetR() const { return RS_UNKNOWN; }
	virtual int GetCC() const { return RS_UNKNOWN; }
	virtual uint8_t GetPrefix() const { return 0x00; }
};

class CExpressionString : public CExpression
{
protected:
	std::string str;
public:
	CExpressionString(std::string& str) : str(str) {}

	virtual bool Assemble(uint8_t* base) { return true; }

	virtual bool getValue(uint64_t& rValue) const { return false; }
	
	virtual bool IsLabel(bool& isLabel) const {isLabel=false; return true;}
	
	virtual int GetPtrSize() const {return 0;}
	
	virtual const char* Identify() const { return "ExpressionString"; }

	virtual size_t MultiCount() { return str.length()-2; }		// -2 accounts for string quotes that are not stripped during lex

	virtual char GetMultiValue(int n) { return str[n+1]; }

};

class CExpressionConst : public CExpression
{
protected:
	CInteger value;
public:
	CExpressionConst(CInteger& v) : value(v) {}

	virtual bool Assemble(uint8_t* base) { return true; }

	virtual bool getValue(uint64_t& rValue) const { rValue=value.getValue(); return true; }
	
	virtual bool IsLabel(bool& isLabel) const {isLabel=false; return true;}
	
	virtual int GetPtrSize() const {return 0;}
};

class CExpressionIdent : public CExpression
{
protected:
	std::string n;
	std::string name;
public:
	CExpressionIdent(std::string& n) : n(n) {}
	
	virtual bool PrePass(uint64_t& inAddress) 
	{
		name = CSymbol::PrefixedSymbol(n);
		address=inAddress;
		return true; 
	}

	virtual bool Assemble(uint8_t* base) { return true; }

	virtual bool getValue(uint64_t& rValue) const 
	{
		if (symbolTable.find(name)==symbolTable.end())
		{
			FlagError((std::string("Error : Unknown symbol : ")+name).c_str());
			return false;
		}
		rValue=symbolTable[name]->getValue();
		return true;
	}
	
	virtual bool IsLabel(bool& isLabel) const 
	{
		if (symbolTable.find(name)==symbolTable.end())
		{
			FlagError((std::string("Error : Unknown symbol : ")+name).c_str());
			return false;
		}
		isLabel=symbolTable[name]->IsLabel(); 
		return true;
	}
	
	virtual int GetPtrSize() const
	{
		if (symbolTable.find(name)==symbolTable.end())
		{
			FlagError((std::string("Error : Unknown symbol : ")+name).c_str());
			return false;
		}
		return symbolTable[name]->GetPtrSize();
	}
};

class CExpressionHere : public CExpression
{
protected:
public:
	CExpressionHere() {address=0xF0F0F0F0;}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress;
		return true; 
	}

	virtual bool Assemble(uint8_t* base) { return true; }

	virtual bool getValue(uint64_t& rValue) const { rValue=GetAddress(); if (rValue==0xF0F0F0F0) { FlagError("Probable assembler bug!"); return false;} return true; }
	
	virtual bool IsLabel(bool& isLabel) const { isLabel=true; return true; }
	
	virtual int GetPtrSize() const {return 0;}
};

class CExpressionSegOffsetPair : public CExpression
{
protected:
	int reg;
	CExpression* expr;
public:
	CExpressionSegOffsetPair(int reg,CExpression* expr) : reg(reg),expr(expr) {}

	virtual bool Assemble(uint8_t* base) { return true; }

	virtual bool getValue(uint64_t& rValue) const 
	{ 
		// TODO test for segment
		if (!expr->getValue(rValue)) 
			return false; 

		return true;
	}

	virtual bool IsLabel(bool& isLabel) const 
	{
		bool t;
		if (!expr->IsLabel(t))
			return false;
		isLabel=t;
		return true;
	}
	virtual int GetPtrSize() const {return 0;}
};

class CExpressionOffset : public CExpression
{
protected:
	CExpression* expr;
public:
	CExpressionOffset(CExpression* expr) : expr(expr) {}

	virtual bool Assemble(uint8_t* base) { return expr->Assemble(base); }

	virtual bool getValue(uint64_t& rValue) const { if (!expr->getValue(rValue)) return false; rValue&=0xFFFF; return true; }
	
	virtual bool IsLabel(bool& isLabel) const 
	{
		isLabel=false;			// OFFSET should always force to use immediate rather than memory addressing
		return true;
	}
	virtual int GetPtrSize() const {return 0;}
};

class CExpressionSeg : public CExpression
{
protected:
	CExpression* expr;
public:
	CExpressionSeg(CExpression* expr) : expr(expr) {}

	virtual bool Assemble(uint8_t* base) { return expr->Assemble(base); }

	virtual bool getValue(uint64_t& rValue) const { if (!expr->getValue(rValue)) return false; rValue/=16; rValue&=0xFFFF; return true; }
	
	virtual bool IsLabel(bool& isLabel) const 
	{
		isLabel=false;			// OFFSET should always force to use immediate rather than memory addressing
		return true;
	}
	virtual int GetPtrSize() const {return 0;}
};

class CExpressionOperand : public CExpression
{
protected:
public:
	CExpressionOperand() { }
	virtual const char* Identify() const { return "Operand"; }
	
	virtual bool Assemble(uint8_t* base) { return true; }

	virtual bool getValue(uint64_t& rValue) const { return false; }
	
	virtual bool IsLabel(bool& isLabel) const 
	{
		return false;
	}
	virtual int GetPtrSize() const {return 0;}

	virtual bool IsOperand() const { return true; }
};

class CExpressionRegSingle : public CExpressionOperand
{
protected:
	int reg;
	CExpression* expr;
public:
	CExpressionRegSingle(int r) : reg(r),expr(NULL) { }
	CExpressionRegSingle(int r,CExpression* e) : reg(r),expr(e) { }
	
	virtual bool PrePass(uint64_t& inAddress) { address=inAddress; if (expr) {return expr->PrePass(inAddress);} return true; }

	virtual bool Assemble(uint8_t* base) { if (expr) { return expr->Assemble(base); } return true; }
	
	virtual bool getValue(uint64_t& rValue) const { return expr->getValue(rValue); }

	virtual const char* Identify() const { return "OperandSingle"; }
	
	virtual bool IsRreg() const { return REG_R==reg; }
	virtual bool IsMC() const { return REG_M_C_M==reg; }		// returns true if operand is (C) aka in / out usage
	virtual bool IsMbc() const { return REG_M_BC_M==reg; }
	virtual bool IsMde() const { return REG_M_DE_M==reg; }
	virtual bool IsMsp() const { return REG_M_SP_M==reg; }

	virtual bool IsIMem() const
	{
		switch (reg)
		{
			case REG_M_IX_M:
			case REG_M_IY_M:
				return true;
			default:
				return false;
		}
	}

	virtual bool IsR() const
	{
		switch (reg)
		{
			case REG_B:
			case REG_C:
			case REG_D:
			case REG_E:
			case REG_H:
			case REG_L:
			case REG_M_HL_M:
			case REG_A:
				return true;
			default:
				return false;
		}
	}

	virtual int GetR() const
	{
		switch (reg)
		{
			case REG_B:
				return 0;
			case REG_C:
				return 1;
			case REG_D:
				return 2;
			case REG_E:
				return 3;
			case REG_H:
				return 4;
			case REG_L:
				return 5;
			case REG_M_HL_M:
				return 6;
			case REG_A:
				return 7;
		}
		return RS_UNKNOWN;
	}

	uint8_t GetPrefix() const
	{
		switch (reg)
		{
			case REG_M_IX_M:
				return 0xDD;
			case REG_M_IY_M:
				return 0xFD;
			default:
				return 0x00;
		}
	}

};

class CExpressionRegPair : public CExpressionOperand
{
protected:
	int reg;
public:
	CExpressionRegPair(int r) : reg(r) { }

	virtual const char* Identify() const { return "OperandPair"; }

	virtual bool IsII() const
	{
		switch (reg)
		{
			case REG_IX:
			case REG_IY:
				return true;
			default:
				return false;
		}
	}
	
	virtual bool IsAF() const { return reg == REG_AF; }
	virtual bool IsAF_() const { return reg == REG_AF_; }

	virtual bool IsRR() const
	{
		switch (reg)
		{
			case REG_BC:
			case REG_DE:
			case REG_HL:
			case REG_SP:
				return true;
			default:
				return false;
		}
	}

	virtual bool IsRR2() const
	{
		switch (reg)
		{
			case REG_BC:
			case REG_DE:
			case REG_HL:
			case REG_AF:
				return true;
			default:
				return false;
		}
	}
	virtual int GetRR() const
	{
		switch (reg)
		{
			case REG_BC:
				return 0;
			case REG_DE:
				return 1;
			case REG_HL:
				return 2;
			case REG_SP:
				return 3;
			default:
				return RS_UNKNOWN;
		}
	}
	virtual int GetRR2() const
	{
		switch (reg)
		{
			case REG_BC:
				return 0;
			case REG_DE:
				return 1;
			case REG_HL:
				return 2;
			case REG_AF:
				return 3;
			default:
				return RS_UNKNOWN;
		}
	}

	uint8_t GetPrefix() const
	{
		switch (reg)
		{
			case REG_IX:
				return 0xDD;
			case REG_IY:
				return 0xFD;
			default:
				return 0x00;
		}
	}

};

class CExpressionCondition : public CExpressionOperand
{
protected:
	int reg;
public:
	CExpressionCondition(int r) : reg(r) { }

	virtual const char* Identify() const { return "OperandCond"; }

	virtual bool IsCC() const
	{
		switch (reg)
		{
			case REG_CC_C:
			case REG_CC_NC:
			case REG_CC_Z:
			case REG_CC_NZ:
			case REG_CC_M:
			case REG_CC_P:
			case REG_CC_PE:
			case REG_CC_PO:
				return true;
			default:
				return false;
		}
	}
	virtual int GetCC() const
	{
		switch (reg)
		{
			case REG_CC_NZ:
				return 0;
			case REG_CC_Z:
				return 1;
			case REG_CC_NC:
				return 2;
			case REG_CC_C:
				return 3;
			case REG_CC_PO:
				return 4;
			case REG_CC_PE:
				return 5;
			case REG_CC_P:
				return 6;
			case REG_CC_M:
				return 7;
			default:
				return RS_UNKNOWN;
		}
	}

};

class CExpressionMemRef : public CExpression
{
protected:
	CExpression* expr;
public:
	CExpressionMemRef(CExpression* e) : expr(e) { }

	virtual bool PrePass(uint64_t& inAddress) { address=inAddress; return expr->PrePass(inAddress); }
	
	virtual size_t MultiCount() { return expr->MultiCount(); }
	virtual char GetMultiValue(int i) { return expr->GetMultiValue(i); }
	
	virtual bool Assemble(uint8_t* base) { return expr->Assemble(base); }
	
	virtual bool getValue(uint64_t& rValue) const { return expr->getValue(rValue); }

	virtual const char* Identify() const { return "PossMemRef"; }
	
	virtual bool IsLabel(bool& isLabel) const { return expr->IsLabel(isLabel); }

	virtual bool IsValidValue() { return expr->IsValidValue(); }

	virtual int GetPtrSize() const { return expr->GetPtrSize(); }

	virtual bool IsMemory() const { return true; }
};

class CExpressionOperator : public CExpression
{
protected:
	int operation;
	CExpression* lhs;
	CExpression* rhs;
public:
	CExpressionOperator(int op,CExpression* l,CExpression* r) : operation(op),lhs(l),rhs(r) {}
	
	virtual bool PrePass(uint64_t& inAddress)
	{
		address=inAddress;
		if (!lhs->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for (lhs)");
			return false;
		}
		if (!rhs->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for (rhs)");
			return false;
		}
		return true; 
	}

	virtual bool Assemble(uint8_t* base) { return lhs->Assemble(base) && rhs->Assemble(base); }

	virtual bool getValue(uint64_t& rValue) const
	{
		uint64_t lVal,rVal;

		if (!lhs->getValue(lVal))
		{
			FlagError("Unable to retrieve constant value (lhs)");
			return false;
		}
		if (!rhs->getValue(rVal))
		{
			FlagError("Unable to retrieve constant value (rhs)");
			return false;
		}
		switch (operation)
		{
			default:
				FlagError("Unknown operation!");
				return false;
			case EO_SUB:
				rValue=lVal-rVal;
				break;
			case EO_ADD:
				rValue=lVal+rVal;
				break;
			case EO_MUL:
				rValue=lVal*rVal;
				break;
			case EO_SHL:
				rValue=lVal<<rVal;
				break;
			case EO_DIV:
				rValue=lVal/rVal;
				break;
			case EO_AND:
				rValue=lVal&rVal;
				break;
			case EO_OR:
				rValue=lVal|rVal;
				break;
			case EO_XOR:
				rValue=lVal^rVal;
				break;
			case EO_MOD:
				rValue=lVal%rVal;
				break;
			case EO_EQU:
				rValue=(lVal==rVal);
				break;
			case EO_GRT:
				rValue=lVal>rVal;
				break;
			case EO_LES:
				rValue=lVal<rVal;
				break;
			case EO_LOW:
				rValue=lVal&0xFF;
				break;
			case EO_HIGH:
				rValue=(lVal>>8)&0xFF;
				break;
		}
		return true;
	}
	
	virtual bool IsLabel(bool& isLabel) const 
	{
		bool t;
		if (!lhs->IsLabel(t))
			return false;
		isLabel=t;
		if (!rhs->IsLabel(t))
			return false;
		isLabel|=t;
		return true;
	}
	virtual int GetPtrSize() const
	{
		int size = lhs->GetPtrSize();
		if (size!=0)
			return size;
		return rhs->GetPtrSize();
	}
};

class CEven : public CNode
{
protected:
public:

	CEven()
	{
	}
	
	virtual bool PrePass(uint64_t& inAddress)
	{
		address=inAddress;
		if (address&1)
			inAddress=inAddress+1;
		return true;
	}
	virtual bool Assemble(uint8_t* base)
	{
		if (address&1)
		{
			WriteToBase(base,address,0x90);
		}
		return true; 
	}
	virtual const char* Identify() const { return "Even"; }
};

class CData : public CNode
{
protected:

	CData() {bits=0;}

	ExpressionList expressions;
	int bits;
public:
	CData(ExpressionList& exprs,int bits) : bits(bits)
	{
		// This is a bit "nasty" - assume no one will ever do an 
		size_t a,b;
		for (a=0;a<exprs.size();a++)
		{
			size_t mCount=exprs[a]->MultiCount();
			if (mCount)
			{
				for (b=0;b<mCount;b++)
				{
					expressions.push_back(new CExpressionConst(*new CInteger(exprs[a]->GetMultiValue(b))));
				}
			}
			else
			{
				expressions.push_back(exprs[a]);
			}
		}
	}
	
	bool UpdateSymbolSizeAtAddress(uint64_t addr)
	{
		std::map<std::string,CSymbol*>::iterator iter=symbolTable.begin();
		
		while (iter!=symbolTable.end())
		{
			if (iter->second->IsLabel())
			{
				if (iter->second->GetAddress()==addr)
				{
					iter->second->SetPtrSize(bits/8);
				}
			}
			++iter;
		}

		return true;
	}

	virtual bool PrePass(uint64_t& inAddress)
	{
		size_t a;
		address=inAddress; 
		if (!UpdateSymbolSizeAtAddress(inAddress)) 
		{
			return false; 
		}
		for (a=0;a<expressions.size();a++)
		{
			if (!expressions[a]->PrePass(inAddress))
			{
				FlagError("Error processing data definitions");
				return false;
			}
		}
		inAddress=address+(bits/8)*expressions.size(); 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		size_t a;
		uint64_t tAddress = address;
		for (a=0;a<expressions.size();a++)
		{
			uint64_t value;
			if (!expressions[a]->getValue(value))
			{
				FlagError("Unable to retrieve constant value");
				return false;
			}
			if (value>=(uint64_t)(1<<bits) && (int64_t)value<(int64_t)(-1<<(bits-1)))
			{
				std::cerr << "WRN : Constant too large : value = " << std::hex << value << std::endl;
//				FlagError("Constant too large");
//				return false;
			}
//			else
			{
				switch(bits)
				{
					default:
						FlagError("Unsupported constant size");
						return false;
					case 8:
						if (tAddress > 0xffff)
							tAddress++;
						WriteToBase(base,tAddress,(uint8_t)value);
						tAddress++;
						break;
					case 16:
						WriteToBase(base,tAddress,(uint8_t)(value));
						WriteToBase(base,tAddress+1,(uint8_t)(value>>8));
						tAddress+=2;
						break;
				}
			}
		}
		return true;
	}
	virtual const char* Identify() const { return "Data"; }
};

class CDataSpace : public CData
{
protected:
	CExpression* num;
public:
	CDataSpace(CExpression* num,ExpressionList& exprs,int bits) : CData(exprs,bits),num(num) {}
	
	virtual bool PrePass(uint64_t& inAddress) 
	{ 
		uint64_t value;
		address=inAddress; 
		
		if (!num->PrePass(inAddress))
		{
			FlagError("Unable to retrieve constant value for space count");
			return false;
		}
		if (!num->getValue(value))
		{
			FlagError("Unable to retrieve constant value for space count");
			return false;
		}
		if (expressions.size()!=1)
		{
			FlagError("Too many/few constants in DEFS");
			return false;
		}
		if (!expressions[0]->PrePass(inAddress))
		{
			FlagError("Failed to retrieve constant value");
			return false;
		}
		inAddress=inAddress+(bits/8)*value; 
		return true;
	}

	virtual bool Assemble(uint8_t* base)
	{
		uint64_t tAddress=address;
		uint64_t value;
		uint64_t count;
		if (!num->getValue(count))
		{
			FlagError("Unable to retrieve constant value for space count");
			return false;
		}

		if (!expressions[0]->getValue(value))
		{
			FlagError("Unable to retrieve constant value");
			return false;
		}

		for (uint64_t a=0;a<count;a++)
		{
			switch (bits)
			{
				default:
					FlagError("Unsupported constant size");
					return false;
				case 16:
					WriteToBase(base,tAddress,value);
					tAddress++;
					// Fall Through Intended
				case 8:
					WriteToBase(base,tAddress,value);
					tAddress++;
					break;
			}
		}
		return true; 
	}
	virtual const char* Identify() const { return "Space"; }
};

extern std::string lastLabel;

class CLabel : public CSymbol
{
public:
	CLabel(std::string& n,bool local) : CSymbol(n,true,false) {if (!local) {lastLabel=n;} }
	CLabel(std::string& n,int size,bool local) : CSymbol(n,true,false) {SetPtrSize(size); if (!local){lastLabel=n;} }
	
	virtual bool Assemble(uint8_t* base) { return true; }
	
	virtual uint64_t getValue() const { return address; }

	virtual const char* Identify() const { return "Label"; }
};

class CSymbolConstant : public CSymbol
{
protected:
	CInteger value;
public:
	CSymbolConstant(std::string& n,CInteger v) : CSymbol(n,false,false),value(v) {}
	
	virtual uint64_t getValue() const { return value.getValue(); }

	virtual const char* Identify() const { return "SymbolConstant"; }
};

class CSymbolExpression : public CSymbol
{
protected:
	CExpression *expr;
	bool redef;
public:
	CSymbolExpression(std::string& n,CExpression *e,bool redef) : CSymbol(n,false,redef),expr(e),redef(redef) {}
	
	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress;
		if (!CSymbol::PrePass(inAddress))
			return false;
		if (!expr->PrePass(inAddress))
			return false;
	
	//	if (redef)
		{
//			std::cout << "COMPUTE expr constant " << getValue() << std::endl; 
			
			expr=new CExpressionConst(*new CInteger(getValue())); 
			symbolTable[name]=this; 
		} 
		
		return true; 
	}

	virtual bool Assemble(uint8_t* base) 
	{ 
		return expr->Assemble(base);
	}

	virtual uint64_t getValue() const { uint64_t v; expr->getValue(v); return v; }

	virtual const char* Identify() const { return "SymbolExpression"; }
};

class COrg : public CNode
{
protected:
	CExpression* writeOverride;
	CExpression* addr;
public:

	COrg(CExpression* addr) : writeOverride(NULL),addr(addr) { }
	COrg(CExpression* writeOverride,CExpression* addr) : writeOverride(writeOverride),addr(addr) { }
	
	virtual bool PrePass(uint64_t& inAddress)
	{
		uint64_t value;

		if (writeOverride)
		{
			if (!writeOverride->PrePass(inAddress))
			{
				FlagError("Unable to retrieve constant value for org");
				return false;
			}
		}
	
		address=inAddress;
		if (!addr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve constant value for org");
			return false;
		}
		if (!addr->getValue(value))
		{
			FlagError("Unable to retrieve constant value for org");
			return false;
		}
		
		// ORG changes only low part of address 
		address&=0xFFF00000;
		inAddress&=0xFFF00000;

		address|=(value&0xFFFF);
		inAddress|=(value&0xFFFF);
		return true;
	}
	virtual bool Assemble(uint8_t* base)
	{
		uint64_t value;

		std::cout << "ORG:" << std::hex << address << std::endl;
		if (writeOverride)
		{
			std::cout << "OVERRIDE" << std::endl;
			if (!writeOverride->getValue(value))
			{
				FlagError("Unable to retrieve constant value for org");
				return false;
			}
			// only switch address relocation on/off at point of assemble
			OverrideWriteAddress(true,value);
		}
		else
		{
			// switch off address relocation
			OverrideWriteAddress(false,0);
		}
		return true; 
	}
	virtual const char* Identify() const { return "Org"; }
};

class CBankNotPDS : public CNode			/* The logical way the bank command should work */
{
protected:
	CExpression* addr;
public:

	CBankNotPDS(CExpression* addr) : addr(addr) { }
	
	virtual bool PrePass(uint64_t& inAddress)
	{
		uint64_t value;
	
		address=inAddress;
		if (!addr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve constant value for bank");
			return false;
		}
		if (!addr->getValue(value))
		{
			FlagError("Unable to retrieve constant value for bank");
			return false;
		}
		if ((value%4) != 0)
		{
			FlagError("Banking on non 64k boundary... not sure what to make of that yet!");
			return false;
		}
		value/=4;
		// ORG changes low part of address - bank changes high part (seems logical way to do it)
		address&=0x0000FFFF;
		inAddress&=0x0000FFFF;

		address|=(value&0xFFFF)<<16;
		inAddress|=(value&0xFFFF)<<16;
		return true;
	}
	virtual bool Assemble(uint8_t* base)
	{
		return true; 
	}
	virtual const char* Identify() const { return "Bank"; }
};

class CBank : public CNode				/* The actual way the bank command works :(  */
{
protected:
	CExpression* addr;
public:

	CBank(CExpression* addr) : addr(addr) { }
	
	virtual bool PrePass(uint64_t& inAddress)
	{
		address=inAddress;
		if (!addr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve constant value for bank");
			return false;
		}

		return true;
	}
	virtual bool Assemble(uint8_t* base)
	{
		uint64_t value;
		if (!addr->Assemble(base))
		{
			FlagError("Unable to retrieve constant value for bank");
			return false;
		}
		if (!addr->getValue(value))
		{
			FlagError("Unable to retrieve constant value for bank");
			return false;
		}

		MoveUpperBase(base,value*16384);

		return true; 
	}
	virtual const char* Identify() const { return "Bank"; }
};

extern uint64_t execAddress;

class CExec : public CNode
{
protected:
	CExpression* addr;
public:

	CExec(CExpression* addr) : addr(addr) { }
	
	virtual bool PrePass(uint64_t& inAddress)
	{
		uint64_t value;
		
		address=inAddress;
		if (!addr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve constant value for org");
			return false;
		}
		if (!addr->getValue(value))
		{
			FlagError("Unable to retrieve constant value for org");
			return false;
		}
		execAddress=value;

		return true;
	}
	virtual bool Assemble(uint8_t* base)
	{
		return true; 
	}
	virtual const char* Identify() const { return "Exec"; }
};

class CInstruction : public CNode
{
protected:
public:
	virtual bool PrePass(uint64_t& inAddress)=0;
	virtual bool Assemble(uint8_t* base)=0;
	
	virtual const char* Identify() const =0;
};

class CInstructionNULL : public CInstruction			/* Remove once unhandled instructions supported! */
{
public:
	CInstructionNULL() {}

	virtual bool PrePass(uint64_t& inAddress) { address=inAddress; return true; }
	virtual bool Assemble(uint8_t* base) { return true; }
	virtual const char* Identify() const { return "InstructionNULL"; }
};

class CTODO : public CInstruction,public CExpression
{
public:
	CTODO() {}
	
	virtual bool PrePass(uint64_t& inAddress) { CInstruction::FlagError("TODO"); return false; }
	virtual bool Assemble(uint8_t* base) { return false; }
	
	virtual const char* Identify() const { return "TODO"; }
	
	virtual bool PrePass(int reg,uint64_t& inAddress) { return false; }
	
	virtual bool getValue(uint64_t& rValue) const { return false; }

	virtual bool IsLabel(bool& isLabel) const {return false; }

	virtual int GetPtrSize() const { return 0; }
	
};

class CNTODO : public CNode
{
public:
	CNTODO() {}
	
	virtual bool PrePass(uint64_t& inAddress) { FlagError("TODO"); return false; }
	virtual bool Assemble(uint8_t* base) { return false; }
	
	virtual const char* Identify() const { return "TODO"; }
	
	virtual bool PrePass(int reg,uint64_t& inAddress) { return false; }
	
	virtual bool getValue(uint64_t& rValue) const { return false; }

	virtual bool IsLabel(bool& isLabel) const {return false; }

	virtual int GetPtrSize() const { return 0; }
	
};



class CInstructionIllegal : public CInstruction
{
protected:
public:
	CInstructionIllegal() 
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		FlagError("Illegal operand combination");
		return false;
	}
	virtual bool Assemble(uint8_t* base)
	{
		FlagError("Illegal operand combination");
		return false;
	}
	virtual const char* Identify() const
	{
		return "InstructionIllegal"; 
	}
};

class CInstructionCall : public CInstruction
{
protected:
	CExpression* cond;
	CExpression* expr;
public:
	CInstructionCall(CExpression* c,CExpression* e) : cond(c),expr(e)
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		if (cond)
		{
			if (!cond->IsCC())
			{
				FlagError("Call Illegal Condition Combination");
				return false;
			}
		}
		address=inAddress; 
		if (expr->IsOperand())
		{
			FlagError("CALL Illegal Operand Combination");
			return false;
		}
		if (expr->IsMemory())
		{
			FlagError("CALL Illegal Memory Operand Combination");
			return false;
		}
		if (!expr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		inAddress+=3; 

		
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		if (cond)
		{
			if (!cond->Assemble(base))
			{
				FlagError("Unable to assemble condition");
				return false;
			}
		}
		uint64_t value;
		if (!expr->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		if (!expr->getValue(value))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}

		if (cond)
		{
			WriteToBase(base,address,0xC4 | (cond->GetCC()<<3));
		}
		else
		{
			WriteToBase(base,address,0xCD);
		}
		WriteToBase(base,address+1,value&0xFF);
		WriteToBase(base,address+2,(value>>8)&0xFF);

		return true;
	}
	virtual const char* Identify() const { return "InstructionCall"; }
};


class CInstructionLd : public CInstruction
{
	enum LD_Type
	{
		LDT_none,
		LDT_rr_nn,
		LDT_ii_nn,
		LDT_rr_M,
		LDT_hl_M,
		LDT_ii_M,
		LDT_r_r,
		LDT_r_n,
		LDT_M_a,
		LDT_a_M,
		LDT_a_bcde,
		LDT_bcde_a,
		LDT_Mii_r,
		LDT_r_Mii,
		LDT_M_hl,
		LDT_M_rr,
		LDT_a_rreg,
		LDT_rreg_a,
		LDT_Mii_n,
		LDT_SP_HL,
	};
protected:
	CExpression* expr1;
	CExpression* expr2;
	LD_Type type;
public:
	CInstructionLd(CExpression* e1,CExpression* e2) : expr1(e1),expr2(e2),type(LDT_none)
	{
	}

	virtual bool IdentifyType()
	{
		if (expr2->IsRR() && (expr2->GetRR()==2) && expr1->IsRR() && (expr1->GetRR()==3))
		{
			type=LDT_SP_HL;
			return true;
		}
		if (expr2->IsR() && (expr2->GetR()==7) && expr1->IsRreg())
		{
			type=LDT_rreg_a;
			return true;
		}
		if (expr1->IsR() && (expr1->GetR()==7) && expr2->IsRreg())
		{
			type=LDT_a_rreg;
			return true;
		}
		if (expr1->IsR() && (expr1->GetR()==7) && (expr2->IsMbc() || expr2->IsMde()))
		{
			type=LDT_a_bcde;
			return true;
		}
		if (expr2->IsR() && (expr2->GetR()==7) && (expr1->IsMbc() || expr1->IsMde()))
		{
			type=LDT_bcde_a;
			return true;
		}
		if (expr1->IsRR() && (!expr2->IsOperand()) && (!expr2->IsMemory()))
		{
			type=LDT_rr_nn;
			return true;
		}
		if (expr1->IsRR() && (expr1->GetRR()==2) && (!expr2->IsOperand()) && (expr2->IsMemory()))
		{
			type=LDT_hl_M;
			return true;
		}
		if (expr1->IsRR() && (!expr2->IsOperand()) && (expr2->IsMemory()))
		{
			type=LDT_rr_M;
			return true;
		}
		if (expr1->IsII() && (!expr2->IsOperand()) && (expr2->IsMemory()))
		{
			type=LDT_ii_M;
			return true;
		}
		if (expr1->IsII() && (!expr2->IsOperand()) && (!expr2->IsMemory()))
		{
			type=LDT_ii_nn;
			return true;
		}
		if (expr1->IsR() && (!expr2->IsOperand()) && (!expr2->IsMemory()))
		{
			type=LDT_r_n;
			return true;
		}
		if (expr1->IsR() && expr2->IsR())
		{
			if (!((expr1->GetR()==6)&&(expr2->GetR()==6)))
			{	
				type=LDT_r_r;
				return true;
			}
		}
		if (expr1->IsIMem() && expr2->IsR() && expr2->GetR()!=6)
		{
			type=LDT_Mii_r;
			return true;
		}
		if (expr2->IsIMem() && expr1->IsR() && expr1->GetR()!=6)
		{
			type=LDT_r_Mii;
			return true;
		}
		if (expr1->IsMemory() && expr2->IsR() && (expr2->GetR()==7))
		{
			type=LDT_M_a;
			return true;
		}
		if (expr2->IsMemory() && expr1->IsR() && (expr1->GetR()==7))
		{
			type=LDT_a_M;
			return true;
		}
		if (expr1->IsMemory() && expr2->IsRR() && (expr2->GetRR()==2))
		{
			type=LDT_M_hl;
			return true;
		}
		if (expr1->IsMemory() && expr2->IsRR())		// Generic, but longer form
		{
			type=LDT_M_rr;
			return true;
		}
		if (expr1->IsIMem() && (!expr2->IsOperand()) && (!expr2->IsMemory()))
		{
			type=LDT_Mii_n;
			return true;
		}

		return false;
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{ 
		address=inAddress; 
		if (!IdentifyType())
		{
			FlagError("LD unknown operand combination");
			return false;
		}
		if (!expr1->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (!expr2->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		switch (type)
		{
			default:
			case LDT_none:
				FlagError("Unknown LD type");
				return false;
			case LDT_ii_nn:
				inAddress+=4; 
				break;
			case LDT_rr_nn:
				inAddress+=3; 
				break;
			case LDT_ii_M:
				inAddress+=4; 
				break;
			case LDT_hl_M:
				inAddress+=3; 
				break;
			case LDT_rr_M:
				inAddress+=4; 
				break;
			case LDT_rreg_a:
				inAddress+=2;
				break;
			case LDT_a_rreg:
				inAddress+=2;
				break;
			case LDT_r_n:
				inAddress+=2;
				break;
			case LDT_a_bcde:
				inAddress+=1;
				break;
			case LDT_bcde_a:
				inAddress+=1;
				break;
			case LDT_r_r:
				inAddress+=1;
				break;
			case LDT_Mii_n:
				inAddress+=4;
				break;
			case LDT_Mii_r:
				inAddress+=3;
				break;
			case LDT_r_Mii:
				inAddress+=3;
				break;
			case LDT_M_a:
				inAddress+=3;
				break;
			case LDT_a_M:
				inAddress+=3;
				break;
			case LDT_M_hl:
				inAddress+=3;
				break;
			case LDT_M_rr:
				inAddress+=4;
				break;
			case LDT_SP_HL:
				inAddress+=1;
				break;
		}
		return true; 
	}

	virtual bool Assemble(uint8_t* base)
	{
		uint64_t value;
		uint64_t value2;
		if (!expr1->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		if (!expr2->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		switch (type)
		{
			default:
			case LDT_none:
				FlagError("Unknown LD type");
				return false;
			case LDT_ii_M:
				if (!expr2->getValue(value))
				{
					FlagError("Unable to retrieve value for expression");
					return false;
				}
				WriteToBase(base,address,expr1->GetPrefix());
				WriteToBase(base,address+1,0x2A | (2<<4));
				WriteToBase(base,address+2,value&0xFF);
				WriteToBase(base,address+3,(value>>8)&0xFF);
				return true;
			case LDT_hl_M:
				if (!expr2->getValue(value))
				{
					FlagError("Unable to retrieve value for expression");
					return false;
				}
				WriteToBase(base,address,0x2A | (expr1->GetRR()<<4));
				WriteToBase(base,address+1,value&0xFF);
				WriteToBase(base,address+2,(value>>8)&0xFF);
				return true;
			case LDT_rr_M:
				if (!expr2->getValue(value))
				{
					FlagError("Unable to retrieve value for expression");
					return false;
				}
				WriteToBase(base,address,0xED);
				WriteToBase(base,address+1,0x4B | (expr1->GetRR()<<4));
				WriteToBase(base,address+2,value&0xFF);
				WriteToBase(base,address+3,(value>>8)&0xFF);
				return true;
			case LDT_ii_nn:
				if (!expr2->getValue(value))
				{
					FlagError("Unable to retrieve value for expression");
					return false;
				}
				WriteToBase(base,address,expr1->GetPrefix());
				WriteToBase(base,address+1,0x01 | (2<<4));
				WriteToBase(base,address+2,value&0xFF);
				WriteToBase(base,address+3,(value>>8)&0xFF);
				return true;
			case LDT_rr_nn:
				if (!expr2->getValue(value))
				{
					FlagError("Unable to retrieve value for expression");
					return false;
				}
				WriteToBase(base,address,0x01 | (expr1->GetRR()<<4));
				WriteToBase(base,address+1,value&0xFF);
				WriteToBase(base,address+2,(value>>8)&0xFF);
				return true;
			case LDT_r_n:
				if (!expr2->getValue(value))
				{
					FlagError("Unable to retrieve value for expression");
					return false;
				}
				WriteToBase(base,address,0x06 | (expr1->GetR()<<3));
				WriteToBase(base,address+1,value&0xFF);
				return true;
			case LDT_r_r:
				WriteToBase(base,address,0x40 | (expr1->GetR()<<3) | (expr2->GetR()));
				return true;
			case LDT_SP_HL:
				WriteToBase(base,address,0xF9);
				return true;
			case LDT_bcde_a:
				if (expr1->IsMbc())
				{
					WriteToBase(base,address,0x02);
				}
				else
				{
					WriteToBase(base,address,0x12);
				}
				return true;
			case LDT_a_bcde:
				if (expr2->IsMbc())
				{
					WriteToBase(base,address,0x0A);
				}
				else
				{
					WriteToBase(base,address,0x1A);
				}
				return true;
			case LDT_r_Mii:
				if (!expr2->getValue(value))
				{
					FlagError("Unable to calculate index displacement");
					return false;
				}
				if ((int64_t)value<-128 || (int64_t)value>127)
				{
					FlagError("Displacement out of range");
					return false;
				}
				WriteToBase(base,address,expr2->GetPrefix());
				WriteToBase(base,address+1,0x40 | (expr1->GetR()<<3) | (6));
				WriteToBase(base,address+2,value&0xFF);
				return true;
			case LDT_Mii_r:
				if (!expr1->getValue(value))
				{
					FlagError("Unable to calculate index displacement");
					return false;
				}
				if ((int64_t)value<-128 || (int64_t)value>127)
				{
					FlagError("Displacement out of range");
					return false;
				}
				WriteToBase(base,address,expr1->GetPrefix());
				WriteToBase(base,address+1,0x40 | (6<<3) | (expr2->GetR()));
				WriteToBase(base,address+2,value&0xFF);
				return true;
			case LDT_Mii_n:
				if (!expr1->getValue(value))
				{
					FlagError("Unable to calculate index displacement");
					return false;
				}
				if (!expr2->getValue(value2))
				{
					FlagError("Unable to calculate immediate byte");
					return false;
				}
				if ((int64_t)value<-128 || (int64_t)value>127)
				{
					FlagError("Displacement out of range");
					return false;
				}
				WriteToBase(base,address,expr1->GetPrefix());
				WriteToBase(base,address+1,0x36);
				WriteToBase(base,address+2,value&0xFF);
				WriteToBase(base,address+3,value2&0xFF);
				return true;
			case LDT_M_a:
				if (!expr1->getValue(value))
				{
					FlagError("Unable to retrieve value for expression");
					return false;
				}
				WriteToBase(base,address,0x32);
				WriteToBase(base,address+1,value&0xFF);
				WriteToBase(base,address+2,(value>>8)&0xFF);
				return true;
			case LDT_a_M:
				if (!expr2->getValue(value))
				{
					FlagError("Unable to retrieve value for expression");
					return false;
				}
				WriteToBase(base,address,0x3A);
				WriteToBase(base,address+1,value&0xFF);
				WriteToBase(base,address+2,(value>>8)&0xFF);
				return true;
			case LDT_M_hl:
				if (!expr1->getValue(value))
				{
					FlagError("Unable to retrieve value for expression");
					return false;
				}
				WriteToBase(base,address,0x22);
				WriteToBase(base,address+1,value&0xFF);
				WriteToBase(base,address+2,(value>>8)&0xFF);
				return true;
			case LDT_M_rr:
				if (!expr1->getValue(value))
				{
					FlagError("Unable to retrieve value for expression");
					return false;
				}
				WriteToBase(base,address,0xED);
				WriteToBase(base,address+1,0x43 | (expr2->GetRR()<<4));
				WriteToBase(base,address+2,value&0xFF);
				WriteToBase(base,address+3,(value>>8)&0xFF);
				return true;
			case LDT_a_rreg:
				WriteToBase(base,address,0xED);
				WriteToBase(base,address+1,0x5F);
				return true;
			case LDT_rreg_a:
				WriteToBase(base,address,0xED);
				WriteToBase(base,address+1,0x4F);
				return true;
				
		}

		return false;
	}
	virtual const char* Identify() const { return "InstructionLd"; }
};

class CInstructionPush : public CInstruction
{
protected:
	CExpression* expr;
public:
	CInstructionPush(CExpression* e) : expr(e)
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{ 
		if (!expr->IsRR2() && !expr->IsII())
		{
			FlagError("PUSH Illegal Operand Combination");
			return false;
		}
		address=inAddress; 
		if (!expr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (expr->IsII())
		{
			inAddress+=2; 
		}
		else
		{
			inAddress+=1; 
		}
		
		return true; 
	}

	virtual bool Assemble(uint8_t* base)
	{
		if (!expr->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		if (expr->IsII())
		{
			WriteToBase(base,address,expr->GetPrefix());
			WriteToBase(base,address+1,0xC5 | (2<<4));
		}
		else
		{
			WriteToBase(base,address,0xC5 | (expr->GetRR2()<<4));
		}

		return true;
	}
	virtual const char* Identify() const { return "InstructionPush"; }
};

class CInstructionPop : public CInstruction
{
protected:
	CExpression* expr;
public:
	CInstructionPop(CExpression* e) : expr(e)
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{ 
		if (!expr->IsRR2() && !expr->IsII())
		{
			FlagError("POP Illegal Operand Combination");
			return false;
		}
		address=inAddress; 
		if (!expr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (expr->IsII())
		{
			inAddress+=2; 
		}
		else
		{
			inAddress+=1; 
		}
		
		return true; 
	}

	virtual bool Assemble(uint8_t* base)
	{
		if (!expr->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		if (expr->IsII())
		{
			WriteToBase(base,address,expr->GetPrefix());
			WriteToBase(base,address+1,0xC1 | (2<<4));
		}
		else
		{
			WriteToBase(base,address,0xC1 | (expr->GetRR2()<<4));
		}

		return true;
	}
	virtual const char* Identify() const { return "InstructionPop"; }
};

class CInstructionIncDec : public CInstruction
{
protected:
	CExpression* expr;
	bool increm;
public:
	CInstructionIncDec(CExpression* e,bool inc) : expr(e),increm(inc)
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{ 
		if (!expr->IsR() && !expr->IsRR() && !expr->IsII() && !expr->IsIMem())
		{
			FlagError("INC/DEC Illegal Operand Combination");
			return false;
		}
		address=inAddress; 
		if (!expr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (expr->IsIMem())
		{
			inAddress+=3;
		}
		else
		{
			if (expr->IsII())
			{
				inAddress+=2;
			}
			else
			{
				inAddress+=1; 
			}
		}
		
		return true; 
	}

	virtual bool Assemble(uint8_t* base)
	{
		uint64_t value;

		if (!expr->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		if (increm)
		{
			if (expr->IsR())
			{
				WriteToBase(base,address,0x04 | (expr->GetR()<<3));
			}
			if (expr->IsRR())
			{
				WriteToBase(base,address,0x03 | (expr->GetRR()<<4));
			}
			if (expr->IsII())
			{
				WriteToBase(base,address,expr->GetPrefix());
				WriteToBase(base,address+1,0x03 | (2<<4));
			}
			if (expr->IsIMem())
			{
				if (!expr->getValue(value))
				{
					FlagError("Unable to calculate index displacement");
					return false;
				}
				if ((int64_t)value<-128 || (int64_t)value>127)
				{
					FlagError("Displacement out of range");
					return false;
				}
				WriteToBase(base,address,expr->GetPrefix());
				WriteToBase(base,address+1,0x04 | (6<<3));
				WriteToBase(base,address+2,value&0xFF);
			}
		}
		else
		{
			if (expr->IsR())
			{
				WriteToBase(base,address,0x05 | (expr->GetR()<<3));
			}
			if (expr->IsRR())
			{
				WriteToBase(base,address,0x0B | (expr->GetRR()<<4));
			}
			if (expr->IsII())
			{
				WriteToBase(base,address,expr->GetPrefix());
				WriteToBase(base,address+1,0x0B | (2<<4));
			}
			if (expr->IsIMem())
			{
				if (!expr->getValue(value))
				{
					FlagError("Unable to calculate index displacement");
					return false;
				}
				if ((int64_t)value<-128 || (int64_t)value>127)
				{
					FlagError("Displacement out of range");
					return false;
				}
				WriteToBase(base,address,expr->GetPrefix());
				WriteToBase(base,address+1,0x05 | (6<<3));
				WriteToBase(base,address+2,value&0xFF);
			}
		}
		return true;
	}
	virtual const char* Identify() const { return "InstructionIncDec"; }
};

class CInstructionAlu : public CInstruction
{
	enum ALU_Type
	{
		ALUT_none,
		ALUT_a_r,
		ALUT_a_n,
		ALUT_add_hl_rr,
		ALUT_adc_hl_rr,
		ALUT_sbc_hl_rr,
		ALUT_ii_rr,
		ALUT_a_Mii,
	};
protected:
	CExpression* expr1;
	CExpression* expr2;
	ALU_Type type;
	int op;
public:
	CInstructionAlu(CExpression* e1,CExpression* e2,int op) : expr1(e1),expr2(e2),type(ALUT_none),op(op)
	{
	}

	virtual bool IdentifyType()
	{
		if (expr1->IsR() && (expr1->GetR()==7) && expr2->IsR())
		{
			type=ALUT_a_r;
			return true;
		}
		if (expr1->IsR() && (expr1->GetR()==7) && (!expr2->IsOperand()) && (!expr2->IsMemory()))
		{
			type=ALUT_a_n;
			return true;
		}
		if (expr2->IsIMem() && expr1->IsR() && (expr1->GetR()==7))
		{
			type=ALUT_a_Mii;
			return true;
		}
		if ((op==0) && expr1->IsRR() && (expr1->GetRR()==2) && (expr2->IsRR()))
		{
			type=ALUT_add_hl_rr;
			return true;
		}
		if ((op==1) && expr1->IsRR() && (expr1->GetRR()==2) && (expr2->IsRR()))
		{
			type=ALUT_adc_hl_rr;
			return true;
		}
		if ((op==3) && expr1->IsRR() && (expr1->GetRR()==2) && (expr2->IsRR()))
		{
			type=ALUT_sbc_hl_rr;
			return true;
		}
		if ((op==0) && expr1->IsII() && ((expr2->IsRR() && expr2->GetRR()!=2) || (expr2->IsII() && expr1->GetPrefix()==expr2->GetPrefix())))
		{
			type=ALUT_ii_rr;
			return true;
		}


		return false;
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{ 
		address=inAddress; 
		if (!expr1->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (!expr2->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (!IdentifyType())
		{
			FlagError("ALU unknown operand combination");
			return false;
		}
		switch (type)
		{
			default:
			case ALUT_none:
				FlagError("Unknown ALU type");
				return false;
			case ALUT_a_r:
				inAddress+=1;
				break;
			case ALUT_a_Mii:
				inAddress+=3;
				break;
			case ALUT_a_n:
				inAddress+=2;
				break;
			case ALUT_adc_hl_rr:
				inAddress+=2;
				break;
			case ALUT_sbc_hl_rr:
				inAddress+=2;
				break;
			case ALUT_add_hl_rr:
				inAddress+=1;
				break;
			case ALUT_ii_rr:
				inAddress+=2;
				break;
		}
		return true; 
	}

	virtual bool Assemble(uint8_t* base)
	{
		uint64_t value;
		if (!expr1->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		if (!expr2->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		switch (type)
		{
			default:
			case ALUT_none:
				FlagError("Unknown ALU type");
				return false;
			case ALUT_a_r:
				WriteToBase(base,address,0x80 | (op<<3) | (expr2->GetR()));
				return true;
			case ALUT_add_hl_rr:
				WriteToBase(base,address,0x09 | (expr2->GetRR()<<4));
				return true;
			case ALUT_adc_hl_rr:
				WriteToBase(base,address,0xED);
				WriteToBase(base,address+1,0x4A | (expr2->GetRR()<<4));
				return true;
			case ALUT_sbc_hl_rr:
				WriteToBase(base,address,0xED);
				WriteToBase(base,address+1,0x42 | (expr2->GetRR()<<4));
				return true;
			case ALUT_ii_rr:
				WriteToBase(base,address,expr1->GetPrefix());
				if (expr2->IsII())
				{
					WriteToBase(base,address+1,0x09 | (2<<4));
				}
				else
				{
					WriteToBase(base,address+1,0x09 | (expr2->GetRR()<<4));
				}
				return true;
			case ALUT_a_n:
				if (!expr2->getValue(value))
				{
					FlagError("Unable to retrieve value for expression");
					return false;
				}
				WriteToBase(base,address,0xC6 | (op<<3));
				WriteToBase(base,address+1,value&0xFF);
				return true;
			case ALUT_a_Mii:
				if (!expr2->getValue(value))
				{
					FlagError("Unable to calculate index displacement");
					return false;
				}
				if ((int64_t)value<-128 || (int64_t)value>127)
				{
					FlagError("Displacement out of range");
					return false;
				}
				WriteToBase(base,address,expr2->GetPrefix());
				WriteToBase(base,address+1,0x80 | (op<<3) | (6));
				WriteToBase(base,address+2,value&0xFF);
				return true;
				
		}

		return false;
	}
	virtual const char* Identify() const { return "InstructionAlu"; }
};

class CInstructionJr : public CInstruction
{
protected:
	CExpression* cond;
	CExpression* expr;
public:
	CInstructionJr(CExpression* c,CExpression* e) : cond(c),expr(e)
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		if (cond)
		{
			if (!cond->IsCC())
			{
				FlagError("JR Illegal Condition Combination");
				return false;
			}
		}
		if (expr->IsOperand() || expr->IsMemory())
		{
			FlagError("JR Illegal Operand Combination");
			return false;
		}
		address=inAddress; 
		if (!expr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}

		inAddress+=2; 
		
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		if (cond)
		{
			if (!cond->Assemble(base))
			{
				FlagError("Unable to assemble condition");
				return false;
			}
		}
		if (!expr->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		uint64_t value;
		uint64_t disp;
		if (!expr->getValue(value))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		disp=value - (address+2);
		if ((int64_t)disp<-128 || (int64_t)disp>127)
		{
			FlagError("Displacement is outside 8Bit range!!");
			printf("and it was : %d\n",(int)disp);
			return false;
		}
		if (cond)
		{
			WriteToBase(base,address,0x00 | ((cond->GetCC()+4)<<3));
		}
		else
		{
			WriteToBase(base,address,0x00 | (3<<3));
		}
		WriteToBase(base,address+1,disp&0xFF);
		return true;
	}
	virtual const char* Identify() const { return "InstructionJr"; }
};

class CInstructionHalt : public CInstruction
{
protected:
public:
	CInstructionHalt()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=1; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0x76);
		return true;
	}
	virtual const char* Identify() const { return "InstructionHalt"; }
};

class CInstructionDjnz : public CInstruction
{
protected:
	CExpression* expr;
public:
	CInstructionDjnz(CExpression* e) : expr(e)
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		if (expr->IsOperand() || expr->IsMemory())
		{
			FlagError("DJNZ Illegal Operand Combination");
			return false;
		}
		address=inAddress; 
		if (!expr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}

		inAddress+=2; 
		
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		if (!expr->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		uint64_t value;
		uint64_t disp;
		if (!expr->getValue(value))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		disp=value - (address+2);
		if ((int64_t)disp<-128 || (int64_t)disp>127)
		{
			FlagError("Displacement is outside 8Bit range!!");
			printf("and it was : %d\n",(int)disp);
			return false;
		}
		WriteToBase(base,address,0x10);
		WriteToBase(base,address+1,disp&0xFF);
		return true;
	}
	virtual const char* Identify() const { return "InstructionDjnz"; }
};

class CInstructionDi : public CInstruction
{
protected:
public:
	CInstructionDi()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=1; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0xF3);
		return true;
	}
	virtual const char* Identify() const { return "InstructionDi"; }
};

class CInstructionEi : public CInstruction
{
protected:
public:
	CInstructionEi()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=1; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0xFB);
		return true;
	}
	virtual const char* Identify() const { return "InstructionEi"; }
};

class CInstructionLdd : public CInstruction
{
protected:
public:
	CInstructionLdd()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=2; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0xED);
		WriteToBase(base,address+1,0xA8);
		return true;
	}
	virtual const char* Identify() const { return "InstructionLddr"; }
};

class CInstructionLddr : public CInstruction
{
protected:
public:
	CInstructionLddr()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=2; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0xED);
		WriteToBase(base,address+1,0xB8);
		return true;
	}
	virtual const char* Identify() const { return "InstructionLddr"; }
};

class CInstructionLdi : public CInstruction
{
protected:
public:
	CInstructionLdi()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=2; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0xED);
		WriteToBase(base,address+1,0xA0);
		return true;
	}
	virtual const char* Identify() const { return "InstructionLdi"; }
};

class CInstructionLdir : public CInstruction
{
protected:
public:
	CInstructionLdir()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=2; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0xED);
		WriteToBase(base,address+1,0xB0);
		return true;
	}
	virtual const char* Identify() const { return "InstructionLdir"; }
};

class CInstructionNeg : public CInstruction
{
protected:
public:
	CInstructionNeg()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=2; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0xED);
		WriteToBase(base,address+1,0x44);
		return true;
	}
	virtual const char* Identify() const { return "InstructionNeg"; }
};

class CInstructionNop : public CInstruction
{
protected:
public:
	CInstructionNop()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=1; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0x00);
		return true;
	}
	virtual const char* Identify() const { return "InstructionNop"; }
};

class CInstructionRet : public CInstruction
{
protected:
	CExpression* cond;
public:
	CInstructionRet(CExpression* cond) : cond(cond)
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		if (cond)
		{
			if (!cond->IsCC())
			{
				FlagError("RET Illegal Condition Combination");
				return false;
			}
		}
		inAddress+=1; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		if (cond)
		{
			WriteToBase(base,address,0xC0 | (cond->GetCC()<<3));
		}
		else
		{
			WriteToBase(base,address,0xC9);
		}
		return true;
	}
	virtual const char* Identify() const { return "InstructionRet"; }
};

class CInstructionReti : public CInstruction
{
protected:
public:
	CInstructionReti()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=2; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0xED);
		WriteToBase(base,address+1,0x4D);
		return true;
	}
	virtual const char* Identify() const { return "InstructionReti"; }
};

class CInstructionRla : public CInstruction
{
protected:
public:
	CInstructionRla()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=1; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0x17);
		return true;
	}
	virtual const char* Identify() const { return "InstructionRla"; }
};

class CInstructionRlca : public CInstruction
{
protected:
public:
	CInstructionRlca()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=1; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0x07);
		return true;
	}
	virtual const char* Identify() const { return "InstructionRlca"; }
};

class CInstructionRra : public CInstruction
{
protected:
public:
	CInstructionRra()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=1; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0x1F);
		return true;
	}
	virtual const char* Identify() const { return "InstructionRra"; }
};

class CInstructionRrca : public CInstruction
{
protected:
public:
	CInstructionRrca()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=1; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0x0F);
		return true;
	}
	virtual const char* Identify() const { return "InstructionRrca"; }
};

class CInstructionOut : public CInstruction
{
	enum OUT_Type
	{
		OUTT_none,
		OUTT_M_a,
	};
protected:
	CExpression* expr1;
	CExpression* expr2;
	OUT_Type type;
public:
	CInstructionOut(CExpression* e1,CExpression* e2) : expr1(e1),expr2(e2),type(OUTT_none)
	{
	}

	virtual bool IdentifyType()
	{
		if (!expr1->IsOperand() && expr1->IsMemory() && expr2->IsR() && (expr2->GetR()==7))
		{
			type=OUTT_M_a;
			return true;
		}

		return false;
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{ 
		address=inAddress; 
		if (!expr1->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (!expr2->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (!IdentifyType())
		{
			FlagError("OUT unknown operand combination");
			return false;
		}
		switch (type)
		{
			default:
			case OUTT_none:
				FlagError("Unknown OUT type");
				return false;
			case OUTT_M_a:
				inAddress+=2;
				break;
		}
		return true; 
	}

	virtual bool Assemble(uint8_t* base)
	{
		uint64_t value;
		if (!expr1->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		if (!expr2->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		switch (type)
		{
			default:
			case OUTT_none:
				FlagError("Unknown OUT type");
				return false;
			case OUTT_M_a:
				if (!expr1->getValue(value))
				{
					FlagError("Unable to retrieve value for expression");
					return false;
				}
				WriteToBase(base,address,0xD3);
				WriteToBase(base,address+1,value&0xFF);
				return true;
				
		}

		return false;
	}
	virtual const char* Identify() const { return "InstructionOut"; }
};

class CInstructionIn : public CInstruction
{
	enum IN_Type
	{
		INT_none,
		INT_a_M,
		INT_a_C,
	};
protected:
	CExpression* expr1;
	CExpression* expr2;
	IN_Type type;
public:
	CInstructionIn(CExpression* e1,CExpression* e2) : expr1(e1),expr2(e2),type(INT_none)
	{
	}

	virtual bool IdentifyType()
	{
		if (expr1->IsR() && (expr1->GetR()==7) && expr2->IsOperand() && expr2->IsMC())
		{
			type=INT_a_C;
			return true;
		}
		if (!expr2->IsOperand() && expr2->IsMemory() && expr1->IsR() && (expr1->GetR()==7))
		{
			type=INT_a_M;
			return true;
		}

		return false;
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{ 
		address=inAddress; 
		if (!expr1->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (!expr2->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (!IdentifyType())
		{
			FlagError("IN unknown operand combination");
			return false;
		}
		switch (type)
		{
			default:
			case INT_none:
				FlagError("Unknown IN type");
				return false;
			case INT_a_C:
				inAddress+=2;
				break;
			case INT_a_M:
				inAddress+=2;
				break;
		}
		return true; 
	}

	virtual bool Assemble(uint8_t* base)
	{
		uint64_t value;
		if (!expr1->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		if (!expr2->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		switch (type)
		{
			default:
			case INT_none:
				FlagError("Unknown IN type");
				return false;
			case INT_a_M:
				if (!expr2->getValue(value))
				{
					FlagError("Unable to retrieve value for expression");
					return false;
				}
				WriteToBase(base,address,0xDB);
				WriteToBase(base,address+1,value&0xFF);
				return true;
			case INT_a_C:
				WriteToBase(base,address,0xED);
				WriteToBase(base,address+1,0x78);
				return true;
				
		}

		return false;
	}
	virtual const char* Identify() const { return "InstructionIn"; }
};

class CInstructionResSetBit : public CInstruction
{
protected:
	CExpression* expr1;
	CExpression* expr2;
	int op;
	bool imem;
public:
	CInstructionResSetBit(CExpression* e1,CExpression* e2,int op) : expr1(e1),expr2(e2),op(op)
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{ 
		address=inAddress; 
		if (!expr1->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (!expr2->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (!expr1->IsOperand() && !expr1->IsMemory() && expr2->IsR())
		{
			inAddress+=2;
			imem=false;
			return true;
		}
		if (!expr1->IsOperand() && !expr1->IsMemory() && expr2->IsIMem())
		{
			inAddress+=4;
			imem=true;
			return true;
		}

		FlagError("RES/SET/BIT Illegal Operand Combination");
		return false;
	}

	virtual bool Assemble(uint8_t* base)
	{
		uint64_t value;
		uint64_t disp;
		if (!expr1->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		if (!expr2->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		if (!expr1->getValue(value))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (value>7)
		{
			FlagError("bit number out of range");
			return false;
		}
		if (imem)
		{
			if (!expr2->getValue(disp))
			{
				FlagError("Unable to calculate index displacement");
				return false;
			}
			if ((int64_t)disp<-128 || (int64_t)disp>127)
			{
				FlagError("Displacement out of range");
				return false;
			}
		}


		if (imem)
		{
			WriteToBase(base,address,expr2->GetPrefix());
			WriteToBase(base,address+1,0xCB);
			WriteToBase(base,address+2,disp&0xFF);
			if (op==0)
			{
				WriteToBase(base,address+3,0x80 | (value<<3) | (6));
				return true;
			}
			if (op==1)
			{
				WriteToBase(base,address+3,0xC0 | (value<<3) | (6));
				return true;
			}
			if (op==2)
			{
				WriteToBase(base,address+3,0x40 | (value<<3) | (6));
				return true;
			}
		}	
		else
		{
			WriteToBase(base,address,0xCB);
			if (op==0)
			{
				WriteToBase(base,address+1,0x80 | (value<<3) | (expr2->GetR()));
				return true;
			}
			if (op==1)
			{
				WriteToBase(base,address+1,0xC0 | (value<<3) | (expr2->GetR()));
				return true;
			}
			if (op==2)
			{
				WriteToBase(base,address+1,0x40 | (value<<3) | (expr2->GetR()));
				return true;
			}
		}

		return false;
	}
	virtual const char* Identify() const { return "InstructionRes"; }
};


class CInstructionJp : public CInstruction
{
protected:
	CExpression* cond;
	CExpression* expr;
public:
	CInstructionJp(CExpression* c,CExpression* e) : cond(c),expr(e)
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		if (cond)
		{
			if (!cond->IsCC())
			{
				FlagError("JP Illegal Condition Combination");
				return false;
			}
		}
		address=inAddress; 
		if (!expr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (cond==NULL && expr->IsR() && expr->GetR()==6)
		{
			inAddress+=1;
		}
		else
		{
			if (expr->IsOperand() || expr->IsMemory())
			{
				FlagError("JP Illegal Operand Combination");
				return false;
			}
			inAddress+=3; 
		}

		
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		if (cond)
		{
			if (!cond->Assemble(base))
			{
				FlagError("Unable to assemble condition");
				return false;
			}
		}
		if (!expr->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		if (cond==NULL && expr->IsR() && expr->GetR()==6)
		{
			WriteToBase(base,address,0xE9);
			return true;
		}
		uint64_t value;
		if (!expr->getValue(value))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (cond)
		{
			WriteToBase(base,address,0xC2 | (cond->GetCC()<<3));
		}
		else
		{
			WriteToBase(base,address,0xC3);
		}
		WriteToBase(base,address+1,value&0xFF);
		WriteToBase(base,address+2,(value>>8)&0xFF);
		return true;
	}
	virtual const char* Identify() const { return "InstructionJP"; }
};

class CInstructionEx : public CInstruction
{
protected:
	CExpression* expr1;
	CExpression* expr2;
public:
	CInstructionEx(CExpression* e1,CExpression* e2) : expr1(e1),expr2(e2)
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{ 
		address=inAddress; 
		if (expr1->IsAF() && expr2->IsAF_())
		{
			inAddress+=1;
			return true;
		}
		if (expr1->IsRR() && expr2->IsRR() && (expr1->GetRR()==1) && (expr2->GetRR()==2))
		{
			inAddress+=1;
			return true;
		}
		if (expr1->IsMsp() && expr2->IsRR() && (expr2->GetRR()==2))
		{
			inAddress+=1;
			return true;
		}

		FlagError("EX Illegal Operand Combination");
		return false;
	}

	virtual bool Assemble(uint8_t* base)
	{
		if (expr1->IsAF() && expr2->IsAF_())
		{
			WriteToBase(base,address,0x08);
			return true;
		}
		if (expr1->IsRR() && expr2->IsRR() && (expr1->GetRR()==1) && (expr2->GetRR()==2))
		{
			WriteToBase(base,address,0xEB);
			return true;
		}
		if (expr1->IsMsp() && expr2->IsRR() && (expr2->GetRR()==2))
		{
			WriteToBase(base,address,0xE3);
			return true;
		}
		return false;
	}
	virtual const char* Identify() const { return "InstructionEx"; }
};

class CInstructionRot : public CInstruction
{
	enum ROT_Type
	{
		ROTT_none,
		ROTT_a_r,
	};
protected:
	CExpression* expr;
	ROT_Type type;
	int op;
public:
	CInstructionRot(CExpression* e,int op) : expr(e),type(ROTT_none),op(op)
	{
	}

	virtual bool IdentifyType()
	{
		if (expr->IsR())
		{
			type=ROTT_a_r;
			return true;
		}

		return false;
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{ 
		address=inAddress; 
		if (!expr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (!IdentifyType())
		{
			FlagError("ROT unknown operand combination");
			return false;
		}
		switch (type)
		{
			default:
			case ROTT_none:
				FlagError("Unknown ROT type");
				return false;
			case ROTT_a_r:
				inAddress+=2;
				break;
		}
		return true; 
	}

	virtual bool Assemble(uint8_t* base)
	{
		if (!expr->Assemble(base))
		{
			FlagError("Unable to assemble expression");
			return false;
		}
		switch (type)
		{
			default:
			case ROTT_none:
				FlagError("Unknown ROT type");
				return false;
			case ROTT_a_r:
				WriteToBase(base,address,0xCB);
				WriteToBase(base,address+1,0x00 | (op<<3) | (expr->GetR()));
				return true;
		}

		return false;
	}
	virtual const char* Identify() const { return "InstructionRot"; }
};

class CInstructionScf : public CInstruction
{
protected:
public:
	CInstructionScf()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=1; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0x37);
		return true;
	}
	virtual const char* Identify() const { return "InstructionScf"; }
};

class CInstructionCcf : public CInstruction
{
protected:
public:
	CInstructionCcf()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=1; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0x3F);
		return true;
	}
	virtual const char* Identify() const { return "InstructionCcf"; }
};

class CInstructionExx : public CInstruction
{
protected:
public:
	CInstructionExx()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=1; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0xD9);
		return true;
	}
	virtual const char* Identify() const { return "InstructionExx"; }
};

class CInstructionCpl : public CInstruction
{
protected:
public:
	CInstructionCpl()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=1; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0x2F);
		return true;
	}
	virtual const char* Identify() const { return "InstructionCpl"; }
};

class CInstructionDaa : public CInstruction
{
protected:
public:
	CInstructionDaa()
	{
	}

	virtual bool PrePass(uint64_t& inAddress) 
	{
		address=inAddress; 
		inAddress+=1; 
		return true; 
	}
	virtual bool Assemble(uint8_t* base)
	{
		WriteToBase(base,address,0x27);
		return true;
	}
	virtual const char* Identify() const { return "InstructionDaa"; }
};



class CStructured: public CNode
{
protected:
	CExpression* expr;
	NodeList* iftrue;
	NodeList* iffalse;
	uint64_t conditionResult;
public:
	CStructured(CExpression* expr,NodeList* iftrue,NodeList* iffalse) : expr(expr),iftrue(iftrue),iffalse(iffalse) { }

	virtual bool PrePass(uint64_t& inAddress)
	{
		// For now, expr must evaluate during prepass so forward refs are illegal
		address=inAddress;
		if (!expr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (!expr->getValue(conditionResult))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}

		if (conditionResult==0)
		{
			if (iffalse)
			{
				for (size_t a=0;a<iffalse->size();a++)
				{
					if (!(*iffalse)[a]->PrePass(inAddress))
					{
						FlagError("Error : Failed during PrePass of false condition");
						return false;
					}
				}
			}
		}
		else
		{
			for (size_t a=0;a<iftrue->size();a++)
			{
				if (!(*iftrue)[a]->PrePass(inAddress))
				{
					FlagError("Error : Failed during PrePass of true condition");
					return false;
				}
			}
		}
		return true;
	}

	virtual bool Assemble(uint8_t* base)
	{
		if (!expr->Assemble(base))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}

		if (conditionResult==0)
		{
			if (iffalse)
			{
				for (size_t a=0;a<iffalse->size();a++)
				{
					if (!(*iffalse)[a]->Assemble(base))
					{
						FlagError("Error : Failed during Assemble of false condition");
						return false;
					}
				}
			}
		}
		else
		{
			for (size_t a=0;a<iftrue->size();a++)
			{
				if (!(*iftrue)[a]->Assemble(base))
				{
					FlagError("Error : Failed during Assemble of true condition");
					return false;
				}
			}
		}
		return true;
	}
	
	virtual const char* Identify() const { return "CStructured"; }
};

class CDoLoop: public CNode
{
protected:
	CExpression* expr;
	NodeList* repeat;
	uint64_t count;
	uint64_t length;
public:
	CDoLoop(CExpression* expr,NodeList* repeat) : expr(expr),repeat(repeat) { }

	virtual bool PrePass(uint64_t& inAddress)
	{
		// For now, expr must evaluate during prepass so forward refs are illegal
		address=inAddress;
		if (!expr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (!expr->getValue(count))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}

		for (size_t a=0;a<repeat->size();a++)
		{
			if (!(*repeat)[a]->PrePass(inAddress))
			{
				FlagError("Error : Failed during PrePass of repeat block");
				return false;
			}
		}
		length=inAddress-address;
		std::cout << "REPEAT : " << length << std::endl;
		inAddress=address + (inAddress-address)*count;
		return true;
	}

	virtual bool Assemble(uint8_t* base)
	{
		if (!expr->Assemble(base))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}

		for (size_t a=0;a<repeat->size();a++)
		{
			if (!(*repeat)[a]->Assemble(base))
			{
				FlagError("Error : Failed during PrePass of repeat block");
				return false;
			}
		}
		for (uint64_t b=1;b<count;b++)
		{
			for (uint64_t a=0;a<length;a++)
			{
				WriteToBase(base,address+b*length+a,ReadFromBase(base,address+a));
			}
		}
		return true;
	}
	
	virtual const char* Identify() const { return "CDoLoop"; }
};

class CRepeat: public CNode
{
protected:
	CExpression* expr;
	CNode* repeat;
	uint64_t count;
	uint64_t length;
public:
	CRepeat(CExpression* expr,CNode* repeat) : expr(expr),repeat(repeat) { }

	virtual bool PrePass(uint64_t& inAddress)
	{
		// For now, expr must evaluate during prepass so forward refs are illegal
		address=inAddress;
		if (!expr->PrePass(inAddress))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}
		if (!expr->getValue(count))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}

		if (!repeat->PrePass(inAddress))
		{
			FlagError("Error : Failed during PrePass of repeat block");
			return false;
		}
		length=inAddress-address;
		std::cout << "REPEAT : " << length << std::endl;
		inAddress=address + (inAddress-address)*count;
		return true;
	}

	virtual bool Assemble(uint8_t* base)
	{
		if (!expr->Assemble(base))
		{
			FlagError("Unable to retrieve value for expression");
			return false;
		}

		if (!repeat->Assemble(base))
		{
			FlagError("Error : Failed during Assemble of repeat block");
			return false;
		}
		
		for (uint64_t b=1;b<count;b++)
		{
			for (uint64_t a=0;a<length;a++)
			{
				WriteToBase(base,address+b*length+a,ReadFromBase(base,address+a));
			}
		}
		return true;
	}
	
	virtual const char* Identify() const { return "CRepeat"; }
};

