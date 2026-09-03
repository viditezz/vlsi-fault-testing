#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;


// ============================================================
// 1. INTERNAL REPRESENTATION
// ============================================================

enum class GateType {
    AND,
    OR,
    NAND,
    NOR,
    NOT,
    XOR,
    BUF
};


struct Signal {
    int name = -1;

    // Default value only for initialization.
    // Actual gate outputs get their type while parsing.
    GateType type = GateType::BUF;

    vector<int> fanins;
    vector<int> fanouts;

    bool is_gate_output = false;
};


struct Circuit {
    vector<Signal> signals;

    // Original .bench signal number -> internal index
    unordered_map<int, int> name_to_index;

    // Internal signal indices
    vector<int> primary_inputs;
    vector<int> primary_outputs;

    // Gate indices in topological order
    vector<int> levelized_order;
};


// ============================================================
// HELPER FUNCTIONS
// ============================================================

string trim(const string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");

    if (start == string::npos)
        return "";

    size_t end = s.find_last_not_of(" \t\r\n");

    return s.substr(start, end - start + 1);
}


string remove_spaces(const string& s)
{
    string result;

    for (char c : s) {
        if (!isspace(static_cast<unsigned char>(c)))
            result += c;
    }

    return result;
}


GateType parse_gate_type(const string& type)
{
    if (type == "AND")
        return GateType::AND;

    if (type == "OR")
        return GateType::OR;

    if (type == "NAND")
        return GateType::NAND;

    if (type == "NOR")
        return GateType::NOR;

    if (type == "NOT")
        return GateType::NOT;

    if (type == "XOR")
        return GateType::XOR;

    if (type == "BUF")
        return GateType::BUF;

    throw runtime_error("Unknown gate type: " + type);
}


// ============================================================
// GET OR CREATE SIGNAL
// ============================================================

int get_or_create_signal(Circuit& circuit, int name)
{
    auto it = circuit.name_to_index.find(name);

    if (it != circuit.name_to_index.end())
        return it->second;

    int index = static_cast<int>(circuit.signals.size());

    Signal signal;

    signal.name = name;
    signal.type = GateType::BUF;
    signal.is_gate_output = false;

    circuit.signals.push_back(signal);

    circuit.name_to_index[name] = index;

    return index;
}


// ============================================================
// 2. NETLIST PARSER
// ============================================================

Circuit parse_bench(const string& filename)
{
    Circuit circuit;

    ifstream file(filename);

    if (!file) {
        throw runtime_error(
            "Could not open file: " + filename
        );
    }

    string line;

    while (getline(file, line)) {

        // Remove leading/trailing whitespace
        line = trim(line);

        // Skip blank lines
        if (line.empty())
            continue;

        // Skip comments
        if (line[0] == '#')
            continue;

        // Remove spaces inside the expression
        line = remove_spaces(line);


        // ----------------------------------------------------
        // INPUT(x)
        // ----------------------------------------------------
        if (line.rfind("INPUT(", 0) == 0) {

            size_t open = line.find('(');
            size_t close = line.find(')');

            if (close == string::npos) {
                throw runtime_error(
                    "Malformed INPUT line: " + line
                );
            }

            int name = stoi(
                line.substr(
                    open + 1,
                    close - open - 1
                )
            );

            int index =
                get_or_create_signal(circuit, name);

            circuit.primary_inputs.push_back(index);

            continue;
        }


        // ----------------------------------------------------
        // OUTPUT(x)
        // ----------------------------------------------------
        if (line.rfind("OUTPUT(", 0) == 0) {

            size_t open = line.find('(');
            size_t close = line.find(')');

            if (close == string::npos) {
                throw runtime_error(
                    "Malformed OUTPUT line: " + line
                );
            }

            int name = stoi(
                line.substr(
                    open + 1,
                    close - open - 1
                )
            );

            int index =
                get_or_create_signal(circuit, name);

            circuit.primary_outputs.push_back(index);

            continue;
        }


        // ----------------------------------------------------
        // Gate:
        //
        // name = TYPE(arg1,arg2,...)
        //
        // Example:
        //
        // 10 = NAND(1,3)
        // ----------------------------------------------------

        size_t equals = line.find('=');

        if (equals == string::npos) {
            throw runtime_error(
                "Malformed gate line: " + line
            );
        }

        string output_name =
            line.substr(0, equals);

        string expression =
            line.substr(equals + 1);


        size_t open = expression.find('(');
        size_t close = expression.rfind(')');

        if (open == string::npos ||
            close == string::npos) {

            throw runtime_error(
                "Malformed gate expression: " + line
            );
        }


        string type_name =
            expression.substr(0, open);

        string args =
            expression.substr(
                open + 1,
                close - open - 1
            );


        int output_signal =
            stoi(output_name);

        int output_index =
            get_or_create_signal(
                circuit,
                output_signal
            );


        // Set gate information
        circuit.signals[output_index].type =
            parse_gate_type(type_name);

        circuit.signals[output_index].is_gate_output =
            true;


        // ----------------------------------------------------
        // Parse fanins
        // ----------------------------------------------------

        stringstream ss(args);
        string arg;

        while (getline(ss, arg, ',')) {

            if (arg.empty())
                continue;

            int fanin_name = stoi(arg);

            int fanin_index =
                get_or_create_signal(
                    circuit,
                    fanin_name
                );

            circuit.signals[output_index]
                .fanins
                .push_back(fanin_index);
        }
    }

    file.close();

    return circuit;
}


