#include <string>
#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <algorithm>
#include <set>
#include <map>

using namespace std;

typedef char cell_type;

int main(int argc, char* argv[])
{
  fstream inputFile(argv[1]);
  auto input = istreambuf_iterator<char>(inputFile);
  auto inputend = istreambuf_iterator<char>();

  vector<cell_type> dataUniverse;
  int N = 1000000, startPoint = 100000;
  dataUniverse.resize(N);
  for (int i = 0; i < N; i++) dataUniverse[i] = 0;
  auto data = dataUniverse.begin() + startPoint;

  vector<char> program;
  set<char> pchar = {'<', '>', '-', '+', ',', '.', '[', ']'};
  for(auto program_it = istreambuf_iterator<char>(cin); program_it != istreambuf_iterator<char>(); program_it++)
  {
    char ch = *program_it;
    if (pchar.find(ch) != pchar.end())
    {
      program.push_back(ch);
    }
  }

  vector<int> cycle_stack;
  map<int, int> cycle_correspondence;
  for (int i = 0; i < program.size(); i++)
  {
    if (program[i] == '[')
      cycle_stack.push_back(i);
    else if (program[i] == ']')
    {
      if (cycle_stack.size() == 0)
      {
         cout << "Unmatched ]" << endl;
         abort();
      }
      int j = cycle_stack.back();
      cycle_stack.pop_back();
      cycle_correspondence[i] = j;
      cycle_correspondence[j] = i;
    }
  }

  int mindata = 0;
  int maxdata = 0;
  int instruction = 0;
  while (instruction < program.size())
  {
    int curdata = distance(dataUniverse.begin() + startPoint, data);
    if (curdata < mindata) mindata = curdata;
    if (curdata > maxdata) maxdata = curdata;
    switch (program[instruction++])
    {
      case '<': data--; break;
      case '>': data++; break;
      case '+': (*data)++; break;
      case '-': (*data)--; break;
      case ',': if (input==inputend) {abort();} *data = *input++; break;
      case '.': cout << (char)*data; break;
      case '[': if (*data == 0) instruction = cycle_correspondence[instruction-1]+1; break;
      case ']': instruction = cycle_correspondence[instruction-1]; break;
    }
  }
  cout << endl << "Range: " << mindata << " " << maxdata << endl;
  return 0;
}