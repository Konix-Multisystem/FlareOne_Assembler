/*

 Line numbers reported should be fine since preprocessing records that information before lexer

 Bugger it, write another assembler!

*/

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <iomanip>
#include <direct.h>
//#include <unistd.h>
#include "ast.h"

using namespace std;

#if !defined(_MAX_PATH)
#define _MAX_PATH 256
#endif // !defined(_MAX_PATH)

struct yy_buffer_state;
typedef yy_buffer_state* YY_BUFFER_STATE;
extern int yyparse();
extern YY_BUFFER_STATE yy_scan_buffer(char *, size_t);

extern uint64_t execAddress;
//extern int yydebug;
std::string g_localSymbolPrefix;

#define ASSEMBLER_VERSION		"0.1c"
extern std::map<std::string,CSymbol*> symbolTable;
extern std::map<std::string,CGlobal*> globalSymbol;
class CompilerOptions
{
public:

	CompilerOptions() { inputFileNum = 0;  outputFile = NULL; startSymbol = NULL; comMode = 0; }
	
	char *inputFiles[32];
	char *outputFile;

	char *startSymbol;

	int inputFileNum;
	int comMode;
};

CompilerOptions	opts;


int Usage()
{
	cerr << "Usage: assembler [opts] inputfile" << endl;
	cerr << "Assemble Z80 source into .FL1 (default writes to stdout)" <<endl <<endl;
	cerr << "-c			Use Flare One COM Style" << endl;
	cerr << "-v			print version" << endl;
	cerr << "-o outputfile		output to file rather than stdout" << endl;
	cerr << "-s startsymbol		set execution start to value of startsymbol" << endl;
	cerr << "-h --help		print usage" << endl;

	return 0;
}

char* preprocessed;
int preprocessedSize;

std::vector<std::string> CheapTokenise(std::string& line)
{
	std::vector<std::string> tokens;

	char lastQuote;
	size_t a=0;
	int state=0;
	std::string tmp;
	if (isspace(line[0]))
		tokens.push_back(" ");
	while (a<=line.length())
	{
		switch (state)
		{
			case 0:
				if (line[a]<=32)
					break;
				state=1;
				// fall through intentional
			case 1:
				if (line[a]>32)
				{
					if (line[a]=='\'' || line[a]=='"')
					{
						tokens.push_back(tmp);
						tmp.clear();
						tmp+=line[a];
						lastQuote=line[a];
						state=2;
						break;
					}
					if (line[a]==',' || line[a]=='(' || line[a]==')' || line[a]=='+' || line[a]=='*' || line[a]=='-' || line[a]=='/')
					{
						tokens.push_back(tmp);
						std::string seper = "";
						seper += line[a];
						tokens.push_back(seper);
						tmp.clear();
						break;
					}
					else
					{
						tmp+=toupper(line[a]);
						break;
					}
				}
				if (tmp=="##!PREPROC!##" && line[a+1]==' ')		// Bodge to ensure any indent at the start of a line in a macro is retained
					tokens.push_back(tmp+" ");
				else
					tokens.push_back(tmp);
				tmp.clear();
				state=0;
				break;
			case 2:
				if ((line[a]==lastQuote))
				{
					tmp+=lastQuote;
					tokens.push_back(tmp);
					tmp.clear();
					state=0;
					break;
				}
				tmp+=line[a];
				break;
		}
		a++;
	}

	std::vector<std::string>::iterator iter=tokens.begin();
	while (iter!=tokens.end())
	{
		int ok=0;
		for (a=0;a<iter->length();a++)
		{
			if (iter->at(a)>32)
				ok=1;
		}
		if (ok==0)
		{
			iter=tokens.erase(iter);
		}
		else
			++iter;
	}
	
	return tokens;
}

struct SMacroDef
{
	std::vector<std::string> params;
	std::string name;
	int lineStart;
	int lineEnd;
};

std::map<std::string,SMacroDef> macroTable;

void CreateMacro(std::vector<std::string>& tokens, size_t start,int num)
{
	if (macroTable.find(tokens[start])!=macroTable.end())
	{
		cerr << "Macro " << tokens[start] << " Already Defined" << std::endl;
		exit(9);
	}

	SMacroDef tmp;
	tmp.name=tokens[start];
	tmp.lineStart=num;
	tmp.lineEnd=-1;

	// Grab parameters
	int state=0;
	start+=2;
	while(start<tokens.size())
	{
		switch (state)
		{
			case 0:
				tmp.params.push_back(tokens[start]);
				state=1;
				break;
			case 1:
				if (tokens[start][0]==',')
				{
					state=0;
					break;
				}
				state=2;
				break;
			case 2:
				break;
		}
		start++;
	}

	macroTable[tmp.name]=tmp;	
}

std::string lastMacro;

