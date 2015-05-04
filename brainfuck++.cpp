//#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/optional/optional_io.hpp>
//#include <boost/variant.hpp>

#include <iostream>
#include <sstream>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <utility>
#include <vector>
#include <memory>

//#define CELL8BIT

using namespace std;
namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace spirit = boost::spirit;



struct block_instruction;

typedef int instruction_var_arg_type;
const instruction_var_arg_type var_name_for_data = -1;

struct simple_instruction_char
{
  boost::optional<instruction_var_arg_type> argument;
  char ichar;
  boost::optional<int> amount;
};

BOOST_FUSION_ADAPT_STRUCT(
    ::simple_instruction_char,
    argument,
    ichar,
    amount)

struct simple_instruction_assign
{
  bool zero_first;
  instruction_var_arg_type argument1, argument2;
};


typedef boost::variant<
    simple_instruction_char
  , simple_instruction_assign
  , boost::recursive_wrapper<block_instruction>
> instruction;

struct block_instruction
{
  boost::optional<instruction_var_arg_type> argument;
  char block_type;
  vector<instruction> inner;
};

BOOST_FUSION_ADAPT_STRUCT(
    ::block_instruction,
    argument,
    block_type,
    inner)


template <class ParsedInstructionProcessor>
class instruction_visitor
    : public boost::static_visitor<>
{
public:
    instruction_visitor(ParsedInstructionProcessor& o) : _o(o), indent(0) {};
    void pindent()
    {
      for (int i = 0; i < indent; i++) _o << " ";
    }

    void operator()(simple_instruction_char& sic)
    {
      pindent();
      _o << "visiting simple_instruction_char " << sic.argument << " " << sic.ichar << " " << sic.amount << endl;
    }

    void operator()(simple_instruction_assign& sia)
    {
      pindent();
      _o << "visiting simple_instruction_assign" << endl;
    }

    void operator()(block_instruction& bi)
    {
      pindent();
      _o << "visiting block_instruction " << bi.argument << " " <<  bi.block_type << endl;
      indent++;
      for (auto it = bi.inner.begin(); it!=bi.inner.end(); ++it)
        boost::apply_visitor(*this, *it);
      indent--;
      pindent();
      _o << "end visiting " << bi.block_type << endl;
    }

private:
    int indent;
    ParsedInstructionProcessor& _o;
};


template <typename Iterator>
struct BrainfuckPPGrammar : qi::grammar<Iterator, vector<instruction>(), ascii::space_type>
{
    BrainfuckPPGrammar() : BrainfuckPPGrammar::base_type(program_p)
    {
        using qi::eps;
        using qi::lit;
        using qi::_val;
        using namespace qi::labels;

        program_p %= *instruction_p;
        repeat_clause_p %= lit('(') >> qi::int_ >> lit(')');
        var_name_p %= qi::uint_;
        instruction_p %= simple_instruction_char_p | block_instruction_p;
        simple_instruction_char_p %= -var_name_p >> qi::char_("+-<>^,.") >> -repeat_clause_p;
        block_instruction_p %= 
          (-var_name_p >> qi::char_('[') >> *instruction_p >> qi::lit(']'))
          |
          (-var_name_p >> qi::char_('{') >> *instruction_p >> qi::lit('}'))
          ;
    }

    qi::rule<Iterator, vector<instruction>(), ascii::space_type> program_p;
    qi::rule<Iterator, int(), ascii::space_type> repeat_clause_p;
    qi::rule<Iterator, instruction_var_arg_type(), ascii::space_type> var_name_p;
    qi::rule<Iterator, instruction(), ascii::space_type> instruction_p;
    qi::rule<Iterator, simple_instruction_char(), ascii::space_type> simple_instruction_char_p;
    qi::rule<Iterator, block_instruction(), ascii::space_type> block_instruction_p;


};


template<class OutputWriter>
class Compiler
{
private:
  int _lines; // number of parallel lines (line 0 for data, others for control).
  shared_ptr<OutputWriter> _output;
  bool _at_control;
  vector<int> _blocks;
public:
  Compiler(shared_ptr<OutputWriter> output, int lines)
    :_output(move(output)), _lines(lines), _at_control(false)
  {
    InitializeControl();
  }

