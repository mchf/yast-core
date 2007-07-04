/*-----------------------------------------------------------*- c++ -*-\
|                                                                      |
|                      __   __    ____ _____ ____                      |
|                      \ \ / /_ _/ ___|_   _|___ \                     |
|                       \ V / _` \___ \ | |   __) |                    |
|                        | | (_| |___) || |  / __/                     |
|                        |_|\__,_|____/ |_| |_____|                    |
|                                                                      |
|                               core system                            |
|                                                        (C) SuSE GmbH |
\----------------------------------------------------------------------/

   File:	YCode.h

   Author:	Klaus Kaempf <kkaempf@suse.de>
   Maintainer:	Klaus Kaempf <kkaempf@suse.de>

/-*/
// -*- c++ -*-

#ifndef YCode_h
#define YCode_h

#include <string>
using std::string;

// MemUsage.h defines/undefines D_MEMUSAGE
#include <y2util/MemUsage.h>
#include "ycp/YCodePtr.h"

#include "ycp/YCPValue.h"
#include "ycp/YCPString.h"
#include "ycp/Type.h"
#include "ycp/YSymbolEntry.h"

/**
 * linked list of ycode pointers
 *
 * used in forming a block consisting of an ordered
 * set of statements.
 */

struct ycodelist {
    struct ycodelist *next;
    YCodePtr code;
    constTypePtr type;
};
typedef struct ycodelist ycodelist_t;

/**
 * @short YCode for precompiled ycp code
 */
class YCode : public Rep
#ifdef D_MEMUSAGE
  , public MemUsage
#endif
{
    REP_BODY(YCode);
public:
    enum ykind {
	yxError = 0,
	// [1] Constants	(-> YCPValue, except(!) term -> yeLocale)
	ycVoid, ycBoolean, ycInteger, ycFloat,	// constants
	ycString, ycByteblock, ycPath, ycSymbol,
	ycList,					// list
	ycMap,					// map
	ycTerm,					// term

	ycEntry,

	ycConstant,		// -- placeholder --
	ycLocale,				// locale constant (gettext)
	ycFunction,				// a function definition (parameters and body)

	// [16] Expressions	(-> declaration_t + values)
	yePropagate,		// type propagation (value, type)
	yeUnary,		// unary (prefix) operator
	yeBinary,		// binary (infix) operator
	yeTriple,		// <exp> ? <exp> : <exp>
	yeCompare,		// compare

	// [21] Value expressions (-> values + internal)
	yeLocale,		// locale expression (ngettext)
	yeList,			// list expression
	yeMap,			// map expression
	yeTerm,			// <name> ( ...)
	yeIs,			// is()
	yeBracket,		// <name> [ <expr>, ... ] : <expr>

	// [27] Block (-> linked list of statements)
	yeBlock,		// block expression
	yeReturn,		// quoted expression, e.g. "``(<exp>)" which really is "{ return <exp>; }"

	// [29] Symbolref (-> SymbolEntry)
	yeVariable,		// variable ref
	yeBuiltin,		// builtin ref + args
	yeFunction,		// function ref + args
	yeReference,		// reference to a variable (identical to yeVariable but with different semantics)
	// SuSE Linux v9.2
	yeFunctionPointer,	// function pointer

	yeExpression,		// -- placeholder --

	// [35] Statements	(-> YCode + next)
	ysTypedef,		// typedef
	ysVariable,		// variable defintion (-> YSAssign)
	ysFunction,		// function definition
	ysAssign,		// variable assignment (-> YSAssign)
	ysBracket,		// <name> [ <expr>, ... ] = <expr>
	ysIf,			// if, then, else
	ysWhile,		// while () do ...
	ysDo,			// do ... while ()
	ysRepeat,		// repeat ... until ()
	ysExpression,		//  any expression (function call)
	ysReturn,		// return
	ysBreak,		// break
	ysContinue,		// continue
	ysTextdomain,		// textdomain
	ysInclude,		// include
	ysFilename,		//  restore filename after include
	ysImport,		// import
	ysBlock,		// a block as statement
	ysSwitch,		// switch (since 10.0)
	ysStatement		// [54] -- placeholder --
    };

public:

    /**
     * Creates a new YCode element
     */
    YCode ();

    /**
     * Cleans up
     */
    virtual ~YCode();

    /**
     * Returns the YCode kind
     */
    virtual ykind kind() const = 0;
   
    /**
     * Returns an ASCII representation of the YCode.
     */
    virtual string toString() const;
    static string toString(ykind kind);

    /**
     * writes YCode to a stream
     * see Bytecode for read
     */
    virtual std::ostream & toStream (std::ostream & str) const = 0;
    virtual std::ostream & toXml (std::ostream & str, int indent ) const = 0;

    /**
     * returns true if the YCode represents a constant
     */
    virtual bool isConstant () const;

    /**
     * returns true if the YCode represents an error
     */
    bool isError () const;

    /**
     * returns true if the YCode represents a statement
     */
    virtual bool isStatement () const;

    /**
     * returns true if the YCode represents a block
     */
    virtual bool isBlock () const;

    /**
     * returns true if the YCode represents something we can reference to
     */
    virtual bool isReferenceable () const;

    /**
     * evaluate YCode to YCPValue
     * if debugger == 0
     *	 called for parse time evaluation (i.e. constant subexpression elimination)
     * else
     *	 called for runtime evaluation		
     */
    virtual YCPValue evaluate (bool cse = false);

   /**
    * return type (interesting mostly for function calls)
    */
  virtual constTypePtr type() const;
};


