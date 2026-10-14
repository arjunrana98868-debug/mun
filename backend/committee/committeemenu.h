// CommitteeMenu.h
// Menu for the Committee & Portfolio module. Call committeeMenu() from Delegate.cpp.
#include "db.h"
#include "addCommittee.h"
#include "dispCommittee.h"
#include "updateCommittee.h"
#include "deleteCommittee.h"

void committeeMenu() {
    static bool dbInitialised = false;
    if (!dbInitialised) {          // connect to MySQL + load saved data once
        dbConnect();
        dbLoad();
        dbInitialised = true;
    }

    int ch;
    do {
        cout << "\n";
        cout << "+--------------------------------------------------+\n";
        cout << "|        COMMITTEE & PORTFOLIO MANAGEMENT          |\n";
        cout << "+------+-------------------------------------------+\n";
        cout << "| S.no | OPERATION                                 |\n";
        cout << "+------+-------------------------------------------+\n";
        cout << "| 1    | Add Committee                             |\n";
        cout << "| 2    | Add Portfolio                             |\n";
        cout << "| 3    | Set Portfolio Capacity                    |\n";
        cout << "| 4    | Check Availability                        |\n";
        cout << "| 5    | Allocate Delegate to Portfolio            |\n";
        cout << "| 6    | Update Committee / Portfolio              |\n";
        cout << "| 7    | Delete Committee / Portfolio              |\n";
        cout << "| 8    | Display Available Portfolios              |\n";
        cout << "| 9    | Display All Committees                    |\n";
        cout << "| 10   | Back to Main Menu                         |\n";
        cout << "+------+-------------------------------------------+\n";
        ch = readInt("Enter your choice: ");

        switch (ch) {
        case 1: addCommittee(); break;
        case 2: addPortfolio(); break;
        case 3: setPortfolioCapacity(); break;
        case 4: checkAvailability(); break;
        case 5: allocateDelegate(); break;
        case 6: {
            int t = readInt("1. Update committee  2. Update portfolio: ");
            if (t == 1) updateCommittee();
            else if (t == 2) updatePortfolio();
            else cout << "Invalid choice.\n";
            break;
        }
        case 7: {
            int t = readInt("1. Delete committee  2. Delete portfolio: ");
            if (t == 1) deleteCommittee();
            else if (t == 2) deletePortfolio();
            else cout << "Invalid choice.\n";
            break;
        }
        case 8: displayAvailablePortfolios(); break;
        case 9: displayCommittees(); break;
        case 10: break;
        default: cout << "Invalid choice.\n";
        }
    } while (ch != 10);
}
