#include <iostream>
#include <sstream>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <utility>
#include <vector>
#include <memory>

using namespace std;

//#define CELL8BIT


template<class OutputWriter>
class Compiler
{
private:
  int _lines; // number of parallel lines (line 0 for data, others for control).
  shared_ptr<OutputWriter> _output;
  bool _at_control;
public:
  Compiler(shared_ptr<OutputWriter> output, int lines)
    :_output(move(output)), _lines(lines), _at_control(false)
  {
    InitializeControl();
  }

  void HLWhileNotZero(int v = -1)
  {
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


  void HLEndWhile(int v = -1)
  {
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
    GoToVar(argument);
    WhileNotZero();
    ReturnFromVar(argument);
  }

  void HLEndIfNotZero(int argument)
  {
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
        case ']': c.HLEndWhile(openCycles.back()); openCycles.pop_back(); break;
        case '}': c.HLEndIfNotZero(openCycles.back()); openCycles.pop_back(); break;
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
  shared_ptr<BFOptimizer> bfo = make_shared<BFOptimizer>();
  Compiler<BFOptimizer> c(bfo, 3);

  shared_ptr<istream> inputptr(&cin, [](istream*) { });
  if (argc > 1)
    inputptr = make_shared<fstream>(argv[1]);
  istream& program = *inputptr;

  Translate(program, c);

  stringstream raw, optimized;
  bfo->Output(optimized, true);
  //bfo->Output(raw, false);
  cout << optimized.str() << endl;
  //cout << raw.str() << endl;
  return 0;
}