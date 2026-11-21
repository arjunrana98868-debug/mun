#include "addDelegate.cpp"
#include "dispDelegate.cpp"
#include "updateDelegate.cpp"
#include "searchDelegate.cpp"
#include"deleteDelegate.cpp"
vector<Delegate> delegates;

void delegateMenu(){
    int ch;
     do{
       cout << "\n";
cout << "+--------------------------------------------------+\n";
cout << "|              MUN MANAGEMENT SYSTEM               |\n";
cout << "+--------------------------------------------------+\n";
cout << "|              DELEGATE MANAGEMENT                 |\n";
cout << "+------+-------------------------------------------+\n";
cout << "| S.no |            OPERATION                      |\n";
cout << "+------+-------------------------------------------+\n";
cout << "|  1   |  Add Delegate                             |\n";
cout << "|  2   |  Display Delegate                         |\n";
cout << "|  3   |  Search Delegate                          |\n";
cout << "|  4   |  Update Delegate                          |\n";
cout << "|  5   |  Delete Delegate                          |\n";
cout << "|  6   |  Exit                                     |\n";
cout << "+------+-------------------------------------------+\n";

cout << "Enter your choice: ";
cin >> ch;
cin.ignore();
        Delegate d;  
        switch(ch){
            case 1:
                d.input();
                delegates.push_back(d);
                exportDelegateData();
                break;
            case 2:
                exportDelegateData();
                display(); 
                break;
            case 3:
                search();
                break;
            case 4:
                update();
                exportDelegateData();
                break;
            case 5:
                deleteDelegate();
                exportDelegateData();
                break;
        }
    }
    while(ch<6);
   
}