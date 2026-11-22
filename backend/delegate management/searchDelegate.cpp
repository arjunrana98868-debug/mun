#include"class.h"

void search()
{
    try
    {
        sql::mysql::MySQL_Driver* driver;
driver = sql::mysql::get_mysql_driver_instance();
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
        int id;
        cout << "\nEnter Delegate ID: ";
        cin >> id;
        sql::PreparedStatement* pstmt =
            con->prepareStatement(
                "SELECT delegate_id, name, age, country, mun_attend "
                "FROM delegates "
                "WHERE delegate_id = ?"
            );
        pstmt->setInt(1, id);
        sql::ResultSet* res = pstmt->executeQuery();
        if (!res->next())
        {
            cout << "\nDelegate not found!\n";
            delete res;
            delete pstmt;
            return;
        }
       cout << "\n";
        cout << "+----+----------------------+-----+--------------+-----------------+----------------------+-----------------+\n";
        cout << "| ID |        Name          | Age |   Country    |   MUN ATTENDED  | Previous Committees  |     Awards      |\n";
        cout << "+----+----------------------+-----+--------------+-----------------+----------------------+-----------------+\n";
        cout << "| " << setw(2) << res->getInt("delegate_id") << " "
                     << "| " << setw(21) << left << res->getString("name")
                     << "| " << setw(3) << res->getInt("age") << " "
                     << "| " << setw(13) << left << res->getString("country")
                     << "| " << setw(15) << res->getInt("mun_attend") << " ";
        delete res;
        delete pstmt;
        pstmt = con->prepareStatement(
                "SELECT committee_name "
                "FROM previous_committees "
                "WHERE delegate_id = ?"
            );
        pstmt->setInt(1, id);

        res = pstmt->executeQuery();
        int count = 1;

        while (res->next())
        {
             cout << "| " <<count++<<"."<< setw(19) << left << res->getString("committee_name");
        }
        if (count == 1)
        {
            cout << "No previous committees.";
        }
        delete res;
        delete pstmt;
        pstmt = con->prepareStatement(
                "SELECT award_name "
                "FROM award "
                "WHERE delegate_id = ?"
            );
        pstmt->setInt(1, id);
        res = pstmt->executeQuery();
        count = 1;
        while (res->next())
        {
           cout << "| " << setw(16) << left <<count++<< res->getString("award_name")<<"|";    
        }
        if (count == 1)
        {
            cout <<"| " << setw(16) << left <<"No awards."<<"|";
        }
        cout << "\n+----+----------------------+-----+--------------+-----------------+----------------------+-----------------+\n";
        delete res;
        delete pstmt;
    }
    catch (sql::SQLException& e)
    {
        cout << "\nDatabase Error: " << e.what() << endl;
        cout << "Error Code: " << e.getErrorCode() << endl;
        cout << "SQL State: " << e.getSQLState() << endl;
    }
}