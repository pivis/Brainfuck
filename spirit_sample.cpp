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
    BrainfuckPPGrammar(ostream* os) : BrainfuckPPGrammar::base_type(program)
    {
        using qi::eps;
        using qi::lit;
        using qi::_val;
        using qi::_1;
        using qi::_r1;
        using ascii::char_;
        program = eps           [ ([os]()->void {(*os) << "Start parsing" << endl; }) ]
                  >>
                  char_('B')    [ ([os]()->void {(*os) << "Found B" << endl; }) ]
                  >>
                  char_('C')    [ ([os]()->void {(*os) << "Found C" << endl; }) ]
        ;
    }

    qi::rule<Iterator, ascii::space_type> program;
};

int main()
{
    typedef BrainfuckPPGrammar<std::string::const_iterator> grammar;
    grammar g(&cout); // Our grammar

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