  void operator << (int x)
  {
    std::cout << "hh " << x << std::endl;
  }

  void HLWhileNotZero(int v = -1)
  {
    _blocks.push_back(v);
    if (v >= 0)
      GoToVar(v);
    else
      GoToData();
    WhileNotZero();
    if (v >= 0)
      ReturnFromVar(v);
  }

  void HLPutZero(int v = -1)
  {
    if (v >= 0)
      GoToVar(v);
    else
      GoToData();
    ZeroCell();
    if (v >= 0)
      ReturnFromVar(v);
  }


  void HLEndWhile()
  {
    int v = _blocks.back();
    _blocks.pop_back();
    if (v >= 0)
      GoToVar(v);
    else
      GoToData();
    EndWhile();
    if (v >= 0)
      ReturnFromVar(v);
  }

  void HLIfNotZero(int argument)
  {
    _blocks.push_back(argument);
    GoToVar(argument);
    WhileNotZero();
    ReturnFromVar(argument);
  }

  void HLEndIfNotZero()
  {
    int argument = _blocks.back();
    _blocks.pop_back();
    GoToVar(argument, true);
    EndWhile();
    ReturnFromVar(argument); // if the argument was not zero we have returned from space instead
    Move(-1);
    WhileNotZero();
      Move();
    EndWhile(); // Stop at zero space at -1 position
    Move();
  }

  void HLTestNotZero(int argument, int result, bool zeroResultFirst=true, int resultMagnitude=1)
  {
    GoToVar(argument);
    WhileNotZero();
      ReturnFromVar(argument);
      GoToVar(result);
      if (zeroResultFirst)
      {
        ZeroCell();
      }
      Add(resultMagnitude);
      ReturnFromVar(result);
  }

  void HLMove(int n = 1) { GoToData(); Move(n*_lines); }
  void HLReadToData() { GoToData(); o() << ','; }
  void HLOutputData() { GoToData(); o() << '.'; }

  void HLReadToVar(int v) { GoToVar(v); o() << ','; ReturnFromVar(v); }
  void HLOutputVar(int v) { GoToVar(v); o() << '.'; ReturnFromVar(v); }

  void HLAddData(int n = 1) { GoToData(); Add(n); }
  void HLAddVar(int v, int n = 1)
  {
    GoToVar(v);
    Add(n);
    ReturnFromVar(v);
  }
  // v += data
  void HLAddVarData(int v)
  {
    GoToData();
    DestructiveCopy(2);
    Move(2);
    DestructiveCopy(-1,-2);
    Move(-1); // copy data value to cell with position +1
    WhileNotZero();
      Add(-1);
      Move(-1);
      GoToVar(v);
      Add(1);
      ReturnFromVar(v);
      GoToData();
      Move(1);
    EndWhile();
    Move(-1);
  }

  // data += v
  void HLAddDataVar(int v)
  {
    GoToVar(v);
    int pv = VariablePosition(v);
    int ev = VariableSpace(v);
    DestructiveCopy(ev - pv);
    Move(ev - pv);
    WhileNotZero();
      Add(-1);
      Move(pv - ev);
      Add(1);
      Move(-pv);
      GoToData();
      Add(1);
      GoToVar(v);
      Move(ev - pv);
    EndWhile();
    Move(-ev);
  }
  // v[i] += v[j]
  void HLAddVarVar(int i, int j)
  {
    EnsureControl();
    int pi = VariablePosition(i);
    int pj = VariablePosition(j);
    int ej = VariableSpace(j);
    Move(pj);
    DestructiveCopy(ej-pj);
    Move(ej-pj);
    DestructiveCopy(pj-ej, pi-ej);
    Move(-ej);
  }

private:

  OutputWriter& o() { return *_output; }

  void DestructiveCopy(int a)
  {
    vector<int> va = {a};
    DestructiveCopy(move(va));
  }

  void DestructiveCopy(int a, int b)
  {
    vector<int> va = {a, b};
    DestructiveCopy(move(va));
  }

  void DestructiveCopy(vector<int> a)
  {
    WhileNotZero();
      Add(-1);
      int pos = 0;
      for (int i = 0; i < a.size(); i++)
      {
        Move(a[i]-pos);
        pos = a[i];
        Add(1);
      }
      Move(-pos);
    EndWhile();
  }

