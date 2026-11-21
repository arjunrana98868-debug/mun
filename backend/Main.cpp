#include <iostream>
#include <cstdlib>
#include <limits>
#include <string>

#include "Delegate Managemet/Delegate.cpp"
#include "committee/committeemenu.h"

using namespace std;

int main() {
    int choice;
    do {
        cout << "\n";
        cout << "+--------------------------------------------------+\n";
        cout << "|              MUN MANAGEMENT SYSTEM               |\n";
        cout << "+--------------------------------------------------+\n";
        cout << "| S.no |            OPERATION                      |\n";
        cout << "+------+-------------------------------------------+\n";
        cout << "|  1   |  Delegate Management                      |\n";
        cout << "|  2   |  CV Information                           |\n";
        cout << "|  3   |  Committee & Portfolio Management         |\n";
        cout << "|  4   |  Delegate Allocation (Web / Engine)       |\n";
        cout << "|  0   |  Exit                                     |\n";
        cout << "+------+-------------------------------------------+\n";
        cout << "Enter your choice: ";
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Invalid input!\n";
            choice = -1;
            continue;
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        switch (choice) {
            case 1:
                delegateMenu();
                break;
            case 2: {
                string cvPath;
                cout << "Enter CV text file path (e.g. cv.txt): ";
                getline(cin, cvPath);
                if (!cvPath.empty()) {
                    #ifdef _WIN32
                    string cmd = "cv_parser.exe \"" + cvPath + "\"";
                    #else
                    string cmd = "./cv_parser \"" + cvPath + "\"";
                    #endif
                    system(cmd.c_str());
                }
                break;
            }
            case 3:
                committeeMenu();
                break;
            case 4:
                cout << "Use Option 3 -> Allocate Delegate or run Auto-Allocate at http://localhost:3000/dashboard.html\n";
                break;
            case 0:
                cout << "Exiting program...\n";
                break;
            default:
                cout << "Invalid choice!\n";
        }
    } while (choice != 0);

    return 0;
}
