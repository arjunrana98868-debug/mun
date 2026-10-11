#include "Committee.h"
#include <sstream>
#include <cctype>

vector<Committee> committees;

string toLower(string s) {
    for (size_t i = 0; i < s.size(); i++) s[i] = tolower((unsigned char)s[i]);
    return s;
}

string readLine(const string& prompt) {
    string s;
    cout << prompt;
    getline(cin, s);
    return s;
}

int readInt(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        stringstream ss(s);
        int x;
        char extra;
        if ((ss >> x) && !(ss >> extra)) return x;
        cout << "Invalid number. Try again.\n";
    }
}

bool confirm(const string& msg) {
    string s = readLine(msg + " (y/n): ");
    return !s.empty() && (s[0] == 'y' || s[0] == 'Y');
}

// Portfolio Implementation
Portfolio::Portfolio(string n, int c) : name(n), capacity(c) {}

int Portfolio::filled() const { return (int)allocated.size(); }
int Portfolio::available() const { return capacity - filled(); }
bool Portfolio::isAvailable() const { return available() > 0; }

bool Portfolio::has(const string& d) const {
    for (size_t i = 0; i < allocated.size(); i++)
        if (toLower(allocated[i]) == toLower(d)) return true;
    return false;
}

bool Portfolio::setCapacity(int c) {
    if (c < filled()) return false;
    capacity = c;
    return true;
}

bool Portfolio::allocate(const string& d) {
    if (!isAvailable() || has(d)) return false;
    allocated.push_back(d);
    return true;
}

bool Portfolio::deallocate(const string& d) {
    for (size_t i = 0; i < allocated.size(); i++) {
        if (toLower(allocated[i]) == toLower(d)) {
            allocated.erase(allocated.begin() + i);
            return true;
        }
    }
    return false;
}

// Committee Implementation
Committee::Committee(string n) : name(n) {}

int Committee::findPortfolio(const string& pn) const {
    for (size_t i = 0; i < portfolios.size(); i++)
        if (toLower(portfolios[i].name) == toLower(pn)) return (int)i;
    return -1;
}

int Committee::totalSeats() const {
    int t = 0;
    for (size_t i = 0; i < portfolios.size(); i++) t += portfolios[i].capacity;
    return t;
}

int Committee::seatsLeft() const {
    int t = 0;
    for (size_t i = 0; i < portfolios.size(); i++) t += portfolios[i].available();
    return t;
}

int findCommittee(const string& n) {
    for (size_t i = 0; i < committees.size(); i++)
        if (toLower(committees[i].name) == toLower(n)) return (int)i;
    return -1;
}

int selectCommittee() {
    if (committees.empty()) {
        cout << "No committees available. Add one first.\n";
        return -1;
    }
    string n = readLine("Enter committee name: ");
    int i = findCommittee(n);
    if (i == -1) cout << "Committee not found.\n";
    return i;
}

int selectPortfolio(int ci) {
    if (committees[ci].portfolios.empty()) {
        cout << "This committee has no portfolios yet.\n";
        return -1;
    }
    string n = readLine("Enter portfolio name: ");
    int i = committees[ci].findPortfolio(n);
    if (i == -1) cout << "Portfolio not found.\n";
    return i;
}

bool isDelegateAllocatedConferenceWide(const string& delegateName, string& foundCommittee, string& foundPortfolio) {
    for (size_t i = 0; i < committees.size(); ++i) {
        for (size_t j = 0; j < committees[i].portfolios.size(); ++j) {
            if (committees[i].portfolios[j].has(delegateName)) {
                foundCommittee = committees[i].name;
                foundPortfolio = committees[i].portfolios[j].name;
                return true;
            }
        }
    }
    return false;
}