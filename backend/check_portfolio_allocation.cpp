#include <iostream>
#include <string>
#include <utility>
using namespace std;
class Delegate {
public:
    string name;
    int experience;
    Delegate(string n, int e)  {
        name = n;
        experience = e;
    }
};
class Allocation : public Delegate {
public:
    Allocation(string n, int e) : Delegate(n, e) {}
};
typedef struct Node {
    Allocation data;
    struct Node* next;
    Node(string n, int e):data(n, e){
        next = nullptr;
    }
} N;
void addDelegate(N*& head, string name, int experience) {
    N* newNode = new N(name, experience);
    if (head == nullptr) {
        head = newNode;
        return;
    }
    N* temp = head;
    while (temp->next != nullptr){
        temp = temp->next;
    }
    temp->next = newNode;
}
void sortDelegates(N* head) {
    for (N* i = head; i != nullptr; i = i->next) {
        for (N* j = i->next; j != nullptr; j = j->next) {
            if (i->data.experience < j->data.experience) {
                swap(i->data.name, j->data.name);
                swap(i->data.experience, j->data.experience);
            }
        }
    }
}
 
void allocateDelegates(N* head) {
    string committees[] = {"UNHRC", "UNGA"};
    string portfolios[] = {"India", "France", "USA", "China"};

    bool portfolioAvailable[] = {true, true, true, true};
    int seats[] = {2, 2};

    sortDelegates(head);

    cout << "\nDELEGATE ALLOCATION RESULTS\n\n";

    for (N* temp = head; temp != nullptr; temp = temp->next) {
        bool assigned = false;

        for (int i = 0; i < 2; i++) {
            if (seats[i] <= 0)
                continue;

            for (int j = 0; j < 4; j++) {
                if (portfolioAvailable[j]) {
                    cout << "Name: " << temp->data.name << endl;
                    cout << "Experience: "
                         << temp->data.experience << " MUNs\n";
                    cout << "Committee: " << committees[i] << endl;
                    cout << "Portfolio: " << portfolios[j] << endl;
                    cout << "Portfolio Status: Allocated\n";
                    cout << "----------------------\n";

                    portfolioAvailable[j] = false;
                    seats[i]--;
                    assigned = true;
                    break;
                }
            }

            if (assigned)
                break;
        }

        if (!assigned) {
            cout << "Name: " << temp->data.name << endl;
            cout << "No seat or portfolio available\n";
            cout << "----------------------\n";
        }
    }

    cout << "\nPORTFOLIO AVAILABILITY\n";

    for (int i = 0; i < 4; i++) {
        cout << portfolios[i] << ": "
             << (portfolioAvailable[i] ? "Available" : "Allocated")
             << endl;
    }
}

void deleteList(N*& head) {
    while (head != nullptr) {
        N* temp = head;
        head = head->next;
        delete temp;
    }
}
int main() {
    N* head = nullptr;
    addDelegate(head, "Rahul", 4);
    addDelegate(head, "Aman", 1);
    addDelegate(head, "Priya", 3);
    addDelegate(head, "Neha", 2);
    addDelegate(head, "Karan", 5);
    allocateDelegates(head);
    deleteList(head);
    return 0;
}