bool IsLineMacroInstance(std::string& outputBuffer,std::string& line,SMacroDef& macro,std::vector<std::string>& instanceParams)
{
	std::vector<std::string> tokens=CheapTokenise(line);
	size_t a=0;
	while (a<tokens.size())
	{
		if (macroTable.find(tokens[a])!=macroTable.end())
		{
			macro=macroTable[tokens[a]];
			//Dump parts before macro
			size_t b=0;
			while (b<a)
			{
				outputBuffer+=tokens[b]+" ";
				b++;
			}
			//printf("MACRO USE FOUND\n");
			// Grab parameters
			int state=0;
			a++;
			while(a<tokens.size())
			{
				switch (state)
				{
					case 0:
						instanceParams.push_back(tokens[a]);
						state=1;
						break;
					case 1:
						if (tokens[a][0]==',')
						{
							state=0;
							break;
						}
						state=2;
						//break; fall through intended
					case 2:
						std::string previous = instanceParams.back();
						previous += " " + tokens[a];
						instanceParams.pop_back();
						instanceParams.push_back(previous);
						state = 1;
						break;
				}
				a++;
			}
			return true;
		}
		a++;
	}
	return false;
}

bool IsLineMacroDef(std::string& line,int num)
{
	std::vector<std::string> tokens=CheapTokenise(line);
	size_t a=5;
	while (a<tokens.size())
	{
		if (stricmp(tokens[a].c_str(),"MACRO")==0)
		{
			//printf("MACRO FOUND %s\n",tokens[a-1].c_str());			// If we find macro, we need to parse parameters after it and cache them and grab the previous token which should be the macro name
			CreateMacro(tokens,a-1,num);
			lastMacro=tokens[a-1];
			return true;
		}
		a++;
	}
	return false;
}

bool IsLineEndMacro(std::string& line,int num)
{
	std::vector<std::string> tokens=CheapTokenise(line);
	size_t a=5;
	//size_t b;
	while (a<tokens.size())
	{
		if (stricmp(tokens[a].c_str(),"ENDM")==0)
		{
			//printf("ENDMACRO FOUND\n");
			if (macroTable.find(lastMacro)==macroTable.end())
			{
				cerr << "Unknown Macro : " << lastMacro << " for end marker" << std::endl;
				exit(9);
			}
			//printf("HERE\n");
			macroTable[lastMacro].lineEnd=num;
			//printf("Macro : %s\n",lastMacro.c_str());
			//printf("Line Start : %d\n",macroTable[lastMacro].lineStart);
			//printf("Line End : %d\n",macroTable[lastMacro].lineEnd);
			//for (b=0;b<macroTable[lastMacro].params.size();b++)
			//{
			//	printf("Params : %s\n",macroTable[lastMacro].params[b].c_str());
			//}
			return true;
		}
		a++;
	}
	return false;
}

void Replace(std::string& str,const char* find,const char* replace)
{
	size_t pos = str.find(find);
	while (pos!=string::npos)
	{
		str.replace(pos,strlen(find),replace);

		pos = str.find(find);
	}
}

void ReplaceSafe(std::string& str,const char* find,const char* replace)
{
	size_t pos;
	int stage=0;

	for (pos=0;pos<str.length();pos++)
	{
//		printf("pos : %08X | stage : %d | total : %08X\r",pos,stage,str.length());
		switch (stage)
		{
			case 0:
				if (str[pos]=='"')
					stage=1;

				if (str.find(find)==pos)
				{
					str.replace(pos,strlen(find),replace);
				}
				break;
			case 1:
				if (str[pos]=='"')
					stage=0;
				break;
		}
	}

/*
	size_t pos = str.find(find);
	while (pos!=string::npos)
	{
		str.replace(pos,strlen(find),replace);

		pos = str.find(find);
	}*/
}