// ============================================================
// BUILD FANOUT LIST
// ============================================================

void build_fanouts(Circuit& circuit)
{
    // Clear existing fanouts
    for (auto& signal : circuit.signals)
        signal.fanouts.clear();


    // For every gate:
    //
    // gate = TYPE(a,b)
    //
    // a -> gate
    // b -> gate

    for (int gate_index = 0;
         gate_index < static_cast<int>(
             circuit.signals.size()
         );
         ++gate_index) {

        Signal& gate =
            circuit.signals[gate_index];

        if (!gate.is_gate_output)
            continue;

        for (int fanin : gate.fanins) {

            circuit.signals[fanin]
                .fanouts
                .push_back(gate_index);
        }
    }
}


// ============================================================
// 2. LEVELIZATION — KAHN'S ALGORITHM
// ============================================================

void levelize(Circuit& circuit)
{
    int n =
        static_cast<int>(circuit.signals.size());


    // indegree[i] = number of gate outputs
    // feeding gate i.
    //
    // Primary inputs do not count toward indegree.

    vector<int> indegree(n, 0);


    for (int i = 0; i < n; ++i) {

        Signal& signal =
            circuit.signals[i];

        if (!signal.is_gate_output)
            continue;

        for (int fanin : signal.fanins) {

            if (circuit.signals[fanin]
                    .is_gate_output) {

                ++indegree[i];
            }
        }
    }


    // --------------------------------------------------------
    // Start with gates whose fanins are all primary inputs.
    // --------------------------------------------------------

    queue<int> ready;

    for (int i = 0; i < n; ++i) {

        if (circuit.signals[i].is_gate_output &&
            indegree[i] == 0) {

            ready.push(i);
        }
    }


    circuit.levelized_order.clear();


    // --------------------------------------------------------
    // Kahn's algorithm
    // --------------------------------------------------------

    while (!ready.empty()) {

        int current =
            ready.front();

        ready.pop();


        circuit.levelized_order.push_back(
            current
        );


        // Every gate that uses current as a fanin
        // has one fewer unresolved dependency.

        for (int fanout :
             circuit.signals[current].fanouts) {

            --indegree[fanout];

            if (indegree[fanout] == 0)
                ready.push(fanout);
        }
    }


    // --------------------------------------------------------
    // Verify that every gate was processed.
    //
    // If not, there is a cycle or unresolved dependency.
    // --------------------------------------------------------

    int gate_count = 0;

    for (const Signal& signal :
         circuit.signals) {

        if (signal.is_gate_output)
            ++gate_count;
    }


    if (static_cast<int>(
            circuit.levelized_order.size()
        ) != gate_count) {

        throw runtime_error(
            "Levelization failed: circuit contains "
            "a cycle or unresolved dependency."
        );
    }
}


// ============================================================
// GATE EVALUATION
// ============================================================

bool evaluate_gate(
    GateType type,
    const vector<int>& fanins,
    const vector<bool>& values)
{
    switch (type) {

        case GateType::AND: {
            bool result = true;
            for (int fanin : fanins)
                result = result && values[fanin];
            return result;
        }

        case GateType::OR: {
            bool result = false;
            for (int fanin : fanins)
                result = result || values[fanin];
            return result;
        }

        case GateType::NAND: {
            bool result = true;
            for (int fanin : fanins)
                result = result && values[fanin];
            return !result;
        }

        case GateType::NOR: {
            bool result = false;
            for (int fanin : fanins)
                result = result || values[fanin];
            return !result;
        }

        case GateType::NOT:
            if (fanins.size() != 1) {
                throw runtime_error(
                    "NOT gate must have exactly 1 fanin"
                );
            }
            return !values[fanins[0]];


        case GateType::XOR: {
            bool result = false;
            for (int fanin : fanins)
                result = result ^ values[fanin];
            return result;
        }

        case GateType::BUF:
            if (fanins.size() != 1) {
                throw runtime_error(
                    "BUF gate must have exactly 1 fanin"
                );
            }
            return values[fanins[0]];
    }


    throw runtime_error(
        "Invalid gate type"
    );
}


// ============================================================
// 3. FAULT-FREE LOGIC SIMULATION
// ============================================================

