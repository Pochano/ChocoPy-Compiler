#include "parser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cassert>

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delim)) {
        std::string t = trim(item);
        if (!t.empty()) tokens.push_back(t);
    }
    return tokens;
}

int Grammar::getOrAddSymbol(const std::string& name, bool is_nonterminal) {
    auto it = symbol_to_id.find(name);
    if (it != symbol_to_id.end()) {
        if (is_nonterminal) {
            nonterminal_ids.insert(it->second);
            terminal_ids.erase(it->second);
        }
        return it->second;
    }
    int new_id = (int)symbols.size();
    symbols.push_back(name);
    symbol_to_id[name] = new_id;
    if (is_nonterminal) {
        nonterminal_ids.insert(new_id);
    } else {
        terminal_ids.insert(new_id);
    }
    return new_id;
}

std::string Grammar::getSymbolName(int id) const {
    if (id >= 0 && id < (int)symbols.size()) {
        return symbols[id];
    }
    return "";
}

Grammar Grammar::fromString(const std::string& text) {
    Grammar g;
    g.eps_id = g.getOrAddSymbol("ε", false);
    g.eof_id = g.getOrAddSymbol("eof", false);

    std::stringstream ss(text);
    std::string line;

    struct RawRule {
        std::string lhs;
        std::vector<std::string> rhs;
    };
    std::vector<RawRule> raw_rules;
    std::vector<std::string> raw_lhs_order;

    while (std::getline(ss, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t arrow = line.find("->");
        if (arrow == std::string::npos) continue;

        std::string lhs = trim(line.substr(0, arrow));
        std::string rhs_all = trim(line.substr(arrow + 2));

        if (std::find(raw_lhs_order.begin(), raw_lhs_order.end(), lhs) == raw_lhs_order.end()) {
            raw_lhs_order.push_back(lhs);
        }
        g.getOrAddSymbol(lhs, true);

        // Protect || in rhs
        size_t pos = 0;
        while ((pos = rhs_all.find("||", pos)) != std::string::npos) {
            rhs_all.replace(pos, 2, "__OR__");
            pos += 6;
        }

        std::vector<std::string> alts = split(rhs_all, '|');
        for (auto alt : alts) {
            pos = 0;
            while ((pos = alt.find("__OR__", pos)) != std::string::npos) {
                alt.replace(pos, 6, "||");
                pos += 2;
            }

            std::vector<std::string> syms;
            std::stringstream ass(alt);
            std::string sym;
            while (ass >> sym) {
                if (sym == "''" || sym == "ε" || sym == "EPS") {
                    // Empty RHS
                } else if (!sym.empty()) {
                    syms.push_back(sym);
                }
            }
            raw_rules.push_back({lhs, syms});
        }
    }

    if (raw_lhs_order.empty()) {
        throw std::runtime_error("Gramatica vacia o sin reglas validas.");
    }

    std::string user_start = raw_lhs_order[0];
    bool is_already_augmented = (!user_start.empty() && user_start.back() == '\'') || user_start == "Goal";

    if (is_already_augmented && raw_rules[0].rhs.size() == 1) {
        std::string goal_name = raw_rules[0].lhs;
        g.goal_id = g.getOrAddSymbol(goal_name, true);
        g.start_id = g.getOrAddSymbol(raw_rules[0].rhs[0], true);
        g.rules.push_back({0, g.goal_id, {g.start_id}});
    } else {
        std::string goal_name = "Goal";
        g.goal_id = g.getOrAddSymbol(goal_name, true);
        g.start_id = g.getOrAddSymbol(user_start, true);
        g.rules.push_back({0, g.goal_id, {g.start_id}});
    }

    int next_rule_id = 1;
    for (size_t i = 0; i < raw_rules.size(); ++i) {
        const auto& rr = raw_rules[i];
        if (i == 0 && rr.lhs == g.getSymbolName(g.goal_id)) {
            continue; // Already added as rule 0
        }
        int lhs_id = g.getOrAddSymbol(rr.lhs, true);
        std::vector<int> rhs_ids;
        for (const auto& s : rr.rhs) {
            rhs_ids.push_back(g.getOrAddSymbol(s, false));
        }
        g.rules.push_back({next_rule_id++, lhs_id, rhs_ids});
    }

    for (const auto& r : g.rules) {
        g.prods[r.lhs].push_back(r);
    }

    // Determine symbol appearance order (excluding Goal)
    std::vector<int> appearance;
    auto add_app = [&](int sym_id) {
        if (sym_id == g.goal_id || sym_id == g.eps_id) return;
        if (std::find(appearance.begin(), appearance.end(), sym_id) == appearance.end()) {
            appearance.push_back(sym_id);
        }
    };

    // First add nonterminals in order of appearance
    for (const auto& r : g.rules) {
        if (r.lhs != g.goal_id) add_app(r.lhs);
        for (int s : r.rhs) {
            if (g.nonterminal_ids.count(s)) add_app(s);
        }
    }
    // Then add terminals in order of appearance
    for (const auto& r : g.rules) {
        for (int s : r.rhs) {
            if (g.terminal_ids.count(s)) add_app(s);
        }
    }
    add_app(g.eof_id);

    g.symbol_appearance_order = appearance;
    return g;
}

Grammar Grammar::fromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("No se pudo abrir el archivo de gramatica: " + filepath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return fromString(buffer.str());
}

void Grammar::printGrammar() const {
    std::cout << "LR(1) grammar ('' is ε):" << std::endl;
    for (size_t i = 0; i < rules.size(); ++i) {
        const auto& r = rules[i];
        std::cout << "  (" << r.id << ") " << getSymbolName(r.lhs) << " -> ";
        if (r.rhs.empty()) {
            std::cout << "ε";
        } else {
            for (size_t k = 0; k < r.rhs.size(); ++k) {
                std::cout << getSymbolName(r.rhs[k]) << (k + 1 < r.rhs.size() ? " " : "");
            }
        }
        std::cout << std::endl;
    }
}

LR1Builder::LR1Builder(const Grammar& grammar) : g(grammar) {
}

void LR1Builder::first() {
    for (int t : g.terminal_ids) {
        first_sets[t].insert(t);
    }
    for (int nt : g.nonterminal_ids) {
        first_sets[nt] = {};
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& r : g.rules) {
            int A = r.lhs;
            std::set<int> res = firstCadena(r.rhs);
            size_t before = first_sets[A].size();
            for (int s : res) {
                first_sets[A].insert(s);
            }
            if (first_sets[A].size() > before) {
                changed = true;
            }
        }
    }
}

