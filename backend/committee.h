#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdlib>

using namespace std;

inline string toLower(string s) {
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

inline string readLine(const string& prompt) {
    string s;
    cout << prompt;
    if (!getline(cin, s)) {
        cout << "\nInput closed. Exiting.\n";
        exit(0);
    }
    return s;
}

inline int readInt(const string& prompt) {
    while (true) {
        string s = readLine(prompt);
        stringstream ss(s);
        int x;
        char extra;
        if ((ss >> x) && !(ss >> extra)) return x;
        cout << "Invalid number. Try again: ";
    }
}

inline bool confirm(const string& msg) {
    string s = readLine(msg + " (y/n): ");
    return !s.empty() && (s[0] == 'y' || s[0] == 'Y');
}

inline string fitCell(const string& str, size_t width) {
    if (str.length() > width) {
        return str.substr(0, width - 2) + "..";
    }
    return str + string(width - str.length(), ' ');
}

class Portfolio {
public:
    string name;
    int capacity;
    vector<string> allocated;

    Portfolio(string n = "", int c = 0) : name(n), capacity(c) {}

    int filled() const { return (int)allocated.size(); }
    int available() const { return capacity - filled(); }
    bool isAvailable() const { return available() > 0; }

    bool has(const string& d) const {
        for (size_t i = 0; i < allocated.size(); i++) {
            if (toLower(allocated[i]) == toLower(d)) return true;
        }
        return false;
    }

    bool setCapacity(int c) {
        if (c < filled()) return false;
        capacity = c;
        return true;
    }

    bool allocate(const string& d) {
        if (!isAvailable() || has(d)) return false;
        allocated.push_back(d);
        return true;
    }

    bool deallocate(const string& d) {
        for (size_t i = 0; i < allocated.size(); i++) {
            if (toLower(allocated[i]) == toLower(d)) {
                allocated.erase(allocated.begin() + i);
                return true;
            }
        }
        return false;
    }
};

class Committee {
public:
    string name;
    vector<Portfolio> portfolios;

    Committee(string n = "") : name(n) {}

    int findPortfolio(const string& pn) const {
        for (size_t i = 0; i < portfolios.size(); i++) {
            if (toLower(portfolios[i].name) == toLower(pn)) return (int)i;
        }
        return -1;
    }

    int totalSeats() const {
        int t = 0;
        for (size_t i = 0; i < portfolios.size(); i++) t += portfolios[i].capacity;
        return t;
    }

    int totalFilled() const {
        int t = 0;
        for (size_t i = 0; i < portfolios.size(); i++) t += portfolios[i].filled();
        return t;
    }

    int seatsLeft() const {
        return totalSeats() - totalFilled();
    }
};

inline vector<Committee> committees;

inline int findCommittee(const string& n) {
    for (size_t i = 0; i < committees.size(); i++) {
        if (toLower(committees[i].name) == toLower(n)) return (int)i;
    }
    return -1;
}

inline int selectCommittee() {
    if (committees.empty()) {
        cout << "No committees available. Add one first.\n";
        return -1;
    }
    string n = readLine("Enter committee name: ");
    int i = findCommittee(n);
    if (i == -1) cout << "Committee not found.\n";
    return i;
}

inline int selectPortfolio(int ci) {
    if (committees[ci].portfolios.empty()) {
        cout << "This committee has no portfolios yet.\n";
        return -1;
    }
    string n = readLine("Enter portfolio name: ");
    int i = committees[ci].findPortfolio(n);
    if (i == -1) cout << "Portfolio not found.\n";
    return i;
}