// deleteCommittee.h
// Delete Committee / Portfolio
#include "db.h"

void deleteCommittee() {
    int ci = selectCommittee();
    if (ci == -1) return;

    if (!confirm("Delete committee '" + committees[ci].name + "' and all its portfolios?")) {
        cout << "Cancelled.\n";
        return;
    }
    if (!dbDeleteCommittee(committees[ci].name)) return;   // cascades to portfolios + allocations
    committees.erase(committees.begin() + ci);
    cout << "Committee deleted.\n";
}

void deletePortfolio() {
    int ci = selectCommittee();
    if (ci == -1) return;
    int pi = selectPortfolio(ci);
    if (pi == -1) return;

    Portfolio& pf = committees[ci].portfolios[pi];
    string msg = "Delete portfolio '" + pf.name + "'";
    if (pf.filled() > 0) msg += " (" + to_string(pf.filled()) + " delegate(s) allocated)";
    if (!confirm(msg + "?")) {
        cout << "Cancelled.\n";
        return;
    }
    if (!dbDeletePortfolio(committees[ci].name, pf.name)) return;
    committees[ci].portfolios.erase(committees[ci].portfolios.begin() + pi);
    cout << "Portfolio deleted.\n";
}
