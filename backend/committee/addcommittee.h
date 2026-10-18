#pragma once
#include "db.h"

inline void addCommittee() {
    string n = readLine("Enter committee name: ");
    if (n.empty()) {
        cout << "Name cannot be empty.\n";
        return;
    }
    if (findCommittee(n) != -1) {
        cout << "Committee already exists.\n";
        return;
    }
    if (!dbAddCommittee(n)) return;
    committees.push_back(Committee(n));
    cout << "Committee added successfully.\n";
}

inline void addPortfolio() {
    int ci = selectCommittee();
    if (ci == -1) return;

    string p = readLine("Enter portfolio name (e.g. India, USA): ");
    if (p.empty()) {
        cout << "Name cannot be empty.\n";
        return;
    }
    if (committees[ci].findPortfolio(p) != -1) {
        cout << "Portfolio already exists in this committee.\n";
        return;
    }
    int cap = readInt("Enter capacity: ");
    if (cap <= 0) {
        cout << "Capacity must be greater than 0.\n";
        return;
    }
    if (!dbAddPortfolio(committees[ci].name, p, cap)) return;
    committees[ci].portfolios.push_back(Portfolio(p, cap));
    cout << "Portfolio added successfully.\n";
}

inline void setPortfolioCapacity() {
    int ci = selectCommittee();
    if (ci == -1) return;
    int pi = selectPortfolio(ci);
    if (pi == -1) return;

    Portfolio& pf = committees[ci].portfolios[pi];
    cout << "Current capacity: " << pf.capacity << " (filled: " << pf.filled() << ")\n";
    int cap = readInt("Enter new capacity: ");
    if (cap <= 0) {
        cout << "Capacity must be greater than 0.\n";
        return;
    }
    if (cap < pf.filled()) {
        cout << "Capacity cannot be less than delegates already allocated (" << pf.filled() << ").\n";
        return;
    }
    if (!dbSetCapacity(committees[ci].name, pf.name, cap)) return;
    pf.setCapacity(cap);
    cout << "Capacity updated.\n";
}

inline void allocateDelegate() {
    int ci = selectCommittee();
    if (ci == -1) return;
    int pi = selectPortfolio(ci);
    if (pi == -1) return;

    Portfolio& pf = committees[ci].portfolios[pi];
    if (!pf.isAvailable()) {
        cout << "Portfolio is full.\n";
        return;
    }
    string d = readLine("Enter delegate name: ");
    if (d.empty()) {
        cout << "Name cannot be empty.\n";
        return;
    }
    if (pf.has(d)) {
        cout << "Delegate is already allocated to this portfolio.\n";
        return;
    }
    if (!dbAllocate(committees[ci].name, pf.name, d)) return;
    pf.allocate(d);
    cout << "Delegate allocated. Seats left: " << pf.available() << "\n";
}

inline void deallocateDelegate() {
    int ci = selectCommittee();
    if (ci == -1) return;
    int pi = selectPortfolio(ci);
    if (pi == -1) return;

    Portfolio& pf = committees[ci].portfolios[pi];
    if (pf.filled() == 0) {
        cout << "No delegates are currently allocated to this portfolio.\n";
        return;
    }
    string d = readLine("Enter delegate name to deallocate: ");
    if (!pf.has(d)) {
        cout << "Delegate not found in this portfolio.\n";
        return;
    }
    if (!dbDeallocate(committees[ci].name, pf.name, d)) return;
    pf.deallocate(d);
    cout << "Delegate removed. Seats left: " << pf.available() << "\n";
}