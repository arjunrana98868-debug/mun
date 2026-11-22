// allocator.cpp - Eligibility-Aware Experience-Based Delegate Allocation Engine (C++17)
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace std;

class Delegate {
public:
    string name;
    int experience;
    Delegate(string n, int e) : name(move(n)), experience(e) {}
};

class Allocation : public Delegate {
public:
    Allocation(string n, int e) : Delegate(move(n), e) {}
};

struct Node {
    Allocation data;
    Node* next;
    Node(string n, int e) : data(move(n), e), next(nullptr) {}
};

struct PortfolioSlot {
    int id;
    string committee;
    string portfolio;
    int seatsLeft;
    int minMuns;
};

struct AssignedRecord {
    int portfolioId;
    string delegateName;
    int experience;
    string committee;
    string portfolio;
};

void addDelegate(Node*& head, const string& name, int experience) {
    Node* newNode = new Node(name, experience);
    if (!head) {
        head = newNode;
        return;
    }
    Node* temp = head;
    while (temp->next) temp = temp->next;
    temp->next = newNode;
}

void sortDelegates(Node* head) {
    for (Node* i = head; i != nullptr; i = i->next) {
        for (Node* j = i->next; j != nullptr; j = j->next) {
            if (i->data.experience < j->data.experience) {
                swap(i->data.name, j->data.name);
                swap(i->data.experience, j->data.experience);
            }
        }
    }
}

void deleteList(Node*& head) {
    while (head) {
        Node* temp = head;
        head = head->next;
        delete temp;
    }
}

string jsonEscape(const string& s) {
    string out;
    for (unsigned char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r' || c == '\t') out += ' ';
        else if (c < 0x20) out += ' ';
        else out += (char)c;
    }
    return out;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <input.txt> [--json]\n";
        return 1;
    }
    bool asJson = (argc >= 3 && string(argv[2]) == "--json");

    ifstream in(argv[1]);
    if (!in) {
        cerr << "Cannot open input file: " << argv[1] << "\n";
        return 1;
    }

    Node* head = nullptr;
    vector<PortfolioSlot> slots;
    string line;

    while (getline(in, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string tag;
        getline(ss, tag, '|');

        if (tag == "DELEGATE") {
            string name, expStr;
            getline(ss, name, '|');
            getline(ss, expStr, '|');
            if (!name.empty()) {
                addDelegate(head, name, expStr.empty() ? 0 : stoi(expStr));
            }
        } else if (tag == "PORTFOLIO") {
            string idStr, comm, port, seatsStr, minStr;
            getline(ss, idStr, '|');
            getline(ss, comm, '|');
            getline(ss, port, '|');
            getline(ss, seatsStr, '|');
            getline(ss, minStr, '|');
            if (!idStr.empty() && !seatsStr.empty()) {
                int minMuns = minStr.empty() ? 0 : stoi(minStr);
                slots.push_back({stoi(idStr), comm, port, stoi(seatsStr), minMuns});
            }
        }
    }

    sortDelegates(head);

    vector<AssignedRecord> results;
    for (Node* cur = head; cur != nullptr; cur = cur->next) {
        for (auto& slot : slots) {
            if (slot.seatsLeft > 0 && cur->data.experience >= slot.minMuns) {
                results.push_back({slot.id, cur->data.name, cur->data.experience, slot.committee, slot.portfolio});
                slot.seatsLeft--;
                break;
            }
        }
    }

    if (asJson) {
        cout << "{\"allocations\":[";
        for (size_t i = 0; i < results.size(); i++) {
            if (i) cout << ",";
            cout << "{\"portfolio_id\":" << results[i].portfolioId
                 << ",\"delegate_name\":\"" << jsonEscape(results[i].delegateName) << "\""
                 << ",\"experience\":" << results[i].experience
                 << ",\"committee\":\"" << jsonEscape(results[i].committee) << "\""
                 << ",\"portfolio\":\"" << jsonEscape(results[i].portfolio) << "\"}";
        }
        cout << "]}\n";
    } else {
        for (const auto& r : results) {
            cout << r.delegateName << " (" << r.experience << " MUNs) -> "
                 << r.committee << " / " << r.portfolio << "\n";
        }
    }

    deleteList(head);
    return 0;
}