  void InitializeControl()
  {
    // we will store control (pos 0) == 255, cell -1 equal to 0, and cell -2 equal to 1
    Move(-1);
    Add(-1);
    Move(-2);
    Add();
    Move(3);
  }

  void GoToVar(int v, bool variableSpace=false)
  {
    EnsureControl();
    int pv = variableSpace ? VariableSpace(v) : VariablePosition(v);
    Move(pv);
  }

  void ReturnFromVar(int v)
  {
    int pv = VariablePosition(v);
    Move(-pv);
  }


  void GoToData()
  {
    EnsureControl(false);
  }

  void ZeroCell()
  {
    WhileNotZero();
      Add(-1);
    EndWhile();
  }

  void EnsureControl(bool mode = true)
  {
    if (_at_control == mode) return;
    if (mode == true)
    {
      JumpToControl();
    }
    else
    {
      JumpBackFromControl();
    }
    _at_control = !_at_control;
  }

  // set return flag == 1 and jump to control == 255
  void JumpToControl()
  {
    Move(_lines-1);
    Add(2);
    WhileNotZero();
      Add(-1);
      Move(-_lines);
      Add(1);
    EndWhile(); 
    Add(-1);
  }

  // jump from control == 255 to flag == 1
  void JumpBackFromControl()
  {
    Add(-1);
    WhileNotZero();
      Add(1);
      Move(_lines);
      Add(-1);
    EndWhile();
    Move(1-_lines);
  }

  void WhileNotZero()
  {
    o() << '[';
  }

  void EndWhile()
  {
    o() << ']';
  }

  // position of variable v relative to control
  int VariablePosition(int v) { return -3-2*v; }
  int VariableSpace(int v) { return -4-2*v; }

  void Repeat(char c, int n)
  {
    for (int i = 0; i < n; i++)
      o() << c;
  }

  void Move(int n = 1)
  {
    if (n >= 0)
      Repeat('>', n);
    else
      Repeat('<', -n);
  }

  void Add(int n = 1)
  {
    if (n >= 0)
      Repeat('+', n);
    else
      Repeat('-', -n);
  }

};

class BFOptimizer
{
public:
  BFOptimizer& operator<<(char c)
  {
    _raw << c;
    Command n;
    int s;
    switch (c)
    {
      case '+':
      case '-':
        s = (c == '+' ? 1 : -1);
        n._type = ADD;
        if (_commands.size() > 0 && _commands.back()._type == n._type)
        {
           int na = _commands.back()._amount + s;
           if (na == 0)
           {
             _commands.pop_back();
           }
#ifdef CELL8BIT
/* Only applicable to BF interpreters with 8-bit cells */
           else if (na > 128)
           {
             _commands.back()._amount = na - 256;
           }
           else if (na < -128)
           {
             _commands.back()._amount = na + 256;
           }
#endif
           else
           {
             _commands.back()._amount = na;
           }
        }
        else
        {
          n._amount = s;
          _commands.push_back(n);
        }
        break;
      case '>':
      case '<':
        s = (c == '>' ? 1 : -1);
        n._type = MOVE;
        if (_commands.size() > 0 && _commands.back()._type == n._type)
        {
           _commands.back()._amount += s;
           if (_commands.back()._amount == 0)
             _commands.pop_back();
        }
        else
        {
          n._amount = s;
          _commands.push_back(n);
        }
        break;
      case '.':
      case ',':
      case '[':
      case ']':
        n._type = (c == '.' ? OUTPUT : (c == ',' ? INPUT : (c == '[' ? WHILESTART : WHILEEND)));
        if (_commands.size() > 0 && _commands.back()._type == n._type)
        {
           _commands.back()._amount++;
        }
        else
        {
          n._amount = 1;
          _commands.push_back(n);
        }
        break;
    }
    return *this;
  }

