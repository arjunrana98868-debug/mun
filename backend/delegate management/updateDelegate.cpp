#include <iostream> 
#include <string>
#include <mysql/jdbc.h>
using namespace std;
void update()
{
    try
    {
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
            delete con;

            return;
        }

        cout << "\n========== DELEGATE DETAILS ==========\n";

        cout << "Delegate ID: "
             << res->getInt("delegate_id") << endl;

        cout << "Name: "
             << res->getString("name") << endl;

        cout << "Age: "
             << res->getInt("age") << endl;

        cout << "Country: "
             << res->getString("country") << endl;

        cout << "MUN Attended: "
             << res->getInt("mun_attend") << endl;

        delete res;
        delete pstmt;


        pstmt =
            con->prepareStatement(
                "SELECT committee_id, committee_name "
                "FROM previous_committees "
                "WHERE delegate_id = ?"
            );

        pstmt->setInt(1, id);

        res = pstmt->executeQuery();

        cout << "\nPrevious Committees:\n";

        int committeeCount = 0;

        while (res->next())
        {
            committeeCount++;

            cout << committeeCount << ". "
                 << "ID: " << res->getInt("committee_id")
                 << "  "
                 << res->getString("committee_name")
                 << endl;
        }

        delete res;
        delete pstmt;


        pstmt =
            con->prepareStatement(
                "SELECT award_id, award_name "
                "FROM award "
                "WHERE delegate_id = ?"
            );

        pstmt->setInt(1, id);

        res = pstmt->executeQuery();

        cout << "\nAwards:\n";

        int awardCount = 0;

        while (res->next())
        {
            awardCount++;

            cout << awardCount << ". "
                 << "ID: " << res->getInt("award_id")
                 << "  "
                 << res->getString("award_name")
                 << endl;
        }

        delete res;
        delete pstmt;



        int ch;

        cout << "\nWhat do you want to update?\n";
        cout << "1. Age\n";
        cout << "2. Country\n";
        cout << "3. Previous Committees\n";
        cout << "4. MUN Attended\n";
        cout << "5. Awards\n";

        cout << "Enter choice: ";
        cin >> ch;



        if (ch == 1)
        {
            int age;

            cout << "Enter new age: ";
            cin >> age;

            pstmt =
                con->prepareStatement(
                    "UPDATE delegates "
                    "SET age = ? "
                    "WHERE delegate_id = ?"
                );

            pstmt->setInt(1, age);
            pstmt->setInt(2, id);

            pstmt->executeUpdate();

            cout << "\nAge updated successfully!\n";

            delete pstmt;
        }


        else if (ch == 2)
        {
            string country;

            cout << "Enter new country: ";
            cin.ignore();
            getline(cin, country);

            pstmt =
                con->prepareStatement(
                    "UPDATE delegates "
                    "SET country = ? "
                    "WHERE delegate_id = ?"
                );

            pstmt->setString(1, country);
            pstmt->setInt(2, id);

            pstmt->executeUpdate();

            cout << "\nCountry updated successfully!\n";

            delete pstmt;
        }


        else if (ch == 3)
        {
            int committee_id;
            string committee_name;

            cout << "\nEnter Committee ID to update: ";
            cin >> committee_id;

            cout << "Enter new committee name: ";
            cin.ignore();
            getline(cin, committee_name);

            pstmt =
                con->prepareStatement(
                    "UPDATE previous_committees "
                    "SET committee_name = ? "
                    "WHERE committee_id = ? "
                    "AND delegate_id = ?"
                );

            pstmt->setString(1, committee_name);
            pstmt->setInt(2, committee_id);
            pstmt->setInt(3, id);

            int rows = pstmt->executeUpdate();

            if (rows > 0)
                cout << "\nPrevious committee updated successfully!\n";
            else
                cout << "\nCommittee ID not found!\n";

            delete pstmt;
        }


        else if (ch == 4)
        {
            int mun;

            cout << "Enter new number of MUNs attended: ";
            cin >> mun;

            pstmt =
                con->prepareStatement(
                    "UPDATE delegates "
                    "SET mun_attend = ? "
                    "WHERE delegate_id = ?"
                );

            pstmt->setInt(1, mun);
            pstmt->setInt(2, id);

            pstmt->executeUpdate();

            cout << "\nMUN attended updated successfully!\n";

            delete pstmt;
        }

        else if (ch == 5)
        {
            int award_id;
            string award_name;

            cout << "\nEnter Award ID to update: ";
            cin >> award_id;

            cout << "Enter new award name: ";
            cin.ignore();
            getline(cin, award_name);

            pstmt =
                con->prepareStatement(
                    "UPDATE award "
                    "SET award_name = ? "
                    "WHERE award_id = ? "
                    "AND delegate_id = ?"
                );

            pstmt->setString(1, award_name);
            pstmt->setInt(2, award_id);
            pstmt->setInt(3, id);

            int rows = pstmt->executeUpdate();

            if (rows > 0)
                cout << "\nAward updated successfully!\n";
            else
                cout << "\nAward ID not found!\n";

            delete pstmt;
        }

        else
        {
            cout << "\nInvalid choice!\n";
        }

        delete con;
    }

    catch (sql::SQLException& e)
    {
        cout << "\nSQL Error: " << e.what() << endl;
    }
}