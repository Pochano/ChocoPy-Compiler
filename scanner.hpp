#ifndef SCANNER_HPP
#define SCANNER_HPP

#include "parser.hpp"
#include <string>
#include <vector>

class Scanner {
public:
    Scanner(const std::string& filename);
    ~Scanner();
    std::vector<Token> scan();
    bool hasError() const;

private:
    std::string filename;
    char* text_Arr;
    int index_pos;
    int sizee;
    int line_global;
    int col_global;
    int error_count;
    std::vector<int> indent_stack;
    int bracket_depth;
    bool at_line_start;

    char get_char();
    char peek_actual_char();
    void report_error(const std::string& message, int line, int col);
    void process_indentation(std::vector<Token>& tokens);
    void scan_next_tokens(std::vector<Token>& tokens);
};

#endif