void InsertMacro(std::string& output,std::vector<std::string>& lines,SMacroDef& macro,std::vector<std::string>& paramsIn)
{
	int a;
	size_t c;
	for (c=0;c<paramsIn.size();c++)
	{
//		printf("PARAM %d : %s\n",c,paramsIn[c].c_str());
	}
	output+="\n";

	// Need recursive approach, there are some macros, that use other macros :(

	string tmpLine;
	int state=0;
	for (a=macro.lineStart;a<=macro.lineEnd;a++)
	{
		std::vector<std::string> tokens=CheapTokenise(lines[a]);
		size_t b=0;
		while (b<tokens.size())
		{
			if (macro.params.size())
			{
				// Check for token = param and replace
				for (c=0;c<macro.params.size();c++)			// This is slowww
				{
//					printf("Checking %s : %s\n",tokens[b].c_str(),macro.params[c].c_str());
					if (stricmp(macro.params[c].c_str(),tokens[b].c_str())==0)
					{
						// replace token
						tokens[b]=paramsIn[c];
//						printf("PARAM replaced %s : %s\n",macro.params[c].c_str(),paramsIn[c].c_str());
					}
				}
			}
			else
			{
				// Check for token = param and replace
				for (c=0;c<paramsIn.size();c++)			// This is slowww
				{
					char tmp[20];
	
					sprintf(tmp,"@%d",c+1);
					Replace(tokens[b],tmp,paramsIn[c].c_str());
				}
			}
			b++;
		}
		b=0;
		int lineUsed=false;
		while (b<tokens.size())
		{
			switch (state)
			{
				case 0:							// LOOKING FOR IFS
					if (stricmp(tokens[b].c_str(),"IFS")==0)
					{
						if (b+2<tokens.size())
						{
							if (stricmp(tokens[b+1].c_str(),tokens[b+2].c_str())==0)
							{
								b+=2;
								state=3;
								break;
							}
							else
							{
								b+=2;
								state=1;
								break;
							}
						}
						else
						{
							printf("Wrong number of parameters to IFS\n");
							exit(1);
						}
					}
					if (stricmp(tokens[b].c_str(),"EXITM")==0)
					{
						state=5;
						break;
					}
					tmpLine+=tokens[b];
					tmpLine+=" ";
					lineUsed=true;
					break;
				case 1:							// IFS condition false
					if (stricmp(tokens[b].c_str(),"IFS")==0)
					{
						printf("Error nested IFS\n");
						exit(2);
					}
					if (stricmp(tokens[b].c_str(),"ELSE")==0)
					{
						state=2;
						break;
					}
					if (stricmp(tokens[b].c_str(),"ENDIF")==0)
					{
						state=0;
						break;
					}
					break;
				case 2:							// ELSE condition true
					if (stricmp(tokens[b].c_str(),"IFS")==0)
					{
						printf("Error nested IFS\n");
						exit(2);
					}
					if (stricmp(tokens[b].c_str(),"ELSE")==0)
					{
						printf("Error nested ELSE\n");
						exit(2);
						break;
					}
					if (stricmp(tokens[b].c_str(),"ENDIF")==0)
					{
						state=0;
						break;
					}
					if (stricmp(tokens[b].c_str(),"EXITM")==0)
					{
						state=5;
						break;
					}
					tmpLine+=tokens[b];
					tmpLine+=" ";
					lineUsed=true;
					break;
				case 3:							// IFS condition true
					if (stricmp(tokens[b].c_str(),"IFS")==0)
					{
						printf("Error nested IFS\n");
						exit(2);
					}
					if (stricmp(tokens[b].c_str(),"ENDIF")==0)
					{
						state=0;
						break;
					}
					if (stricmp(tokens[b].c_str(),"ELSE")==0)
					{
						state=4;
						break;
					}
					if (stricmp(tokens[b].c_str(),"EXITM")==0)
					{
						state=5;
						break;
					}
					tmpLine+=tokens[b];
					tmpLine+=" ";
					lineUsed=true;
					break;
				case 4:							// ELSE condition false
					if (stricmp(tokens[b].c_str(),"IFS")==0)
					{
						printf("Error nested IFS\n");
						exit(2);
					}
					if (stricmp(tokens[b].c_str(),"ELSE")==0)
					{
						printf("Error nested ELSE\n");
						exit(2);
						break;
					}
					if (stricmp(tokens[b].c_str(),"ENDIF")==0)
					{
						state=0;
						break;
					}
					break;
				case 5:
					break;
			}
					
			b++;

		}

		if (lineUsed)
		{
			SMacroDef macro;
			std::vector<std::string> paramsIn;
			if (IsLineMacroInstance(output,tmpLine,macro,paramsIn))
			{
				InsertMacro(output,lines,macro,paramsIn);
				tmpLine="";
			}
			else
			{
				output+=tmpLine+"\n";
				tmpLine="";
			}
		}
	}
}