std::set<int> LR1Builder::firstCadena(const std::vector<int>& beta) const {
    std::set<int> res;
    if (beta.empty()) {
        res.insert(g.eps_id);
        return res;
    }
    for (int X : beta) {
        if (X == g.eof_id) {
            res.insert(g.eof_id);
            return res;
        }
        if (g.terminal_ids.count(X)) {
            res.insert(X);
            return res;
        }
        auto it = first_sets.find(X);
        if (it != first_sets.end()) {
            const auto& fX = it->second;
            for (int s : fX) {
                if (s != g.eps_id) res.insert(s);
            }
            if (fX.find(g.eps_id) == fX.end()) return res;
        }
    }
    res.insert(g.eps_id);
    return res;
}

std::set<Item> LR1Builder::closure(const std::set<Item>& s) {
    std::set<Item> Iset = s;
    bool changed = true;
    while (changed) {
        changed = false;
        std::vector<Item> list_items(Iset.begin(), Iset.end());
        for (const auto& it : list_items) {
            const Rule& r = g.rules[it.rule_id];
            if (it.dot < (int)r.rhs.size()) {
                int B = r.rhs[it.dot];
                if (g.nonterminal_ids.count(B)) {
                    std::vector<int> beta;
                    for (size_t k = it.dot + 1; k < r.rhs.size(); ++k) {
                        beta.push_back(r.rhs[k]);
                    }
                    beta.push_back(it.la_id);

                    std::set<int> la_candidates = firstCadena(beta);
                    auto pit = g.prods.find(B);
                    if (pit != g.prods.end()) {
                        for (const auto& rule_b : pit->second) {
                            for (int b : la_candidates) {
                                if (b != g.eps_id) {
                                    Item cand{rule_b.id, 0, b};
                                    if (Iset.find(cand) == Iset.end()) {
                                        Iset.insert(cand);
                                        changed = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return Iset;
}

std::set<Item> LR1Builder::gotoState(const std::set<Item>& s, int x) {
    std::set<Item> moved;
    for (const auto& it : s) {
        const Rule& r = g.rules[it.rule_id];
        if (it.dot < (int)r.rhs.size() && r.rhs[it.dot] == x) {
            moved.insert(Item{it.rule_id, it.dot + 1, it.la_id});
        }
    }
    if (moved.empty()) return {};
    return closure(moved);
}

void LR1Builder::coleccionCanonica() {
    Item start_item{0, 0, g.eof_id};
    std::set<Item> init_kernel = {start_item};
    CC.push_back(closure(init_kernel));
    cc_kernels[0] = init_kernel;
    cc_gotos[0] = "";

    std::deque<int> work = {0};
    std::map<std::set<Item>, int> seen;
    seen[CC[0]] = 0;

    while (!work.empty()) {
        int i = work.front();
        work.pop_front();
        std::set<Item> I = CC[i];

        for (int X : g.symbol_appearance_order) {
            std::set<Item> moved;
            for (const auto& it : I) {
                const Rule& r = g.rules[it.rule_id];
                if (it.dot < (int)r.rhs.size() && r.rhs[it.dot] == X) {
                    moved.insert(Item{it.rule_id, it.dot + 1, it.la_id});
                }
            }
            if (moved.empty()) continue;

            std::set<Item> J = closure(moved);
            if (seen.find(J) == seen.end()) {
                int new_id = (int)CC.size();
                seen[J] = new_id;
                CC.push_back(J);
                cc_kernels[new_id] = moved;
                cc_gotos[new_id] = "goto(" + std::to_string(i) + ", " + g.getSymbolName(X) + ")";
                work.push_back(new_id);
            }
            transition_registry[{i, X}] = seen[J];
        }
    }
}

bool LR1Builder::llenarTablas() {
    bool ok = true;
    for (size_t i = 0; i < CC.size(); ++i) {
        const auto& cci = CC[i];
        for (const auto& it : cci) {
            const Rule& r = g.rules[it.rule_id];
            if (it.dot < (int)r.rhs.size()) {
                int c = r.rhs[it.dot];
                if (g.terminal_ids.count(c)) {
                    auto tit = transition_registry.find({(int)i, c});
                    if (tit != transition_registry.end()) {
                        int j = tit->second;
                        std::string act = "s" + std::to_string(j);
                        auto key = std::make_pair((int)i, c);
                        if (Action.find(key) != Action.end() && Action[key] != act) {
                            conflicts.push_back({(int)i, c, Action[key], act, {it}});
                            ok = false;
                        } else {
                            Action[key] = act;
                        }
                    }
                }
            } else {
                if (r.lhs == g.goal_id && it.la_id == g.eof_id) {
                    Action[{(int)i, g.eof_id}] = "acc";
                } else if (r.lhs != g.goal_id) {
                    std::string act = "r" + std::to_string(r.id);
                    auto key = std::make_pair((int)i, it.la_id);
                    if (Action.find(key) != Action.end() && Action[key] != act) {
                        conflicts.push_back({(int)i, it.la_id, Action[key], act, {it}});
                        ok = false;
                    } else {
                        Action[key] = act;
                    }
                }
            }
        }

        for (int N : g.nonterminal_ids) {
            if (N == g.goal_id) continue;
            auto tit = transition_registry.find({(int)i, N});
            if (tit != transition_registry.end()) {
                Goto[{(int)i, N}] = tit->second;
            }
        }
    }

    if (!conflicts.empty()) {
        std::cout << "\n==================================================" << std::endl;
        std::cout << "          DETECCION DE CONFLICTOS                 " << std::endl;
        std::cout << "==================================================" << std::endl;
        for (const auto& conf : conflicts) {
            std::cout << "Conflict en Estado " << conf.state
                      << ", simbolo '" << g.getSymbolName(conf.terminal_id) << "': "
                      << conf.action1 << " vs " << conf.action2 << std::endl;
        }
    }

    return ok;
}

static std::string formatItemsGrouped(const Grammar& g, const std::set<Item>& items) {
    typedef std::pair<int, int> CoreKey; // rule_id, dot
    std::map<CoreKey, std::set<int>> grouped;

    for (const auto& it : items) {
        grouped[{it.rule_id, it.dot}].insert(it.la_id);
    }

    std::string result = "{";
    bool first = true;
    for (const auto& kv : grouped) {
        if (!first) result += "; ";
        first = false;

        const Rule& r = g.rules[kv.first.first];
        int dot = kv.first.second;

        std::string rhs_str = "";
        for (int k = 0; k < (int)r.rhs.size(); ++k) {
            if (k == dot) rhs_str += ".";
            else rhs_str += g.getSymbolName(r.rhs[k]);
            if (k + 1 < (int)r.rhs.size() || k == dot) rhs_str += " ";
        }
        if (dot == (int)r.rhs.size()) rhs_str += ".";
        rhs_str = trim(rhs_str);
        if (rhs_str.empty()) rhs_str = ".";

        std::string las_str = "";
        bool first_la = true;
        for (int la : kv.second) {
            if (!first_la) las_str += "/";
            first_la = false;
            las_str += g.getSymbolName(la);
        }

        result += "[" + g.getSymbolName(r.lhs) + " -> " + rhs_str + ", " + las_str + "]";
    }
    result += "}";
    return result;
}

void LR1Builder::printFirstTable() const {
    std::cout << "\nFIRST table" << std::endl;
    std::cout << "-------------------------" << std::endl;
    std::cout << std::left << std::setw(14) << "Symbol" << " | FIRST" << std::endl;
    std::cout << "-------------------------" << std::endl;

    for (int s_id : g.symbol_appearance_order) {
        auto it = first_sets.find(s_id);
        if (it != first_sets.end()) {
            std::cout << std::left << std::setw(14) << g.getSymbolName(s_id) << " | {";
            bool f = true;
            for (int la : it->second) {
                if (!f) std::cout << ", ";
                f = false;
                std::cout << g.getSymbolName(la);
            }
            std::cout << "}" << std::endl;
        }
    }
    std::cout << "-------------------------" << std::endl;
}

void LR1Builder::printClosureTable() const {
    std::cout << "\n==================================================" << std::endl;
    std::cout << "                LR(1) closure table               " << std::endl;
    std::cout << "==================================================" << std::endl;

    std::cout << std::left << std::setw(12) << "Goto"
              << " | " << std::setw(28) << "Kernel"
              << " | " << std::setw(6) << "State"
              << " | " << "Closure" << std::endl;
    std::cout << "--------------------------------------------------------------------------------" << std::endl;

    for (size_t s = 0; s < CC.size(); ++s) {
        std::string goto_str = cc_gotos.at(s);
        std::string kernel_str = formatItemsGrouped(g, cc_kernels.at(s));
        std::string closure_str = formatItemsGrouped(g, CC[s]);

        std::cout << std::left << std::setw(12) << goto_str
                  << " | " << std::setw(28) << kernel_str
                  << " | " << std::setw(6) << s
                  << " | " << closure_str << std::endl;
    }
}

void LR1Builder::printLRTable() const {
    std::vector<int> term_cols;
    for (int s : g.symbol_appearance_order) {
        if (g.terminal_ids.count(s)) term_cols.push_back(s);
    }
    if (std::find(term_cols.begin(), term_cols.end(), g.eof_id) == term_cols.end()) {
        term_cols.insert(term_cols.begin(), g.eof_id);
    }

    std::vector<int> nt_cols;
    for (int s : g.symbol_appearance_order) {
        if (g.nonterminal_ids.count(s) && s != g.goal_id) nt_cols.push_back(s);
    }

    std::cout << "\n=========================================" << std::endl;
    std::cout << "                LR table                 " << std::endl;
    std::cout << "=========================================" << std::endl;

    std::cout << std::left << std::setw(6) << "Estado" << " | ";
    for (int t : term_cols) {
        std::cout << std::setw(6) << g.getSymbolName(t);
    }
    std::cout << " | ";
    for (int nt : nt_cols) {
        std::cout << std::setw(6) << g.getSymbolName(nt);
    }
    std::cout << std::endl;

    std::cout << "---------------------------------------------------------" << std::endl;

    for (size_t s = 0; s < CC.size(); ++s) {
        std::cout << std::left << std::setw(6) << s << " | ";
        for (int t : term_cols) {
            auto it = Action.find({(int)s, t});
            std::string val = (it != Action.end()) ? it->second : "";
            std::cout << std::setw(6) << val;
        }
        std::cout << " | ";
        for (int nt : nt_cols) {
            auto it = Goto.find({(int)s, nt});
            std::string val = (it != Goto.end()) ? std::to_string(it->second) : "";
            std::cout << std::setw(6) << val;
        }
        std::cout << std::endl;
    }
}

bool LR1Builder::saveTable(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out.is_open()) return false;

    out << Action.size() << "\n";
    for (const auto& kv : Action) {
        out << kv.first.first << " " << kv.first.second << " " << kv.second << "\n";
    }

    out << Goto.size() << "\n";
    for (const auto& kv : Goto) {
        out << kv.first.first << " " << kv.first.second << " " << kv.second << "\n";
    }
    return true;
}

bool LR1Builder::loadTable(const std::string& filename) {
    std::ifstream in(filename);
    if (!in.is_open()) return false;

    Action.clear();
    Goto.clear();

    size_t action_sz, goto_sz;
    if (!(in >> action_sz)) return false;

    for (size_t i = 0; i < action_sz; ++i) {
        int state, sym_id;
        std::string act;
        in >> state >> sym_id >> act;
        Action[{state, sym_id}] = act;
    }

    if (!(in >> goto_sz)) return false;

    for (size_t i = 0; i < goto_sz; ++i) {
        int state, sym_id, next_state;
        in >> state >> sym_id >> next_state;
        Goto[{state, sym_id}] = next_state;
    }
    return true;
}

// Seguimiento paso a paso (Trace) con Modo Pánico
bool LR1Builder::parseTokensWithTrace(const std::vector<Token>& tokens) const {
    struct StackElem {
        int state;
        std::string symbol;
    };

    std::vector<StackElem> stack;
    stack.push_back({0, ""});

    size_t cursor = 0;
    int step = 1;

    std::cout << "\n+------+---------------------------------+----------------------+----------+\n";
    std::cout << "| Step | Stack                           | Input                | Action   |\n";
    std::cout << "+------+---------------------------------+----------------------+----------+\n";

    auto printStackStr = [&]() {
        std::string s = "";
        for (const auto& elem : stack) {
            if (!elem.symbol.empty()) s += elem.symbol + " ";
            s += std::to_string(elem.state) + " ";
        }
        if (!s.empty() && s.back() == ' ') s.pop_back();
        return s;
    };

    auto printInputStr = [&](size_t from) {
        std::string s = "";
        for (size_t i = from; i < tokens.size(); ++i) {
            s += tokens[i].token_name + " ";
        }
        s += "$";
        return s;
    };

    bool has_error = false;

    while (true) {

        int state = stack.back().state;
        std::string curr_token = (cursor < tokens.size()) ? tokens[cursor].token_name : "EOF";

        // Normalizamos EOF
        if (curr_token == "EOF") {
            curr_token = "eof";
        }

        int sym_id = g.symbol_to_id.count(curr_token) ? g.symbol_to_id.at(curr_token) : -1;

        auto it = Action.find({state, sym_id});
        std::string act = (it != Action.end()) ? it->second : "";

        std::cout << "| " << std::left << std::setw(4) << step++
                  << " | " << std::setw(31) << printStackStr()
                  << " | " << std::setw(20) << printInputStr(cursor)
                  << " | " << std::setw(8) << act << " |\n";

        if (act.empty()) {
            has_error = true;
            std::cout << "\n[ERROR SINTACTICO] En linea " 
                    << tokens[cursor].line << ", columna " << tokens[cursor].col 
                    << ": Token inesperado '" << tokens[cursor].lexeme << "' (" << curr_token << ")\n";

            // --- CAMBIO: INICIO DEL MODO PÁNICO ---
            // Explicación: En vez de morir aquí, tratamos de recuperarnos.

            std::cout << "[MODO PANICO] Limpiando pila y buscando punto de sincronizacion...\n";

            // 1. Buscamos IDs de No-Terminales que sean seguros para continuar (como una sentencia entera)
            int nt_stmt = g.symbol_to_id.count("stmt") ? g.symbol_to_id.at("stmt") : -1;
            int nt_decl_item = g.symbol_to_id.count("decl_item") ? g.symbol_to_id.at("decl_item") : -1;
            int nt_simple_stmt = g.symbol_to_id.count("simple_stmt") ? g.symbol_to_id.at("simple_stmt") : -1;

            // 2. Desempilamos la pila (borramos lo que estábamos analizando a medias) hasta llegar a un estado donde podamos aceptar un 'stmt'
            while (stack.size() > 1) {
                int st = stack.back().state;
                
                // Verificamos si existe transicion Goto para un no-terminal de nivel superior
                bool can_recover = (nt_stmt != -1 && Goto.find({st, nt_stmt}) != Goto.end()) ||
                                (nt_decl_item != -1 && Goto.find({st, nt_decl_item}) != Goto.end()) ||
                                (nt_simple_stmt != -1 && Goto.find({st, nt_simple_stmt}) != Goto.end());

                if (can_recover) {
                    break; // Encontramos un estado base limpio y seguro
                }
                stack.pop_back(); // Limpiamos la produccion incompleta
            }

            // 3. Ignoramos los tokens siguientes hasta encontrar un limitador lógico (Ej: salto de línea)
            std::set<std::string> sync_tokens = {"NEWLINE", "SEMICOLON", "DEDENT", "EOF"};

            while (cursor < tokens.size() && sync_tokens.find(tokens[cursor].token_name) == sync_tokens.end()) {
                std::cout << "  -> Retrocediendo/Descartando token: " << tokens[cursor].lexeme << "\n";
                cursor++;
            }

            if (cursor >= tokens.size() || tokens[cursor].token_name == "EOF") {
                std::cout << "[MODO PANICO] No se pudo recuperar antes de alcanzar el final del archivo.\n";
                return false;
            }

           // 4. Retomamos el análisis en la siguiente línea limpia
            std::cout << "  -> Sincronizado en el token: " << tokens[cursor].token_name << "\n";
            cursor++; // Consumir NEWLINE / DEDENT
            continue;
        }

        if (act == "acc") {
            std::cout << "+------+---------------------------------+----------------------+----------+\n";
            std::cout << "\n>> Entrada ACEPTADA por el parser.\n";
            return !has_error;
        }

        if (act[0] == 's') {
            int next_state = std::stoi(act.substr(1));
            stack.push_back({next_state, curr_token});
            cursor++;
        } else if (act[0] == 'r') {
            int rule_id = std::stoi(act.substr(1));
            const Rule& r = g.rules[rule_id];

            for (size_t k = 0; k < r.rhs.size(); ++k) {
                stack.pop_back();
            }

            int top_state = stack.back().state;
            int lhs_id = r.lhs;
            std::string lhs_name = g.getSymbolName(lhs_id);

            auto git = Goto.find({top_state, lhs_id});
            if (git != Goto.end()) {
                int goto_state = git->second;

                stack.push_back({goto_state, lhs_name});
            } else {
                std::cerr << "[ERROR CRÍTICO] Estado Goto no encontrado.\n";
                return false;
            }
        }
    }
}
