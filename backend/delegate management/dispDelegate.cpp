#include"class.h"
void display() {
    try {
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        unique_ptr<sql::Connection> con(
            driver->connect("tcp://127.0.0.1:3306", "root", "admin")
        );
        con->setSchema("mun_management");

        unique_ptr<sql::Statement> stmt(con->createStatement());
        unique_ptr<sql::ResultSet> res(
            stmt->executeQuery("SELECT * FROM delegates")
        );

        if (res->next()) {
            cout << "\n";
            cout << "+----+----------------------+-----+--------------+-----------------+----------------------+-----------------+\n";
            cout << "| ID |        Name          | Age |   Country    |   MUN ATTENDED  | Previous Committees  |     Awards      |\n";
            cout << "+----+----------------------+-----+--------------+-----------------+----------------------+-----------------+\n";

            do {
                int delegate_id = res->getInt("delegate_id");
                string name = res->getString("name");
                int age = res->getInt("age");
                string country = res->getString("country");
                int mun_attend = res->getInt("mun_attend");

                cout << "| " << setw(2) << delegate_id << " "
                     << "| " << setw(21) << left << name
                     << "| " << setw(3) << age << " "
                     << "| " << setw(13) << left << country
                     << "| " << setw(15) << mun_attend << " ";

                unique_ptr<sql::PreparedStatement> commStmt(
                    con->prepareStatement("SELECT committee_name FROM previous_committees WHERE delegate_id = ?")
                );
                commStmt->setInt(1, delegate_id);
                unique_ptr<sql::ResultSet> commRes(commStmt->executeQuery());

                string committees;
                while (commRes->next()) {
                    committees += commRes->getString("committee_name") + " ";
                }
                if (committees.empty()) committees = "None";

                cout << "| " << setw(21) << left << committees;
                unique_ptr<sql::PreparedStatement> awardStmt(
                    con->prepareStatement("SELECT award_name FROM award WHERE delegate_id = ?")
                );
                awardStmt->setInt(1, delegate_id);
                unique_ptr<sql::ResultSet> awardRes(awardStmt->executeQuery());
                string awards;
                while (awardRes->next()) {
                    awards += awardRes->getString("award_name") + " ";
                }
                if (awards.empty()) awards = "0";

                cout << "| " << setw(16) << left << awards << "|\n";

            } while (res->next());

            cout << "+----+----------------------+-----+--------------+-----------------+----------------------+-----------------+\n";
        } else {
            cout << "No delegates found.\n";
        }
    }
    catch (sql::SQLException &e) {
        cout << "Database Error: " << e.what() << endl;
    }
}