std::vector<std::string> LoadAndLinifyFile(const char* filename)
{

	size_t fsize;
	char* inBuffer;
	FILE* inFile=fopen(filename,"rb");
	if (inFile==NULL)
	{
		std::cerr << "Failed to Open : " << filename << std::endl;
		exit(9);
	}
		
	fseek(inFile,0,SEEK_END);
	fsize=ftell(inFile);
	fseek(inFile,0,SEEK_SET);
	inBuffer=(char*)malloc(fsize);
	fread(inBuffer,1,fsize,inFile);
	fclose(inFile);

	std::string outputBuffer;
	std::string processBuffer(inBuffer);

	std::vector<std::string> lines;

	size_t a=0;
	int line=1;
	std::string tmpLine;
	bool cancelString=false;
	bool instring=false,inquote=false;
	char lastchar;
	while (a<fsize)
	{
		if ((processBuffer[a]=='\n')/* || (processBuffer[a]=='\r')*/)
		{
			lastchar=0;
			if (!tmpLine.empty())
			{
				char tBuffer[200];
				std::string withPreProc;
				withPreProc+="##!PREPROC!##";
				withPreProc+=" \"";
				withPreProc+=filename;
				withPreProc+="\" ";
				withPreProc+="##!PREPROC!##";
				withPreProc+=" ";
				sprintf(tBuffer,"%d",line);
				withPreProc+=tBuffer;
				withPreProc+=" ";
				withPreProc+="##!PREPROC!##";
				withPreProc+=" ";

				if (instring && tmpLine[tmpLine.length()-1]!='"')
					tmpLine+='"';

				withPreProc+=tmpLine;
				lines.push_back(withPreProc);
				tmpLine.clear();
			}
			line++;
			cancelString=false;
			instring=false;
			inquote=false;
		}
		else
		{
			if (processBuffer[a]==';' && !inquote && !instring)
			{
				cancelString=true;
			}
			if (!cancelString)
			{
				if (processBuffer[a]=='"' && lastchar!='"' && !inquote)
					instring=!instring;
				if (processBuffer[a]=='\'' && lastchar!='\'' && !instring)
					inquote=!inquote;
				if (processBuffer[a]>31)
				{
					tmpLine+=processBuffer[a];
				}
				if (processBuffer[a]=='\t')
				{
					tmpLine+=" ";
				}
				lastchar=processBuffer[a];
			}
		}
		a++;
	}

	if (!tmpLine.empty())
	{
		char tBuffer[200];
		std::string withPreProc;
		withPreProc+="##!PREPROC!##";
		withPreProc+=" \"";
		withPreProc+=filename;
		withPreProc+="\" ";
		withPreProc+="##!PREPROC!##";
		withPreProc+=" ";
		sprintf(tBuffer,"%d",line);
		withPreProc+=tBuffer;
		withPreProc+=" ";
		withPreProc+="##!PREPROC!##";
		withPreProc+=" ";

//		if (instring)
//			tmpLine+='"';

		withPreProc+=tmpLine;
		lines.push_back(withPreProc);
		tmpLine.clear();
	}

	free(inBuffer);
	return lines;
}

std::vector<std::string> LoadAndLinifyBinary(const char* filename)
{
	std::vector<std::string> lines;

	size_t fsize;
	uint8_t* inBuffer;
	FILE* inFile=fopen(filename,"rb");
	if (inFile==NULL)
	{
		std::cerr << "Failed to Open : " << filename << std::endl;
		exit(9);
	}
		
	fseek(inFile,0,SEEK_END);
	fsize=ftell(inFile);
	fseek(inFile,0,SEEK_SET);
	inBuffer=(uint8_t*)malloc(fsize);
	fread(inBuffer,1,fsize,inFile);
	fclose(inFile);

	int line=0;
	size_t remain;
	std::string tmpLine;
	while (fsize)
	{
		remain=fsize;
		if (remain>=16)
			remain=16;
		fsize-=remain;

		char tBuffer[200];
		std::string withPreProc;
		withPreProc+="##!PREPROC!##";
		withPreProc+=" \"";
		withPreProc+=filename;
		withPreProc+="\" ";
		withPreProc+="##!PREPROC!##";
		withPreProc+=" ";
		sprintf(tBuffer,"%d",line);
		withPreProc+=tBuffer;
		withPreProc+=" ";
		withPreProc+="##!PREPROC!##";
		withPreProc+=" ";
		withPreProc+="DEFB ";
		while (remain)
		{
			char tmpByte[5];
			sprintf(tmpByte,"0%02XH",inBuffer[line]);
			withPreProc+=tmpByte;
			if (remain!=1)
			{
				withPreProc+=",";
			}

			line++;
			remain--;
		}

		lines.push_back(withPreProc);
	}

	free(inBuffer);
	return lines;
}

std::string RemoveQuotes(std::string& input)
{
	std::string ret(input);

	while (*ret.begin()=='"')
		ret.erase(ret.begin());
	while (*(--ret.end())=='"')
		ret.erase((--ret.end()));
	return ret;
}

void AddLabel(std::string& input,size_t pos)
{
	pos=input.find(" ",pos);
	if (pos!=string::npos)
	{
		input.insert(pos,":");//replace(pos,strlen(find),replace);

	//	pos = str.find(find);
	}
}
	

