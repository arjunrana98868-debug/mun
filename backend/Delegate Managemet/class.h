#ifndef CLASS_H
#define CLASS_H
#include<iostream>
#include<memory>
#include<fstream>
#include<string>
#include<vector>
#include<iomanip>
#include<mysql/jdbc.h>
using namespace std;

class Delegate{
   public: 
   string name, country;
    int age, num, mun_attend, num1;
    vector<string> prev_committees;
    vector<string>awards;
    void input();
     };
sql::Connection* con = nullptr;
extern vector<Delegate> delegates;
void exportDelegateData() {
    try {
      sql::mysql::MySQL_Driver* driver;
        driver = sql::mysql::get_mysql_driver_instance();
        sql::Connection* con;
        con = driver->connect(
            "tcp://127.0.0.1:3306",
            "root",
            "admin"
        );
        con->setSchema("mun_management");
        if (con == nullptr)
        {
            cout << "\nDatabase connection is not available!\n";
            return;
        }
        ofstream file("../Data/delegate_data.txt",ios::out);
        if (!file.is_open()) {
            cout << "Cannot create delegate_data.txt\n";
            return;
        }
        unique_ptr<sql::Statement> stmt(
            con->createStatement()
        );
        unique_ptr<sql::ResultSet> delegates(
            stmt->executeQuery(
                "SELECT delegate_id, name, age, country, mun_attend "
                "FROM delegates ORDER BY delegate_id"
            )
        );
        while (delegates->next()) {
            int id = delegates->getInt("delegate_id");

            file << "Delegate ID: " << id << '\n';
            file << "Name: "
                 << delegates->getString("name") << '\n';
            file << "Age: "
                 << delegates->getInt("age") << '\n';
            file << "Country: "
                 << delegates->getString("country") << '\n';
            file << "MUN Attendance: "
                 << delegates->getInt("mun_attend") << '\n';
            unique_ptr<sql::PreparedStatement> awardStmt(
                con->prepareStatement(
                    "SELECT award_name FROM award "
                    "WHERE delegate_id = ?"
                )
            );
            awardStmt->setInt(1, id);
            unique_ptr<sql::ResultSet> award(
                awardStmt->executeQuery()
            );
            file << "Awards:\n";
            while (award->next()) {
                file << "  - "
                     << award->getString("award_name")
                     << '\n';
            }
            unique_ptr<sql::PreparedStatement> committeeStmt(
                con->prepareStatement(
                    "SELECT committee_name FROM previous_committees "
                    "WHERE delegate_id = ?"
                )
            );
            committeeStmt->setInt(1, id);
            unique_ptr<sql::ResultSet> committees(
                committeeStmt->executeQuery()
            );
            file << "Previous Committees:\n";
            while (committees->next()) {
                file << "  - "
                     << committees->getString("committee_name")
                     << '\n';
            }
            file << "--------------------------\n";
        }
        file.close();
        cout << "Export completed: delegate_data.txt\n";
    } catch (sql::SQLException& e) {
        cerr << "MySQL error: " << e.what() << '\n';
    }
}
#endif