vector<bool> simulate(
    const Circuit& circuit,
    const vector<bool>& input_vector)
{
    // --------------------------------------------------------
    // Check input size
    // --------------------------------------------------------

    if (input_vector.size() !=
        circuit.primary_inputs.size()) {

        throw runtime_error(
            "Input vector size does not match "
            "number of primary inputs."
        );
    }


    // --------------------------------------------------------
    // One value for every internal signal
    // --------------------------------------------------------

    vector<bool> values(
        circuit.signals.size(),
        false
    );


    // --------------------------------------------------------
    // Assign primary inputs
    // --------------------------------------------------------

    for (size_t i = 0;
         i < circuit.primary_inputs.size();
         ++i) {

        int signal_index =
            circuit.primary_inputs[i];

        values[signal_index] =
            input_vector[i];
    }


    // --------------------------------------------------------
    // Evaluate gates in topological order
    // --------------------------------------------------------

    for (int gate_index :
         circuit.levelized_order) {

        const Signal& gate =
            circuit.signals[gate_index];

        values[gate_index] =
            evaluate_gate(
                gate.type,
                gate.fanins,
                values
            );
    }


    // --------------------------------------------------------
    // Read primary outputs
    // --------------------------------------------------------

    vector<bool> outputs;

    for (int output_index :
         circuit.primary_outputs) {

        outputs.push_back(
            values[output_index]
        );
    }

    return outputs;
}


// ============================================================
// DEBUG / PRINT FUNCTIONS
// ============================================================

string gate_type_to_string(GateType type)
{
    switch (type) {

        case GateType::AND:
            return "AND";

        case GateType::OR:
            return "OR";

        case GateType::NAND:
            return "NAND";

        case GateType::NOR:
            return "NOR";

        case GateType::NOT:
            return "NOT";

        case GateType::XOR:
            return "XOR";

        case GateType::BUF:
            return "BUF";
    }

    return "UNKNOWN";
}


void print_circuit(const Circuit& circuit)
{
    cout << "\nSignals: "
         << circuit.signals.size()
         << '\n';


    cout << "Primary inputs: ";

    for (int index :
         circuit.primary_inputs) {

        cout << circuit.signals[index].name
             << ' ';
    }


    cout << "\nPrimary outputs: ";

    for (int index :
         circuit.primary_outputs) {

        cout << circuit.signals[index].name
             << ' ';
    }


    cout << "\n\nGates:\n";


    for (int index :
         circuit.levelized_order) {

        const Signal& signal =
            circuit.signals[index];


        cout << signal.name
             << " = "
             << gate_type_to_string(signal.type)
             << "(";


        for (size_t i = 0;
             i < signal.fanins.size();
             ++i) {

            if (i > 0)
                cout << ", ";

            cout << circuit
                        .signals[
                            signal.fanins[i]
                        ]
                        .name;
        }


        cout << ")\n";
    }


    cout << "\nLevelized order:\n";


    for (int index :
         circuit.levelized_order) {

        cout << circuit.signals[index].name
             << ' ';
    }


    cout << "\n";
}


// ============================================================
// MAIN
// ============================================================

int main(int argc, char* argv[])
{
    // --------------------------------------------------------
    // Expect:
    //
    // ./faultsim ../benchmarks/c17.bench
    // --------------------------------------------------------

    if (argc != 2) {

        cerr << "Usage: "
             << argv[0]
             << " <bench_file>\n";

        return 1;
    }


    try {

        // ----------------------------------------------------
        // Parse .bench file
        // ----------------------------------------------------

        Circuit circuit =
            parse_bench(argv[1]);


        // ----------------------------------------------------
        // Build reverse fanout lists
        // ----------------------------------------------------

        build_fanouts(circuit);


        // ----------------------------------------------------
        // Generate topological evaluation order
        // ----------------------------------------------------

        levelize(circuit);


        // ----------------------------------------------------
        // Print parsed circuit
        // ----------------------------------------------------

        print_circuit(circuit);


        // ----------------------------------------------------
        // Simple simulation test
        //
        // For now, every primary input = 0.
        //
        // We will replace this with a manually selected
        // c17 test vector when validating the simulator.
        // ----------------------------------------------------

        vector<bool> input_vector(
            circuit.primary_inputs.size(),
            false
        );


        vector<bool> outputs =
            simulate(
                circuit,
                input_vector
            );


        cout << "\nSimulation test:\n";


        cout << "Inputs:  ";

        for (bool value :
             input_vector) {

            cout << value << ' ';
        }


        cout << "\nOutputs: ";

        for (bool value :
             outputs) {

            cout << value << ' ';
        }


        cout << '\n';
    }
    catch (const exception& e) {

        cerr << "Error: "
             << e.what()
             << '\n';

        return 1;
    }


    return 0;
}