void ReplaceEQU(string& str)
{
	std::vector<std::string> tokens=CheapTokenise(str);
	size_t a=5;		// SKIP preprocessor tokens
		
	if (tokens.size()==0)
		return;

	while (a<(tokens.size()-1))
	{
		if (stricmp(tokens[a+1].c_str(),"EQU")==0)
		{
			if (tokens[a].c_str()[tokens[a].size()-1]!=':')
			{
				tokens[a]+=":";
			}
		}
		a++;
	}

	str="";
	a=0;
	while (a<tokens.size())
	{
		str+=tokens[a].c_str();
		if (a!=(tokens.size()-1))
			str+=" ";
		a++;
	}
}

void PreProcess(const char* filename)
{
	size_t a=0;
	std::string outputBuffer;
	std::string tmp;
	std::vector<std::string> lines=LoadAndLinifyFile(filename);

	macroTable.clear();	// macros are per compiled input (not shared)

	// PASS 1 includes/incbins					-- TODO clean all this up, its very slow

	bool changed=true;

	while (changed)
	{
	changed=false;
	std::vector<std::string>::iterator lineIter=lines.begin();
	while (lineIter!=lines.end())
	{
		Replace(*lineIter,"d:\\pds\\work\\","");
		Replace(*lineIter,"\\pds\\work\\","");
		Replace(*lineIter,"hitch\\","");
		Replace(*lineIter,"transfor\\","");
		Replace(*lineIter,"cube\\","");
		Replace(*lineIter,"palette\\","");
		Replace(*lineIter,"invaders\\","");

		std::vector<std::string> tokens=CheapTokenise(*lineIter);
		size_t a=5;		// SKIP preprocessor tokens
		size_t b=0;

		while (a<tokens.size())
		{
			if (stricmp(tokens[a].c_str(),"INCLUDE")==0)
			{
				changed=true;
				// Need to insert everyline from the file at the current line position (and split current line into before and after)
				
				lineIter=lines.erase(lineIter);

				tmp.clear();
				for (b=0;b<a;b++)
				{
					tmp+=tokens[b];
					tmp+=" ";
				}

				lineIter=lines.insert(lineIter,tmp);
				lineIter++;

				tmp.clear();
				for (b=0;b<5;b++)
				{
					tmp+=tokens[b];
					tmp+=" ";
				}

				b=a+2;
				while (b<tokens.size())
				{
					tmp+=tokens[b];
					tmp+=" ";
					b++;
				}

				std::vector<std::string> newLines=LoadAndLinifyFile(RemoveQuotes(tokens[a+1]).c_str());
				std::vector<std::string>::iterator newLineIter=newLines.begin();
				while (newLineIter!=newLines.end())
				{
					lineIter=lines.insert(lineIter,*newLineIter);
					lineIter++;
					newLineIter++;
				}

				lineIter=lines.insert(lineIter,tmp);
				lineIter--;
			}
			if (stricmp(tokens[a].c_str(),"INCBIN")==0)
			{
				changed=true;
				// Need to insert everyline from the file at the current line position (and split current line into before and after)
				
				lineIter=lines.erase(lineIter);

				tmp.clear();
				for (b=0;b<a;b++)
				{
					tmp+=tokens[b];
					tmp+=" ";
				}

				lineIter=lines.insert(lineIter,tmp);
				lineIter++;

				tmp.clear();
				for (b=0;b<5;b++)
				{
					tmp+=tokens[b];
					tmp+=" ";
				}

				b=a+2;
				while (b<tokens.size())
				{
					tmp+=tokens[b];
					tmp+=" ";
					b++;
				}

				std::vector<std::string> newLines=LoadAndLinifyBinary(RemoveQuotes(tokens[a+1]).c_str());
				std::vector<std::string>::iterator newLineIter=newLines.begin();
				while (newLineIter!=newLines.end())
				{
					lineIter=lines.insert(lineIter,*newLineIter);
					lineIter++;
					newLineIter++;
				}

				lineIter=lines.insert(lineIter,tmp);
				lineIter--;
			}
			if (!opts.comMode && stricmp(tokens[a].c_str(),"HEX")==0)
			{
				changed=true;
				// Need to insert everyline from the file at the current line position (and split current line into before and after)
				
				lineIter=lines.erase(lineIter);

				tmp.clear();
				for (b=0;b<a;b++)
				{
					tmp+=tokens[b];
					tmp+=" ";
				}
				
				// iterator across hex string and split every 2 chars
				tmp+="DEFB ";
				size_t cc=0;
				while (cc<tokens[a+1].length())
				{
					tmp+="#";
					tmp+=tokens[a+1][cc];
					tmp+=tokens[a+1][cc+1];
					tmp+=",";
					cc+=2;
				}

				lineIter=lines.insert(lineIter,tmp);
				lineIter++;
				/*

				tmp.clear();
				for (b=0;b<5;b++)
				{
					tmp+=tokens[b];
					tmp+=" ";
				}

				b=a+2;
				while (b<tokens.size())
				{
					tmp+=tokens[b];
					tmp+=" ";
					b++;
				}

				std::vector<std::string> newLines=LoadAndLinifyBinary(RemoveQuotes(tokens[a+1]).c_str());
				std::vector<std::string>::iterator newLineIter=newLines.begin();
				while (newLineIter!=newLines.end())
				{
					lineIter=lines.insert(lineIter,*newLineIter);
					lineIter++;
					newLineIter++;
				}

				lineIter=lines.insert(lineIter,tmp);
				lineIter--;*/
			}

			a++;
		}

		++lineIter;
	}
	}

	// PASS 2 macros expansion
	int state=0;
	for (a=0;a<lines.size();a++)
	{
		switch(state)
		{
			case 0:
				// Does this line contain a macro definition?
				if (IsLineMacroDef(lines[a],a+1))
				{
					//printf("Macro Definition Found\n");
					state=1;
					break;
				}
				else
				{
					SMacroDef macro;
					std::vector<std::string> paramsIn;
					if (IsLineMacroInstance(outputBuffer,lines[a],macro,paramsIn))
					{
						//printf("Macro Instance Found : %s\n",macro.name.c_str());

						
						InsertMacro(outputBuffer,lines,macro,paramsIn);
						break;
					}
				}
				outputBuffer+=lines[a]+"\n";
				break;
			case 1:
				if (IsLineEndMacro(lines[a],a-1))
				{
					//printf("End Macro Found\n");
					state=0;
				}
				break;
		}
	}

	// PASS 3.0 - attempt to fix labels
/*
	size_t pos=0;
//	while (pos!=string::npos)
//	{
//		if (!isspace(outputBuffer[pos]))
//		{
//			AddLabel(outputBuffer,pos);
//		}
//
//		pos=outputBuffer.find("\n",pos);
//		if (pos==string::npos)
//			break;
//		pos++;
//	}

	ReplaceSafe(outputBuffer,"=","equ");			// THIS IS A BIT DANGEROUS!!! 

	int triStep=0;
	int cnter=0;
	string lastName("VAR_PC");
	char tmpNewName[200];

	pos=outputBuffer.find("VAR_PC");		// HUGE BODGE
	while (pos!=string::npos)
	{
		switch (triStep)
		{
			case 0:
				// Use last count
				outputBuffer.replace(pos,6,lastName);
				triStep++;
				break;
			case 1:
				sprintf(tmpNewName,"VAR_PC_%d",cnter++);
				outputBuffer.replace(pos,6,tmpNewName);
				triStep++;
				break;
			case 2:
				outputBuffer.replace(pos,6,lastName);
				lastName=tmpNewName;
				triStep=0;
				break;
		}
		pos++;
		pos=outputBuffer.find("VAR_PC",pos);
	}
*/
	preprocessedSize=outputBuffer.size()+2;
	preprocessed=(char*)malloc(preprocessedSize);
	memcpy(preprocessed,outputBuffer.c_str(),preprocessedSize-2);
	preprocessed[preprocessedSize-2]=0;
	preprocessed[preprocessedSize-1]=0;




	printf("%s\n",preprocessed);
}

