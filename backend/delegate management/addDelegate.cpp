#include "class.h"
 
 void Delegate::input(){
    cout << "\n";
    cout << "+--------------------------------------------------+\n";
    cout << "|                 ADD DELEGATE                     |\n";
    cout << "+--------------------------------------------------+\n";
     
    cout<<"\n+-------------------+---------------------------+\n";
    cout << "| Field             | Input                     |\n";
    cout << "+-------------------+---------------------------+\n";
    string committee;
     cout<<"Enter Delegate Name: ";
     getline(cin,name);
     cout<<"Enter Age: ";
    cin>>age;
    cin.ignore();
    cout<<"Enter Country: ";
    getline(cin,country);
    cout<<"How many previous committees attends? : ";
    cin>>num;
    cin.ignore();
    if(num>0){
     cout<<"Enter Previous Committees name: ";
     for(int i=0; i<num; i++){
            getline(cin,committee);
            prev_committees.push_back(committee);
        }
        cout<<"How many MUN have you attended?: ";
        cin>>mun_attend;
        cout<<"How many awards have you received?: ";
        cin>>num1;
        cin.ignore();
        for(int i=0;i<num1; i++){
            string award;
            cout<<"Enter award: "<<i+1<<":";
            getline(cin,award);
            awards.push_back(award);
        }
    }else{
        mun_attend = 0;
    }
        try{
        sql::mysql::MySQL_Driver* driver =
        sql::mysql::get_mysql_driver_instance();

    std::unique_ptr<sql::Connection> con(
        driver->connect(
            "tcp://127.0.0.1:3306",
            "root",
            "admin"
        )
    );
    con->setSchema("mun_management");
    std::unique_ptr<sql::PreparedStatement> pstmt(
        con->prepareStatement(
            "INSERT INTO delegates "
            "(name, age, country, mun_attend) "
            "VALUES (?, ?, ?, ?)"
        )
    );
    pstmt->setString(1, name);
    pstmt->setInt(2, age);
    pstmt->setString(3, country);
    pstmt->setInt(4, mun_attend);
    pstmt->executeUpdate();
    int delegate_id = 0;
    std::unique_ptr<sql::PreparedStatement> idStmt(
        con->prepareStatement(
            "SELECT delegate_id FROM delegates "
            "WHERE name = ? AND age = ? AND country = ? "
            "AND mun_attend = ? "
            "ORDER BY delegate_id DESC LIMIT 1"
        )
    );
    idStmt->setString(1, name);
    idStmt->setInt(2, age);
    idStmt->setString(3, country);
    idStmt->setInt(4, mun_attend);
    std::unique_ptr<sql::ResultSet> result(
        idStmt->executeQuery()
    );
    if (result->next()) {
        delegate_id = result->getInt("delegate_id");
        cout<<"Delegate_id:"<<delegate_id<<endl;
    }
    std::unique_ptr<sql::PreparedStatement> committeeStmt(
        con->prepareStatement(
            "INSERT INTO previous_committees (delegate_id, committee_name) VALUES (?, ?)"
        )
    );
    for (string committee : prev_committees) {
        committeeStmt->setInt(1, delegate_id);
        committeeStmt->setString(2, committee);
        committeeStmt->executeUpdate();
        cout<<"Committee Saved:"<<committee<<endl;
    }
    std::unique_ptr<sql::PreparedStatement> awardStmt(
        con->prepareStatement(
            "INSERT INTO award (delegate_id, award_name) VALUES (?, ?)"
        )
    );
    for (string award : awards) {
        awardStmt->setInt(1, delegate_id);
        awardStmt->setString(2, award);
        awardStmt->executeUpdate();
        cout<<"Award saved:"<<award<<endl;
    }
    cout << "Delegate saved to database successfully!" << endl;
}

catch (sql::SQLException &e) {
    cout << "Database Error: " << e.what() << endl;
        }
        cout << "+----------------------+---------------------------+\n";
        }