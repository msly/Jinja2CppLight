// Copyright Hugh Perkins 2015 hughperkins at gmail
//
// This Source Code Form is subject to the terms of the Mozilla Public License, 
// v. 2.0. If a copy of the MPL was not distributed with this file, You can 
// obtain one at http://mozilla.org/MPL/2.0/.

// the intent here is to create a templates library that:
// - is based on Jinja2 syntax
// - doesn't depend on boost, qt, etc ...

// for now, will handle:
// - variable substitution, ie {{myvar}}
// - for loops, ie {% for i in range(myvar) %}

#include <string>
#include <iostream>
#include <map>
#include <vector>
#include <stdexcept>
#include <sstream>

#include "stringhelper.h"

#define VIRTUAL virtual
#define STATIC static

namespace Jinja2CppLight {

class render_error : public std::runtime_error {
public:
    render_error( const std::string &what ) :
        std::runtime_error( what ) {
    }
};

class Value {
public:
    virtual ~Value() {}
    virtual std::string render() = 0;
    virtual bool isTrue() const = 0;
};
class IntValue : public Value {
public:
    int value;
    IntValue( int value ) :
        value( value ) {
    }
    virtual std::string render() {
        return toString( value );
    }
    bool isTrue() const {
        return value != 0;
    }
};
class FloatValue : public Value {
public:
    float value;
    FloatValue( float value ) :
        value( value ) {
    }
    virtual std::string render() {
        return toString( value );
    }
    bool isTrue() const {
        return value != 0.0;
    }
};
class StringValue : public Value {
public:
    std::string value;
    StringValue( std::string value ) :
        value( value ) {
    }
    virtual std::string render() {
        return value;
    }
    bool isTrue() const {
        return !value.empty();
    }
};

class Root;
class ControlSection;

class Template {
public:
    std::string sourceCode;

    std::map< std::string, Value * > valueByName;
//    std::vector< std::string > varNameStack;
    Root *root;

    // [[[cog
    // import cog_addheaders
    // cog_addheaders.add(classname='Template')
    // ]]]
    // generated, using cog:
    Template( std::string sourceCode );
    STATIC bool isNumber( std::string astring, int *p_value );
    VIRTUAL ~Template();
    Template &setValue( std::string name, int value );
    Template &setValue( std::string name, float value );
    Template &setValue( std::string name, std::string value );
    std::string render();
    void print(ControlSection *section);
    int eatSection( int pos, ControlSection *controlSection );
    STATIC std::string doSubstitutions( std::string sourceCode, std::map< std::string, Value *> valueByName );

    // [[[end]]]
};

class ControlSection {
public:
    std::vector< ControlSection * >sections;
    virtual std::string render( std::map< std::string, Value *> &valueByName ) = 0;
    virtual void print() {
        print("");
    }
    virtual void print(std::string prefix) = 0;
};

class Container : public ControlSection {
public:
//    std::vector< ControlSection * >sections;
    int sourceCodePosStart;
    int sourceCodePosEnd;

//    std::string render( std::map< std::string, Value *> valueByName );
    virtual void print( std::string prefix ) {
        std::cout << prefix << "Container ( " << sourceCodePosStart << ", " << sourceCodePosEnd << " ) {" << std::endl;
        for( int i = 0; i < (int)sections.size(); i++ ) {
            sections[i]->print( prefix + "    " );
        }
        std::cout << prefix << "}" << std::endl;
    }
};

class ForSection : public ControlSection {
public:
    int loopStart;
    int loopEnd;
    std::string varName;
    int startPos;
    int endPos;
    std::string render( std::map< std::string, Value *> &valueByName ) {
        std::string result = "";
//        bool nameExistsBefore = false;
        if( valueByName.find( varName ) != valueByName.end() ) {
            throw render_error("variable " + varName + " already exists in this context" );
        }
        for( int i = loopStart; i < loopEnd; i++ ) {
            valueByName[varName] = new IntValue( i );
            for( size_t j = 0; j < sections.size(); j++ ) {
                result += sections[j]->render( valueByName );
            }
            delete valueByName[varName];
            valueByName.erase( varName );
        }
        return result;
    }
    //Container *contents;
    virtual void print( std::string prefix ) {
        std::cout << prefix << "For ( " << varName << " in range(" << loopStart << ", " << loopEnd << " ) {" << std::endl;
        for( int i = 0; i < (int)sections.size(); i++ ) {
            sections[i]->print( prefix + "    " );
        }
        std::cout << prefix << "}" << std::endl;
    }
};

class Code : public ControlSection {
public:
//    vector< ControlSection * >sections;
    int startPos;
    int endPos;
    std::string templateCode;

    std::string render();
    virtual void print( std::string prefix ) {
        std::cout << prefix << "Code ( " << startPos << ", " << endPos << " ) {" << std::endl;
        for( int i = 0; i < (int)sections.size(); i++ ) {
            sections[i]->print( prefix + "    " );
        }
        std::cout << prefix << "}" << std::endl;
    }
    virtual std::string render( std::map< std::string, Value *> &valueByName ) {
//        std::string templateString = sourceCode.substr( startPos, endPos - startPos );
//        std::cout << "Code section, rendering [" << templateCode << "]" << std::endl;
        std::string processed = Template::doSubstitutions( templateCode, valueByName );
//        std::cout << "Code section, after rendering: [" << processed << "]" << std::endl;
        return processed;
    }
};

class Root : public ControlSection {
public:
    virtual ~Root() {}
//    std::vector< ControlSection * >sections;
    virtual std::string render( std::map< std::string, Value *> &valueByName ) {
        std::string resultString = "";
        for( int i = 0; i < (int)sections.size(); i++ ) {
            resultString += sections[i]->render( valueByName );
        }     
        return resultString;   
    }
    virtual void print(std::string prefix) {
        std::cout << prefix << "Root {" << std::endl;
        for( int i = 0; i < (int)sections.size(); i++ ) {
            sections[i]->print( prefix + "    " );
        }
        std::cout << prefix << "}" << std::endl;
    }    
};

class IfSection : public ControlSection {
public:
    IfSection(const std::string& expression) {
        parseIfCondition(expression);
    }

    std::string render(std::map< std::string, Value *> &valueByName) {
        std::stringstream ss;
        const bool expressionValue = computeExpression(valueByName);
        if (expressionValue) {
            for (size_t j = 0; j < sections.size(); j++) {
                ss << sections[j]->render(valueByName);
            }
        }
        const std::string renderResult = ss.str();
        return renderResult;
    }

    void print(std::string prefix) {
        std::cout << prefix << "if ( " 
            << ((m_isNegation) ? "not " : "") 
            << m_variableName << " ) {" << std::endl;
        if (true) {
            for (int i = 0; i < (int)sections.size(); i++) {
                sections[i]->print(prefix + "    ");
            }
        }
        std::cout << prefix << "}" << std::endl;
    }

private:
    //? It determines m_isNegation and m_variableName from @param[in] expression.
    //? @param[in] expression E.g. "if not myVariable" where myVariable is set by myTemplate.setValue( "myVariable", <any_value> );
    //?                       The result of this statement is false if myVariable is initialized.
    void parseIfCondition(const std::string& expression);

    bool computeExpression(const std::map< std::string, Value *> &valueByName) const;

    bool m_isNegation; ///< Tells whether is there "if not" or just "if" at the begin of expression.
    std::string m_variableName; ///< This simple "if" implementation allows single variable condition only.
};

}

