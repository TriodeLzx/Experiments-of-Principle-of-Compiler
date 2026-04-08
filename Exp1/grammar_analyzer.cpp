#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>

using namespace std;

struct Production {
    vector<string> lhs;
    vector<string> rhs;
};

struct Error {
    string type;
    string symbol;
    string location;
};

// 文法分析器
class GrammarAnalyzer {
public:
    set<string> N;
    set<string> T;
    vector<Production> P;
    string start_symbol;
    vector<Error> errors;
    bool format_error = false;
    
    // 解析文法文件
    void parse(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            format_error = true;
            return;
        }

        string line;
        int state = 0; // 0: 等待 N, 1: 等待 T, 2: 等待 P, 3: 正在读取 P, 4: 等待 START
        int prod_count = 0;

        bool has_N = false, has_T = false, has_P = false, has_START = false;
        
        while (getline(file, line)) {
            // 去除行首/行尾的空白字符
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty() || line[0] == '#') continue;

            if (line.find("N:") == 0) {
                stringstream ss(line.substr(2));
                string sym;
                while (ss >> sym) N.insert(sym);
                has_N = true;
                state = 1;
            } else if (line.find("T:") == 0) {
                stringstream ss(line.substr(2));
                string sym;
                while (ss >> sym) T.insert(sym);
                has_T = true;
                state = 2;
            } else if (line.find("P:") == 0) {
                has_P = true;
                state = 3;
            } else if (line.find("START:") == 0) {
                stringstream ss(line.substr(6));
                ss >> start_symbol;
                has_START = true;
                state = 4;
            } else if (state == 3) {
                size_t arrow_pos = line.find("->");
                if (arrow_pos != string::npos) {
                    prod_count++;
                    string lhs_str = line.substr(0, arrow_pos);
                    string rhs_str = line.substr(arrow_pos + 2);

                    vector<string> lhs;
                    stringstream lhs_ss(lhs_str);
                    string sym;
                    while (lhs_ss >> sym) lhs.push_back(sym);

                    stringstream rhs_ss(rhs_str);
                    string rhs_part;
                    while (getline(rhs_ss, rhs_part, '|')) {
                        vector<string> rhs;
                        stringstream part_ss(rhs_part);
                        while (part_ss >> sym) {
                            if (sym == "ε" || sym == "epsilon") rhs.clear(); // 当接收到 epsilon 时清空作为空的右部
                            else rhs.push_back(sym);
                        }
                        P.push_back({lhs, rhs});
                    }
                }
            } else {
                format_error = true;
            }
        }

        if (!has_N || !has_T || !has_P || !has_START) format_error = true;
    }

    // 文法合法性检查
    bool validate() {
        if (format_error) {
            errors.push_back({"FORMAT_ERROR", "", ""});
            return false;
        }

        if (N.find(start_symbol) == N.end()) {
            errors.push_back({"START_NOT_IN_N", start_symbol, ""});
        }

        for (size_t i = 0; i < P.size(); ++i) {
            for (const string& sym : P[i].lhs) {
                if (N.find(sym) == N.end() && T.find(sym) == T.end()) {
                    errors.push_back({"UNDEFINED_SYMBOL", sym, "production " + to_string(i + 1)});
                }
            }
            for (const string& sym : P[i].rhs) {
                if (N.find(sym) == N.end() && T.find(sym) == T.end()) {
                    errors.push_back({"UNDEFINED_SYMBOL", sym, "production " + to_string(i + 1)});
                }
            }
        }

        // 可达性检查 (Reachability)
        set<string> reachable;
        reachable.insert(start_symbol);
        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& prod : P) {
                bool lhs_reachable = true;
                for (const string& sym : prod.lhs) {
                    if (reachable.find(sym) == reachable.end()) {
                        lhs_reachable = false;
                        break;
                    }
                }
                if (lhs_reachable) {
                    for (const string& sym : prod.rhs) {
                        if (N.find(sym) != N.end() && reachable.find(sym) == reachable.end()) {
                            reachable.insert(sym);
                            changed = true;
                        }
                    }
                }
            }
        }

        for (const string& n : N) {
            if (reachable.find(n) == reachable.end()) {
                errors.push_back({"UNREACHABLE", n, ""});
            }
        }

        // 可推导性（生成性）检查 (Generating)
        set<string> generating;
        for (const string& t : T) generating.insert(t);
        changed = true;
        while (changed) {
            changed = false;
            for (const auto& prod : P) {
                bool all_gen = true;
                for (const string& sym : prod.rhs) {
                    if (generating.find(sym) == generating.end()) {
                        all_gen = false;
                        break;
                    }
                }
                if (all_gen) {
                    for (const string& lsym : prod.lhs) {
                        if (N.find(lsym) != N.end() && generating.find(lsym) == generating.end()) {
                            generating.insert(lsym);
                            changed = true;
                        }
                    }
                }
            }
        }

        for (const string& n : N) {
            if (generating.find(n) == generating.end()) {
                errors.push_back({"NON_GENERATING", n, ""});
            }
        }

        return errors.empty();
    }

    string checkType() {
        if (!validate() || format_error) return "INVALID";

        bool is_type_2 = true, is_type_3_right = true, is_type_3_left = true;
        bool has_epsilon_start = false, start_in_rhs = false;

        for (const auto& prod : P) {
            if (prod.lhs.size() != 1 || N.find(prod.lhs[0]) == N.end()) {
                is_type_2 = false;
                is_type_3_right = false;
                is_type_3_left = false;
                break;
            }
            
            if (prod.rhs.empty() && prod.lhs[0] == start_symbol) has_epsilon_start = true;
            for (const string& sym : prod.rhs) {
                if (sym == start_symbol) start_in_rhs = true;
            }

            if (is_type_2) {
                if (prod.rhs.size() > 2) {
                    is_type_3_right = false;
                    is_type_3_left = false;
                } else if (prod.rhs.size() == 2) {
                    if (T.find(prod.rhs[0]) != T.end() && N.find(prod.rhs[1]) != N.end()) {
                        is_type_3_left = false; // 排除左线性特性，确认为右线性 (aB)
                    } else if (N.find(prod.rhs[0]) != N.end() && T.find(prod.rhs[1]) != T.end()) {
                        is_type_3_right = false; // 排除右线性特性，确认为左线性 (Ba)
                    } else {
                        is_type_3_right = false;
                        is_type_3_left = false;
                    }
                } else if (prod.rhs.size() == 1) {
                    if (N.find(prod.rhs[0]) != N.end()) {
                    }
                }
            }
        }
        
        // 严格审查 3 型文法，以处理类似 A -> a 和 A -> aB / Ba 的混合情况
        if (is_type_2) {
             for (const auto& prod : P) {
                 if (prod.rhs.size() > 2) { is_type_3_left = is_type_3_right = false; }
                 else if (prod.rhs.size() == 2) {
                     if (T.find(prod.rhs[0]) == T.end() || N.find(prod.rhs[1]) == N.end()) is_type_3_right = false;
                     if (N.find(prod.rhs[0]) == N.end() || T.find(prod.rhs[1]) == T.end()) is_type_3_left = false;
                 } else if (prod.rhs.size() == 1) {
                     if (T.find(prod.rhs[0]) == T.end()) {
                         is_type_3_right = is_type_3_left = false; 
                     }
                 } else if (prod.rhs.empty()) {
                     // 允许存在空转移产生式
                 }
             }
        }

        if (is_type_3_right && !is_type_3_left) return "TYPE_3_RIGHT";
        if (is_type_3_left && !is_type_3_right) return "TYPE_3_LEFT";
        if (is_type_2) return "TYPE_2";

        bool is_type_1 = true;
        for (const auto& prod : P) {
            if (prod.rhs.empty()) {
                if (prod.lhs.size() == 1 && prod.lhs[0] == start_symbol && !start_in_rhs) {
                    // S -> epsilon 但 S 永不出现在其它产生式右部的合法例外情况
                } else {
                    is_type_1 = false;
                }
            } else if (prod.lhs.size() > prod.rhs.size()) {
                is_type_1 = false;
            }
        }

        if (is_type_1) return "TYPE_1";
        return "TYPE_0";
    }

    // 二义性检查
    void checkAmbiguity(int max_length) {
        if (format_error || N.find(start_symbol) == N.end()) {
            cout << "{\"ambiguous\": false, \"max_length_checked\": " << max_length << "}\n";
            return;
        }

        struct State {
            vector<string> sf;
            int depth;
        };

        vector<State> queue;
        queue.push_back({{start_symbol}, 0});
        
        map<string, int> terminal_counts;
        int max_depth = max_length * 4 + 10;
        int max_queue_size = 300000;
        int head = 0;

        while (head < queue.size() && queue.size() < max_queue_size) {
            State curr = queue[head++];
            
            if (curr.depth > max_depth) continue;

            int first_nt_idx = -1;
            int t_count = 0;
            for (size_t i = 0; i < curr.sf.size(); ++i) {
                if (N.find(curr.sf[i]) != N.end()) {
                    if (first_nt_idx == -1) first_nt_idx = i;
                } else if (T.find(curr.sf[i]) != T.end()) {
                    t_count++;
                }
            }

            if (t_count > max_length) continue;

            // 如果全部是终结符
            if (first_nt_idx == -1) {
                string t_str = "";
                for (size_t i = 0; i < curr.sf.size(); ++i) {
                    if (i > 0) t_str += " ";
                    t_str += curr.sf[i];
                }
                
                terminal_counts[t_str]++;
                if (terminal_counts[t_str] > 1) {
                    cout << "{\"ambiguous\": true, \"witness\": \"" << t_str << "\", \"parse_count\": " << terminal_counts[t_str] << "}\n";
                    return;
                }
                continue;
            }

            // 对于 CFG：进行严格的最左推导（Leftmost Derivation），确保不重复记录相同的语法分析树
            for (const auto& prod : P) {
                if (first_nt_idx + prod.lhs.size() > curr.sf.size()) continue;
                
                bool match = true;
                for (size_t j = 0; j < prod.lhs.size(); ++j) {
                    if (curr.sf[first_nt_idx + j] != prod.lhs[j]) {
                        match = false;
                        break;
                    }
                }
                
                if (match) {
                    State next_state;
                    next_state.depth = curr.depth + 1;
                    
                    // 前缀
                    for (size_t j = 0; j < first_nt_idx; ++j) 
                        next_state.sf.push_back(curr.sf[j]);
                        
                    // 替换体 (如果包含 epsilon 就不压入内容)
                    for (const string& r : prod.rhs) {
                        if (r != "ε" && r != "epsilon") {
                            next_state.sf.push_back(r);
                        }
                    }
                    
                    // 后缀
                    for (size_t j = first_nt_idx + prod.lhs.size(); j < curr.sf.size(); ++j) {
                        next_state.sf.push_back(curr.sf[j]);
                    }
                    
                    // 长度膨胀过大的树枝直接剪尾，避免死循环爆内存
                    if (next_state.sf.size() <= max_length + 15) {
                        queue.push_back(next_state);
                    }
                }
            }
        }
        
        cout << "{\"ambiguous\": false, \"max_length_checked\": " << max_length << "}\n";
    }

    void printStats() {
        bool has_eps = false;
        for (const auto& p : P) if (p.rhs.empty()) has_eps = true;
        
        cout << "{\n";
        cout << "  \"num_nonterminals\": " << N.size() << ",\n";
        cout << "  \"num_terminals\": " << T.size() << ",\n";
        cout << "  \"num_productions\": " << P.size() << ",\n";
        cout << "  \"has_epsilon\": " << (has_eps ? "true" : "false") << ",\n";
        cout << "  \"start_symbol\": \"" << start_symbol << "\"\n";
        cout << "}\n";
    }

    void printValidation() {
        if (errors.empty() && !format_error) {
            cout << "{\"valid\": true}\n";
        } else {
            cout << "{\"valid\": false, \"errors\": [\n";
            for (size_t i = 0; i < errors.size(); ++i) {
                cout << "  {\"type\": \"" << errors[i].type << "\"";
                if (!errors[i].symbol.empty()) cout << ", \"symbol\": \"" << errors[i].symbol << "\"";
                if (!errors[i].location.empty()) cout << ", \"location\": \"" << errors[i].location << "\"";
                cout << "}";
                if (i < errors.size() - 1) cout << ",";
                cout << "\n";
            }
            cout << "]}\n";
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " input.txt [--check-type | --validate | --stats]\n";
        return 1;
    }

    string filename = argv[1];
    string mode = argv[2];
    int max_length = 8;

    for (int i = 3; i < argc; ++i) {
        if (string(argv[i]) == "--max-length" && i + 1 < argc) {
            max_length = stoi(argv[i + 1]);
            i++;
        }
    }

    GrammarAnalyzer analyzer;
    analyzer.parse(filename);

    if (mode == "--validate") {
        analyzer.validate();
        analyzer.printValidation();
    } else if (mode == "--stats") {
        analyzer.printStats();
    } else if (mode == "--check-type") {
        cout << analyzer.checkType() << "\n";
    } else if (mode == "--ambiguity-check") {
        analyzer.checkAmbiguity(max_length);
    } else {
        cerr << "Unknown mode.\n";
        return 1;
    }

    return 0;
}