  void Output(ostream& o, bool optimized)
  {
    if (optimized)
    {
      for (auto it = _commands.begin(); it != _commands.end(); it++)
      {
        char c;
        int n = it->_amount;
        if (it->_type == MOVE)
        {
          if (n > 0)
          {
            c = '>';
          }
          else
          {
            c = '<';
            n = -n;
          }
        }
        else if (it->_type == ADD)
        {
          if (n > 0)
          {
            c = '+';
          }
          else
          {
            c = '-';
            n = -n;
          }
        }
        else if (it->_type == INPUT)
        {
          c = ',';
        }
        else if (it->_type == OUTPUT)
        {
          c = '.';
        }
        else if (it->_type == WHILESTART)
        {
          c = '[';
        }
        else if (it->_type == WHILEEND)
        {
          c = ']';
        }

        for (int i = 0; i < n; i++)
          o << c;
      }
    }
    else
    {
      o << _raw.str();
    }
  }

private:
  enum CommandType { MOVE, ADD, OUTPUT, INPUT, WHILESTART, WHILEEND };
  struct Command
  {
    CommandType _type;
    int _amount;
  };
  vector<Command> _commands;
  stringstream _raw;
};

template <class CompilerType>
void Translate(istream& source, CompilerType& c)
{
  auto it = istreambuf_iterator<char>(source);
  auto itend = istreambuf_iterator<char>();
  int varInd = -1;
  vector<int> openCycles;
  while (it != itend)
  {
    char ch = *it++;
    if (varInd < 0)
    {
      switch (ch)
      {
        case '^': c.HLPutZero(-1); break;
        case '<': c.HLMove(-1); break;
        case '>': c.HLMove(1); break;
        case '+': c.HLAddData(); break;
        case '-': c.HLAddData(-1); break;
        case ',': c.HLReadToData(); break;
        case '.': c.HLOutputData(); break;
        case '[': c.HLWhileNotZero(); openCycles.push_back(-1); break;
        case ']': c.HLEndWhile(); openCycles.pop_back(); break;
        case '}': c.HLEndIfNotZero(); openCycles.pop_back(); break;
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
          varInd = ((int)ch) - '0';
          while (it != itend && *it >= '0' && *it <= '9')
          {
            ch = *it++;
            varInd *= 10;
            varInd += ((int)ch) - '0';
          }
          break;
      }
    }
    else
    {
      switch (ch)
      {
        case '^': c.HLPutZero(varInd); break;
        case '<': c.HLAddVarData(varInd); break;
        case '>': c.HLAddDataVar(varInd); break;
        case '+': c.HLAddVar(varInd); break;
        case '-': c.HLAddVar(varInd, -1); break;
        case ',': c.HLReadToVar(varInd); break;
        case '.': c.HLOutputVar(varInd); break;
        case '[': c.HLWhileNotZero(varInd); openCycles.push_back(varInd); break;
        case '{': c.HLIfNotZero(varInd); openCycles.push_back(varInd); break;
        default: cout << "Invalid command after var: '" << ch << "'" << endl; abort(); break;
      }
      varInd = -1;
    }
  }
}

int main(int argc, char* argv[])
{
  // declare optimizer - this will accumulate low-level instructions and generate final program optimizing low-level instructions
  shared_ptr<BFOptimizer> bfo = make_shared<BFOptimizer>();

  // compiler - takes high level instructions and generates low-level instructions passing them to low level optimizer
  Compiler<BFOptimizer> c(bfo, 3);

  // parsing grammar
  typedef BrainfuckPPGrammar<spirit::istream_iterator> grammar_type;
  grammar_type grammar;

  // prepare input reading
  shared_ptr<istream> inputptr(&cin, [](istream*) { });
  if (argc > 1)
    inputptr = make_shared<fstream>(argv[1]);
  istream& program = *inputptr;
  program.unsetf(std::ios::skipws);


  spirit::istream_iterator iter(program), iterend;
  
  typedef vector<instruction> result_attribute_type;
  result_attribute_type parsed_result;
  //bool r = qi::phrase_parse(iter, iterend, grammar, ascii::space);
  bool r = qi::phrase_parse(iter, iterend, grammar, ascii::space, parsed_result);
  instruction_visitor<ostream> vis(cout);
  for (int i = 0; i < parsed_result.size(); i++)
    boost::apply_visitor(vis, parsed_result[i]);

  cout << "Parsing status: " << (r ? "OK" : "FAILED") << endl << "Input consumed " << (iter == iterend ? "fully" : "partially") << endl;

  stringstream optimized;
  bfo->Output(optimized, true);
  cout << optimized.str() << endl;
  return 0;
}