/**
 * constant (-> YCPValue)
 */

class YConst : public YCode
{
    REP_BODY(YConst);
    ykind m_kind;
    YCPValue m_value;		// constant (not allowed in union :-( )
public:
    YConst (ykind kind, YCPValue value);		// Constant
    YConst (ykind kind, bytecodeistream & str);
    ~YConst () {};
    YCPValue value() const;
    virtual ykind kind() const;
    string toString() const;
    std::ostream & toStream (std::ostream & str) const;
    std::ostream & toXml (std::ostream & str, int indent ) const;
    /** yes */
    virtual bool isConstant () const { return true; }
    YCPValue evaluate (bool cse = false);
    constTypePtr type() const;
};

#include <ext/hash_map>
#include <string>
#include <cstddef>

/**
 * locale
 * string and textdomain
 * @see: YELocale
 */

class YELocale;

class YLocale : public YCode
{
    REP_BODY(YLocale);
    const char *m_locale;		// the string to be translated

    struct eqstr
    {
	bool operator()(const char* s1, const char* s2) const
	{
	    return strcmp(s1, s2) == 0;
	}
    };

public:
    typedef __gnu_cxx::hash_map<const char*, bool, __gnu_cxx::hash<const char*>, eqstr> t_uniquedomains;

    static t_uniquedomains domains;	// keep every textdomain only once
    static t_uniquedomains::const_iterator setDomainStatus (const string& domain, bool status);
    static void ensureBindDomain (const string& domain);

    YLocale (const char *locale, const char *textdomain);
    YLocale (bytecodeistream & str);
    ~YLocale ();
    virtual ykind kind () const { return ycLocale; }
    const char *value () const;
    const char *domain () const;
    string toString() const;
    std::ostream & toStream (std::ostream & str) const;
    std::ostream & toXml (std::ostream & str, int indent ) const;
    YCPValue evaluate (bool cse = false);
    constTypePtr type() const { return Type::Locale; }
    
private:

    t_uniquedomains::const_iterator m_domain;

};

/**
 * function declaration (m_body == 0) or definition (m_body != 0)
 *   it's anonymouse here ! The code() member of the corresponding
 *   SymbolEntry points to YFunction.
 */

class YFunction : public YCode
{
    REP_BODY(YFunction);
    // array of formal arguments of a function
    // the formal parameters must be available in the local scope during parse
    // of the function body (startDefinition()) and removed afterwards (endDefintion()).
    // Keep track of the table entries in this block.
    //
    // When calling a function during execution, the actual
    // arguments (values) are bound to the formal arguments
    // (this array) so the function body can be evaluated.
    // @see YEFunction::attachActualParameter()
    //
    // if NULL, it's a (void) function
    YBlockPtr m_declaration;

    // the function definition ('body') is the block defining this function
    YBlockPtr m_definition;

    bool m_is_global;

public:
    YFunction (YBlockPtr parameterblock, const SymbolEntryPtr entry = 0);
    YFunction (bytecodeistream & str);
    ~YFunction ();
    virtual ykind kind () const { return ycFunction; }

    // access to formal parameters
    unsigned int parameterCount () const;
    YBlockPtr declaration () const;
    SymbolEntryPtr parameter (unsigned int position) const;

    // access to definition block (= 0 if declaration only)
    YBlockPtr definition () const;
    void setDefinition (YBlockPtr body);
    // read definition from stream
    void setDefinition (bytecodeistream & str);

    string toStringDeclaration () const;
    string toString () const;
    std::ostream & toStreamDefinition (std::ostream & str ) const;
    std::ostream & toXmlDefinition (std::ostream & str, int indent ) const;
    std::ostream & toStream (std::ostream & str ) const;
    std::ostream & toXml (std::ostream & str, int indent ) const;
    virtual YCPValue evaluate (bool cse = false);
    constTypePtr type() const;
};


#endif   // YCode_h
