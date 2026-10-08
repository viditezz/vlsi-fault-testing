// circuit.cpp — .bench parser, fanout construction, Kahn's-algorithm levelization.
#include "circuit.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <queue>
#include <sstream>
#include <stdexcept>

namespace {

std::string trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string upper(std::string s)
{
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

GateType parse_type(const std::string& raw, int line_no)
{
    std::string t = upper(raw);
    if (t == "AND") return GateType::AND;
    if (t == "OR") return GateType::OR;
    if (t == "NAND") return GateType::NAND;
    if (t == "NOR") return GateType::NOR;
    if (t == "NOT" || t == "INV") return GateType::NOT;
    if (t == "BUF" || t == "BUFF") return GateType::BUF;
    if (t == "XOR") return GateType::XOR;
    if (t == "XNOR") return GateType::XNOR;
    if (t == "DFF")
        throw std::runtime_error("line " + std::to_string(line_no) +
                                 ": DFF found — only combinational circuits are supported");
    throw std::runtime_error("line " + std::to_string(line_no) + ": unknown gate type '" + raw + "'");
}

// Returns the id for a name, creating an (as yet undefined) signal if needed.
// Lazy creation is what lets a gate reference a fanin defined later in the file.
int get_or_create(Circuit& c, const std::string& name)
{
    auto it = c.index.find(name);
    if (it != c.index.end()) return it->second;
    int id = static_cast<int>(c.sig.size());
    Signal s;
    s.name = name;
    c.sig.push_back(std::move(s));
    c.index.emplace(name, id);
    return id;
}

// Text between the first '(' and the last ')'.
std::string inside_parens(const std::string& s, int line_no)
{
    size_t a = s.find('('), b = s.rfind(')');
    if (a == std::string::npos || b == std::string::npos || b < a)
        throw std::runtime_error("line " + std::to_string(line_no) + ": malformed: " + s);
    return trim(s.substr(a + 1, b - a - 1));
}

void build_fanouts(Circuit& c)
{
    for (auto& s : c.sig) s.fanouts.clear();
    for (int g = 0; g < c.num_signals(); ++g) {
        if (!c.sig[g].is_gate) continue;
        for (int f : c.sig[g].fanins) {
            auto& fo = c.sig[f].fanouts;
            if (fo.empty() || fo.back() != g) fo.push_back(g);  // dedupe same-gate pins
        }
    }
    for (auto& s : c.sig) {
        std::sort(s.fanouts.begin(), s.fanouts.end());
        s.fanouts.erase(std::unique(s.fanouts.begin(), s.fanouts.end()), s.fanouts.end());
    }
}

// Kahn's algorithm. indegree counts only fanins that are gate outputs, so a gate
// fed purely by primary inputs is ready immediately. Also assigns levels.
void levelize(Circuit& c)
{
    const int n = c.num_signals();
    std::vector<int> indeg(n, 0);
    int gates = 0;
    for (int i = 0; i < n; ++i) {
        const Signal& s = c.sig[i];
        if (!s.is_gate) continue;
        ++gates;
        // Count distinct gate fanins (fanouts are deduplicated, so match that).
        std::vector<int> f = s.fanins;
        std::sort(f.begin(), f.end());
        f.erase(std::unique(f.begin(), f.end()), f.end());
        for (int x : f)
            if (c.sig[x].is_gate) ++indeg[i];
    }

    std::queue<int> ready;
    for (int i = 0; i < n; ++i)
        if (c.sig[i].is_gate && indeg[i] == 0) ready.push(i);

    c.order.clear();
    while (!ready.empty()) {
        int g = ready.front();
        ready.pop();
        c.order.push_back(g);
        int lvl = 0;
        for (int f : c.sig[g].fanins) lvl = std::max(lvl, c.sig[f].level);
        c.sig[g].level = lvl + 1;
        for (int fo : c.sig[g].fanouts)
            if (--indeg[fo] == 0) ready.push(fo);
    }
    if (static_cast<int>(c.order.size()) != gates)
        throw std::runtime_error("levelization failed: combinational cycle in " + c.name);

    c.max_level = 0;
    for (const auto& s : c.sig) c.max_level = std::max(c.max_level, s.level);
}

}  // namespace

Circuit parse_bench_text(const std::string& text, const std::string& name)
{
    Circuit c;
    c.name = name;
    std::istringstream in(text);
    std::string line;
    int line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;
        size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        line = trim(line);
        if (line.empty()) continue;

        std::string head = upper(line.substr(0, std::min<size_t>(line.size(), 7)));
        if (head.rfind("INPUT", 0) == 0 && line.find('=') == std::string::npos) {
            int id = get_or_create(c, inside_parens(line, line_no));
            if (c.sig[id].is_pi || c.sig[id].is_gate)
                throw std::runtime_error("line " + std::to_string(line_no) + ": signal defined twice");
            c.sig[id].is_pi = true;
            c.sig[id].type = GateType::INPUT;
            c.pis.push_back(id);
            continue;
        }
        if (head.rfind("OUTPUT", 0) == 0 && line.find('=') == std::string::npos) {
            int id = get_or_create(c, inside_parens(line, line_no));
            c.sig[id].is_po = true;
            c.pos.push_back(id);
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos)
            throw std::runtime_error("line " + std::to_string(line_no) + ": malformed: " + line);
        std::string out = trim(line.substr(0, eq));
        std::string expr = trim(line.substr(eq + 1));
        size_t open = expr.find('(');
        if (open == std::string::npos)
            throw std::runtime_error("line " + std::to_string(line_no) + ": malformed: " + line);
        GateType type = parse_type(trim(expr.substr(0, open)), line_no);

        int g = get_or_create(c, out);
        if (c.sig[g].is_pi || c.sig[g].is_gate)
            throw std::runtime_error("line " + std::to_string(line_no) + ": signal '" + out +
                                     "' defined twice");
        std::vector<int> fanins;
        std::stringstream args(inside_parens(expr, line_no));
        std::string a;
        while (std::getline(args, a, ',')) {
            a = trim(a);
            if (a.empty()) continue;
            fanins.push_back(get_or_create(c, a));
        }
        if (fanins.empty())
            throw std::runtime_error("line " + std::to_string(line_no) + ": gate with no inputs");
        if ((type == GateType::NOT || type == GateType::BUF) && fanins.size() != 1)
            throw std::runtime_error("line " + std::to_string(line_no) + ": " +
                                     gate_name(type) + " must have exactly 1 input");
        c.sig[g].type = type;
        c.sig[g].is_gate = true;
        c.sig[g].fanins = std::move(fanins);
    }

    for (const auto& s : c.sig)
        if (!s.is_pi && !s.is_gate)
            throw std::runtime_error("signal '" + s.name + "' is used but never defined");
    if (c.pis.empty() || c.pos.empty())
        throw std::runtime_error(name + ": circuit needs at least one input and one output");

    build_fanouts(c);
    levelize(c);
    return c;
}

Circuit parse_bench(const std::string& path)
{
    std::ifstream f(path);
    if (!f) throw std::runtime_error("could not open file: " + path);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string name = path;
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    size_t dot = name.rfind('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return parse_bench_text(ss.str(), name);
}

const char* gate_name(GateType t)
{
    switch (t) {
        case GateType::INPUT: return "INPUT";
        case GateType::AND: return "AND";
        case GateType::OR: return "OR";
        case GateType::NAND: return "NAND";
        case GateType::NOR: return "NOR";
        case GateType::NOT: return "NOT";
        case GateType::BUF: return "BUF";
        case GateType::XOR: return "XOR";
        case GateType::XNOR: return "XNOR";
    }
    return "?";
}

bool is_inverting(GateType t)
{
    return t == GateType::NAND || t == GateType::NOR || t == GateType::NOT || t == GateType::XNOR;
}
