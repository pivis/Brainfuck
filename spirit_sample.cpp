#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>

#include <iostream>
#include <string>
#include <vector>

using namespace std;
namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

template <typename Iterator>
struct BrainfuckPPGrammar : qi::grammar<Iterator, ascii::space_type>
{
    BrainfuckPPGrammar() : BrainfuckPPGrammar::base_type(program)
    {
        using qi::eps;
        using qi::lit;
        using qi::_val;
        using qi::_1;
        using qi::_r1;
        using ascii::char_;

        program = eps         //  [_r1 << "Start parsing"]
                  >>
                  char_('B')  //  [_r1 << "Symbol B found"]
                  >>
                  char_('C')  //  [_r1 << "Symbol B found"]
        ;
/*
        start = eps             [_val = 0] >>
            (
                +lit('M')       [_val += 1000]
                ||  hundreds    [_val += _1]
                ||  tens        [_val += _1]
                ||  ones        [_val += _1]
            )
        ;
*/
    }

    qi::rule<Iterator, ascii::space_type> program;
};

int main()
{
    typedef BrainfuckPPGrammar<std::string::const_iterator> grammar;
    grammar g; // Our grammar

    vector<string> samples;
    samples.push_back("B C");
    samples.push_back("BC");
    samples.push_back("B");
    samples.push_back("B C ");
    samples.push_back("E");
    for (string& storage : samples)
    {
      string::const_iterator iter = storage.begin();
      string::const_iterator end = storage.end();

      bool r = qi::phrase_parse(iter, end, g, ascii::space);
      cout << "String '" << storage << "': " << r << " " << (iter == end) << endl;
    }
    return 0;
}

