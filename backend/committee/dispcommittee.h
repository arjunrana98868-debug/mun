#pragma once
#include "Committee.h"

inline void checkAvailability() {
    int ci = selectCommittee();
    if (ci == -1) return;
    int pi = selectPortfolio(ci);
    if (pi == -1) return;

    const Portfolio& pf = committees[ci].portfolios[pi];
    cout << "\n+----------------------+----------------------+----------+--------+------------+\n";
    cout << "| Committee            | Portfolio            | Capacity | Filled | Status     |\n";
    cout << "+----------------------+----------------------+----------+--------+------------+\n";
    cout << "| " << fitCell(committees[ci].name, 20)
         << " | " << fitCell(pf.name, 20)
         << " | " << setw(8) << pf.capacity
         << " | " << setw(6) << pf.filled()
         << " | " << fitCell(pf.isAvailable() ? "AVAILABLE" : "FULL", 10) << " |\n";
    cout << "+----------------------+----------------------+----------+--------+------------+\n";
}

inline void displayAvailablePortfolios() {
    if (committees.empty()) {
        cout << "No committees available.\n";
        return;
    }
    bool any = false;
    cout << "\n+----------------------+----------------------+----------+--------+------------+\n";
    cout << "| Committee            | Available Portfolio  | Capacity | Left   | Status     |\n";
    cout << "+----------------------+----------------------+----------+--------+------------+\n";
    for (size_t i = 0; i < committees.size(); i++) {
        for (size_t j = 0; j < committees[i].portfolios.size(); j++) {
            const Portfolio& p = committees[i].portfolios[j];
            if (p.isAvailable()) {
                cout << "| " << fitCell(committees[i].name, 20)
                     << " | " << fitCell(p.name, 20)
                     << " | " << setw(8) << p.capacity
                     << " | " << setw(6) << p.available()
                     << " | " << fitCell("OPEN", 10) << " |\n";
                any = true;
            }
        }
    }
    cout << "+----------------------+----------------------+----------+--------+------------+\n";
    if (!any) cout << "No vacant portfolios are currently available.\n";
}

inline void displayCommittees() {
    if (committees.empty()) {
        cout << "No committees available.\n";
        return;
    }

    // Committee Summary Table
    cout << "\n============================== COMMITTEE SUMMARY ==============================\n";
    cout << "+-----+----------------------+-------------+--------------+------------+\n";
    cout << "| No. | Committee Name       | Total Seats | Seats Filled | Seats Left |\n";
    cout << "+-----+----------------------+-------------+--------------+------------+\n";
    for (size_t i = 0; i < committees.size(); i++) {
        const Committee& c = committees[i];
        cout << "| " << setw(3) << (i + 1)
             << " | " << fitCell(c.name, 20)
             << " | " << setw(11) << c.totalSeats()
             << " | " << setw(12) << c.totalFilled()
             << " | " << setw(10) << c.seatsLeft() << " |\n";
    }
    cout << "+-----+----------------------+-------------+--------------+------------+\n";

    // Detailed Portfolio & Allocation Matrix Table
    cout << "\n============================ PORTFOLIO & DELEGATE MATRIX ============================\n";
    cout << "+----------------------+----------------------+----------+--------+--------------------------+\n";
    cout << "| Committee            | Portfolio            | Capacity | Status | Allocated Delegate(s)    |\n";
    cout << "+----------------------+----------------------+----------+--------+--------------------------+\n";
    for (size_t i = 0; i < committees.size(); i++) {
        const Committee& c = committees[i];
        if (c.portfolios.empty()) {
            cout << "| " << fitCell(c.name, 20)
                 << " | " << fitCell("[No Portfolios]", 20)
                 << " | " << setw(8) << "-"
                 << " | " << fitCell("N/A", 6)
                 << " | " << fitCell("-", 24) << " |\n";
            continue;
        }
        for (size_t j = 0; j < c.portfolios.size(); j++) {
            const Portfolio& p = c.portfolios[j];
            string delegates = "";
            for (size_t k = 0; k < p.allocated.size(); k++) {
                delegates += (k == 0 ? "" : ", ") + p.allocated[k];
            }
            if (delegates.empty()) delegates = "Unassigned";

            cout << "| " << fitCell(c.name, 20)
                 << " | " << fitCell(p.name, 20)
                 << " | " << setw(8) << p.capacity
                 << " | " << fitCell(p.isAvailable() ? "OPEN" : "FULL", 6)
                 << " | " << fitCell(delegates, 24) << " |\n";
        }
    }
    cout << "+----------------------+----------------------+----------+--------+--------------------------+\n";
}