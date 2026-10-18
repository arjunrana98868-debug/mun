#pragma once
#include "db.h"

inline void updateCommittee() {
    int ci = selectCommittee();
    if (ci == -1) return;

    string n = readLine("Enter new committee name: ");
    if (n.empty()) {
        cout << "Name cannot be empty.\n";
        return;
    }
    int existing = findCommittee(n);
    if (existing != -1 && existing != ci) {
        cout << "Another committee already uses this name.\n";
        return;
    }
    if (!dbRenameCommittee(committees[ci].name, n)) return;
    committees[ci].name = n;
    cout << "Committee updated successfully.\n";
}

inline void updatePortfolio() {
    int ci = selectCommittee();
    if (ci == -1) return;
    int pi = selectPortfolio(ci);
    if (pi == -1) return;

    Portfolio& pf = committees[ci].portfolios[pi];

    string n = readLine("Enter new portfolio name (press Enter to keep '" + pf.name + "'): ");
    if (n.empty()) n = pf.name;
    int existing = committees[ci].findPortfolio(n);
    if (existing != -1 && existing != pi) {
        cout << "Another portfolio in this committee already uses this name.\n";
        return;
    }

    int cap = readInt("Enter new capacity (0 to keep " + to_string(pf.capacity) + "): ");
    if (cap < 0) {
        cout << "Capacity cannot be negative.\n";
        return;
    }
    if (cap == 0) cap = pf.capacity;
    if (cap < pf.filled()) {
        cout << "Capacity cannot be less than delegates already allocated (" << pf.filled() << ").\n";
        return;
    }

    if (!dbUpdatePortfolio(committees[ci].name, pf.name, n, cap)) return;
    pf.name = n;
    pf.setCapacity(cap);
    cout << "Portfolio updated successfully.\n";
}