int CountLineInformation(NodeList* nodes)
{
	size_t a;
	int total =0;

	for (a=0;a<nodes->size();a++)
	{
		total++;
	}

	return total;
}

void DumpLineInformation(FILE* debugOut,const char* name,NodeList* nodes,uint32_t curAddress)
{
	size_t a;

	for (a=0;a<nodes->size();a++)
	{
		fprintf(debugOut,"%d\t%d\t%s\r\n",(uint32_t)(*nodes)[a]->GetAddress(),(uint32_t)(*nodes)[a]->GetLineNumber(),(*nodes)[a]->GetFilename().c_str());
		curAddress=(*nodes)[a]->GetAddress();
	}
}

int isUsingCom()
{
	return opts.comMode;
}

int main(int argc, char **argv)
{

	for (int a=1;a<argc;a++)
	{
		if (argv[a][0]=='-')
		{
			if (strcmp(argv[a],"-o")==0)
			{
				if (a+1>=argc)
				{
					return Usage();
				}
				opts.outputFile=argv[a+1];
				a++;
			}
			if (strcmp(argv[a],"-s")==0)
			{
				if (a+1>=argc)
				{
					return Usage();
				}
				opts.startSymbol=argv[a+1];
				a++;
			}
			if (strcmp(argv[a],"-v")==0)
			{
				cout << "assembler.exe "<<ASSEMBLER_VERSION<<endl;
				return 0;
			}
			if (strcmp(argv[a],"-c")==0)
			{
				opts.comMode=1;
			}
			if (strcmp(argv[a],"-h")==0 || strcmp(argv[a],"--help")==0)
			{
				return Usage();
			}
		}
		else
		{
			if (opts.inputFileNum<32)
			{
				opts.inputFiles[opts.inputFileNum++] = argv[a];
			}
			else
			{
				return Usage();
			}
		}
	}
	if (opts.inputFileNum==0)
	{
		return Usage();
	}

	for (int aa = 0; aa < opts.inputFileNum; aa++)
	{
		if (opts.inputFiles[aa] && opts.outputFile && strcmp(opts.inputFiles[aa], opts.outputFile) == 0)
		{
			cerr << "Input and Output names match!!!" << endl;
			return 5;
		}
	}

	uint64_t curAddress = 0x40000;		/* Default address if none specified -- sources would indicate the default bank is 4 */

	if (opts.comMode)
	{
		curAddress = 0x100;
	}

	NodeList* inputNodes[32];
	uint64_t a;

	for (int aa = 0; aa < opts.inputFileNum; aa++)
	{
		globalSymbol.clear();

		if (opts.inputFileNum == 1)
			g_localSymbolPrefix = "";
		else
		{
			g_localSymbolPrefix = "prefix";
			g_localSymbolPrefix += (char)(aa + 33);
			g_localSymbolPrefix += "_";
		}

		if (opts.inputFiles[aa])
		{
			PreProcess(opts.inputFiles[aa]);
			yy_scan_buffer(preprocessed, preprocessedSize);
			g_FileName = opts.inputFiles[aa];
		}

		g_ProgramBlock = NULL;

		//	yydebug=1;
		yyparse();

		if (g_ProgramBlock == 0)
		{
			cerr << "Error : Unable to parse input" << endl;
			return 1;
		}

		inputNodes[aa] = g_ProgramBlock;

		// 2 Pass assembler - First pass figures out the address for each node

		for (a = 0; a < inputNodes[aa]->size(); a++)
		{
			//		cout << "PreProcessing : " << (*g_ProgramBlock)[a]->Identify() << endl;
			if (!(*inputNodes[aa])[a]->PrePass(curAddress))
			{
				cerr << "Error : Failed during PrePass of rom" << endl;
				return 4;
			}
		}
	}

	uint32_t romSize = 1024*1024 + 16384;			// Last 16k is used to assemble data to temporarily

	uint8_t* romData=new uint8_t[romSize];	// For now just allocate 1MB!

	memset(romData,0,romSize);

	for (int aa = 0; aa < opts.inputFileNum; aa++)
	{
		// 2nd Pass - Generate the rom
		for (a = 0; a < inputNodes[aa]->size(); a++)
		{
			//		cout << "Processing : " << (*g_ProgramBlock)[a]->Identify() << endl;
			if (!(*inputNodes[aa])[a]->Assemble(romData))
			{
				cerr << "Error : Failed to Assemble rom" << endl;
				return 3;
			}
		}
	}
		
	size_t todo2;
	for (todo2=0;todo2<addressRanges.size();todo2++)
	{
		if (addressRanges[todo2].GetBegin()&0xF00000)
			continue;
		addressRanges[todo2].Dump();
	}

	// Print the contents of our rom (for checking)
	if (opts.outputFile==NULL)
	{
		size_t todo;
		for (todo=0;todo<addressRanges.size();todo++)
		{
			if (addressRanges[todo].GetBegin()&0xF00000)
				continue;
			for (a=addressRanges[todo].GetBegin()/16;a<=addressRanges[todo].GetEnd()/16;a++)
			{
				int remain=romSize - a*16>=16?16:romSize - a*16;
				if (remain)
				{
					cout << setfill('0') << setw(16) << hex << a*16 << " : " ;
					for (int b=0;b<remain;b++)
					{
						cout << setfill('0') << setw(2) << hex << (int)romData[a*16+b] << " ";
					}
					cout << endl;
				}
			}
		}
	
		if (opts.comMode)
		{
			execAddress=0x100;
		}

		cout << "Execution Start : " << execAddress << endl;
	}
	else
	{
		uint16_t seg,off;
		uint8_t marker=0xF1;
		size_t todo;
		FILE* outFile = fopen(opts.outputFile,"wb");

		if (outFile==NULL)
		{
			cerr << "Failed to open " << opts.outputFile << " for writing!" << endl;
			return 6;
		}
		
		if (opts.comMode)
		{
			for (todo=0;todo<addressRanges.size();todo++)
			{
				uint64_t start=addressRanges[todo].GetBegin();
				uint64_t end=addressRanges[todo].GetEnd();
				uint64_t size=(end-start)+1;

				if (addressRanges[todo].GetBegin()&0xF00000)
					continue;
				if (size!=fwrite(&romData[start],1,size,outFile))
				{
					fclose(outFile);
					cerr << "Unexpected error writing results" << endl;
					return 7;
				}

			}

		}
		else
		{
			// Write header (FL1 is a variant of MSU format - 0xF1	<- single byte section indicates Flare One

			if (1!=fwrite(&marker,1,1,outFile))
			{
				fclose(outFile);
				cerr << "Unexpected error writing results" << endl;
				return 7;
			}

			// Output execution address  - for now this is simply the first address of the last segment! -- unless -s specified
			marker=0xCA;
			execAddress=0x40000|execAddress;
			seg=(execAddress/*addressRanges[addressRanges.size()-1].GetBegin()*/>>4)&0xF000;
			off=execAddress/*addressRanges[addressRanges.size()-1].GetBegin()*/&0xFFFF;

			if (opts.startSymbol)
			{
				uint64_t address;
				if (symbolTable.find(opts.startSymbol)==symbolTable.end())
				{
					cerr << "Unable to find specified start symbol!" << endl;
					return 7;
				}
				address=symbolTable[opts.startSymbol]->GetAddress();
				seg=(address>>4)&0xF000;
				off=address&0xFFFF;
			}
			if (1!=fwrite(&marker,1,1,outFile))
			{
				fclose(outFile);
				cerr << "Unexpected error writing results" << endl;
				return 7;
			}
			if (2!=fwrite(&seg,1,2,outFile))
			{
				fclose(outFile);
				cerr << "Unexpected error writing results" << endl;
				return 7;
			}
			if (2!=fwrite(&off,1,2,outFile))
			{
				fclose(outFile);
				cerr << "Unexpected error writing results" << endl;
				return 7;
			}

			marker=0xC8;
			for (todo=0;todo<addressRanges.size();todo++)
			{
				uint64_t start=addressRanges[todo].GetBegin();
				uint64_t end=addressRanges[todo].GetEnd();
				uint64_t size=(end-start)+1;

				if (addressRanges[todo].GetBegin()&0xF00000)
					continue;

				while (size != 0)
				{
					uint64_t safesize = size;
					if (safesize > 0xFFFF)
						safesize = 0xFFFF;

					if (1 != fwrite(&marker, 1, 1, outFile))
					{
						fclose(outFile);
						cerr << "Unexpected error writing results" << endl;
						return 7;
					}
					seg = start >> 4;
					off = start & 0xF;
					if (2 != fwrite(&seg, 1, 2, outFile))
					{
						fclose(outFile);
						cerr << "Unexpected error writing results" << endl;
						return 7;
					}
					if (2 != fwrite(&off, 1, 2, outFile))
					{
						fclose(outFile);
						cerr << "Unexpected error writing results" << endl;
						return 7;
					}
					seg = 0;		// skip unknown
					off = safesize;	// offset to next section
					if (2 != fwrite(&seg, 1, 2, outFile))
					{
						fclose(outFile);
						cerr << "Unexpected error writing results" << endl;
						return 7;
					}
					if (2 != fwrite(&off, 1, 2, outFile))
					{
						fclose(outFile);
						cerr << "Unexpected error writing results" << endl;
						return 7;
					}
					if (safesize != fwrite(&romData[start], 1, safesize, outFile))
					{
						fclose(outFile);
						cerr << "Unexpected error writing results" << endl;
						return 7;
					}

					start += safesize;
					size -= safesize;
				}


			}
		}

		fclose(outFile);
	}

	char directory[_MAX_PATH];
	getcwd(directory, sizeof(directory));

	if (opts.outputFile!=NULL)
	{
		char tmp[1024];
		sprintf(tmp,"%s.dbg",opts.outputFile);
		FILE* outDebug=fopen(tmp,"wb");
		if (outDebug==NULL)
		{
			cerr << "Failed to open " << tmp << " for writing!" << endl;
			return 7;
		}

		fprintf(outDebug,"%s\r\n",directory);
		int numSourceLines = 0;
		for (int aa = 0; aa < opts.inputFileNum; aa++)
		{
			numSourceLines += CountLineInformation(inputNodes[aa]);
		}
		fprintf(outDebug, "%d\r\n", numSourceLines);
		for (int aa = 0; aa < opts.inputFileNum; aa++)
		{
			DumpLineInformation(outDebug, opts.inputFiles[aa], inputNodes[aa], 0);
		}

		{
			// Symbol dump	
			std::map<std::string,CSymbol*>::iterator iter;
			for (iter=symbolTable.begin();iter!=symbolTable.end();++iter)
			{
				fprintf(outDebug,"%u\t%s\r\n",(uint32_t)iter->second->getValue(),iter->first.c_str());
			}
		}

		fclose(outDebug);
	}
	else
	{
		// Symbol dump	
		std::map<std::string,CSymbol*>::iterator iter;
		for (iter=symbolTable.begin();iter!=symbolTable.end();++iter)
		{
			printf("%08X\t%s\n",(uint32_t)iter->second->getValue(),iter->first.c_str());
		}
	}

	return 0;
}

