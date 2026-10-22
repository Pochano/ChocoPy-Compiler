#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>
#include <vector>
#include <set>
#include <map>
#include <deque>
#include <iostream>

// --- CAMBIO: Estructura Token ---
// Explicación: Antes pasábamos simples strings. Ahora guardamos el nombre, lexema, línea y columna 
// para poder decirle al usuario exactamente dónde se equivocó.

struct Token {
    std::string token_name;
    std::string lexeme;
    int line;
    int col;

    Token(std::string name = "", std::string lex = "", int l = 0, int c = 0)
        : token_name(name), lexeme(lex), line(l), col(c) {}
};

struct Rule {
    int id;
    int lhs;
    std::vector<int> rhs;
};

struct Item {
    int rule_id;
    int dot;
    int la_id;

    bool operator<(const Item& other) const {
        if (rule_id != other.rule_id) return rule_id < other.rule_id;
        if (dot != other.dot) return dot < other.dot;
        return la_id < other.la_id;
    }

    bool operator==(const Item& other) const {
        return rule_id == other.rule_id && dot == other.dot && la_id == other.la_id;
    }
};

class Grammar {
public:
    std::vector<std::string> symbols;
    std::map<std::string, int> symbol_to_id;
    std::set<int> nonterminal_ids;
    std::set<int> terminal_ids;
    std::vector<int> symbol_appearance_order;

    int goal_id;
    int start_id;
    int eof_id;
    int eps_id;

    std::vector<Rule> rules;
    std::map<int, std::vector<Rule>> prods;

    int getOrAddSymbol(const std::string& name, bool is_nonterminal = false);
    std::string getSymbolName(int id) const;

    static Grammar fromString(const std::string& text);
    static Grammar fromFile(const std::string& filepath);
    void printGrammar() const;
};

struct ConflictReport {
    int state;
    int terminal_id;
    std::string action1;
    std::string action2;
    std::vector<Item> items;
};

class LR1Builder {
public:
    Grammar g;
    std::map<int, std::set<int>> first_sets;

    std::vector<std::set<Item>> CC;
    std::map<int, std::set<Item>> cc_kernels;
    std::map<int, std::string> cc_gotos;
    std::map<std::pair<int, int>, int> transition_registry;

    std::map<std::pair<int, int>, std::string> Action;
    std::map<std::pair<int, int>, int> Goto;
    std::vector<ConflictReport> conflicts;

    LR1Builder(const Grammar& grammar);

    void first();
    std::set<int> firstCadena(const std::vector<int>& beta) const;
    std::set<Item> closure(const std::set<Item>& s);
    std::set<Item> gotoState(const std::set<Item>& s, int x);
    void coleccionCanonica();
    bool llenarTablas();

    void printFirstTable() const;
    void printClosureTable() const;
    void printLRTable() const;

    // --- CAMBIO: Nuevas funciones declaradas ---
    // Métodos para guardar/cargar la tabla LR(1) en un archivo binario/texto
    bool saveTable(const std::string& filename) const;
    bool loadTable(const std::string& filename);

    // Método para parsear tokens con tabla de seguimiento y modo pánico
    bool parseTokensWithTrace(const std::vector<Token>& tokens) const;
};

#endif